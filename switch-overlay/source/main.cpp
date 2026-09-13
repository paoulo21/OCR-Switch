#define TESLA_INIT_IMPL
#include <memory>

#ifdef __SWITCH__
#include <switch.h>
#include <tesla.hpp>
#endif

#include "gui/overlay_gui.hpp"

#ifdef __SWITCH__

class FullscreenOcrUI : public tsl::gui::UI {
public:
    FullscreenOcrUI() {
        m_gui.init();
    }

    virtual ~FullscreenOcrUI() = default;

    virtual tsl::elm::Element* createUI() override {
        auto rootFrame = new tsl::elm::OverlayFrame("", "");
        return rootFrame;
    }

    virtual void update() override {
        u64 keysDown = hidKeysDown(CONTROLLER_P1_AUTO);
        u64 keysHeld = hidKeysHeld(CONTROLLER_P1_AUTO);

        // Touch handling
        hidScanInput();
        touchPosition touch;
        u32 touchCount = hidTouchCount();
        bool touching = (touchCount > 0);
        int tx = -1, ty = -1;
        if (touching) {
            hidTouchRead(&touch, 0);
            tx = touch.px;
            ty = touch.py;
        }

        m_gui.update(keysDown, keysHeld, tx, ty, touching);
    }

    virtual void draw(tsl::gfx::Renderer* renderer) override {
        m_gui.render();
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState& touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        // B button exits overlay
        if (keysDown & HidNpadButton_B) {
            tsl::goBack();
            return true;
        }
        return false;
    }

private:
    switch_ocr::OverlayGui m_gui;
};

class SwitchOcrOverlay : public tsl::Overlay {
public:
    virtual void initServices() override {
        smInitialize();
        capsInitialize();
    }

    virtual void exitServices() override {
        capsExit();
        smExit();
    }

    virtual void onShow() override {}
    virtual void onHide() override {}

    virtual std::unique_ptr<tsl::gui::UI> loadInitialUI() override {
        return std::make_unique<FullscreenOcrUI>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<SwitchOcrOverlay>(argc, argv);
}

#else

int main(int argc, char **argv) {
    // Non-Switch stub for syntax validation
    return 0;
}

#endif
