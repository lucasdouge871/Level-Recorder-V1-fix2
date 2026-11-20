#include "Panel.hpp"
#include "Recorder.hpp"
#include <Geode/ui/GeodeUI.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace geode::prelude;

namespace lr {

// ---- layout constants, in mockup pixels (origin top-left, y down) ----
static constexpr float MW = 663.f, MH = 112.f;
static constexpr float SPEED_X0 = 50.f, SPEED_X1 = 232.f, SPEED_Y = 30.f;
static constexpr float SPEED_STOPS_X[6] = {49.f, 82.f, 112.f, 139.f, 181.f, 225.f};
static constexpr float SPEED_VALUES[6] = {0.25f, 0.5f, 0.75f, 1.f, 2.f, 4.f};
static constexpr char const* SPEED_NAMES[6] = {"0.25x", "0.50x", "0.75x", "1x", "2x", "4x"};
static constexpr float VOL_X0 = 342.f, VOL_X1 = 524.f, VOL_Y = 35.f;
static constexpr float SEEK_X0 = 52.f, SEEK_X1 = 604.f, SEEK_Y = 82.f;
static constexpr float BTN_X0 = 250.f, BTN_Y0 = 5.f, BTN_X1 = 308.f, BTN_Y1 = 64.f;
static constexpr float ROW_Y = 103.f;

static char const* modeName(int i) { return i == 0 ? "REC" : i == 1 ? "PLAY" : "RENDER"; }
static float modeX(int i) { return i == 0 ? 75.f : i == 1 ? 135.f : 205.f; }

static std::string fontFile(bool semi) {
    std::string name = Mod::get()->expandSpriteName(semi ? "rubik-semibold.fnt" : "rubik-regular.fnt");
    auto full = CCFileUtils::sharedFileUtils()->fullPathForFilename(name.c_str(), false);
    if (CCFileUtils::sharedFileUtils()->isFileExist(full.c_str())) return name;
    return "bigFont.fnt"; // fallback so the UI never crashes if the font is missing
}

Panel* Panel::open(PauseLayer* parent) {
    if (parent->getChildByID("lr-panel"_spr)) return nullptr;
    auto p = new Panel();
    if (p && p->init(parent)) {
        p->autorelease();
        p->setID("lr-panel"_spr);
        parent->addChild(p, 1000);
        return p;
    }
    CC_SAFE_DELETE(p);
    return nullptr;
}

bool Panel::init(PauseLayer* parent) {
    if (!CCLayer::init()) return false;
    m_pause = parent;

    auto win = CCDirector::get()->getWinSize();
    float W = std::min(win.width - 20.f, 520.f);
    m_s = W / MW;
    m_h = MH * m_s;
    this->setContentSize({W, m_h});
    this->ignoreAnchorPointForPosition(false);
    this->setAnchorPoint({0.5f, 0.f});
    this->setPosition({win.width / 2.f, 8.f});

    auto& st = State::get();
    st.volume = Mod::get()->getSavedValue<int>("volume", st.volume);
    st.micOn = Mod::get()->getSavedValue<bool>("mic-on", st.micOn);

    // background + frames
    sprite("main-bar.png", 0, 0, MW, MH, -1);
    sprite("menu-bar.png", 530, 13, 641, 29);
    sprite("record-frame.png", BTN_X0, BTN_Y0, BTN_X1, BTN_Y1);
    m_iconPlay = sprite("play.png", 270, 20, 292, 50, 1);
    m_iconPause = sprite("pause.png", 268, 22, 291, 48, 1);
    sprite("stop-x.png", 313, 46, 333, 66);

    // tracks
    sprite("speed-track.png", SPEED_X0, SPEED_Y - 5, SPEED_X1, SPEED_Y + 5);
    sprite("volume-track.png", VOL_X0, VOL_Y - 5, VOL_X1, VOL_Y + 5);
    sprite("seek-track.png", 44, SEEK_Y - 6, 612, SEEK_Y + 6);
    sprite("volume-box.png", 456, 9, 500, 29);

    m_speedThumb = sprite("seek-thumb.png", 0, 0, 14, 14, 2);
    m_volThumb = sprite("seek-thumb.png", 0, 0, 14, 14, 2);
    m_seekThumb = sprite("seek-thumb.png", 0, 0, 16, 16, 2);

    // labels
    for (int i = 0; i < 6; i++) label(SPEED_NAMES[i], SPEED_STOPS_X[i], 16, i == 3 ? 15.f : 9.f);
    label("Speed", 137, 44, 11);
    label("Volume", 372, 18, 11);
    m_volValue = label("80", 478, 19, 11);
    m_time = label("0:00.0", 570, ROW_Y, 11);
    for (int i = 0; i < 3; i++) m_modeLabels[i] = label(modeName(i), modeX(i), ROW_Y, 12, true);
    m_micLabel = label("MIC", 275, ROW_Y, 12, true);

    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);
    this->scheduleUpdate();
    update(0.f);
    return true;
}

void Panel::registerWithTouchDispatcher() {
    CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -600, true);
}

CCPoint Panel::toMock(CCTouch* t) {
    auto p = this->convertTouchToNodeSpace(t);
    return {p.x / m_s, (m_h - p.y) / m_s};
}

CCPoint Panel::fromMock(float mx, float my) {
    return {mx * m_s, m_h - my * m_s};
}

CCSprite* Panel::sprite(char const* name, float x0, float y0, float x1, float y1, int z) {
    std::string path = Mod::get()->expandSpriteName(name);
    auto spr = CCSprite::create(path.c_str());
    if (!spr) { spr = CCSprite::create(); spr->setVisible(false); } // missing texture: hidden, not garbage
    auto cs = spr->getContentSize();
    if (cs.width > 0 && cs.height > 0) {
        spr->setScaleX((x1 - x0) * m_s / cs.width);
        spr->setScaleY((y1 - y0) * m_s / cs.height);
    }
    spr->setPosition(fromMock((x0 + x1) / 2.f, (y0 + y1) / 2.f));
    this->addChild(spr, z);
    return spr;
}

CCLabelBMFont* Panel::label(std::string const& text, float mx, float my, float mockHeight, bool semi) {
    auto l = CCLabelBMFont::create(text.c_str(), fontFile(semi).c_str());
    float targetH = mockHeight * m_s;
    float h = l->getContentSize().height;
    if (h > 0) l->setScale(targetH / h);
    l->setPosition(fromMock(mx, my));
    l->setColor({205, 190, 235});
    this->addChild(l, 3);
    return l;
}

// ---- interaction ----
void Panel::setSpeedFromX(float mx) {
    int best = 0;
    for (int i = 1; i < 6; i++)
        if (std::fabs(SPEED_STOPS_X[i] - mx) < std::fabs(SPEED_STOPS_X[best] - mx)) best = i;
    State::get().speed = SPEED_VALUES[best];
    if (State::get().mode == Mode::Play)
        CCDirector::get()->getScheduler()->setTimeScale(State::get().speed);
}

void Panel::setVolumeFromX(float mx) {
    float t = std::clamp((mx - VOL_X0) / (VOL_X1 - VOL_X0), 0.f, 1.f);
    State::get().volume = static_cast<int>(std::round(t * 100.f));
    Mod::get()->setSavedValue("volume", State::get().volume);
}

void Panel::onStartStop() {
    auto& st = State::get();
    if (st.mode != Mode::Idle) {
        st.stop();
        return;
    }
    if (st.begin(st.selected) && m_pause) {
        auto pause = m_pause;
        this->removeFromParent();
        pause->onRestart(nullptr); // restarts the level; State picks it up in resetLevel
    }
}

void Panel::close() { this->removeFromParent(); }
void Panel::keyBackClicked() { close(); }

bool Panel::ccTouchBegan(CCTouch* t, CCEvent*) {
    auto m = toMock(t);
    auto& st = State::get();

    // file list overlay sits above the bar
    if (m_fileList) {
        if (m.x >= 520 && m.x <= 663 && m.y >= -156 && m.y <= -4) {
            float row = (m.y + 156.f - 22.f) / 17.f; // header takes the first 22px
            int i = static_cast<int>(std::floor(row));
            if (i >= 0 && i < static_cast<int>(m_files.size()) && i < 7) {
                st.loadMacro(m_files[i]);
                m_fileList->removeFromParent();
                m_fileList = nullptr;
            }
            return true;
        }
        m_fileList->removeFromParent();
        m_fileList = nullptr;
    }

    if (m.x < 0 || m.x > MW || m.y < 0 || m.y > MH) return false; // let the pause menu work

    // start / stop box
    if (m.x >= BTN_X0 && m.x <= BTN_X1 && m.y >= BTN_Y0 && m.y <= BTN_Y1) { onStartStop(); return true; }
    // X = cancel and discard
    if (m.x >= 311 && m.x <= 335 && m.y >= 44 && m.y <= 68) { st.abort(); return true; }
    // top-right menu: Exit | File | Settings
    if (m.y >= 11 && m.y <= 31 && m.x >= 530 && m.x <= 641) {
        if (m.x < 568) close();
        else if (m.x < 602) buildFileList();
        else geode::openSettingsPopup(Mod::get());
        return true;
    }
    // mode selector + mic toggle
    if (m.y >= 93 && m.y <= 113) {
        for (int i = 0; i < 3; i++) {
            if (std::fabs(m.x - modeX(i)) < 28 && st.mode == Mode::Idle) {
                st.selected = i == 0 ? Mode::Record : i == 1 ? Mode::Play : Mode::Render;
                return true;
            }
        }
        if (std::fabs(m.x - 275) < 22) {
            st.micOn = !st.micOn;
            Mod::get()->setSavedValue("mic-on", st.micOn);
            if (st.micOn && !Mic::available()) notify("No microphone detected", true);
            return true;
        }
    }
    // sliders
    if (m.y >= 18 && m.y <= 42 && m.x >= SPEED_X0 - 8 && m.x <= SPEED_X1 + 8) {
        m_drag = Drag::Speed; setSpeedFromX(m.x); return true;
    }
    if (m.y >= 24 && m.y <= 46 && m.x >= VOL_X0 - 8 && m.x <= VOL_X1 + 8) {
        m_drag = Drag::Volume; setVolumeFromX(m.x); return true;
    }
    return true; // swallow the rest of the bar
}

void Panel::ccTouchMoved(CCTouch* t, CCEvent*) {
    auto m = toMock(t);
    if (m_drag == Drag::Speed) setSpeedFromX(m.x);
    else if (m_drag == Drag::Volume) setVolumeFromX(m.x);
}
void Panel::ccTouchEnded(CCTouch*, CCEvent*) { m_drag = Drag::None; }
void Panel::ccTouchCancelled(CCTouch*, CCEvent*) { m_drag = Drag::None; }

void Panel::buildFileList() {
    if (m_fileList) { m_fileList->removeFromParent(); m_fileList = nullptr; }
    m_files = io::list();
    m_fileList = CCNode::create();
    this->addChild(m_fileList, 10);

    std::string bgPath = Mod::get()->expandSpriteName("side-panel.png");
    auto bg = CCSprite::create(bgPath.c_str());
    if (bg) {
        auto cs = bg->getContentSize();
        bg->setScaleX(143.f * m_s / cs.width);
        bg->setScaleY(152.f * m_s / cs.height);
        bg->setPosition(fromMock(591.5f, -80.f));
        m_fileList->addChild(bg);
    }
    auto head = CCLabelBMFont::create("Macros (tap to load)", fontFile(true).c_str());
    head->limitLabelWidth(130.f * m_s, 1.f, 0.05f);
    head->setPosition(fromMock(591.5f, -145.f));
    m_fileList->addChild(head);

    if (m_files.empty()) {
        auto e = CCLabelBMFont::create("No macros yet", fontFile(false).c_str());
        e->limitLabelWidth(120.f * m_s, 1.f, 0.05f);
        e->setPosition(fromMock(591.5f, -100.f));
        m_fileList->addChild(e);
    }
    for (size_t i = 0; i < m_files.size() && i < 7; i++) {
        auto name = geode::utils::string::pathToString(m_files[i].stem());
        auto l = CCLabelBMFont::create(name.c_str(), fontFile(false).c_str());
        l->limitLabelWidth(128.f * m_s, 1.f, 0.05f);
        l->setPosition(fromMock(591.5f, -156.f + 22.f + 17.f * i + 8.5f));
        m_fileList->addChild(l);
    }
}

void Panel::update(float) {
    auto& st = State::get();

    bool active = st.mode != Mode::Idle;
    m_iconPause->setVisible(active);
    m_iconPlay->setVisible(!active);

    auto speedIdx = 3;
    for (int i = 0; i < 6; i++) if (std::fabs(SPEED_VALUES[i] - st.speed) < 0.001f) speedIdx = i;
    m_speedThumb->setPosition(fromMock(SPEED_STOPS_X[speedIdx], SPEED_Y));
    m_volThumb->setPosition(fromMock(VOL_X0 + (VOL_X1 - VOL_X0) * st.volume / 100.f, VOL_Y));
    m_volValue->setString(std::to_string(st.volume).c_str());

    float total = static_cast<float>(std::max<uint32_t>(st.macro.length(), 1));
    float prog = (st.mode == Mode::Play || st.mode == Mode::Render) ? std::min(st.frame / total, 1.f) : 0.f;
    m_seekThumb->setPosition(fromMock(SEEK_X0 + (SEEK_X1 - SEEK_X0) * prog, SEEK_Y));

    float secs = st.frame / 240.f; // default TPS
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d:%04.1f", static_cast<int>(secs) / 60, std::fmod(secs, 60.f));
    m_time->setString(buf);

    for (int i = 0; i < 3; i++) {
        Mode md = i == 0 ? Mode::Record : i == 1 ? Mode::Play : Mode::Render;
        bool on = st.selected == md;
        m_modeLabels[i]->setColor(on ? ccColor3B{235, 120, 255} : ccColor3B{150, 130, 175});
    }
    m_micLabel->setColor(st.micOn ? ccColor3B{235, 120, 255} : ccColor3B{150, 130, 175});
}

}
