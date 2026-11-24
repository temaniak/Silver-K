#pragma once
#include "silverProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace silverK
{
class Browser;
#if SILVER_SOULTRAIN_JUCE
class SoulTrainJuce;
#elif SILVER_SOULTRAIN_PREVIEW
class SoulTrainWeb;
#endif
class Editor final : public juce::AudioProcessorEditor
{
public:
    explicit Editor(Processor&);
    ~Editor() override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void setUiScale(double);
    double getUiScale() const;
    bool loadSkin(const juce::File&, juce::String& error);
    bool reloadSkin(juce::String& error);
private:
    struct Surface;
    bool selectSkin(const juce::File&, const juce::String& builtinChoice, juce::String& error);
    void installSurface(std::unique_ptr<Surface>);
    void saveBrowserView();
    juce::Point<int> skinSize() const;
    void fitToSkin();
    void setStatus(const juce::String&);
    void startFirmwareLoad(const juce::File&);
    void chooseDataFolder();
    void scanDataFolder(const juce::File&);
    void openSkinsFolder();
    void exportSkinTemplate();
    void showDiagnosticsMenu(juce::Component* trigger);
    void panic();
#if SILVER_SOULTRAIN_PREVIEW || SILVER_SOULTRAIN_JUCE
    bool installSoulTrain(juce::String& error);
    void refreshNativeSkinMenu();
    void soulTrainAction(const juce::String&);
    std::unique_ptr<Browser> m_nativeBrowser;
    std::unique_ptr<juce::Component> m_soulTrain;
    std::vector<std::pair<juce::String,juce::File>> m_nativeSkins;
    juce::var m_nativeSkinMenu;
    juce::String m_nativeStatus;
#if SILVER_SOULTRAIN_JUCE && SILVER_SOULTRAIN_PREVIEW
    bool m_useWebKit=false;
#endif
#endif
    void showSizeMenu(juce::Point<int> screenPosition);
    void showCustomSize();
    Processor& m_processor;
    std::unique_ptr<Surface> m_surface;
    std::unique_ptr<juce::FileChooser> m_setupChooser;
    std::unique_ptr<juce::AlertWindow> m_sizeDialog;
    bool m_sizeMenuOpen=false;
};
}
