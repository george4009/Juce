#include "../Source/PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <random>
#include <chrono>
void runAuditTests()
{
    juce::ScopedJuceInitialiser_GUI gui;
    int failures=0;
    const auto check=[&](bool ok,const char* name) { std::cout<<(ok ? "PASS " : "FAIL ")<<name<<'\n'; if(!ok) ++failures; };
    float closedPeak=0;
    for(int choice=0;choice<SpectralEngine::choices;++choice)
    {
        auto engine=std::make_unique<SpectralEngine>();engine->prepare(48000,choice);
        SpectralEngine::Settings s;s.chain={};s.high=20;
        for(int reset=0;reset<2;++reset)
        {
            engine->selectSize(choice);
            for(int i=0;i<24000;++i)
            { float in[2]{.3f,0},wet[2]{},dry[2]{};engine->process(in,wet,dry,1,s);closedPeak=std::max(closedPeak,std::abs(wet[0])); }
        }
    }
    std::cout<<"Closed filter startup peak: "<<closedPeak<<'\n';
    check(closedPeak<1e-7f,"closed filter remains silent from startup/reset at every FFT size");
    auto p=std::make_unique<SpectralStripProcessor>();p->setChain({0x11});
    auto set=[&](const char* id,float v){auto* parameter=p->state.getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(v));};
    set("freeze",1);juce::MemoryBlock saved;p->getStateInformation(saved);p->prepareToPlay(48000,256);
    juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;int t=0;
    for(int block=0;block<200;++block)
    { for(int i=0;i<256;++i,++t)for(int ch=0;ch<2;++ch)audio.setSample(ch,i,.2f*std::sin(juce::MathConstants<float>::twoPi*1500*t/48000));p->processBlock(audio,midi); }
    double before=0;for(int block=0;block<40;++block){audio.clear();p->processBlock(audio,midi);before+=audio.getMagnitude(0,256);}
    check(before>.1,"freeze fixture sustains before state recall");
    p->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));double after=0;
    for(int block=0;block<200;++block){audio.clear();p->processBlock(audio,midi);if(block>150)after+=audio.getMagnitude(0,256);}
    std::cout<<"Freeze energy proxy before/after recall: "<<before<<" / "<<after<<'\n';
    check(after<1e-5,"same-chain state recall clears old captured audio");
    p.reset();
    for(double rate : {44100.0,96000.0}) for(int choice : {0,4})
    {
        auto e=std::make_unique<SpectralEngine>(); e->prepare(rate,choice);
        SpectralEngine::Settings s; s.useInstances=true; s.chain={0x11,0x22,0x33,0x44,0x55,0x36,0x47,0x58};
        std::mt19937 rng(987);std::uniform_real_distribution<float> noise(-.06f,.06f);
        float peak=0,leak=0; bool finite=true;
        const auto start=std::chrono::steady_clock::now();
        for(int i=0;i<static_cast<int>(rate*2);++i)
        {
            if(i%1024==0)
            {
                const bool alternate=(i/1024)%2;
                s.low=alternate ? 20.0f : 200.0f;s.high=alternate ? 20000.0f : 3000.0f;
                for(auto& m:s.modules)
                {
                    m.freeze=alternate;m.smear=alternate ? .8f : 0;m.shimmer=.6f;
                    m.gate=true;m.threshold=alternate ? -60.0f : -30.0f;
                    m.delay=true;m.feedback=.92f;m.delayMs=alternate ? 25.0f : 700.0f;m.spread=alternate ? 1.0f : -1.0f;
                    m.blurWidth=alternate ? 4000.0f : 20.0f;m.blurAmount=.8f;
                    m.tremoloRate=alternate ? 20.0f : .1f;m.tremoloDepth=1;m.tremoloShape=alternate ? 1.0f : 0.0f;
                }
            }
            float in[2]{noise(rng),0},wet[2]{},dry[2]{};e->process(in,wet,dry,2,s);
            finite=finite && std::isfinite(wet[0]) && std::isfinite(wet[1]);peak=std::max(peak,std::abs(wet[0]));leak=std::max(leak,std::abs(wet[1]));
        }
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"Mixed automation stress "<<rate<<" Hz FFT "<<e->getSize()<<": peak "<<peak<<", render/audio "<<seconds/2<<'\n';
        check(finite && peak<4 && leak<1e-7f,"eight-module automation stays finite and stereo-isolated");
    }
    // Test SHIMMER through the processor parameter IDs, for original and duplicate slots.
    for(int slot : {0,3})
    {
        const auto render=[&](bool hold,float amount)
        {
            auto processor=std::make_unique<SpectralStripProcessor>(); processor->setChain({0x10| (slot+1)});
            const auto set=[&](const char* id,float v)
            {auto* parameter=processor->state.getParameter(SpectralStripProcessor::moduleParamID(slot,id));parameter->setValueNotifyingHost(parameter->convertTo0to1(v));};
            processor->prepareToPlay(48000,256); juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer midi;
            std::vector<float> output;
            for(int block=0;block<400;++block)
            {
                if(block==64 && hold)set("freeze",1);
                if(block==128)set("shimmer",amount);
                for(int i=0;i<256;++i)
                {
                    const float input=hold && block>=96 ? 0.0f : .2f*std::sin(juce::MathConstants<float>::twoPi*440*(block*256+i)/48000);
                    audio.setSample(0,i,input);audio.setSample(1,i,input);
                }
                processor->processBlock(audio,midi);
                if(block>200)for(int i=0;i<256;++i)output.push_back(audio.getSample(0,i));
            }
            return output;
        };
        for(bool hold : {false,true})
        {
            const auto still=render(hold,0),moving=render(hold,1);double energy=0,difference=0;bool finite=true;
            for(size_t i=0;i<still.size();++i)
            {energy+=still[i]*still[i];difference+=(still[i]-moving[i])*(still[i]-moving[i]);finite=finite && std::isfinite(moving[i]);}
            const double ratio=difference/std::max(energy,1e-20);
            std::cout<<"Processor Shimmer slot "<<slot+1<<" Freeze "<<hold<<" relative difference "<<ratio<<'\n';
            check(finite && energy>.01 && ratio<1e-10,"Retired Freeze Shimmer has no effect, including legacy instance IDs");
        }
    }
    // A smoothed tone must retain its pitch after the input becomes silent.
    for(int choice : {0,1,2,3,4})
    {
        auto engine=std::make_unique<SpectralEngine>();engine->prepare(48000,choice);
        SpectralEngine::Settings settings;settings.chain={0x11};settings.smear=1;
        double energy=0,frozenEnergy=0;std::complex<double> fundamental {},frozenFundamental {};int count=0;
        for(int i=0;i<144000;++i)
        {
            if(i==108000)settings.freeze=true;
            float input[2]{i<48000 ? .2f*std::sin(juce::MathConstants<float>::twoPi*440*i/48000) : 0.0f,0},wet[2]{},dry[2]{};
            engine->process(input,wet,dry,1,settings);
            if(i>=72000 && i<96000){energy+=wet[0]*wet[0];fundamental+=static_cast<double>(wet[0])*std::polar(1.0,-juce::MathConstants<double>::twoPi*440*i/48000);++count;}
            if(i>=120000){frozenEnergy+=wet[0]*wet[0];frozenFundamental+=static_cast<double>(wet[0])*std::polar(1.0,-juce::MathConstants<double>::twoPi*440*i/48000);}
        }
        check(frozenEnergy>.01 && 2*std::norm(frozenFundamental)/(24000*std::max(frozenEnergy,1e-20))>.5,"Freeze captures Smear tail with correct pitch after input silence");
        const double pitchFraction=2*std::norm(fundamental)/(count*std::max(energy,1e-20));
        std::cout<<"Smear FFT "<<engine->getSize()<<" tail energy "<<energy<<" pitch fraction "<<pitchFraction<<'\n';
        check(energy>.01 && pitchFraction>.5,"Smear tail preserves the input pitch after silence");
        engine->selectSize(choice);float residue=0;
        for(int i=0;i<24000;++i){float in[2]{},wet[2]{},dry[2]{};engine->process(in,wet,dry,1,settings);residue=std::max(residue,std::abs(wet[0]));}
        check(residue==0,"Smear reset clears both amplitude and phase history");
    }
    bool checkStereoLeak=false;
    // Host parameters, duplicate slots, bypass, and the pre-capture meaning
    // of Smear: changing it must not rewrite an already frozen snapshot.
    for(int slot : {0,3})
    {
        const auto render=[&](float amount,bool enabled,bool frozen)
        {
            auto processor=std::make_unique<SpectralStripProcessor>();processor->setChain({0x10|(slot+1)});
            const auto change=[&](const char* id,float value)
            {auto* parameter=processor->state.getParameter(SpectralStripProcessor::moduleParamID(slot,id));parameter->setValueNotifyingHost(parameter->convertTo0to1(value));};
            change("freezeModule",enabled ? 1.0f : 0.0f);processor->prepareToPlay(48000,256);
            juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;std::vector<float> result;
            for(int block=0;block<576;++block)
            {
                if(block==32 && frozen)change("freeze",1);
                if(block==64)change("smear",amount);
                for(int i=0;i<256;++i)
                {const float x=block<188 ? .2f*std::sin(juce::MathConstants<float>::twoPi*440*(block*256+i)/48000) : 0;buffer.setSample(0,i,x);buffer.setSample(1,i,0);}
                processor->processBlock(buffer,midi);
                if(block>=400)for(int i=0;i<256;++i){result.push_back(buffer.getSample(0,i));checkStereoLeak=checkStereoLeak || std::abs(buffer.getSample(1,i))>1e-7f;}
            }
            return result;
        };
        const auto zero=render(0,true,false),middle=render(.5f,true,false),full=render(1,true,false),bypassed=render(1,false,false);
        const auto power=[](const std::vector<float>& x){double sum=0;for(float v:x)sum+=v*v;return sum;};
        std::cout<<"Smear parameter slot "<<slot+1<<" zero/mid/full/bypass tail "<<power(zero)<<" / "<<power(middle)<<" / "<<power(full)<<" / "<<power(bypassed)<<'\n';
        check(power(zero)<1e-12 && power(bypassed)<1e-12 && power(full)>power(middle)*2 && power(middle)>.00001,"Smear amount controls tail and bypass removes it through host parameters");
        const auto heldZero=render(0,true,true),heldFull=render(1,true,true);double error=0;
        for(size_t i=0;i<heldZero.size();++i)error+=std::pow(heldZero[i]-heldFull[i],2);
        check(error<1e-10 && power(heldZero)>.1,"Smear automation preserves an already captured Freeze");
    }
    check(!checkStereoLeak,"Smear does not leak into a silent stereo channel");
    // A frozen harmonic tone should not acquire drifting, independent bins.
    for(int rate : {44100,48000,96000})for(int choice : {0,1,2,3,4})
    {
        if(rate!=48000 && choice!=2)continue;
        auto e=std::make_unique<SpectralEngine>();e->prepare(rate,choice);
        SpectralEngine::Settings s;s.chain={0x11};s.bandFilter=false;
        double energy=0;std::array<std::complex<double>,4> partials {};int count=0;
        double minWindow=1e9,maxWindow=0,window=0;
        for(int i=0;i<rate*4;++i)
        {
            float x=0;for(int h=1;h<=4;++h)x+=.12f/h*std::sin(juce::MathConstants<double>::twoPi*220*h*i/rate+.31*h);
            s.freeze=i>=rate;float in[2]{i<rate*5/4 ? x : 0,0},wet[2]{},dry[2]{};e->process(in,wet,dry,1,s);
            if(i>=rate*2)
            {
                energy+=wet[0]*wet[0];window+=wet[0]*wet[0];++count;
                for(int h=1;h<=4;++h)partials[static_cast<size_t>(h-1)]+=static_cast<double>(wet[0])*std::polar(1.0,-juce::MathConstants<double>::twoPi*220*h*i/rate);
                if(count%(rate/20)==0){minWindow=std::min(minWindow,window);maxWindow=std::max(maxWindow,window);window=0;}
            }
        }
        double tonal=0;for(auto a:partials)tonal+=2*std::norm(a)/count;
        const double purity=tonal/std::max(energy,1e-20),variation=(maxWindow-minWindow)/std::max(maxWindow,1e-20);
        std::cout<<"Freeze harmonic rate "<<rate<<" FFT "<<e->getSize()<<" pitch purity "<<purity<<" level variation "<<variation<<" energy "<<energy/count<<'\n';
        check(purity>.9 && variation<.15,"Freeze preserves harmonic tone without strong artificial beating");
    }
    if(failures)throw std::runtime_error("General audit regressions failed");
}
