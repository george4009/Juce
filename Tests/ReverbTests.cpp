#include "../Source/PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <random>
static void demand(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void runReverbTests()
{
    for(int choice=0;choice<SpectralEngine::choices;++choice)for(int copies : {1,2})
    {
        auto engine=std::make_unique<SpectralEngine>();engine->prepare(48000,choice);
        SpectralEngine::Settings s;s.useInstances=true;s.chain=copies==1 ? std::array<int,SpectralEngine::maxModules>{0x61} : std::array<int,SpectralEngine::maxModules>{0x61,0x62};
        s.modules[0].reverbMix=s.modules[1].reverbMix=copies==1 ? 0.0f : 1.0f;
        s.modules[0].reverb=s.modules[1].reverb=copies==1;
        const int latency=engine->getLatency(s.chain);std::vector<float> source(60000);float error=0;
        for(int i=0;i<60000;++i)
        {
            source[static_cast<size_t>(i)]=.1f*std::sin(.121f*i)+.08f*std::cos(.077f*i);
            float in[2]{source[static_cast<size_t>(i)],0},wet[2]{},dry[2]{};engine->process(in,wet,dry,1,s);
            if(i>30000){const float expected=source[static_cast<size_t>(i-latency)];error=std::max(error,std::abs(wet[0]-expected));demand(std::abs(dry[0]-expected)<1e-7f,"Reverb dry latency mismatch");}
        }
        std::cout<<"Reverb bridge FFT "<<engine->getSize()<<" copies "<<copies<<" latency "<<latency<<" error "<<error<<'\n';
        demand(error<2e-5f,"Reverb bridge dry/wet alignment or neutral reconstruction failed");
    }
    {
        auto engine=std::make_unique<SpectralEngine>();engine->prepare(48000,4);
        SpectralEngine::Settings s;s.useInstances=true;s.chain={0x61,0x12};s.modules[0].reverbMix=0;s.modules[1].freeze=true;
        double energy=0;
        for(int i=0;i<96000;++i)
        {float in[2]{i<24000 ? .2f*std::sin(juce::MathConstants<float>::twoPi*1500*i/48000) : 0.0f,0},wet[2]{},dry[2]{};engine->process(in,wet,dry,1,s);if(i>60000)energy+=wet[0]*wet[0];}
        demand(energy>1,"Freeze after reverb captured empty bridge at startup");
        engine->selectSize(4);
        for(int i=0;i<48000;++i){float in[2]{},wet[2]{},dry[2]{};engine->process(in,wet,dry,1,s);demand(wet[0]==0,"Stale reverb/Freeze memory after reset");}
    }
    auto tail=[](float decay,float amount,float tone)
    {
        ShimmerReverb reverb;reverb.prepare(48000,0);reverb.set(decay,.6f,amount,tone,1);reverb.reset();
        std::vector<float> out(144000);
        for(int i=0;i<144000;++i)
        {
            const float input=i<12000 ? .2f*std::sin(juce::MathConstants<float>::twoPi*440*i/48000) : 0.0f;
            out[static_cast<size_t>(i)]=reverb.process(input);
            demand(std::isfinite(out[static_cast<size_t>(i)]) && std::abs(out[static_cast<size_t>(i)])<2,"Unstable reverb tail");
        }
        return out;
    };
    const auto shortTail=tail(.5f,0,.8f),longTail=tail(8,0,.8f),shifted=tail(8,1,.8f);
    const auto energy=[](const std::vector<float>& x){double sum=0;for(size_t i=48000;i<x.size();++i)sum+=x[i]*x[i];return sum;};
    const auto band=[](const std::vector<float>& x,double frequency)
    {
        double power=0;
        for(int start=24000;start<96000;start+=4096)
        {std::complex<double> sum {};for(int i=0;i<4096;++i)sum+=static_cast<double>(x[static_cast<size_t>(start+i)])*std::polar(.5-.5*std::cos(juce::MathConstants<double>::twoPi*i/4096),-juce::MathConstants<double>::twoPi*frequency*i/48000);power+=std::norm(sum);}
        return power;
    };
    std::cout<<"Reverb tails short/long "<<energy(shortTail)<<" / "<<energy(longTail)<<" octave plain/shifted "<<band(longTail,880)<<" / "<<band(shifted,880)<<'\n';
    demand(energy(longTail)>.001 && energy(longTail)>energy(shortTail)*10,"Decay does not control a sustained reverb tail");
    demand(band(shifted,880)>band(longTail,880)*10 && band(shifted,880)>.001,"Reverb feedback does not generate upper octave");
    // Regression: an octave alone is not enough if turning Shimmer up kills
    // the long tail. Exercise several pitches, rates and both stereo networks.
    for(int rate : {44100,48000,96000})for(float frequency : {220.0f,440.0f,450.0f})
    {
        double late[2]{},octave[2]{};
        for(int enabled=0;enabled<2;++enabled)
        {
            ShimmerReverb r;r.prepare(rate,rate==44100 ? 1 : 0);
            r.set(8,.6f,static_cast<float>(enabled),.8f,1);r.reset();
            std::complex<double> windowSum {};int count=0;
            for(int i=0;i<rate*6;++i)
            {
                const float in=i<rate/4 ? .2f*std::sin(juce::MathConstants<double>::twoPi*frequency*i/rate) : 0.0f;
                const float out=r.process(in);
                demand(std::isfinite(out) && std::abs(out)<2,"Shimmer sweep produced invalid output");
                if(i>=rate*2 && i<rate*4)late[enabled]+=out*out;
                if(i>=rate/2 && i<rate*2)
                {
                    windowSum+=static_cast<double>(out)*std::polar(.5-.5*std::cos(juce::MathConstants<double>::twoPi*count/4096),-juce::MathConstants<double>::twoPi*frequency*2*i/rate);
                    if(++count==4096){octave[enabled]+=std::norm(windowSum);windowSum={};count=0;}
                }
            }
        }
        std::cout<<"Shimmer sweep "<<rate<<" Hz / tone "<<frequency<<" tail ratio "<<late[1]/late[0]<<" octave ratio "<<octave[1]/octave[0]<<'\n';
        demand(late[1]>late[0]*.1,"Shimmer collapses the long reverb tail");
        demand(octave[1]>octave[0]*8,"Shimmer octave is missing on part of the pitch/rate sweep");
    }
    // Actual FFT bridge must deliver the octave and its sustained tail too.
    {
        auto e=std::make_unique<SpectralEngine>();e->prepare(48000,2);
        SpectralEngine::Settings settings;settings.useInstances=true;settings.chain={0x61};
        settings.modules[0].reverbDecay=8;settings.modules[0].reverbShimmer=1;
        settings.modules[0].reverbTone=.8f;settings.modules[0].reverbMix=1;
        double late=0;std::complex<double> octave {};double octavePower=0;int count=0;
        for(int i=0;i<192000;++i)
        {
            float in[2]{i<12000 ? .2f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*440*i/48000)) : 0.0f,0},wet[2]{},dry[2]{};
            e->process(in,wet,dry,1,settings);
            if(i>=96000)late+=wet[0]*wet[0];
            if(i>=24000 && i<96000)
            {
                octave+=static_cast<double>(wet[0])*std::polar(.5-.5*std::cos(juce::MathConstants<double>::twoPi*count/4096),-juce::MathConstants<double>::twoPi*880*i/48000);
                if(++count==4096){octavePower+=std::norm(octave);octave={};count=0;}
            }
        }
        demand(late>.1 && octavePower>1,"Shimmer octave/tail lost through spectral bridge");
    }
    // Long feedback at the extremes must stay finite under automation and
    // decay after excitation stops; reset must invalidate all pitch history.
    for(int rate : {44100,192000})
    {
        ShimmerReverb r;r.prepare(rate,1);r.set(20,0,1,1,1);r.reset();
        std::mt19937 random(17);std::uniform_real_distribution<float> noise(-.4f,.4f);
        double early=0,late=0;float peak=0;
        for(int i=0;i<rate*12;++i)
        {
            if(i==rate)r.set(20,1,1,1,1);
            if(i==rate*2)r.set(.5f,1,1,1,1);
            float out=r.process(i<rate*2 ? noise(random) : 0);
            demand(std::isfinite(out),"Shimmer feedback instability under automation");
            peak=std::max(peak,std::abs(out));if(i>=rate*2 && i<rate*3)early+=out*out;
            if(i>=rate*10)late+=out*out;
        }
        demand(peak<4 && late<early*1e-6,"Shimmer feedback fails to decay or grows excessively");
        r.reset();for(int i=0;i<rate;++i)demand(r.process(0)==0,"Stale octave history after reset");
    }
    juce::ScopedJuceInitialiser_GUI gui;
    auto p=std::make_unique<SpectralStripProcessor>();p->setChain({});demand(p->addInstance(6) && p->addInstance(6),"Reverb insertion failed");
    const auto set=[&](int slot,const char* id,float value){auto* q=p->state.getParameter(SpectralStripProcessor::moduleParamID(slot,id));q->setValueNotifyingHost(q->convertTo0to1(value));};
    set(0,"reverbDecay",2);set(1,"reverbDecay",12);set(1,"reverbShimmer",.8f);p->setChain({0x62,0x61});
    juce::MemoryBlock saved;p->getStateInformation(saved);auto restored=std::make_unique<SpectralStripProcessor>();restored->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));restored->prepareToPlay(48000,256);
    demand(restored->getChain()==SpectralStripProcessor::Chain{0x62,0x61},"Reverb chain recall failed");
    demand(restored->getLatencySamples()==2048+2*1536,"Reverb host latency wrong");
    demand(std::abs(restored->state.getRawParameterValue(SpectralStripProcessor::moduleParamID(1,"reverbDecay"))->load()-12)<.01f,"Reverb independent values lost");
    juce::AudioBuffer<float> buffer(2,256);juce::MidiBuffer midi;
    for(int block=0;block<160;++block)
    {for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)buffer.setSample(ch,i,.1f);restored->processBlock(buffer,midi);}
    restored->setChain({});
    for(int block=0;block<160;++block)
    {for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)buffer.setSample(ch,i,.1f);restored->processBlock(buffer,midi);}
    demand(restored->getLatencySamples()==2048 && std::abs(buffer.getSample(0,255)-.1f)<1e-5f,"Removing reverb did not restore latency/audio");
    std::cout<<"PASS Shimmer Reverb tail, octave feedback, neutral bridges, instances and host latency\n";
}
