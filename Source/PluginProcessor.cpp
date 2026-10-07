#include "PluginProcessor.h"
#include "PluginEditor.h"
SpectralStripProcessor::SpectralStripProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SpectralStripState", makeParameters())
{
    const char* ids[]={"freezeModule","freeze","gate","invert","delay","smear","shimmer","threshold","delayMs","spread","gateAttack","gateRelease","gateTilt","feedback","damping","delayMix","blur","blurAmount","blurWidth","blurMix","tremolo","tremoloRate","tremoloDepth","tremoloShape","reverb","reverbDecay","reverbSize","reverbShimmer","reverbTone","reverbMix"};
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
        for(int index=0;index<30;++index)
            moduleParameters[static_cast<size_t>(slot)][static_cast<size_t>(index)]=state.getRawParameterValue(moduleParamID(slot,ids[index]));
}
juce::AudioProcessorValueTreeState::ParameterLayout SpectralStripProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto toggle = [&](const char* id, const char* name, bool v)
    { p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1}, name, v)); };
    auto number = [&](const char* id, const char* name, float lo, float hi, float def, float skew = 1.0f)
    { p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1}, name,
              juce::NormalisableRange<float>(lo, hi, 0.001f, skew), def)); };
    toggle("freezeModule", "Freeze module", true); toggle("freeze", "Capture freeze", false);
    number("smear", "Temporal smear", 0, 1, 0);
    toggle("gate", "Gate enabled", false); toggle("invert", "Gate inverted", false);
    number("threshold", "Gate threshold", -90, 0, -48);
    toggle("delay", "Delay enabled", false);
    number("delayMs", "Delay time (ms)", 10, 1000, 250, 0.5f);
    number("spread", "Delay dispersion", -1, 1, 0.5f);
    number("feedback", "Delay feedback", 0, 0.92f, 0.3f);
    number("delayMix", "Delay mix", 0, 1, 0.5f);
    number("low", "Selection low (Hz)", 20, 20000, 20, 0.25f);
    number("high", "Selection high (Hz)", 20, 20000, 20000, 0.25f);
    number("mix", "Global mix", 0, 1, 1);
    number("output", "Output (dB)", -24, 12, 0);
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"fftSize",1},
          "FFT Size", juce::StringArray{"512", "1024", "2048", "4096", "8192"}, 2));
    toggle("bandFilter", "Band filter enabled", true);
    number("shimmer", "Freeze shimmer", 0, 1, 0);
    number("gateAttack", "Gate attack (ms)", 1, 500, 10, 0.4f);
    number("gateRelease", "Gate release (ms)", 5, 2000, 100, 0.4f);
    number("gateTilt", "Gate tilt (dB/oct)", 0, 6, 3);
    number("damping", "Delay damping", 0, 1, 0);
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        auto addToggle=[&](const char* id, bool def)
        { const auto key=moduleParamID(slot,id); if(key!=id) toggle(key.toRawUTF8(),key.toRawUTF8(),def); };
        auto addNumber=[&](const char* id,float lo,float hi,float def,float skew=1.0f)
        { const auto key=moduleParamID(slot,id); if(key!=id) number(key.toRawUTF8(),key.toRawUTF8(),lo,hi,def,skew); };
        addToggle("freezeModule",true); addToggle("freeze",false); addToggle("gate",true); addToggle("invert",false); addToggle("delay",true);
        addNumber("smear",0,1,0); addNumber("shimmer",0,1,0); addNumber("threshold",-90,0,-48);
        addNumber("delayMs",10,1000,250,.5f); addNumber("spread",-1,1,.5f);
        addNumber("gateAttack",1,500,10,.4f); addNumber("gateRelease",5,2000,100,.4f); addNumber("gateTilt",0,6,3);
        addNumber("feedback",0,.92f,.3f); addNumber("damping",0,1,0); addNumber("delayMix",0,1,.5f);
    }
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        const auto key=[&](const char* id) { return moduleParamID(slot,id); };
        toggle(key("blur").toRawUTF8(),("Blur "+juce::String(slot+1)+" enabled").toRawUTF8(),true);
        number(key("blurAmount").toRawUTF8(),("Blur "+juce::String(slot+1)+" amount").toRawUTF8(),0,1,.6f);
        number(key("blurWidth").toRawUTF8(),("Blur "+juce::String(slot+1)+" width (Hz)").toRawUTF8(),20,4000,400,.35f);
        number(key("blurMix").toRawUTF8(),("Blur "+juce::String(slot+1)+" mix").toRawUTF8(),0,1,1);
    }
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        const auto key=[&](const char* id) { return moduleParamID(slot,id); };
        const auto name="Tremolo "+juce::String(slot+1);
        toggle(key("tremolo").toRawUTF8(),(name+" enabled").toRawUTF8(),true);
        number(key("tremoloRate").toRawUTF8(),(name+" rate (Hz)").toRawUTF8(),.1f,20,4,.4f);
        number(key("tremoloDepth").toRawUTF8(),(name+" depth").toRawUTF8(),0,1,.75f);
        number(key("tremoloShape").toRawUTF8(),(name+" shape").toRawUTF8(),0,1,0);
    }
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        const auto key=[&](const char* id) { return moduleParamID(slot,id); };const auto name="Shimmer Reverb "+juce::String(slot+1);
        toggle(key("reverb").toRawUTF8(),(name+" enabled").toRawUTF8(),true);
        number(key("reverbDecay").toRawUTF8(),(name+" decay (s)").toRawUTF8(),.5f,20,6,.5f);
        number(key("reverbSize").toRawUTF8(),(name+" size").toRawUTF8(),0,1,.6f);
        number(key("reverbShimmer").toRawUTF8(),(name+" shimmer").toRawUTF8(),0,1,.45f);
        number(key("reverbTone").toRawUTF8(),(name+" tone").toRawUTF8(),0,1,.55f);
        number(key("reverbMix").toRawUTF8(),(name+" mix").toRawUTF8(),0,1,.35f);
    }
    return p;
}
bool SpectralStripProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    const auto out = b.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
        && out == b.getMainInputChannelSet();
}
void SpectralStripProcessor::prepareToPlay(double sr, int)
{
    activeChainCode = chainCode.load();
    activeStateRevision = stateRevision.load();
    engine.prepare(sr, static_cast<int>(value("fftSize")));
    setLatencySamples(activeLatency()); activeFFTSize.store(engine.getSize());
    reconfigureGain.reset(sr, 0.01); reconfigureGain.setCurrentAndTargetValue(1);
    warmupSamples = 0;
    fadeSamples = std::max(1, static_cast<int>(sr * 0.005));
    startupPosition = 0; bridgePosition = fadeSamples;
    lastOutput.fill(0); bridgeOutput.fill(0);
    wasPlaying = haveExpectedPosition = false; prepared = true;
    mix.reset(sr, 0.035); outputGain.reset(sr, 0.035);
    mix.setCurrentAndTargetValue(value("mix"));
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value("output")));
}
void SpectralStripProcessor::restartStream()
{
    // Drop stale STFT/delay data on a new transport segment. No allocations.
    engine.selectSize(engine.getChoice());
    startupPosition = 0;
    bridgeOutput = lastOutput; bridgePosition = 0;
}
void SpectralStripProcessor::reset()
{
    if (prepared) restartStream();
    wasPlaying = haveExpectedPosition = false;
}
void SpectralStripProcessor::trackTransport(int blockSamples)
{
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
        {
            const bool playing = position->getIsPlaying();
            const auto time = position->getTimeInSamples();
            const bool seek = playing && wasPlaying && time && haveExpectedPosition
                           && std::abs(*time - expectedPosition) > 2;
            if (playing && (!wasPlaying || seek)) restartStream();
            wasPlaying = playing;
            haveExpectedPosition = playing && time.hasValue();
            if (time) expectedPosition = *time + blockSamples;
            return;
        }
    wasPlaying = haveExpectedPosition = false;
}
void SpectralStripProcessor::render(juce::AudioBuffer<float>& buffer, bool bypass)
{
    juce::ScopedNoDenormals guard;
    if (buffer.getNumSamples() == 0) return;
    trackTransport(buffer.getNumSamples());
    const int requested = juce::jlimit(0, SpectralEngine::choices-1, static_cast<int>(value("fftSize")));
    const auto requestedChain = chainCode.load();
    const auto requestedRevision = stateRevision.load();
    if (requested != engine.getChoice() || requestedChain != activeChainCode || requestedRevision != activeStateRevision)
    {
        if (reconfigureGain.getCurrentValue() <= 0.0f)
        {
            engine.selectSize(requested);
            activeChainCode = requestedChain;
            activeStateRevision = requestedRevision;
            setLatencySamples(activeLatency()); activeFFTSize.store(engine.getSize());
            warmupSamples = getLatencySamples();
        }
        reconfigureGain.setTargetValue(0);
    }
    else if (warmupSamples == 0) reconfigureGain.setTargetValue(1);
    const int count = juce::jmin(2, getTotalNumInputChannels());
    for (int ch = count; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, buffer.getNumSamples());
    SpectralEngine::Settings s;
    s.useInstances=true;
    for(int i=0;i<SpectralEngine::maxModules;++i)
    {
        s.chain[static_cast<size_t>(i)]=static_cast<int>((activeChainCode >> (8*i)) & 255);
        auto& settings=s.modules[static_cast<size_t>(i)];
        const auto read=[&](int index) { return moduleParameters[static_cast<size_t>(i)][static_cast<size_t>(index)]->load(); };
        settings.freezeModule=read(0)>.5f; settings.freeze=read(1)>.5f; settings.gate=read(2)>.5f;
        settings.invert=read(3)>.5f; settings.delay=read(4)>.5f;
        settings.smear=read(5); settings.shimmer=read(6); settings.threshold=read(7); settings.delayMs=read(8); settings.spread=read(9);
        settings.gateAttack=read(10); settings.gateRelease=read(11); settings.gateTilt=read(12);
        settings.feedback=read(13); settings.damping=read(14); settings.delayMix=read(15);
        settings.blur=read(16)>.5f; settings.blurAmount=read(17); settings.blurWidth=read(18); settings.blurMix=read(19);
        settings.tremolo=read(20)>.5f; settings.tremoloRate=read(21); settings.tremoloDepth=read(22); settings.tremoloShape=read(23);
        settings.reverb=read(24)>.5f;settings.reverbDecay=read(25);settings.reverbSize=read(26);settings.reverbShimmer=read(27);settings.reverbTone=read(28);settings.reverbMix=read(29);
    }
    s.bandFilter=value("bandFilter")>.5f; s.low=value("low"); s.high=value("high");
    mix.setTargetValue(bypass ? 0.0f : value("mix"));
    outputGain.setTargetValue(bypass ? 1.0f : juce::Decibels::decibelsToGain(value("output")));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float in[2]{}, wet[2]{}, dry[2]{};
        // Fade the INPUT so the ramp travels through the FFT and the compensated dry.
        // A fade of the output during its initial latency would finish before audio arrives.
        const float t = static_cast<float>(startupPosition) / static_cast<float>(fadeSamples);
        const float onsetGain = t*t*(3.0f-2.0f*t);
        startupPosition = std::min(fadeSamples, startupPosition+1);
        for (int ch = 0; ch < count; ++ch) in[ch] = buffer.getSample(ch, i) * onsetGain;
        engine.process(in, wet, dry, count, s);
        float transitionGain = reconfigureGain.getNextValue();
        if (warmupSamples > 0)
        {
            transitionGain = 0;
            if (--warmupSamples == 0 && requested == engine.getChoice() && requestedChain == activeChainCode && requestedRevision == activeStateRevision) reconfigureGain.setTargetValue(1);
        }
        const float blend = mix.getNextValue(), gain = outputGain.getNextValue() * transitionGain;
        const float b = static_cast<float>(bridgePosition) / static_cast<float>(fadeSamples);
        const float bridgeGain = 1.0f-b*b*(3.0f-2.0f*b);
        bridgePosition = std::min(fadeSamples, bridgePosition+1);
        for (int ch = 0; ch < count; ++ch)
        {
            const auto channel = static_cast<size_t>(ch);
            const float sample = (dry[ch] + blend * (wet[ch]-dry[ch])) * gain
                               + bridgeOutput[channel] * bridgeGain;
            buffer.setSample(ch, i, sample); lastOutput[channel] = sample;
        }
    }
}
void SpectralStripProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { render(b, false); }
void SpectralStripProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { render(b, true); }
juce::AudioProcessorEditor* SpectralStripProcessor::createEditor() { return new SpectralStripEditor(*this); }
void SpectralStripProcessor::getStateInformation(juce::MemoryBlock& data)
{ auto saved = state.copyState(); saved.setProperty("instanceChain", chainCode.load(), nullptr);
  saved.removeProperty("chainCode",nullptr);
  auto xml = saved.createXml(); copyXmlToBinary(*xml, data); }
void SpectralStripProcessor::setStateInformation(const void* data, int bytes)
{ auto xml = getXmlFromBinary(data, bytes); if (xml && xml->hasTagName(state.state.getType()))
    {
        auto restored = juce::ValueTree::fromXml(*xml);
        // Values that reproduce how older sessions sounded before each parameter existed.
        const std::pair<const char*, float> legacy[] = {{"fftSize", 2}, {"shimmer", 0}, {"gateAttack", 35},
                                                        {"gateRelease", 35}, {"gateTilt", 0}, {"damping", 0}};
        for (const auto& [id, legacyValue] : legacy)
            if (!restored.getChildWithProperty("id", id).isValid())
            {
                juce::ValueTree parameter("PARAM"); parameter.setProperty("id", id, nullptr);
                parameter.setProperty("value", legacyValue, nullptr); restored.appendChild(parameter, nullptr);
            }
        Chain chain {};
        if(restored.hasProperty("instanceChain"))
        {
            const auto packed=static_cast<juce::int64>(restored.getProperty("instanceChain"));
            for(int i=0;i<SpectralEngine::maxModules;++i) chain[static_cast<size_t>(i)]=static_cast<int>((packed >> (8*i)) & 255);
        }
        else
        {
            // Version 0.3: remove Filter from the movable chain; retain effect order.
            const int old=static_cast<int>(restored.getProperty("chainCode",0x4321));
            int index=0;
            for(int i=0;i<4;++i)
            { const int type=(old >> (4*i)) & 15; if(type>=1 && type<=3) chain[static_cast<size_t>(index++)]=(type << 4)|type; }
        }
        setChain(chain,false);
        // Old sessions do not carry instance parameters. Fill missing values explicitly,
        // so state recall cannot inherit knobs from the previously loaded preset.
        for(auto* parameter : getParameters())
            if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))
                if(!restored.getChildWithProperty("id",ranged->paramID).isValid())
                {
                    juce::ValueTree child("PARAM"); child.setProperty("id",ranged->paramID,nullptr);
                    child.setProperty("value",ranged->convertFrom0to1(ranged->getDefaultValue()),nullptr); restored.appendChild(child,nullptr);
                }
        state.replaceState(restored);
        // Request a faded audio-thread reset even when topology/FFT are unchanged.
        stateRevision.fetch_add(1);
    } }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpectralStripProcessor; }


SpectralStripProcessor::Chain SpectralStripProcessor::getChain() const noexcept
{
    const auto code=chainCode.load(); Chain chain {};
    for(int i=0;i<SpectralEngine::maxModules;++i) chain[static_cast<size_t>(i)]=static_cast<int>((code >> (8*i)) & 255);
    return chain;
}
void SpectralStripProcessor::setChain(const Chain& chain,bool notifyHost)
{
    juce::int64 code=0; int position=0,seen=0;
    for(const int entry : chain)
    {
        const int slot=(entry & 15)-1,type=entry >> 4;
        if(slot>=0 && slot<SpectralEngine::maxModules && type>=1 && type<=6 && !(seen & (1 << slot)))
        { code|=static_cast<juce::int64>(entry) << (8*position++); seen|=1 << slot; }
    }
    if(chainCode.exchange(code)!=code && notifyHost) updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
bool SpectralStripProcessor::addInstance(int type)
{
    if(type<1 || type>6) return false;
    auto chain=getChain(); int used=0,position=0;
    for(const int entry : chain) if(entry) { used|=1 << ((entry & 15)-1); ++position; }
    if(position==SpectralEngine::maxModules) return false;
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
        if(!(used & (1 << slot))) { chain[static_cast<size_t>(position)]=(type << 4)|(slot+1); setChain(chain); return true; }
    return false;
}
juce::String SpectralStripProcessor::moduleParamID(int slot,const char* id)
{
    const juce::String name(id);
    if((slot==0 && (name=="freezeModule" || name=="freeze" || name=="smear" || name=="shimmer"))
       || (slot==1 && (name=="gate" || name=="invert" || name=="threshold" || name=="gateAttack" || name=="gateRelease" || name=="gateTilt"))
       || (slot==2 && (name=="delay" || name=="delayMs" || name=="spread" || name=="feedback" || name=="damping" || name=="delayMix"))) return name;
    return "instance"+juce::String(slot+1)+"_"+name;
}
