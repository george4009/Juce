#pragma once
#include <juce_dsp/juce_dsp.h>
#include "ShimmerReverb.h"
#include <array>
#include <atomic>
#include <complex>
#include <vector>

// Selectable-size STFT, sqrt-Hann analysis/synthesis, 4x overlap.
// Output frames are scheduled N samples after their corresponding input.
class SpectralEngine
{
public:
    static constexpr int minOrder = 9, maxOrder = 13, choices = maxOrder-minOrder+1;
    static constexpr int defaultSize = 2048, defaultHop = defaultSize/4;
    static constexpr int maxSize = 1 << maxOrder, maxBins = maxSize/2+1, displayBins = 96;
    SpectralEngine()
    {
        for (int index = 0; index < choices; ++index)
        {
            const int n = 1 << (minOrder + index);
            transforms[static_cast<size_t>(index)] = std::make_unique<juce::dsp::FFT>(minOrder+index);
            for (int i = 0; i < n; ++i)
                windows[static_cast<size_t>(index)][static_cast<size_t>(i)] =
                    std::sqrt(0.5f - 0.5f*std::cos(twoPi*static_cast<float>(i)/static_cast<float>(n)));
        }
    }
    int getSize() const noexcept { return size; }
    int getChoice() const noexcept { return activeChoice; }
    static constexpr int maxModules = 8;
    struct ModuleSettings
    {
        bool freezeModule = true, freeze = false, gate = false, invert = false, delay = false, blur = true, tremolo = true, reverb = true;
        float smear=0, shimmer=0, threshold=-48, delayMs=250, spread=.5f;
        float gateAttack=10, gateRelease=100, gateTilt=3;
        float feedback=.3f, damping=0, delayMix=.5f;
        float blurAmount=.6f, blurWidth=400, blurMix=1;
        float tremoloRate=4, tremoloDepth=.75f, tremoloShape=0;
        float reverbDecay=6,reverbSize=.6f,reverbShimmer=.45f,reverbTone=.55f,reverbMix=.35f;
    };
    struct Settings : ModuleSettings
    {
        // High nibble = type; low nibble = stable instance slot + 1.
        std::array<int, maxModules> chain {0x11,0x22,0x33,0,0,0,0,0};
        std::array<ModuleSettings, maxModules> modules {};
        bool useInstances = false, bandFilter = true;
        float low=20, high=20000;
    };
    int getLatency(const std::array<int,maxModules>& chain) const noexcept
    {int extra=0;for(const int entry:chain)if((entry>>4)==6)extra+=size-hop;return size+extra;}
    // 0 at LOW, 1 at HIGH, evenly spaced in octaves.
    static float bandPosition(float frequency, float lo, float hi) noexcept
    {
        lo = std::max(lo, 1.0f);
        const float span = std::log(std::max(hi, lo) / lo);
        if (span < 1.0e-4f) return 0.5f;
        return juce::jlimit(0.0f, 1.0f, std::log(std::max(frequency, lo) / lo) / span);
    }
    std::array<std::atomic<float>, displayBins> spectrum {};
    void prepare(double sampleRate, int choice = 2)
    {
        sr = sampleRate;
        // Allocate the maximum needed storage across all choices, not max frames * max bins.
        size_t capacity = 0;
        for (int index = 0; index < choices; ++index)
        {
            const int n = 1 << (minOrder+index);
            capacity = std::max(capacity, static_cast<size_t>((static_cast<int>(std::ceil(2.0*sr/(n/4)))+3)*(n/2+1)));
        }
        for (int ch=0;ch<2;++ch) for (auto& module : channels[static_cast<size_t>(ch)].modules)
        {module.history.resize(capacity);module.reverb.prepare(sr,ch);}
        selectSize(choice);
    }
    // Audio-thread only, at a muted block boundary. Storage/plans already exist.
    // History is invalidated logically, avoiding a multi-megabyte clear in the callback.
    void selectSize(int choice) noexcept
    {
        activeChoice = juce::jlimit(0, choices-1, choice);
        size = 1 << (minOrder+activeChoice); hop = size/4; bins = size/2+1;
        historyFrames = static_cast<int>(std::ceil(2.0*sr/hop))+3;
        for (auto& c : channels)
        {
            c.input.fill(0); c.output.fill(0); c.fftData.fill(0);c.dryHistory.fill(0);
            c.filterGain.fill(1);
            for (int i=0; i<maxModules; ++i)
            {
                auto& module=c.modules[static_cast<size_t>(i)];
                module.previousPhase.fill(0); module.heldPhase.fill(0); module.heldStep.fill(0);
                module.smearPhase.fill(0);module.smearStep.fill(0);module.previousMagnitude.fill(0);module.captureStep.fill(0);
                module.heldMagnitude.fill(0); module.smoothedMagnitude.fill(0); module.gateGain.fill(0);
                module.reverb.reset();module.bridgeInput.fill(0);module.bridgeOutput.fill(0);module.bridgeDry.fill(0);module.bridgePosition=0;
                module.blurPhase.fill(0); module.blurInitialised=false;
                module.blurBlend=module.blurAmount=0; module.blurWidth=400;
                module.tremoloPhase=0; module.tremoloDepth=0; module.tremoloRate=4; module.tremoloShape=0;
                module.frozen=false; module.freezeBlend=module.gateBlend=module.delayBlend=0;
                module.noise=(0x9e3779b9u+static_cast<uint32_t>(i)*0x85ebca6bu) | 1u;
            }
        }
        for (auto& module : channels[1].modules) module.noise ^= 0x7f4a7c15u;
        position = dryPosition = hopCount = frameIndex = validFrames = samplesSinceReset = 0;
        for (auto& v : spectrum) v.store(-90.0f, std::memory_order_relaxed);
    }
    // Called once per sample. No allocation, locks, or GUI access here.
    void process(const float* in, float* wet, float* dry, int count, const Settings& s)
    {
        for (int ch = 0; ch < count; ++ch)
        {
            auto& c = channels[static_cast<size_t>(ch)];
            const int latency=getLatency(s.chain);
            dry[ch] = c.dryHistory[static_cast<size_t>((dryPosition-latency+maxDrySize)%maxDrySize)];
            c.dryHistory[static_cast<size_t>(dryPosition)]=in[ch];
            c.input[static_cast<size_t>(position)] = in[ch];
            wet[ch] = c.output[static_cast<size_t>(position)];
            c.output[static_cast<size_t>(position)] = 0;
        }
        samplesSinceReset = std::min(maxDrySize, samplesSinceReset+1);
        dryPosition=(dryPosition+1)%maxDrySize;
        position = (position + 1) % size;
        if (++hopCount == hop)
        {
            hopCount = 0;
            for (int ch = 0; ch < count; ++ch) transform(channels[static_cast<size_t>(ch)], s, ch == 0);
            frameIndex = (frameIndex + 1) % historyFrames;
            validFrames = std::min(historyFrames, validFrames+1);
        }
    }
private:
    static constexpr float twoPi = juce::MathConstants<float>::twoPi;
    struct ModuleState
    {
        std::array<float, maxBins> previousPhase {}, heldPhase {}, heldStep {}, heldMagnitude {}, smoothedMagnitude {}, gateGain {};
        std::array<float,maxBins> smearPhase {},smearStep {},previousMagnitude {},captureStep {};
        ShimmerReverb reverb;
        std::array<float,maxSize> bridgeInput {},bridgeOutput {},bridgeDry {};
        int bridgePosition=0;
        std::array<float,maxBins> blurPhase {};
        bool blurInitialised=false;
        float blurBlend=0,blurAmount=0,blurWidth=400;
        double tremoloPhase=0;
        float tremoloDepth=0,tremoloRate=4,tremoloShape=0;
        std::vector<std::complex<float>> history;
        bool frozen = false;
        float freezeBlend = 0, gateBlend = 0, delayBlend = 0;
        uint32_t noise = 1;
        // Uniform in [-1, 1]; xorshift32 keeps the audio thread allocation-free and deterministic.
        float random() noexcept
        {
            noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5;
            return static_cast<float>(noise) * (2.0f / 4294967295.0f) - 1.0f;
        }
    };
    static constexpr int maxDrySize=maxSize*(maxModules+1);
    struct Channel
    {
        std::array<float,maxSize> input {},output {};
        std::array<float,maxDrySize> dryHistory {};
        std::array<float,maxSize*2> fftData {}, tremoloData {}, bridgeFFT {};
        std::array<float,maxBins> filterGain {};
        std::array<std::complex<float>,maxBins> working {},original {};
        std::array<float,maxBins> blurPower {},blurTemporary {};
        std::array<double,maxBins+1> blurPrefix {};
        std::array<ModuleState,maxModules> modules;
    };
    std::array<Channel, 2> channels;
    std::array<std::array<float, maxSize>, choices> windows {};
    std::array<std::unique_ptr<juce::dsp::FFT>, choices> transforms;
    int size = defaultSize, hop = defaultHop, bins = defaultSize/2+1, activeChoice = 2;
    int validFrames = 0, samplesSinceReset = 0;
    double sr = 44100;
    int position = 0, dryPosition=0, hopCount = 0, frameIndex = 0, historyFrames = 0;
    static float wrapped(float x) { return std::remainder(x, twoPi); }
    void applyBlur(Channel& c,ModuleState& module,const ModuleSettings& params,float lo,float hi,float transition)
    {
        module.blurBlend+=transition*((params.blur ? params.blurMix : 0.0f)-module.blurBlend);
        module.blurAmount+=transition*(params.blurAmount-module.blurAmount);
        module.blurWidth+=transition*(params.blurWidth-module.blurWidth);
        if(module.blurBlend<1e-7f || module.blurAmount<1e-7f) return;
        // DC/Nyquist stay real and unmodified. The selected band is a closed domain.
        const int first=juce::jlimit(1,bins-2,static_cast<int>(std::ceil(lo*size/sr)));
        const int last=juce::jlimit(1,bins-2,static_cast<int>(std::floor(hi*size/sr)));
        if(last<first) return;
        if(!module.blurInitialised)
        { for(int k=0;k<bins;++k) module.blurPhase[static_cast<size_t>(k)]=module.random()*juce::MathConstants<float>::pi; module.blurInitialised=true; }
        double inputPower=0; float peak=0;
        for(int k=first;k<=last;++k)
        {
            const float power=std::norm(c.working[static_cast<size_t>(k)]);
            c.blurPower[static_cast<size_t>(k)]=power; inputPower+=power; peak=std::max(peak,power);
        }
        // Three moving averages approximate a smooth bell, in O(bins) per pass.
        const float radius=juce::jlimit(0.0f,static_cast<float>(last-first),module.blurWidth*static_cast<float>(size/sr)/2.0f);
        const int whole=static_cast<int>(radius); const float fractional=radius-static_cast<float>(whole);
        for(int pass=0;pass<3;++pass)
        {
            c.blurPrefix[static_cast<size_t>(first)]=0;
            for(int k=first;k<=last;++k) c.blurPrefix[static_cast<size_t>(k+1)]=c.blurPrefix[static_cast<size_t>(k)]+c.blurPower[static_cast<size_t>(k)];
            for(int k=first;k<=last;++k)
            {
                const auto average=[&](int r)
                { const int a=std::max(first,k-r),b=std::min(last,k+r); return (c.blurPrefix[static_cast<size_t>(b+1)]-c.blurPrefix[static_cast<size_t>(a)])/(b-a+1); };
                c.blurTemporary[static_cast<size_t>(k)]=static_cast<float>((1-fractional)*average(whole)+fractional*average(whole+1));
            }
            for(int k=first;k<=last;++k) c.blurPower[static_cast<size_t>(k)]=c.blurTemporary[static_cast<size_t>(k)];
        }
        double spreadPower=0;
        for(int k=first;k<=last;++k) spreadPower+=c.blurPower[static_cast<size_t>(k)];
        const float normalise=spreadPower>1e-20 ? static_cast<float>(inputPower/spreadPower) : 1;
        const float feather=std::max(static_cast<float>(sr/size),30.0f);
        for(int k=first;k<=last;++k)
        {
            auto& value=c.working[static_cast<size_t>(k)];
            const float power=std::norm(value);
            auto& phase=module.blurPhase[static_cast<size_t>(k)];
            phase=wrapped(phase+twoPi*static_cast<float>(k*hop)/static_cast<float>(size));
            // Anchor occupied partials; newly filled bins run with continuous phase.
            if(power>peak*.25f && power>1e-20f) phase=std::arg(value);
            // Amount blends once with the fully diffused spectrum below.
            const float magnitude=std::sqrt(std::max(0.0f,c.blurPower[static_cast<size_t>(k)]*normalise));
            const float frequency=static_cast<float>(k*sr/size);
            const float mask=juce::jlimit(0.0f,1.0f,(frequency-lo)/feather)*juce::jlimit(0.0f,1.0f,(hi-frequency)/feather);
            value+=module.blurBlend*module.blurAmount*mask*(std::polar(magnitude,phase)-value);
        }
    }
    void lockFrozenPartials(ModuleState& module)
    {
        // All bins belonging to a captured partial must keep their original
        // relative phase. Independent frequency estimates otherwise make a
        // single partial spread and beat against itself during the hold.
        std::array<int,maxBins> peaks {};int count=0;
        float maximum=0;for(int k=1;k<bins-1;++k)maximum=std::max(maximum,module.heldMagnitude[static_cast<size_t>(k)]);
        if(maximum<1e-8f)return;
        for(int k=1;k<bins-1;++k)
        {
            const auto b=static_cast<size_t>(k);const float m=module.heldMagnitude[b];
            if(m>maximum*.001f && m>=module.heldMagnitude[b-1] && m>module.heldMagnitude[b+1])peaks[static_cast<size_t>(count++)]=k;
        }
        if(count==0)return;
        int region=0;
        for(int k=1;k<bins-1;++k)
        {
            while(region+1<count && 2*k>peaks[static_cast<size_t>(region)]+peaks[static_cast<size_t>(region+1)])++region;
            module.heldStep[static_cast<size_t>(k)]=module.heldStep[static_cast<size_t>(peaks[static_cast<size_t>(region)])];
        }
    }
    void applyTremolo(Channel& c,ModuleState& module,const ModuleSettings& params,const Settings& settings,float transition)
    {
        module.tremoloDepth+=transition*((params.tremolo ? params.tremoloDepth : 0.0f)-module.tremoloDepth);
        module.tremoloRate+=transition*(params.tremoloRate-module.tremoloRate);
        module.tremoloShape+=transition*(params.tremoloShape-module.tremoloShape);
        const double phase=module.tremoloPhase;
        const double step=static_cast<double>(module.tremoloRate)/sr;
        module.tremoloPhase=std::fmod(phase+hop*step,1.0);
        if(module.tremoloDepth<1e-7f) return;
        // Modulate each sample of the current analysis frame, not once per FFT hop.
        // This preserves rapid LFO motion even at the largest FFT, at this chain position.
        auto& fft=*transforms[static_cast<size_t>(activeChoice)];
        c.tremoloData.fill(0);
        for(int k=0;k<bins;++k)
        {
            c.tremoloData[static_cast<size_t>(2*k)]=c.working[static_cast<size_t>(k)].real();
            c.tremoloData[static_cast<size_t>(2*k+1)]=(k==0 || k==bins-1) ? 0.0f : c.working[static_cast<size_t>(k)].imag();
        }
        fft.performRealOnlyInverseTransform(c.tremoloData.data());
        // Morph both the edge duration and, above halfway, the duty cycle.
        // At Shape zero this raised-cosine envelope is exactly the sine LFO.
        // At halfway it is a rounded square; at one a short 12.5% pulse.
        const float shape=juce::jlimit(0.0f,1.0f,module.tremoloShape);
        const float narrow=juce::jlimit(0.0f,1.0f,2*shape-1);
        const float duty=.5f-.375f*narrow*narrow*(3-2*narrow);
        // At least 2 ms per edge at high rates, avoiding hard discontinuities.
        const float edge=std::max(.5f*std::pow(.04f,std::min(2*shape,1.0f)),module.tremoloRate*.002f);
        for(int i=0;i<size;++i)
        {
            const double cycle=phase+i*step;
            const float distance=static_cast<float>(std::abs(cycle-std::floor(cycle+.5)));
            const float ramp=juce::jlimit(0.0f,1.0f,(distance-.5f*(duty-edge))/edge);
            const float envelope=.5f+.5f*std::cos(juce::MathConstants<float>::pi*ramp);
            const float gain=1-module.tremoloDepth*(1-envelope);
            c.tremoloData[static_cast<size_t>(i)]*=gain;
        }
        fft.performRealOnlyForwardTransform(c.tremoloData.data(),true);
        const float lo=std::min(settings.low,settings.high), hi=std::max(settings.low,settings.high);
        const float feather=std::max(static_cast<float>(sr/size),30.0f);
        for(int k=0;k<bins;++k)
        {
            const float frequency=static_cast<float>(k*sr/size);
            const float mask=(lo<=20 ? 1.0f : juce::jlimit(0.0f,1.0f,(frequency-lo)/feather))
                            *(hi>=20000 ? 1.0f : juce::jlimit(0.0f,1.0f,(hi-frequency)/feather));
            const std::complex<float> modulated {c.tremoloData[static_cast<size_t>(2*k)],c.tremoloData[static_cast<size_t>(2*k+1)]};
            auto& value=c.working[static_cast<size_t>(k)]; value+=mask*(modulated-value);
        }
    }
    void applyReverb(Channel& c,ModuleState& module,const ModuleSettings& params,const Settings& settings)
    {
        auto& fft=*transforms[static_cast<size_t>(activeChoice)];const auto& window=windows[static_cast<size_t>(activeChoice)];
        c.tremoloData.fill(0);
        for(int k=0;k<bins;++k)
        {c.tremoloData[static_cast<size_t>(2*k)]=c.working[static_cast<size_t>(k)].real();c.tremoloData[static_cast<size_t>(2*k+1)]=(k==0 || k==bins-1) ? 0.0f : c.working[static_cast<size_t>(k)].imag();}
        fft.performRealOnlyInverseTransform(c.tremoloData.data());
        for(int i=0;i<size;++i)module.bridgeInput[static_cast<size_t>((position+i)%size)]+=c.tremoloData[static_cast<size_t>(i)]*window[static_cast<size_t>(i)]*.5f;
        module.reverb.set(params.reverbDecay,params.reverbSize,params.reverbShimmer,params.reverbTone,params.reverb ? params.reverbMix : 0.0f);
        if(validFrames==0)module.reverb.reset();
        // Consume only the finalised hop: never process overlapping samples twice.
        for(int i=0;i<hop;++i)
        {
            const auto index=static_cast<size_t>((position+i)%size);const float input=module.bridgeInput[index];module.bridgeInput[index]=0;
            module.bridgeDry[static_cast<size_t>(module.bridgePosition)]=input;
            module.bridgeOutput[static_cast<size_t>(module.bridgePosition)]=module.reverb.process(input);
            module.bridgePosition=(module.bridgePosition+1)%size;
        }
        c.tremoloData.fill(0);c.bridgeFFT.fill(0);
        for(int i=0;i<size;++i)
        {const auto index=static_cast<size_t>((module.bridgePosition+i)%size);c.tremoloData[static_cast<size_t>(i)]=module.bridgeOutput[index]*window[static_cast<size_t>(i)];c.bridgeFFT[static_cast<size_t>(i)]=module.bridgeDry[index]*window[static_cast<size_t>(i)];}
        fft.performRealOnlyForwardTransform(c.tremoloData.data(),true);fft.performRealOnlyForwardTransform(c.bridgeFFT.data(),true);
        const float lo=std::min(settings.low,settings.high),hi=std::max(settings.low,settings.high),feather=std::max(static_cast<float>(sr/size),30.0f);
        for(int k=0;k<bins;++k)
        {
            const float frequency=static_cast<float>(k*sr/size);
            const float mask=(lo<=20 ? 1.0f : juce::jlimit(0.0f,1.0f,(frequency-lo)/feather))*(hi>=20000 ? 1.0f : juce::jlimit(0.0f,1.0f,(hi-frequency)/feather));
            const std::complex<float> dry {c.bridgeFFT[static_cast<size_t>(2*k)],c.bridgeFFT[static_cast<size_t>(2*k+1)]}, wet {c.tremoloData[static_cast<size_t>(2*k)],c.tremoloData[static_cast<size_t>(2*k+1)]};
            c.working[static_cast<size_t>(k)]=dry+mask*(wet-dry);
        }
    }
    void transform(Channel& c, const Settings& s, bool display)
    {
        auto& fft = *transforms[static_cast<size_t>(activeChoice)];
        const auto& window = windows[static_cast<size_t>(activeChoice)];
        for (int i = 0; i < size; ++i)
            c.fftData[static_cast<size_t>(i)] = c.input[static_cast<size_t>((position + i) % size)] * window[static_cast<size_t>(i)];
        fft.performRealOnlyForwardTransform(c.fftData.data(), true);
        const float transition=1.0f-std::exp(-static_cast<float>(hop/(sr*.035)));
        const float smearTracking=1-std::exp(-static_cast<float>(hop/(sr*.05)));
        const float captureTracking=1-std::exp(-static_cast<float>(hop/(sr*.020)));
        const auto follow=[this](float ms) { return 1.0f-std::exp(-static_cast<float>(hop/(sr*std::max(.1f,ms)*.001))); };
        std::array<float,maxModules> smoothing {},attack {},release {};
        std::array<bool,maxModules> hold {};
        std::array<int,maxModules> readyAfter {};readyAfter.fill(size);
        int precedingLatency=0;
        for(const int entry:s.chain)
        {
            const int slot=(entry&15)-1;
            if(slot>=0 && slot<maxModules)readyAfter[static_cast<size_t>(slot)]=size+precedingLatency;
            if((entry>>4)==6)precedingLatency+=size-hop;
        }
        for (int i=0; i<maxModules; ++i)
        {
            auto& module=c.modules[static_cast<size_t>(i)];
            const auto& params=s.useInstances ? s.modules[static_cast<size_t>(i)] : static_cast<const ModuleSettings&>(s);
            hold[static_cast<size_t>(i)]=params.freezeModule && params.freeze && samplesSinceReset>=readyAfter[static_cast<size_t>(i)];
            module.freezeBlend+=transition*((hold[static_cast<size_t>(i)] ? 1.0f : 0.0f)-module.freezeBlend);
            module.gateBlend+=transition*((params.gate ? 1.0f : 0.0f)-module.gateBlend);
            module.delayBlend+=transition*((params.delay ? params.delayMix : 0.0f)-module.delayBlend);
            const float seconds=params.freezeModule ? params.smear*params.smear*1.5f : 0;
            smoothing[static_cast<size_t>(i)]=seconds>.0001f ? std::exp(-static_cast<float>(hop/sr)/seconds) : 0;
            attack[static_cast<size_t>(i)]=follow(params.gateAttack); release[static_cast<size_t>(i)]=follow(params.gateRelease);

        }
        const float lo=std::min(s.low,s.high), hi=std::max(s.low,s.high);
        const bool originalOrder=s.chain==std::array<int,maxModules>{0x11,0x22,0x33,0,0,0,0,0};
        std::array<float, displayBins> levels;
        levels.fill(-90.0f);
        for(int k=0;k<bins;++k)
            c.working[static_cast<size_t>(k)]=c.original[static_cast<size_t>(k)]={c.fftData[static_cast<size_t>(2*k)],c.fftData[static_cast<size_t>(2*k+1)]};
        const float feather=std::max(static_cast<float>(sr/size),30.0f);
        for(const int entry : s.chain)
        {
            if(!entry) continue;
            const int slot=(entry & 15)-1,type=entry >> 4;
            if(slot<0 || slot>=maxModules || type<1 || type>6) continue;
            auto& module=c.modules[static_cast<size_t>(slot)];
            const auto& params=s.useInstances ? s.modules[static_cast<size_t>(slot)] : static_cast<const ModuleSettings&>(s);
            if(type==4) { applyBlur(c,module,params,lo,hi,transition); continue; }
            if(type==6) { applyReverb(c,module,params,s); continue; }
            if(type==5) { applyTremolo(c,module,params,s,transition); continue; }
            for(int k=0;k<bins;++k)
            {
                const float frequency=static_cast<float>(k*sr/size);
                const float expected=twoPi*static_cast<float>(k*hop)/static_cast<float>(size);
                const float mask=juce::jlimit(0.0f,1.0f,(frequency-lo)/feather)*juce::jlimit(0.0f,1.0f,(hi-frequency)/feather);
                auto& value=c.working[static_cast<size_t>(k)];
                const auto before=value;
                switch(type)
                {
                case 1:
                {
                    const auto bin=static_cast<size_t>(k);
                    const float magnitude=std::abs(value),inputPhase=std::arg(value);
                    auto& smoothed=module.smoothedMagnitude[bin];
                    smoothed=smoothing[static_cast<size_t>(slot)]*smoothed+(1-smoothing[static_cast<size_t>(slot)])*magnitude;
                    // Remember the last reliable phase advance. arg(0) is zero:
                    // reusing it for a nonzero tail locks the output to the hop
                    // frequency instead of sustaining the original partials.
                    if(magnitude>std::max(1e-8f,smoothed*.05f))
                    {
                        if(module.previousMagnitude[bin]>1e-8f && magnitude>=module.previousMagnitude[bin]*.98f && magnitude<=module.previousMagnitude[bin]*1.02f)
                            {
                            const float measured=expected+wrapped(inputPhase-module.previousPhase[bin]-expected);
                            module.smearStep[bin]+=smearTracking*wrapped(measured-module.smearStep[bin]);
                        }
                        else if(module.previousMagnitude[bin]<=1e-8f)
                            module.smearStep[bin]=expected;
                        module.smearPhase[bin]=inputPhase;
                    }
                    else
                        module.smearPhase[bin]=wrapped(module.smearPhase[bin]+module.smearStep[bin]);
                    // A short running frequency estimate rejects capture-frame
                    // phase jitter while keeping the current magnitudes/phases.
                    if(magnitude>1e-8f && module.previousMagnitude[bin]>1e-8f)
                    {
                        const float measured=expected+wrapped(inputPhase-module.previousPhase[bin]-expected);
                        module.captureStep[bin]+=captureTracking*wrapped(measured-module.captureStep[bin]);
                    }
                    else module.captureStep[bin]=expected;
                    module.previousMagnitude[bin]=magnitude;
                    if (hold[static_cast<size_t>(slot)] && !module.frozen)
                    {
                        module.heldMagnitude[static_cast<size_t>(k)] = module.smoothedMagnitude[static_cast<size_t>(k)];
                        module.heldPhase[static_cast<size_t>(k)] = smoothing[static_cast<size_t>(slot)]>0 ? module.smearPhase[bin] : inputPhase;
                        module.heldStep[static_cast<size_t>(k)] = smoothing[static_cast<size_t>(slot)]>0 ? module.smearStep[bin] : module.captureStep[bin];
                    }
                    else
                    {
                        module.heldPhase[static_cast<size_t>(k)] = wrapped(module.heldPhase[static_cast<size_t>(k)] + module.heldStep[static_cast<size_t>(k)]);
                    }
                    value = std::polar(smoothed, smoothing[static_cast<size_t>(slot)]>0 ? module.smearPhase[bin] : inputPhase);
                    value += module.freezeBlend * (std::polar(module.heldMagnitude[static_cast<size_t>(k)], module.heldPhase[static_cast<size_t>(k)]) - value);
                    module.previousPhase[static_cast<size_t>(k)] = std::arg(before);
                    break;
                }
                case 2:
                {
                    const float db = juce::Decibels::gainToDecibels(std::abs(value) * 2.0f / static_cast<float>(size), -120.0f);
                    // Tilt lowers the threshold above 1 kHz and raises it below, per octave.
                    const float threshold = params.threshold - params.gateTilt * std::log2(std::max(frequency, 20.0f) / 1000.0f);
                    float gain = juce::jlimit(0.0f, 1.0f, (db - threshold + 6.0f) / 12.0f);
                    gain = gain * gain * (3 - 2 * gain);
                    if (params.invert) gain = 1 - gain;
                    auto& gateGain = module.gateGain[static_cast<size_t>(k)];
                    gateGain += (gain > gateGain ? attack[static_cast<size_t>(slot)] : release[static_cast<size_t>(slot)]) * (gain - gateGain);
                    value *= 1 + module.gateBlend * (gateGain - 1);
                    break;
                }
                case 3:
                {
                    const float proportion = bandPosition(frequency, lo, hi);
                    const float seconds = params.delayMs * 0.001f * (1 + params.spread * (2 * proportion - 1));
                    const float frames = juce::jlimit(1.0f, static_cast<float>(historyFrames-2), seconds * static_cast<float>(sr / hop));
                    const int d = static_cast<int>(frames);
                    const float fraction = frames - static_cast<float>(d);
                    const auto a = validFrames >= d ? module.history[static_cast<size_t>(((frameIndex-d+historyFrames)%historyFrames)*bins+k)] : std::complex<float>{};
                    const auto b = validFrames >= d+1 ? module.history[static_cast<size_t>(((frameIndex-d-1+historyFrames)%historyFrames)*bins+k)] : std::complex<float>{};
                    // Interpolate magnitude and unwrapped phase: mixing complex values
                    // would cancel partials whose phase advances by about half a turn per hop.
                    std::complex<float> delayed = a + fraction * (b-a);
                    const float magA = std::abs(a), magB = std::abs(b);
                    if (magA > 0 && magB > 0)
                    {
                        const float advance = expected + wrapped(std::arg(a) - std::arg(b) - expected);
                        delayed = std::polar(magA + fraction * (magB-magA), std::arg(a) - fraction * advance);
                    }
                    // Damping: each repeat loses up to 90% at 20 kHz, nothing below 200 Hz.
                    const float darkening = juce::jlimit(0.0f, 1.0f, std::log2(std::max(frequency, 200.0f) / 200.0f) / std::log2(100.0f));
                    const float damp = 1.0f - 0.9f * params.damping * darkening;
                    // Bound each feedback bin; no nonlinear processing in the neutral path.
                    auto stored = value + juce::jlimit(0.0f, 0.92f, params.feedback) * damp * delayed;
                    const float storedMag = std::abs(stored);
                    if (storedMag > static_cast<float>(size) * 4.0f) stored *= static_cast<float>(size) * 4.0f / storedMag;
                    module.history[static_cast<size_t>(frameIndex*bins+k)] = params.delay ? stored : std::complex<float>{};
                    value += module.delayBlend * (delayed - value);
                    break;
                }
                default: break;
                }
                if(!originalOrder) value=before+mask*(value-before);
            }
            if(type==1 && hold[static_cast<size_t>(slot)] && !module.frozen)lockFrozenPartials(module);
        }
        for(int k=0;k<bins;++k)
        {
            auto value=c.working[static_cast<size_t>(k)];
            const float frequency=static_cast<float>(k*sr/size);
            const float mask=juce::jlimit(0.0f,1.0f,(frequency-lo)/feather)*juce::jlimit(0.0f,1.0f,(hi-frequency)/feather);
            if (originalOrder) value=c.original[static_cast<size_t>(k)]+mask*(value-c.original[static_cast<size_t>(k)]);
            // The endpoints are fully open, so the default band remains transparent.
            const float highPass = s.low <= 20.0f ? 1.0f : juce::jlimit(0.0f, 1.0f, (frequency-s.low)/feather);
            const float lowPass = s.high >= 20000.0f ? 1.0f : juce::jlimit(0.0f, 1.0f, (s.high-frequency)/feather);
            // Keep cutoff identities: crossing the controls closes, never reopens, the band.
            const bool closed = s.high <= 20.0f || s.low >= 20000.0f || s.low >= s.high;
            const float filterTarget = s.bandFilter ? (closed ? 0.0f : highPass * lowPass) : 1.0f;
            auto& filterGain = c.filterGain[static_cast<size_t>(k)];
            // A fresh stream has no previous response to crossfade from.
            if(validFrames==0) filterGain=filterTarget;
            else filterGain += transition * (filterTarget-filterGain);
            value *= filterGain;
            c.fftData[static_cast<size_t>(2*k)] = value.real();
            c.fftData[static_cast<size_t>(2*k+1)] = (k == 0 || k == size/2) ? 0.0f : value.imag();
            if (display && frequency >= 20)
            {
                const int index = juce::jlimit(0, displayBins-1, static_cast<int>(std::log(frequency/20.0f)/std::log(1000.0f)*(displayBins-1)));
                levels[static_cast<size_t>(index)] = std::max(levels[static_cast<size_t>(index)], juce::Decibels::gainToDecibels(std::abs(value)*2.0f/static_cast<float>(size), -90.0f));
            }
        }
        for (int i=0;i<maxModules;++i) c.modules[static_cast<size_t>(i)].frozen=hold[static_cast<size_t>(i)];
        fft.performRealOnlyInverseTransform(c.fftData.data());
        for (int i = 0; i < size; ++i)
            c.output[static_cast<size_t>((position+i)%size)] += c.fftData[static_cast<size_t>(i)] * window[static_cast<size_t>(i)] * 0.5f;
        if (display) for (int i = 0; i < displayBins; ++i) spectrum[static_cast<size_t>(i)].store(levels[static_cast<size_t>(i)], std::memory_order_relaxed);
    }
};
