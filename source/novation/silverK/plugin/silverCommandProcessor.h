#pragma once

#include "../../controller/command/commandAudioDevice.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <mutex>

namespace silverK {
namespace cmd = novation::controller::command;

class CommandProcessor final : public juce::AudioProcessor {
public:
    struct Resources {
        cmd::Model model = cmd::Model::K;
        juce::File firmware, wave, data;
        bool parallel = false;
    };
    enum class ProgramTarget { Part, Single, Performance };
    struct ProgramEntry { uint8_t bank, program; bool performance; juce::String name; };

    CommandProcessor();
    ~CommandProcessor() override;
    const juce::String getName() const override { return "Silver-K Command"; }
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current program"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    Resources resources() const;
    bool activate(Resources);
    juce::String status() const;
    std::optional<cmd::NativeEditorState> projection();
    void selectPart(unsigned part) noexcept { selectedPart_.store(std::min(part, 3u)); }
    unsigned selectedPart() const noexcept { return selectedPart_.load(); }
    std::vector<ProgramEntry> catalog();
    bool selectProgram(unsigned bank, unsigned program, ProgramTarget);
    bool editControl(bool nrpn, uint8_t number, uint8_t value);
    void keyboardNote(bool on, uint8_t note, uint8_t velocity);

private:
    class ControllerParameter;
    struct RetiredCallback {
        std::atomic<unsigned>& count;
        explicit RetiredCallback(std::atomic<unsigned>& value) noexcept : count(value) { ++count; }
        ~RetiredCallback() { --count; }
    };

    void detachLocked(bool save);
    void bootLocked();
    bool captureLocked();
    juce::MidiBuffer* returnBuffer() noexcept;
    void reserveMidiLocked();

    Resources resources_;
    std::vector<uint8_t> archive_;
    juce::String message_;
    double sampleRate_ = 0;
    unsigned maximumBlock_ = 0;
    mutable std::mutex control_;
    std::unique_ptr<cmd::CommandAudioDevice> engine_;
    std::atomic<cmd::CommandAudioDevice*> audible_{nullptr};
    std::atomic<unsigned> callbacks_{0}, selectedPart_{0};
    std::array<juce::MidiBuffer, 2> returnMidi_;
    std::array<const uint8_t*, 2> reservedMidi_{};
    ControllerParameter* cutoff_;
    ControllerParameter* volume_;
    std::array<std::optional<uint8_t>, 128> keyboardChannels_;
};
}
