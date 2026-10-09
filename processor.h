#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>

class ResonanceProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int historySize = 180;

    ResonanceProcessor();

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int size) override;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> currentFrequency { 0.0f };
    std::atomic<float> currentCents { 0.0f };
    std::atomic<float> currentConfidence { 0.0f };
    std::atomic<float> currentTempo { 0.0f };
    std::atomic<float> currentScore { 0.0f };
    std::atomic<float> currentRms { 0.0f };
    std::atomic<bool> referenceActive { false };
    std::atomic<int> historyHead { 0 };
    std::array<std::atomic<float>, historySize> pitchHistory {};

private:
    static constexpr int analysisSize = 2048;
    static constexpr int analysisHop = 512;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void analyse(float threshold);
    void updateTempo(float rms);

    double sampleRate = 44100.0;
    std::array<float, analysisSize> ring {};
    std::array<float, historySize> history {};
    std::array<double, 8> onsetIntervals {};
    int writePosition = 0;
    int samplesCollected = 0;
    int samplesUntilAnalysis = analysisHop;
    int samplesSinceOnset = 0;
    int onsetCount = 0;
    int historyPosition = 0;
    int detectedNote = -1;
    int outputNote = -1;
    int referenceNote = -1;
    int comparedFrames = 0;
    int matchedFrames = 0;
    float previousRms = 0.0f;
};
