#include "processor.h"

#include <cmath>

namespace
{
const juce::Colour background { 0xff101318 };
const juce::Colour panel { 0xff191e25 };
const juce::Colour muted { 0xff87919d };
const juce::Colour mint { 0xff72e0c0 };
const juce::Colour blue { 0xff7aa7ff };
const juce::Colour coral { 0xffff8d83 };

juce::String noteName(int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                   "F#", "G", "G#", "A", "A#", "B" };
    if (midiNote < 0)
        return "--";
    return juce::String(names[midiNote % 12]) + juce::String(midiNote / 12 - 1);
}

class ResonanceEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ResonanceEditor(ResonanceProcessor& audioProcessor)
        : AudioProcessorEditor(audioProcessor), owner(audioProcessor)
    {
        setSize(960, 650);
        setResizable(true, true);
        setResizeLimits(780, 650, 1440, 1000);
        addAndMakeVisible(sensitivity);
        sensitivity.setSliderStyle(juce::Slider::LinearHorizontal);
        sensitivity.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 24);
        sensitivity.setColour(juce::Slider::trackColourId, mint);
        sensitivity.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        sensitivity.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        sensitivity.setColour(juce::Slider::textBoxBackgroundColourId, panel);
        sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            owner.parameters, "threshold", sensitivity);

        midiToggle.setButtonText("MIDI OUT");
        midiToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        midiToggle.setColour(juce::ToggleButton::tickColourId, mint);
        addAndMakeVisible(midiToggle);
        midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            owner.parameters, "midiOutput", midiToggle);
        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(background);
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff152027));
        g.fillRect(bounds.removeFromTop(5.0f));
        drawHeader(g);
        drawMainCard(g);
        drawMetrics(g);
        drawBottom(g);
    }

    void resized() override
    {
        const int width = getWidth();
        sensitivity.setBounds(juce::roundToInt(width * 0.60f), 581, juce::roundToInt(width * 0.28f), 34);
        midiToggle.setBounds(juce::roundToInt(width * 0.82f), 80, 120, 28);
    }

private:
    void timerCallback() override { repaint(); }

    void drawHeader(juce::Graphics& g)
    {
        g.setColour(mint);
        g.fillRoundedRectangle(34.0f, 26.0f, 34.0f, 34.0f, 10.0f);
        g.setColour(background);
        g.setFont(juce::FontOptions(21.0f, juce::Font::bold));
        g.drawFittedText("R", 34, 27, 34, 32, juce::Justification::centred, 1);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.drawText("resonance", 78, 23, 160, 28, juce::Justification::centredLeft);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("PITCH  /  PERFORMANCE ANALYSIS", 79, 49, 280, 19, juce::Justification::centredLeft);

        g.setColour(mint.withAlpha(0.14f));
        g.fillRoundedRectangle(34.0f, 82.0f, 126.0f, 27.0f, 13.0f);
        g.setColour(mint);
        g.fillEllipse(47.0f, 91.0f, 8.0f, 8.0f);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("LIVE ANALYSIS", 62, 83, 92, 25, juce::Justification::centredLeft);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("REAL-TIME INPUT", 35, 121, 150, 20, juce::Justification::centredLeft);
    }

    void drawMainCard(juce::Graphics& g)
    {
        auto card = juce::Rectangle<float>(32.0f, 151.0f, static_cast<float>(getWidth() - 64), 245.0f);
        g.setColour(panel);
        g.fillRoundedRectangle(card, 18.0f);
        g.setColour(muted);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("LIVE PITCH TRACK", 54, 168, 220, 20, juce::Justification::centredLeft);

        const float frequency = owner.currentFrequency.load();
        int note = frequency > 0.0f
            ? juce::roundToInt(69.0f + 12.0f * std::log2(frequency / 440.0f)) : -1;
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(66.0f, juce::Font::bold));
        g.drawText(noteName(note), 54, 193, 214, 80, juce::Justification::centredLeft);
        g.setColour(muted);
        g.setFont(juce::FontOptions(15.0f));
        g.drawText(frequency > 0.0f ? juce::String(frequency, 1) + " Hz" : "Waiting for signal",
                   58, 277, 180, 26, juce::Justification::centredLeft);

        const float cents = owner.currentCents.load();
        const float meterX = 65.0f;
        const float meterY = 335.0f;
        const float meterWidth = 220.0f;
        g.setColour(juce::Colour(0xff2c343d));
        g.fillRoundedRectangle(meterX, meterY, meterWidth, 7.0f, 3.5f);
        g.setColour(mint);
        g.fillRoundedRectangle(meterX + meterWidth * 0.5f - 2.0f, meterY - 6.0f, 4.0f, 19.0f, 2.0f);
        const float marker = meterX + meterWidth * 0.5f + juce::jlimit(-1.0f, 1.0f, cents / 50.0f) * meterWidth * 0.5f;
        g.setColour(std::abs(cents) < 8.0f ? mint : coral);
        g.fillEllipse(marker - 5.0f, meterY - 4.0f, 10.0f, 10.0f);
        g.setColour(muted);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("-50", 62, 349, 35, 16, juce::Justification::centredLeft);
        g.drawText("IN TUNE", 148, 349, 55, 16, juce::Justification::centred);
        g.drawText("+50 cents", 235, 349, 58, 16, juce::Justification::centredRight);

        const float chartX = 327.0f;
        const float chartY = 207.0f;
        const float chartW = static_cast<float>(getWidth()) - chartX - 58.0f;
        const float chartH = 151.0f;
        g.setColour(juce::Colour(0xff222a32));
        for (int i = 0; i < 4; ++i)
        {
            const float y = chartY + chartH * static_cast<float>(i) / 3.0f;
            g.drawHorizontalLine(juce::roundToInt(y), chartX, chartX + chartW);
        }
        juce::Path pitchPath;
        bool pathStarted = false;
        const int historyHead = owner.historyHead.load();
        for (int i = 0; i < ResonanceProcessor::historySize; ++i)
        {
            const int index = (historyHead + i) % ResonanceProcessor::historySize;
            const float value = owner.pitchHistory[static_cast<size_t>(index)].load();
            if (value < 0.0f)
            {
                pathStarted = false;
                continue;
            }
            const float x = chartX + chartW * static_cast<float>(i) /
                static_cast<float>(ResonanceProcessor::historySize - 1);
            const float y = chartY + chartH * (1.0f - juce::jlimit(0.0f, 1.0f, (value - 48.0f) / 36.0f));
            if (! pathStarted)
                pitchPath.startNewSubPath(x, y);
            else
                pitchPath.lineTo(x, y);
            pathStarted = true;
        }
        g.setColour(mint);
        g.strokePath(pitchPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(muted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("PITCH HISTORY", juce::roundToInt(chartX), 170, 130, 20, juce::Justification::centredLeft);
        g.drawText("TIME", juce::roundToInt(chartX + chartW - 42), 365, 42, 16, juce::Justification::centredRight);
    }

    void drawMetrics(juce::Graphics& g)
    {
        const float top = 414.0f;
        const float gap = 14.0f;
        const float cardWidth = (static_cast<float>(getWidth()) - 64.0f - 2.0f * gap) / 3.0f;
        drawMetricCard(g, 32.0f, top, cardWidth, "TEMPO", owner.currentTempo.load() > 0.0f
                           ? juce::String(owner.currentTempo.load(), 0) : "--", "BPM  /  ONSET TRACKING", blue);
        drawMetricCard(g, 32.0f + cardWidth + gap, top, cardWidth, "PERFORMANCE SCORE",
                       owner.referenceActive.load() ? juce::String(owner.currentScore.load(), 0) + "%" : "--",
                       owner.referenceActive.load() ? "MIDI REFERENCE ACTIVE" : "PLAY MIDI REFERENCE", mint);
        drawMetricCard(g, 32.0f + 2.0f * (cardWidth + gap), top, cardWidth, "CONFIDENCE",
                       juce::String(owner.currentConfidence.load() * 100.0f, 0) + "%", "YIN PITCH DETECTION", coral);
    }

    void drawMetricCard(juce::Graphics& g, float x, float y, float width, const juce::String& title,
                        const juce::String& value, const juce::String& detail, juce::Colour accent)
    {
        g.setColour(panel);
        g.fillRoundedRectangle(x, y, width, 132.0f, 15.0f);
        g.setColour(accent);
        g.fillRoundedRectangle(x + 17.0f, y + 19.0f, 3.0f, 17.0f, 1.5f);
        g.setColour(muted);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, juce::roundToInt(x + 30.0f), juce::roundToInt(y + 17.0f),
                   juce::roundToInt(width - 45.0f), 22, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(32.0f, juce::Font::bold));
        g.drawText(value, juce::roundToInt(x + 18.0f), juce::roundToInt(y + 46.0f),
                   juce::roundToInt(width - 36.0f), 41, juce::Justification::centredLeft);
        g.setColour(muted);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(detail, juce::roundToInt(x + 19.0f), juce::roundToInt(y + 98.0f),
                   juce::roundToInt(width - 38.0f), 17, juce::Justification::centredLeft);
    }

    void drawBottom(juce::Graphics& g)
    {
        g.setColour(muted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("PITCH SENSITIVITY", 34, 585, 170, 23, juce::Justification::centredLeft);
        g.drawText("MIDI input notes are used as the performance reference.", 34, 614,
                   getWidth() - 68, 18, juce::Justification::centredLeft);
    }

    ResonanceProcessor& owner;
    juce::Slider sensitivity;
    juce::ToggleButton midiToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiAttachment;
};
}

juce::AudioProcessorEditor* ResonanceProcessor::createEditor()
{
    return new ResonanceEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ResonanceProcessor();
}
