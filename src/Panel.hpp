#pragma once
#include <Geode/Geode.hpp>
#include <filesystem>
#include <vector>

namespace lr {

// The media-player style control bar, laid out in the coordinate space of the
// original mockup (663x112) and scaled to fit.
class Panel : public cocos2d::CCLayer {
public:
    static Panel* open(PauseLayer* parent);

    bool init(PauseLayer* parent);
    void registerWithTouchDispatcher() override;
    bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchMoved(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void ccTouchCancelled(cocos2d::CCTouch*, cocos2d::CCEvent*) override;
    void keyBackClicked() override;
    void update(float dt) override;

private:
    enum class Drag { None, Speed, Volume };

    cocos2d::CCPoint toMock(cocos2d::CCTouch*);
    cocos2d::CCPoint fromMock(float mx, float my);
    cocos2d::CCSprite* sprite(char const* name, float x0, float y0, float x1, float y1, int z = 0);
    cocos2d::CCLabelBMFont* label(std::string const& text, float mx, float my, float mockHeight, bool semi = false);
    void buildFileList();
    void setSpeedFromX(float mx);
    void setVolumeFromX(float mx);
    void onStartStop();
    void close();

    PauseLayer* m_pause = nullptr;
    float m_s = 1.f, m_h = 0.f;
    Drag m_drag = Drag::None;

    cocos2d::CCSprite* m_iconPlay = nullptr;
    cocos2d::CCSprite* m_iconPause = nullptr;
    cocos2d::CCSprite* m_seekThumb = nullptr;
    cocos2d::CCSprite* m_speedThumb = nullptr;
    cocos2d::CCSprite* m_volThumb = nullptr;
    cocos2d::CCLabelBMFont* m_volValue = nullptr;
    cocos2d::CCLabelBMFont* m_time = nullptr;
    cocos2d::CCLabelBMFont* m_modeLabels[3] = {};
    cocos2d::CCLabelBMFont* m_micLabel = nullptr;

    cocos2d::CCNode* m_fileList = nullptr;
    std::vector<std::filesystem::path> m_files;
};

}
