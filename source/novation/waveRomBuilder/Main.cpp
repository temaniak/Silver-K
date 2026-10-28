#include "WaveEditor.h"
#include <iostream>

class WaveApplication final : public juce::JUCEApplication
{
    class Window final : public juce::DocumentWindow
    {
        WaveApplication& app;
    public:
        waveRom::WaveEditor* editor=nullptr;
        explicit Window(WaveApplication& a):DocumentWindow("Silver-K Wave ROM Builder",juce::Colour(0xffced1d5),allButtons),app(a)
        {
            setUsingNativeTitleBar(true);editor=new waveRom::WaveEditor;
            setContentOwned(editor,true);setResizable(true,true);setResizeLimits(1050,760,2200,1500);centreWithSize(1320,930);setVisible(true);
            editor->onChanged=[this]{setName("Silver-K Wave ROM Builder"+juce::String(editor->hasChanges()?" *":""));};
        }
        void closeButtonPressed() override {app.systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
    bool closing=false;
public:
    const juce::String getApplicationName() override{return "Silver-K Wave ROM Builder";}
    const juce::String getApplicationVersion() override{return "0.1.0";}
    void initialise(const juce::String& commandLine) override
    {
        window=std::make_unique<Window>(*this);
        auto args=juce::StringArray::fromTokens(commandLine,true);args.removeEmptyStrings();for(auto& s:args)s=s.unquoted();
        if(args.size()==2&&args[0]=="--smoke"){
            juce::Timer::callAfterDelay(400,[this,path=args[1]]{try{window->editor->smoke(juce::File(path));setApplicationReturnValue(0);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';setApplicationReturnValue(1);}quit();});
        }else if(args.size()==1)window->editor->open(juce::File(args[0]));
    }
    void shutdown() override{window.reset();}
    void systemRequestedQuit() override
    {
        if(closing)return;
        if(window&&window->editor->hasChanges()){
            closing=true;juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("Unsaved changes").withMessage("Close the editor? Save the project or ROM first to keep your changes.").withButton("Close").withButton("Cancel"),[this](int answer){closing=false;if(answer==1)quit();});
        }else quit();
    }
};
START_JUCE_APPLICATION(WaveApplication)
