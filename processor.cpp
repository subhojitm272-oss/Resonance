#include "processor.h"

#include <cmath>

ResonanceProcessor::ResonanceProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameters())
{
}

void ResonanceProcessor::prepareToPlay(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    ring.fill(0.0f);
    writePosition = 0;
    samplesCollected = 0;
    samplesUntilAnalysis = analysisHop;
    previousRms = 0.0f;
    samplesSinceOnset = static_cast<int>(sampleRate);
    onsetCount = 0;
    onsetIntervals.fill(0.0);
    detectedNote = -1;
    outputNote = -1;
    referenceNote = -1;
    comparedFrames = 0;
    matchedFrames = 0;
    history.fill(-1.0f);
    historyPosition = 0;
    historyHead.store(0);
    currentFrequency.store(0.0f);
    currentCents.store(0.0f);
    currentConfidence.store(0.0f);
    currentTempo.store(0.0f);
    currentScore.store(0.0f);
    currentRms.store(0.0f);
    referenceActive.store(false);
}

void ResonanceProcessor::releaseResources()
{
}

bool ResonanceProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && ! layouts.getMainInputChannelSet().isDisabled();
}

void ResonanceProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int channelCount = audio.getNumChannels();
    const int sampleCount = audio.getNumSamples();
    const bool midiEnabled = parameters.getRawParameterValue("midiOutput")->load() > 0.5f;
    const float threshold = parameters.getRawParameterValue("threshold")->load();

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            referenceNote = message.getNoteNumber();
            referenceActive.store(true);
        }
        else if (message.isNoteOff()
                 && (referenceNote == message.getNoteNumber()))
        {
            referenceNote = -1;
            referenceActive.store(false);
        }
    }
    midi.clear();

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        float mono = 0.0f;
        for (int channel = 0; channel < channelCount; ++channel)
            mono += audio.getSample(channel, sample);
        mono /= static_cast<float>(juce::jmax(1, channelCount));

        ring[static_cast<size_t>(writePosition)] = mono;
        writePosition = (writePosition + 1) % analysisSize;
        samplesCollected = juce::jmin(analysisSize, samplesCollected + 1);
        samplesUntilAnalysis--;
        samplesSinceOnset++;

        if (samplesUntilAnalysis <= 0)
        {
            samplesUntilAnalysis = analysisHop;
            if (samplesCollected == analysisSize)
            {
                analyse(threshold);
                if (midiEnabled && detectedNote >= 0 && detectedNote != outputNote)
                {
                    if (outputNote >= 0)
                        midi.addEvent(juce::MidiMessage::noteOff(1, outputNote), sample);
                    outputNote = detectedNote;
                    midi.addEvent(juce::MidiMessage::noteOn(1, outputNote, 0.85f), sample);
                }
                else if (midiEnabled && detectedNote < 0 && outputNote >= 0)
                {
                    midi.addEvent(juce::MidiMessage::noteOff(1, outputNote), sample);
                    outputNote = -1;
                }
            }
        }
    }

    if (! midiEnabled && outputNote >= 0)
    {
        midi.addEvent(juce::MidiMessage::noteOff(1, outputNote), 0);
        outputNote = -1;
    }
}

void ResonanceProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destination);
}

void ResonanceProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout ResonanceProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "threshold", 1 }, "Pitch sensitivity",
        juce::NormalisableRange<float>(0.06f, 0.30f, 0.005f), 0.14f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "midiOutput", 1 }, "MIDI output", true));
    return layout;
}

void ResonanceProcessor::analyse(float threshold)
{
    std::array<float, analysisSize> frame {};
    float squareSum = 0.0f;
    for (int i = 0; i < analysisSize; ++i)
    {
        const int index = (writePosition + i) % analysisSize;
        frame[static_cast<size_t>(i)] = ring[static_cast<size_t>(index)];
        squareSum += frame[static_cast<size_t>(i)] * frame[static_cast<size_t>(i)];
    }
    const float rms = std::sqrt(squareSum / static_cast<float>(analysisSize));
    currentRms.store(rms);
    updateTempo(rms);

    const int minimumLag = juce::jmax(2, static_cast<int>(sampleRate / 1000.0));
    const int maximumLag = juce::jmin(analysisSize / 2, static_cast<int>(sampleRate / 65.0));
    std::array<float, analysisSize / 2 + 1> difference {};
    std::array<float, analysisSize / 2 + 1> normalized {};
    float cumulative = 0.0f;
    int bestLag = -1;

    if (rms > 0.002f)
    {
        for (int lag = 1; lag <= maximumLag; ++lag)
        {
            float sum = 0.0f;
            for (int i = 0; i < analysisSize - lag; i += 2)
            {
                const float delta = frame[static_cast<size_t>(i)] - frame[static_cast<size_t>(i + lag)];
                sum += delta * delta;
            }
            difference[static_cast<size_t>(lag)] = sum;
            cumulative += sum;
            normalized[static_cast<size_t>(lag)] =
                sum * static_cast<float>(lag) / juce::jmax(1.0e-12f, cumulative);
        }

        for (int lag = minimumLag; lag <= maximumLag; ++lag)
        {
            if (normalized[static_cast<size_t>(lag)] < threshold)
            {
                while (lag < maximumLag
                       && normalized[static_cast<size_t>(lag + 1)] < normalized[static_cast<size_t>(lag)])
                    ++lag;
                bestLag = lag;
                break;
            }
        }
    }

    float frequency = 0.0f;
    float confidence = 0.0f;
    if (bestLag >= 0)
    {
        const float left = normalized[static_cast<size_t>(juce::jmax(minimumLag, bestLag - 1))];
        const float center = normalized[static_cast<size_t>(bestLag)];
        const float right = normalized[static_cast<size_t>(juce::jmin(maximumLag, bestLag + 1))];
        const float divisor = left - 2.0f * center + right;
        const float adjustment = std::abs(divisor) > 1.0e-8f ? 0.5f * (left - right) / divisor : 0.0f;
        frequency = static_cast<float>(sampleRate) / (static_cast<float>(bestLag) + adjustment);
        confidence = juce::jlimit(0.0f, 1.0f, 1.0f - center);
    }

    int nextNote = -1;
    if (frequency > 0.0f && confidence > 0.55f)
        nextNote = juce::jlimit(0, 127, juce::roundToInt(69.0f + 12.0f * std::log2(frequency / 440.0f)));

    detectedNote = nextNote;
    currentFrequency.store(frequency);
    currentConfidence.store(confidence);
    const float midiPitch = frequency > 0.0f ? 69.0f + 12.0f * std::log2(frequency / 440.0f) : 0.0f;
    currentCents.store(frequency > 0.0f ? (midiPitch - std::round(midiPitch)) * 100.0f : 0.0f);
    history[static_cast<size_t>(historyPosition)] = frequency > 0.0f
        ? static_cast<float>(juce::roundToInt(midiPitch)) : -1.0f;
    pitchHistory[static_cast<size_t>(historyPosition)].store(history[static_cast<size_t>(historyPosition)]);
    historyPosition = (historyPosition + 1) % historySize;
    historyHead.store(historyPosition);

    if (referenceNote >= 0)
    {
        ++comparedFrames;
        if (nextNote >= 0 && std::abs(nextNote - referenceNote) <= 1)
            ++matchedFrames;
        currentScore.store(100.0f * static_cast<float>(matchedFrames) /
                           static_cast<float>(juce::jmax(1, comparedFrames)));
    }
}

void ResonanceProcessor::updateTempo(float rms)
{
    const float rise = rms - previousRms;
    previousRms = previousRms * 0.82f + rms * 0.18f;
    if (rise > 0.012f && samplesSinceOnset > static_cast<int>(sampleRate * 0.20))
    {
        if (onsetCount > 0)
        {
            const double interval = static_cast<double>(samplesSinceOnset) / sampleRate;
            if (interval > 0.25 && interval < 2.0)
            {
                onsetIntervals[static_cast<size_t>(onsetCount % static_cast<int>(onsetIntervals.size()))] = interval;
                ++onsetCount;
                const int count = juce::jmin(onsetCount, static_cast<int>(onsetIntervals.size()));
                double average = 0.0;
                for (int i = 0; i < count; ++i)
                    average += onsetIntervals[static_cast<size_t>(i)];
                average /= static_cast<double>(count);
                float bpm = static_cast<float>(60.0 / average);
                while (bpm < 70.0f) bpm *= 2.0f;
                while (bpm > 180.0f) bpm *= 0.5f;
                currentTempo.store(bpm);
            }
        }
        samplesSinceOnset = 0;
    }
}
