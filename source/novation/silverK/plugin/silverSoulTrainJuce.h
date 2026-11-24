#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <memory>

namespace silverK {

class Processor;

class SoulTrainJuce final : public juce::Component {
public:
    struct Callbacks {
        std::function<juce::var()> browserSnapshot;
        std::function<void(const juce::var&)> browserAction;
        std::function<void(const juce::String&)> nativeAction;
        std::function<juce::String()> nativeStatus;
        std::function<juce::var()> skinMenu;
    };

    SoulTrainJuce(Processor&, Callbacks, juce::File assets = {});
    ~SoulTrainJuce() override;
    void resized() override;
    bool showPage(const juce::String&);
    void refreshBrowser();
    juce::String currentScene() const;

private:
    class Canvas;
    juce::Component scaledSurface;
    std::unique_ptr<Canvas> canvas;
#if JUCE_WINDOWS
    class OpenGLMode;
    std::unique_ptr<OpenGLMode> openGLMode;
    void useOpenGL(bool);
    juce::String rendererLabel() const;
    juce::String rendererStatus() const;
#endif
};

}
