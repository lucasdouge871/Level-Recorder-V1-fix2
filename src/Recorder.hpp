#pragma once
#include <Geode/Geode.hpp>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "Proc.hpp"

namespace lr {

enum class Mode { Idle, Record, Play, Render };

struct Input {
    uint32_t frame = 0;   // physics step index since level start
    int button = 1;       // 1 = jump
    bool down = true;
    bool player2 = false;
};

struct Macro {
    std::string level;
    int levelID = 0;
    bool completed = false;
    std::vector<Input> inputs;
    uint32_t length() const { return inputs.empty() ? 0 : inputs.back().frame; }
    void clear() { inputs.clear(); completed = false; }
};

namespace io {
    std::filesystem::path macroDir();
    std::filesystem::path importDir();
    std::filesystem::path renderDir();
    bool save(Macro const& m, std::filesystem::path const& path);
    // Reads the native .lrm format and the generic text format (see README).
    bool load(Macro& m, std::filesystem::path const& path);
    // Newest first, from macros/ and imports/.
    std::vector<std::filesystem::path> list();
}

class Mic {
public:
    static bool available();
    bool start();
    bool stopAndSave(std::filesystem::path const& wav);
    void discard();
    bool active() const { return m_active; }
private:
    void run();
    std::atomic<bool> m_active{false};
    std::thread m_thread;
    std::vector<int16_t> m_pcm;
    std::mutex m_mutex;
};

class Video {
public:
    bool start(int fps, std::filesystem::path const& out);
    void capture();
    bool finish();
    void kill();
    bool active() const { return m_proc.running(); }
private:
    Proc m_proc;
    int m_w = 0, m_h = 0;
    std::vector<uint8_t> m_buf;
};

struct State {
    static State& get();

    Mode mode = Mode::Idle;
    Mode selected = Mode::Record;
    bool pending = false;     // start requested, waiting for the level restart
    bool armed = false;       // first physics step has run
    bool completed = false;
    bool captureDue = false;
    bool injecting = false;
    bool micOn = false;
    int volume = 80;          // 0-100, mic gain at mux time
    float speed = 1.f;        // preview playback speed
    uint32_t frame = 0;
    size_t idx = 0;
    uint32_t tailCount = 0, tailTarget = 0;
    int fps = 60;

    Macro macro;
    std::filesystem::path macroPath;
    Mic mic;
    Video video;
    std::filesystem::path videoTmp;
    std::string musicPath;

    // UI-facing
    bool begin(Mode m);          // validates + arms; caller then restarts the level
    void stop();                 // finish current action (saves macro / finalises video)
    void abort();                // cancel and discard
    bool loadMacro(std::filesystem::path const& p);

    // Hook-facing
    void onReset(PlayLayer* pl);
    void preStep(GJBaseGameLayer* g);
    bool onButton(bool down, int button, bool isP1);
    void onComplete(PlayLayer* pl);
    void onQuit();
    void tick();
    void onSwap();

private:
    void startRun(PlayLayer* pl);
    void finishRecord();
    void finishRender();
    void setTimeScale(float s);
};

void notify(std::string const& msg, bool error = false);

}
