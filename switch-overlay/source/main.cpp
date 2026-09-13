#define TESLA_INIT_IMPL
#include <memory>

#ifdef __SWITCH__
#include <switch.h>
#include <tesla.hpp>
#endif

#include "overlay_gui.hpp"
#include "screen_capture.hpp"

#ifdef __SWITCH__

class FullscreenOcrGui : public tsl::Gui {
public:
    FullscreenOcrGui() {
        m_gui.init();
    }

    virtual ~FullscreenOcrGui() = default;

    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("Switch OCR", "v1.0.0");
        auto drawer = new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            m_gui.render(renderer, x, y, w, h);
        });
        frame->setContent(drawer);
        return frame;
    }

    virtual void update() override {
        m_gui.update();
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
        return m_gui.handleInput(keysDown, keysHeld, touchPos, leftJoyStick, rightJoyStick);
    }

private:
    switch_ocr::OverlayGui m_gui;
};

class SwitchOcrOverlay : public tsl::Overlay {
public:
    virtual void initServices() override {
        switch_ocr::ScreenCapture::initialize();
        socketInitializeDefault();
    }

    virtual void exitServices() override {
        socketExit();
        switch_ocr::ScreenCapture::exit();
    }

    virtual void onShow() override {}
    virtual void onHide() override {}

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<FullscreenOcrGui>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<SwitchOcrOverlay>(argc, argv);
}

#else

int main(int argc, char **argv) {
    return 0;
}

#endif
