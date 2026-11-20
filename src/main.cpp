#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include "Recorder.hpp"
#include "Panel.hpp"

using namespace geode::prelude;

static bool isPlay(GJBaseGameLayer* g) {
    return static_cast<GJBaseGameLayer*>(PlayLayer::get()) == g;
}

class $modify(LRPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        lr::State::get().onReset(this);
    }
    void levelComplete() {
        PlayLayer::levelComplete();
        lr::State::get().onComplete(this);
    }
    void onQuit() {
        lr::State::get().onQuit();
        PlayLayer::onQuit();
    }
};

class $modify(LRBaseGameLayer, GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (!isPlay(this)) {
            GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
            return;
        }
        lr::State::get().preStep(this);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
    void handleButton(bool down, int button, bool isPlayer1) {
        if (isPlay(this) && !lr::State::get().onButton(down, button, isPlayer1)) return;
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

// Fixed timestep while rendering => game lag can never drop video frames.
class $modify(LRScheduler, CCScheduler) {
    void update(float dt) {
        auto& s = lr::State::get();
        if (s.mode == lr::Mode::Render && s.video.active()) dt = 1.f / static_cast<float>(s.fps);
        CCScheduler::update(dt);
        s.tick();
    }
};

class $modify(LRView, CCEGLView) {
    void swapBuffers() {
        lr::State::get().onSwap();
        CCEGLView::swapBuffers();
    }
};

class $modify(LRPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto logoPath = Mod::get()->expandSpriteName("logo-button.png");
        auto spr = CCSprite::create(logoPath.c_str());
        if (!spr) return;
        spr->setScale(30.f / spr->getContentSize().width);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(LRPauseLayer::onLR));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({32.f, 32.f});
        menu->setID("lr-menu"_spr);
        this->addChild(menu);
    }
    void onLR(CCObject*) {
        lr::Panel::open(this);
    }
};
