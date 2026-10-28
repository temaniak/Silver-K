#pragma once
#include "WaveBank.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

namespace waveRom
{
struct SlotState
{
    std::vector<float> source,result;
    double rate=48000;
    Selection selection;
    Processing processing;
    juce::String sourceName;
    bool occupied=false,edited=false;
};

class Waveform final : public juce::Component
{
public:
    void set(const std::vector<float>*,double,Selection,bool editable,bool overview=false);
    void setSeam(const std::vector<float>*,double rate);
    void zoom(double);
    void navigate(double);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    std::function<void(Selection,bool)> onChange;
    std::function<void(double)> onNavigate;
private:
    const std::vector<float>* data=nullptr;
    double sampleRate=48000,left=0,right=1;
    Selection selection;
    bool editing=false,overview=false,seam=false;
    std::vector<float> seamCache;
    int dragging=0;
    double position(float) const;
};

class WaveEditor final : public juce::AudioAppComponent, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    WaveEditor();
    ~WaveEditor() override;
    void prepareToPlay(int,double) override;
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    void open(const juce::File&);
    bool hasChanges() const { return modified; }
    void smoke(const juce::File& directory);
    std::function<void()> onChanged;
private:
    Bank bank;
    std::array<SlotState,24> states;
    size_t selected=1;
    bool modified=false,busy=false,updating=false;
    uint64_t generation=0,previewGeneration=0;
    std::atomic<double> deviceRate{48000};
    std::atomic<float> volume{.35f};
    PreviewVoice voice;
    juce::ThreadPool jobs{1};
    juce::AudioFormatManager formats;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File loadedRom,project;
    juce::LookAndFeel_V4 look;
    juce::ComboBox slot,note;
    juce::Label slotLabel,type,fileName,status,sourceTitle,resultTitle,seamTitle,processingTitle,beginLabel,endLabel,periodLabel,normalizeLabel,fadeInLabel,fadeOutLabel,noteLabel,volumeLabel,progress,hint;
    juce::TextButton loadWav{"Load WAV"},openBank{"Open ROM / Project"},newBank{"New Bank"},audioSettings{"Audio Output"},detect{"Find Again"},zoomIn{"+"},zoomOut{"-"},resetView{"Full WAV"},play{"Play"},stop{"Stop"},saveProject{"Save Project"},saveSlot{"Save Slot"},exportBank{"Build ROM"};
    juce::ToggleButton normalize{"Normalize"},snap{"Snap to Zero"},loop{"Loop"},original{"Original"},result{"Result"};
    juce::Slider begin,end,peakDb,fadeIn,fadeOut,gain;
    Waveform overview,editor,output,seam;
    void timerCallback() override;
    void refresh(bool resetViewport=false);
    void updateProcessing();
    void process(bool find);
    void audition();
    void stopAudio();
    void choose(int role);
    void loadWavFile(const juce::File&);
    void loadRomFile(const juce::File&);
    void loadProjectFile(const juce::File&);
    void saveProjectFile(const juce::File&);
    void exportRom(const juce::File&);
    void commitSlot();
    void markChanged();
    void failure(const juce::String&);
    void selectionChanged(Selection,bool);
    Bank composed() const;
    juce::var projectData() const;
    void restore(const juce::var&);
    void writeAtomic(const juce::File&,const void*,size_t);
    void run(std::function<void()>,std::function<void(const juce::String&)>);
};
}
