#include "silverCommandProcessor.h"

#include <cmath>

namespace silverK {
namespace {

class CommandEditor final : public juce::AudioProcessorEditor, private juce::Timer,
                            private juce::MidiKeyboardState::Listener {
public:
    explicit CommandEditor(CommandProcessor& processor)
        : AudioProcessorEditor(processor), processor_(processor), paths_(processor.resources()),
          keyboard_(keys_, juce::MidiKeyboardComponent::horizontalKeyboard) {
        title_.setText("Silver-K Command", juce::dontSendNotification);
        title_.setFont(juce::Font(25.0f, juce::Font::bold));
        model_.addItem("K-Station", 1);
        model_.addItem("KS", 2);
        model_.setSelectedId(paths_.model == cmd::Model::KS ? 2 : 1, juce::dontSendNotification);
        model_.onChange = [this] {
            paths_.model = model_.getSelectedId() == 2 ? cmd::Model::KS : cmd::Model::K;
            paths_.firmware = juce::File{};
            updateResources();
        };
        firmware_.onClick = [this] { choose("Firmware", "*.syx;*.os;*.bin", &CommandProcessor::Resources::firmware); };
        waves_.onClick = [this] { choose("KS Wave ROM", "*.bin", &CommandProcessor::Resources::wave); };
        data_.onClick = [this] { choose("KS All Data or user Flash", "*.mid;*.midi;*.bin", &CommandProcessor::Resources::data); };
        parallel_.setToggleState(paths_.parallel, juce::dontSendNotification);
        load_.onClick = [this] {
            paths_.parallel = parallel_.getToggleState();
            processor_.activate(paths_);
            part_.setSelectedId(1, juce::dontSendNotification);
            timerCallback();
        };
        for (int i = 1; i <= 4; ++i)
            part_.addItem("Part " + juce::String(i), i);
        part_.setSelectedId(int(processor_.selectedPart() + 1), juce::dontSendNotification);
        part_.onChange = [this] { processor_.selectPart(unsigned(part_.getSelectedId() - 1)); };
        setNumber(bank_, 1, 8, 1, "Bank");
        setNumber(program_, 0, 99, 0, "Program");
        recallTarget_.addItem("Part program", 1);
        recallTarget_.addItem("Single", 2);
        recallTarget_.addItem("Performance", 3);
        recallTarget_.setSelectedId(2, juce::dontSendNotification);
        recall_.onClick = [this] {
            const auto target = CommandProcessor::ProgramTarget(std::max(0, recallTarget_.getSelectedId() - 1));
            if (!processor_.selectProgram(unsigned(bank_.getValue()), unsigned(program_.getValue()), target))
                feedback_.setText("Program request rejected", juce::dontSendNotification);
        };
        browse_.onClick = [this] { browse(); };
        mapping_.addItem("CC", 1);
        mapping_.addItem("NRPN", 2);
        mapping_.setSelectedId(1, juce::dontSendNotification);
        setNumber(controller_, 0, 127, 105, "Controller number");
        setNumber(value_, 0, 127, 64, "Value");
        apply_.onClick = [this] {
            if (!processor_.editControl(mapping_.getSelectedId() == 2, uint8_t(controller_.getValue()), uint8_t(value_.getValue())))
                feedback_.setText("Control request rejected", juce::dontSendNotification);
        };
        setNumber(cutoff_, 0, 127, 64, "Cutoff");
        cutoff_.onValueChange = [this] { setParameter(0, cutoff_); };
        setNumber(volume_, 0, 127, 100, "Volume");
        volume_.onValueChange = [this] { setParameter(1, volume_); };
        keys_.addListener(this);
        keyboard_.setAvailableRange(36, 84);
        keyboard_.setLowestVisibleKey(36);
        const std::array<juce::Component*, 23> controls{&title_, &status_, &model_, &firmware_, &waves_, &data_,
            &parallel_, &load_, &patch_, &browse_, &part_, &bank_, &program_, &recallTarget_, &recall_, &cutoff_, &volume_,
            &mapping_, &controller_, &value_, &apply_, &feedback_, &keyboard_};
        for (auto* component : controls)
            addAndMakeVisible(*component);
        updateResources();
        setSize(730, 462);
        timerCallback();
        startTimerHz(15);
    }

    ~CommandEditor() override {
        stopTimer();
        keys_.allNotesOff(0);
        keys_.removeListener(this);
    }

    void paint(juce::Graphics& graphics) override { graphics.fillAll(juce::Colour(0xff20242a)); }

    void resized() override {
        auto body = getLocalBounds().reduced(24, 16);
        title_.setBounds(body.removeFromTop(40));
        status_.setBounds(body.removeFromTop(29));
        body.removeFromTop(10);
        auto sources = body.removeFromTop(30);
        model_.setBounds(sources.removeFromLeft(150));
        sources.removeFromLeft(8);
        firmware_.setBounds(sources.removeFromLeft(166));
        sources.removeFromLeft(8);
        waves_.setBounds(sources.removeFromLeft(166));
        sources.removeFromLeft(8);
        data_.setBounds(sources);
        body.removeFromTop(8);
        auto boot = body.removeFromTop(30);
        parallel_.setBounds(boot.removeFromLeft(170));
        load_.setBounds(boot.removeFromRight(130));
        body.removeFromTop(14);
        auto current = body.removeFromTop(27);
        browse_.setBounds(current.removeFromRight(130));
        patch_.setBounds(current);
        auto recall = body.removeFromTop(30);
        part_.setBounds(recall.removeFromLeft(106));
        bank_.setBounds(recall.removeFromLeft(122).reduced(3, 0));
        program_.setBounds(recall.removeFromLeft(122).reduced(3, 0));
        recallTarget_.setBounds(recall.removeFromLeft(126));
        recall_.setBounds(recall.removeFromRight(130));
        body.removeFromTop(10);
        auto params = body.removeFromTop(34);
        cutoff_.setBounds(params.removeFromLeft(params.getWidth() / 2).reduced(4, 0));
        volume_.setBounds(params.reduced(4, 0));
        body.removeFromTop(6);
        auto edit = body.removeFromTop(30);
        mapping_.setBounds(edit.removeFromLeft(106));
        controller_.setBounds(edit.removeFromLeft(206).reduced(3, 0));
        value_.setBounds(edit.removeFromLeft(186).reduced(3, 0));
        apply_.setBounds(edit.removeFromRight(130));
        feedback_.setBounds(body.removeFromTop(26));
        keyboard_.setBounds(body.removeFromBottom(66));
    }

private:
    static void setNumber(juce::Slider& slider, int min, int max, int value, const juce::String& label) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, label.length() > 7 ? 110 : 72, 24);
        slider.setTextValueSuffix(" " + label);
        slider.setTitle(label);
        slider.setRange(min, max, 1);
        slider.setValue(value, juce::dontSendNotification);
    }
    void setParameter(int index, const juce::Slider& slider) {
        auto* parameter = processor_.getParameters()[index];
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(float(slider.getValue()) / 127);
        parameter->endChangeGesture();
    }

    void choose(const juce::String& title, const juce::String& pattern, juce::File CommandProcessor::Resources::* field) {
        fileChooser_ = std::make_unique<juce::FileChooser>(title, paths_.*field, pattern);
        fileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<CommandEditor>(this), field](const juce::FileChooser& chooser) {
                if (safe && chooser.getResult().existsAsFile()) {
                    safe->paths_.*field = chooser.getResult();
                    safe->updateResources();
                }
            });
    }

    void browse() {
        auto entries = processor_.catalog();
        if (entries.empty()) {
            feedback_.setText("Programs are still loading. Try again after the current MIDI finishes.", juce::dontSendNotification);
            return;
        }
        juce::PopupMenu root;
        for (unsigned group = 1; group <= 9; ++group) {
            juce::PopupMenu section;
            for (size_t i = 0; i < entries.size(); ++i) {
                const auto& entry = entries[i];
                if (group == (entry.performance ? 9u : entry.bank))
                    section.addItem(int(i + 1), entry.name);
            }
            if (section.getNumItems())
                root.addSubMenu(group == 9 ? "Performances" : "Bank " + juce::String(group), section);
        }
        root.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(browse_),
            [safe = juce::Component::SafePointer<CommandEditor>(this), entries = std::move(entries)](int selected) {
                if (safe && selected > 0) {
                    const auto& entry = entries.at(size_t(selected - 1));
                    safe->bank_.setValue(entry.bank, juce::dontSendNotification);
                    safe->program_.setValue(entry.program, juce::dontSendNotification);
                    if (entry.performance)
                        safe->recallTarget_.setSelectedId(3, juce::dontSendNotification);
                    else if (safe->recallTarget_.getSelectedId() == 3)
                        safe->recallTarget_.setSelectedId(2, juce::dontSendNotification);
                    safe->recall_.triggerClick();
                }
            });
    }

    void updateResources() {
        const bool ks = paths_.model == cmd::Model::KS;
        firmware_.setButtonText(paths_.firmware.existsAsFile() ? paths_.firmware.getFileName() : "Firmware…");
        waves_.setButtonText(paths_.wave.existsAsFile() ? paths_.wave.getFileName() : "Wave ROM…");
        data_.setButtonText(paths_.data.existsAsFile() ? paths_.data.getFileName() : "KS Data…");
        waves_.setEnabled(ks);
        data_.setEnabled(ks);
        parallel_.setEnabled(ks);
    }

    void timerCallback() override {
        status_.setText(processor_.status(), juce::dontSendNotification);
        const auto state = processor_.projection();
        const bool ready = bool(state);
        const bool ks = ready && state->model == cmd::Model::KS;
        part_.setEnabled(ready && state->performance);
        recallTarget_.setEnabled(ks);
        if (!ks)
            recallTarget_.setSelectedId(2, juce::dontSendNotification);
        recall_.setEnabled(ready);
        browse_.setEnabled(ready);
        apply_.setEnabled(ready);
        if (!ready) {
            patch_.setText({}, juce::dontSendNotification);
            return;
        }
        if (!state->performance && part_.getSelectedId() != 1) {
            part_.setSelectedId(1, juce::dontSendNotification);
            processor_.selectPart(0);
        }
        bank_.setRange(1, ks ? 8 : 4, 1);
        const auto& part = state->parts[size_t(part_.getSelectedId() - 1)];
        const auto name = juce::String(part.name.data()).trim();
        juce::String title = state->performance ? "Performance " + juce::String(state->performanceProgram) + "  /  "
                                                : juce::String{};
        title += juce::String(part.program.bank) + "-" + juce::String(part.program.program).paddedLeft('0', 2);
        if (name.isNotEmpty())
            title += "  " + name;
        if (state->loadingProgram)
            title += "  (loading)";
        patch_.setText(title, juce::dontSendNotification);
        if (!cutoff_.isMouseButtonDown())
            cutoff_.setValue(part.cutoff, juce::dontSendNotification);
        if (!volume_.isMouseButtonDown())
            volume_.setValue(part.volume, juce::dontSendNotification);
    }

    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) override {
        processor_.keyboardNote(true, uint8_t(note), uint8_t(std::lround(velocity * 127)));
    }
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float velocity) override {
        processor_.keyboardNote(false, uint8_t(note), uint8_t(std::lround(velocity * 127)));
    }

    CommandProcessor& processor_;
    CommandProcessor::Resources paths_;
    juce::Label title_, status_, patch_, feedback_;
    juce::ComboBox model_, part_, mapping_, recallTarget_;
    juce::TextButton firmware_, waves_, data_, load_{"Load controller"}, browse_{"Browse…"}, recall_{"Recall"}, apply_{"Send"};
    juce::ToggleButton parallel_{"Parallel KS DSP"};
    juce::Slider bank_, program_, cutoff_, volume_, controller_, value_;
    juce::MidiKeyboardState keys_;
    juce::MidiKeyboardComponent keyboard_;
    std::unique_ptr<juce::FileChooser> fileChooser_;
};
}

juce::AudioProcessorEditor* CommandProcessor::createEditor() { return new CommandEditor(*this); }
}
