#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "SpectralEngine.h"
class SpectralStripProcessor : public juce::AudioProcessor
{
public:
    SpectralStripProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Spectral Strip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 120.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
    SpectralEngine engine;
    using Chain = std::array<int, SpectralEngine::maxModules>;
    Chain getChain() const noexcept;
    void setChain(const Chain&, bool notifyHost = true);
    bool addInstance(int type);
    static juce::String moduleParamID(int slot, const char* id);
    std::atomic<int> activeFFTSize { SpectralEngine::defaultSize };
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void render(juce::AudioBuffer<float>&, bool bypass);
    float value(const char* id) const { return state.getRawParameterValue(id)->load(); }
    juce::SmoothedValue<float> mix, outputGain, reconfigureGain;
    std::atomic<juce::int64> chainCode {0x332211};
    juce::int64 activeChainCode = 0x332211;
    std::atomic<unsigned> stateRevision {0};
    unsigned activeStateRevision = 0;
    std::array<std::array<std::atomic<float>*,30>,SpectralEngine::maxModules> moduleParameters {};
    int warmupSamples = 0;
    int activeLatency() const noexcept
    {Chain chain {};for(int i=0;i<SpectralEngine::maxModules;++i)chain[static_cast<size_t>(i)]=static_cast<int>((activeChainCode>>(8*i))&255);return engine.getLatency(chain);}
    void restartStream();
    void trackTransport(int blockSamples);
    int fadeSamples = 240, startupPosition = 0, bridgePosition = 240;
    std::array<float, 2> lastOutput {}, bridgeOutput {};
    bool prepared = false, wasPlaying = false, haveExpectedPosition = false;
    juce::int64 expectedPosition = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralStripProcessor)
};
