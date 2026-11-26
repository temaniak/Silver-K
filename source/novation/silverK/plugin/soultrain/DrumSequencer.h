#pragma once

#include "../silverProcessor.h"

#include <functional>
#include <memory>
#include <vector>

namespace soultrain {

class SkinArt;
struct ControlShadow;

class DrumSequencer final : public juce::Component {
public:
    using Menu = std::function<void(juce::Component&, juce::StringArray, int, bool, std::function<void(int)>)>;
    // A negative note represents an unassigned range.
    using NoteMenu = std::function<void(juce::Component&, int, std::function<void(int)>)>;
    static constexpr auto Name = "Drum Sequencer";
    static constexpr auto EditName = "Seq";

    DrumSequencer(silverK::Processor&, const SkinArt&, Menu, NoteMenu, std::function<void()> close);
    ~DrumSequencer() override;

    void paint(juce::Graphics&) override;
    bool hitTest(int x,int y) override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void focusClose();
    void refreshPlayback();

private:
    using Edit = silverK::Processor::DrumGridEdit;
    using Pattern = novation::controller::command::KSUserDrumPattern;
    class Pad;
    class Choice;
    class Toggle;
    class StepValue;
    class Playhead;

    juce::String soundName(uint8_t) const;
    const Pattern& pattern() const noexcept { return info_.sequences.patterns[editingPattern_]; }
    void showSounds(size_t row);
    void showKeys(Edit::Kind);
    void showSwitchAfter();
    void showLengths();
    void showStepParameters();
    void selectPattern(uint8_t);
    void refreshPattern();
    void refreshOptions();
    void updatePlayhead();
    void updateSound(size_t row);
    void selectStep(size_t index, int velocity);
    void setStepVelocity(size_t index, int velocity);
    int parameterValue(size_t index) const noexcept;
    void setParameterValue(int value);
    void updateStepEditor(int velocity);
    void removeStep(size_t index);
    bool apply(Edit::Kind, uint8_t index, uint8_t value);
    void setError(juce::String);

    silverK::Processor& processor_;
    const SkinArt& skin_;
    Menu menu_;
    NoteMenu noteMenu_;
    std::function<void()> close_;
    silverK::Processor::DrumSequencerInfo info_;
    std::unique_ptr<Choice> closeButton_;
    std::unique_ptr<Toggle> retrigger_, assignedKeys_;
    std::array<std::unique_ptr<Choice>,Edit::Patterns> patterns_;
    std::unique_ptr<Choice> lowKey_, highKey_, switchAfter_, length_, parameter_, remove_;
    std::array<std::unique_ptr<Choice>,Edit::Rows> sounds_;
    std::array<std::unique_ptr<Pad>,Edit::Cells> pads_;
    std::unique_ptr<StepValue> stepValue_;
    std::unique_ptr<Playhead> playhead_;
    std::vector<ControlShadow> buttonShadows_;
    juce::String error_;
    silverK::Processor::DrumPlayhead playback_;
    uint8_t editingPattern_=0;
    int selected_=-1;
    size_t parameterIndex_=0;
    bool draggingValue_=false;
};

}
