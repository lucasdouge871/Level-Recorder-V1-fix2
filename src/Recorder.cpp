#include "Recorder.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>

using namespace geode::prelude;
namespace fs = std::filesystem;

namespace lr {

// ---------- helpers ----------
void notify(std::string const& msg, bool error) {
    Loader::get()->queueInMainThread([msg, error] {
        Notification::create(msg, error ? NotificationIcon::Error : NotificationIcon::Success)->show();
    });
}

static std::string pstr(fs::path const& p) { return geode::utils::string::pathToString(p); }

static std::string safeName(std::string s) {
    for (auto& c : s) if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
    if (s.empty()) s = "level";
    return s.substr(0, 40);
}

static std::string stamp() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char b[32];
    std::strftime(b, sizeof b, "%Y%m%d-%H%M%S", std::localtime(&t));
    return b;
}

// ---------- io ----------
namespace io {
    static fs::path dir(char const* n) {
        auto p = Mod::get()->getSaveDir() / n;
        std::error_code ec;
        fs::create_directories(p, ec);
        return p;
    }
    fs::path macroDir()  { return dir("macros"); }
    fs::path importDir() { return dir("imports"); }
    fs::path renderDir() { return dir("renders"); }

    bool save(Macro const& m, fs::path const& path) {
        std::ofstream f(path);
        if (!f) return false;
        f << "#LevelRecorder 1\nlevel=" << m.level << "\nid=" << m.levelID
          << "\ncompleted=" << (m.completed ? 1 : 0) << "\n";
        for (auto& i : m.inputs)
            f << i.frame << ' ' << i.button << ' ' << (i.down ? 1 : 0) << ' ' << (i.player2 ? 1 : 0) << '\n';
        return true;
    }

    bool load(Macro& m, fs::path const& path) {
        std::ifstream f(path);
        if (!f) return false;
        Macro out;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (auto eq = line.find('='); eq != std::string::npos) {
                auto k = line.substr(0, eq), v = line.substr(eq + 1);
                if (k == "level") out.level = v;
                else if (k == "id") out.levelID = atoi(v.c_str());
                else if (k == "completed") out.completed = atoi(v.c_str()) != 0;
                continue;
            }
            std::istringstream ss(line);
            long a[4]; int n = 0;
            while (n < 4 && (ss >> a[n])) n++;
            Input in;
            if (n == 4) { in = {static_cast<uint32_t>(a[0]), static_cast<int>(a[1]), a[2] != 0, a[3] != 0}; }
            else if (n == 3) { in = {static_cast<uint32_t>(a[0]), 1, a[1] != 0, a[2] != 0}; }
            else if (n == 2) { in = {static_cast<uint32_t>(a[0]), 1, a[1] != 0, false}; }
            else continue;
            out.inputs.push_back(in);
        }
        std::stable_sort(out.inputs.begin(), out.inputs.end(),
                         [](Input const& x, Input const& y) { return x.frame < y.frame; });
        if (out.inputs.empty()) return false;
        m = std::move(out);
        return true;
    }

    std::vector<fs::path> list() {
        std::vector<fs::path> v;
        for (auto const& d : {macroDir(), importDir()}) {
            std::error_code ec;
            for (auto& e : fs::directory_iterator(d, ec)) {
                auto ext = pstr(e.path().extension());
                if (ext == ".lrm" || ext == ".txt") v.push_back(e.path());
            }
        }
        std::sort(v.begin(), v.end(), [](fs::path const& a, fs::path const& b) {
            std::error_code ec;
            return fs::last_write_time(a, ec) > fs::last_write_time(b, ec);
        });
        return v;
    }
}

// ---------- Video ----------
bool Video::start(int fps, fs::path const& out) {
    auto view = CCDirector::get()->getOpenGLView();
    auto fs_ = view->getFrameSize();
    m_w = static_cast<int>(fs_.width) & ~1;
    m_h = static_cast<int>(fs_.height) & ~1;
    m_buf.resize(static_cast<size_t>(fs_.width) * static_cast<size_t>(fs_.height) * 3);

    auto ff = Mod::get()->getSettingValue<std::string>("ffmpeg-path");
    auto codec = Mod::get()->getSettingValue<std::string>("codec");
    auto br = Mod::get()->getSettingValue<std::string>("bitrate");

    std::ostringstream c;
    c << '"' << ff << "\" -y -f rawvideo -pix_fmt rgb24 -s " << static_cast<int>(fs_.width) << 'x'
      << static_cast<int>(fs_.height) << " -r " << fps << " -i - -vf \"vflip,crop=" << m_w << ':' << m_h
      << "\" -c:v " << codec << " -b:v " << br << " -pix_fmt yuv420p \"" << pstr(out) << '"';
    return m_proc.start(c.str(), true);
}

void Video::capture() {
    if (!m_proc.running()) return;
    auto fs_ = CCDirector::get()->getOpenGLView()->getFrameSize();
    if (static_cast<size_t>(fs_.width) * static_cast<size_t>(fs_.height) * 3 != m_buf.size()) return; // window resized
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, static_cast<GLsizei>(fs_.width), static_cast<GLsizei>(fs_.height),
                 GL_RGB, GL_UNSIGNED_BYTE, m_buf.data());
    m_proc.write(m_buf.data(), m_buf.size());
}

bool Video::finish() { return m_proc.finish() == 0; }
void Video::kill() { m_proc.kill(); }

// ---------- State ----------
State& State::get() { static State s; return s; }

void State::setTimeScale(float s) {
    CCDirector::get()->getScheduler()->setTimeScale(s);
}

bool State::loadMacro(fs::path const& p) {
    Macro m;
    if (!io::load(m, p)) { notify("Could not read macro", true); return false; }
    macro = std::move(m);
    macroPath = p;
    notify("Loaded " + pstr(p.filename()));
    return true;
}

bool State::begin(Mode m) {
    if (mode != Mode::Idle) return false;
    if (!PlayLayer::get()) return false;

    if (m == Mode::Play || m == Mode::Render) {
        if (macro.inputs.empty()) { notify("No macro loaded. Record or load one first.", true); return false; }
    }
    if (m == Mode::Render) {
        auto ff = Mod::get()->getSettingValue<std::string>("ffmpeg-path");
        if (runHidden('"' + ff + "\" -version") != 0) {
            notify("ffmpeg not found. Set its path in the mod settings.", true);
            return false;
        }
    }
    if (m == Mode::Record && micOn && !Mic::available()) {
        notify("No microphone found, recording without it.", true);
    }
    mode = m;
    pending = true;
    return true;
}

void State::startRun(PlayLayer* pl) {
    frame = 0; idx = 0; armed = false; completed = false; captureDue = false;
    tailCount = 0; tailTarget = 0;
    switch (mode) {
        case Mode::Record:
            macro.clear();
            macro.level = std::string(pl->m_level->m_levelName.c_str());
            macro.levelID = 0;
            mic.discard();
            if (micOn && Mic::available()) mic.start();
            break;
        case Mode::Play:
            setTimeScale(speed);
            break;
        case Mode::Render: {
            fps = Mod::get()->getSettingValue<int64_t>("render-fps");
            tailTarget = static_cast<uint32_t>(Mod::get()->getSettingValue<double>("tail-seconds") * fps);
            videoTmp = io::renderDir() / (safeName(macro.level) + "_" + stamp() + ".video.mp4");
            if (!video.start(fps, videoTmp)) {
                notify("Could not start ffmpeg", true);
                mode = Mode::Idle;
                return;
            }
            musicPath.clear();
            if (Mod::get()->getSettingValue<bool>("include-music")) {
                auto* lvl = pl->m_level;
                if (lvl->m_songID) {
                    musicPath = std::string(MusicDownloadManager::sharedState()->pathForSong(lvl->m_songID).c_str());
                } else {
                    auto n = LevelTools::getAudioFileName(lvl->m_audioTrack);
                    musicPath = std::string(CCFileUtils::sharedFileUtils()->fullPathForFilename(n.c_str(), false).c_str());
                }
            }
            break;
        }
        default: break;
    }
}

void State::onReset(PlayLayer* pl) {
    if (mode == Mode::Idle) return;
    if (pending) {
        pending = false;
        startRun(pl);
        return;
    }
    if (mode == Mode::Record) {
        startRun(pl); // new attempt: only the latest attempt is kept
    } else if (armed) {
        // died or restarted during playback/render: the macro no longer matches
        notify("Run desynced or restarted, stopping.", true);
        abort();
    }
}

void State::preStep(GJBaseGameLayer* g) {
    if (mode == Mode::Idle || pending) return;
    armed = true;
    // use the game's own step counter so recording and replay agree exactly
    frame = static_cast<uint32_t>(g->m_gameState.m_currentProgress);
    if (mode == Mode::Play || mode == Mode::Render) {
        injecting = true;
        while (idx < macro.inputs.size() && macro.inputs[idx].frame <= frame) {
            auto& in = macro.inputs[idx++];
            g->handleButton(in.down, in.button, !in.player2);
        }
        injecting = false;
    }
}

bool State::onButton(bool down, int button, bool isP1) {
    if (mode == Mode::Record && armed) {
        macro.inputs.push_back({frame, button, down, !isP1});
        return true;
    }
    if ((mode == Mode::Play || mode == Mode::Render) && !injecting) return false; // block real input
    return true;
}

void State::onComplete(PlayLayer*) {
    switch (mode) {
        case Mode::Record:
            macro.completed = true;
            finishRecord();
            break;
        case Mode::Play:
            mode = Mode::Idle;
            setTimeScale(1.f);
            notify("Playback finished");
            break;
        case Mode::Render:
            completed = true; // keep rendering the end screen for the tail
            break;
        default: break;
    }
}

void State::onQuit() {
    if (mode == Mode::Record) finishRecord();
    else if (mode == Mode::Play) { mode = Mode::Idle; setTimeScale(1.f); }
    else if (mode == Mode::Render) { notify("Render cancelled (left level)", true); abort(); }
    pending = false;
}

void State::tick() {
    if (mode != Mode::Render || !armed) return;
    captureDue = true;
    if (completed && ++tailCount >= tailTarget) finishRender();
}

void State::onSwap() {
    if (captureDue) {
        captureDue = false;
        video.capture();
    }
}

void State::stop() {
    switch (mode) {
        case Mode::Record: finishRecord(); break;
        case Mode::Render: finishRender(); break;
        case Mode::Play: mode = Mode::Idle; setTimeScale(1.f); break;
        default: break;
    }
}

void State::abort() {
    if (mode == Mode::Render) {
        video.kill();
        std::error_code ec;
        fs::remove(videoTmp, ec);
    }
    if (mode == Mode::Record) { mic.discard(); macro.clear(); }
    setTimeScale(1.f);
    mode = Mode::Idle;
    pending = false; captureDue = false;
}

void State::finishRecord() {
    mode = Mode::Idle;
    if (macro.inputs.empty() && !macro.completed) {
        mic.discard();
        notify("Nothing recorded", true);
        return;
    }
    auto stem = safeName(macro.level) + "_" + stamp();
    macroPath = io::macroDir() / (stem + ".lrm");
    bool ok = io::save(macro, macroPath);
    if (mic.active()) mic.stopAndSave(io::macroDir() / (stem + ".wav"));
    notify(ok ? (macro.completed ? "Completion recorded and saved" : "Macro saved") : "Failed to save macro", !ok);
}

void State::finishRender() {
    mode = Mode::Idle;
    captureDue = false;
    bool ok = video.finish();
    auto tmp = videoTmp;
    if (!ok) {
        notify("ffmpeg failed to finish the video", true);
        return;
    }
    auto stem = macroPath.empty() ? safeName(macro.level) : pstr(macroPath.stem());
    auto out = io::renderDir() / (stem + "_" + stamp() + ".mp4");
    auto micWav = macroPath.empty() ? fs::path{} : macroPath.parent_path() / (macroPath.stem().string() + ".wav");
    bool useMic = micOn && !micWav.empty() && fs::exists(micWav);
    auto music = musicPath;
    float gain = volume / 100.f;
    auto ff = Mod::get()->getSettingValue<std::string>("ffmpeg-path");

    std::thread([=] {
        std::ostringstream c;
        bool hasMusic = !music.empty() && fs::exists(music);
        c << '"' << ff << "\" -y -i \"" << pstr(tmp) << '"';
        int next = 1, musicIdx = -1, micIdx = -1;
        if (hasMusic) { c << " -i \"" << music << '"'; musicIdx = next++; }
        if (useMic)   { c << " -i \"" << pstr(micWav) << '"'; micIdx = next++; }

        if (musicIdx < 0 && micIdx < 0) {
            std::error_code ec;
            fs::rename(tmp, out, ec);
            notify(ec ? "Render finished, but could not move file" : "Render saved to the renders folder", !!ec);
            return;
        }
        if (musicIdx >= 0 && micIdx >= 0) {
            c << " -filter_complex \"[" << micIdx << ":a]volume=" << gain << "[m];[" << musicIdx
              << ":a][m]amix=inputs=2:duration=longest[aout]\" -map 0:v -map \"[aout]\"";
        } else if (micIdx >= 0) {
            c << " -af volume=" << gain << " -map 0:v -map " << micIdx << ":a";
        } else {
            c << " -map 0:v -map " << musicIdx << ":a";
        }
        c << " -c:v copy -c:a aac -b:a 192k -shortest \"" << pstr(out) << '"';

        int rc = runHidden(c.str());
        std::error_code ec;
        if (rc == 0) fs::remove(tmp, ec);
        notify(rc == 0 ? "Render saved to the renders folder" : "Audio mux failed, video-only file kept", rc != 0);
    }).detach();
}

}
