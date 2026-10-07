#pragma once
#include "PluginProcessor.h"
class SpectralStripEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ScrollBar::Listener
{
public:
    explicit SpectralStripEditor(SpectralStripProcessor&);
    ~SpectralStripEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    struct ModuleHeader : juce::TextButton
    {
        std::function<void(const juce::MouseEvent&)> begin, move, end;
        void mouseDown(const juce::MouseEvent& e) override { if (begin) begin(e); }
        void mouseDrag(const juce::MouseEvent& e) override { if (move) move(e); }
        void mouseUp(const juce::MouseEvent& e) override { if (end) end(e); }
    };
    juce::ComboBox addModule;
    std::array<ModuleHeader, SpectralEngine::maxModules> moduleHeaders;
    std::array<juce::TextButton, SpectralEngine::maxModules> removeModule;
    SpectralStripProcessor::Chain visibleChain {};
    juce::ScrollBar stripScroll {false};
    int firstVisible = 0;
    void scrollBarMoved(juce::ScrollBar*, double) override;
    int draggedModule = 0, dropSlot = -1;

    SpectralStripProcessor& owner;
    struct StripLookAndFeel : juce::LookAndFeel_V4
    {
        juce::Label* createSliderTextBox(juce::Slider& slider) override
        {
            auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
            label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
            label->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            label->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            label->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            return label;
        }
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float position, float start, float end, juce::Slider& slider) override
        {
            // Keep room for the thumb above the arc when it points straight up.
            const int extra = slider.getProperties().getWithDefault("largeDelayKnob", false) ? 4 : 0;
            juce::LookAndFeel_V4::drawRotarySlider(g, x-extra, y-extra, width+2*extra,
                                                  height+2*extra, position, start, end, slider);
        }
    };
    StripLookAndFeel look;
    juce::ComboBox fftSizeChoice;
    juce::Label fftSizeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftSizeAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::array<float, SpectralEngine::displayBins> display {};
    void addKnob(const char*, const char*, const char* suffix);
    void addSwitch(const char*, const char*);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralStripEditor)
};
