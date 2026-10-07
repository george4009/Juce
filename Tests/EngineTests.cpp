#include "../Source/SpectralEngine.h"
#include <iostream>
#include <random>
#include <stdexcept>
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void runProcessorTests();
void runAuditTests();
void runReverbTests();
int main(int argc,char* argv[])
{
    try
    {
        if(argc==2 && std::string(argv[1])=="--audit"){runAuditTests();return 0;}
        runReverbTests();
        runAuditTests();
        for (double sr : {44100.0, 48000.0, 96000.0, 192000.0})
        {
            auto engine = std::make_unique<SpectralEngine>(); engine->prepare(sr);
            SpectralEngine::Settings s;
            std::mt19937 rng(42); std::uniform_real_distribution<float> dist(-0.3f,0.3f);
            std::vector<std::array<float,2>> source(30000);
            float maxError=0;
            for (int i=0;i<30000;++i)
            {
                float in[2]{dist(rng), dist(rng)}, wet[2]{}, dry[2]{};
                source[static_cast<size_t>(i)] = {in[0],in[1]};
                engine->process(in,wet,dry,2,s);
                for(int ch=0;ch<2;++ch)
                {
                    float expected = i >= SpectralEngine::defaultSize ? source[static_cast<size_t>(i-SpectralEngine::defaultSize)][static_cast<size_t>(ch)] : 0;
                    require(std::abs(dry[ch]-expected)<1e-7f, "Dry latency mismatch");
                    maxError=std::max(maxError,std::abs(wet[ch]-expected));
                }
            }
            require(maxError < 0.00001f, "Neutral STFT reconstruction failed");
            engine->prepare(sr);
            s.freeze=true; s.smear=1; s.gate=true; s.invert=true; s.delay=true; s.feedback=.92f;
            s.shimmer=1; s.damping=.5f; s.gateAttack=1; s.gateRelease=2000; s.gateTilt=6;
            for(int i=0;i<100000;++i)
            {
                if(i%4096==0) {s.freeze=!s.freeze; s.spread=-s.spread; s.delayMs=10+static_cast<float>(i%990);}
                float in[2]{dist(rng),dist(rng)}, wet[2]{}, dry[2]{};
                engine->process(in,wet,dry,2,s);
                require(std::isfinite(wet[0]) && std::isfinite(wet[1]), "Non-finite effects output");
            }
            std::cout << "PASS " << sr << " Hz: stereo reconstruction, latency, effects stress; max error " << maxError << '\n';
        }
        // Frozen tone must remain audible after the input becomes silent.
        auto engine=std::make_unique<SpectralEngine>(); engine->prepare(48000);
        SpectralEngine::Settings s; float energy=0;
        for(int i=0;i<60000;++i)
        {
            s.freeze=i>=12000;
            float in[2]{i<18000 ? 0.2f*std::sin(juce::MathConstants<float>::twoPi*440*i/48000.0f):0,0}, wet[2]{}, dry[2]{};
            engine->process(in,wet,dry,1,s);
            if(i>40000) energy+=wet[0]*wet[0];
        }
        require(energy>1, "Freeze does not sustain the captured tone");
        std::cout << "PASS mono freeze sustain\n";
        // Gate must suppress a quiet tone and inverted gate must retain it.
        auto gateEnergy = [](bool enabled, bool inverted)
        {
            auto e = std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.gate=enabled; settings.invert=inverted;
            settings.threshold=-10;
            double sum=0;
            for(int i=0;i<48000;++i)
            {
                float in[2]{0.02f*std::sin(juce::MathConstants<float>::twoPi*1500*i/48000.0f),0}, wet[2]{}, dry[2]{};
                e->process(in,wet,dry,1,settings);
                if(i>24000) sum+=wet[0]*wet[0];
            }
            return sum;
        };
        const double reference=gateEnergy(false,false);
        require(gateEnergy(true,false)<reference*0.001, "Gate did not suppress a below-threshold tone");
        require(gateEnergy(true,true)>reference*0.95, "Inverted gate did not retain a quiet tone");
        std::cout << "PASS normal and inverted gate attenuation\n";
        // A tone burst must emerge at the requested frame delay, with no dry burst.
        engine->prepare(48000);
        s=SpectralEngine::Settings{}; s.delay=true; s.delayMix=1; s.feedback=0;
        s.spread=0; s.delayMs=1000.0f*10*SpectralEngine::defaultHop/48000.0f;
        double early=0, echo=0, error=0, expectedEnergy=0;
        constexpr int start=24000, end=30000;
        constexpr int latency=SpectralEngine::defaultSize+10*SpectralEngine::defaultHop;
        auto burst=[](int t) { return t>=start && t<end ? 0.2f*std::sin(juce::MathConstants<float>::twoPi*1500*t/48000.0f):0.0f; };
        for(int i=0;i<48000;++i)
        {
            float in[2]{burst(i),0}, wet[2]{}, dry[2]{};
            engine->process(in,wet,dry,1,s);
            if(i>=start+SpectralEngine::defaultSize && i<start+latency-2048) early+=wet[0]*wet[0];
            if(i>=start+latency+2048 && i<end+latency-2048)
            {
                const float expected=burst(i-latency);
                echo+=wet[0]*wet[0]; error+=(wet[0]-expected)*(wet[0]-expected);
                expectedEnergy+=expected*expected;
            }
        }
        require(echo>1 && early<echo*0.001, "Spectral delay timing or dry suppression failed");
        require(error<expectedEnergy*0.001, "Spectral delay failed to preserve delayed tone");
        std::cout << "PASS spectral delay timing and delayed waveform\n";
        // Exercise every size, including revisiting sizes with dirty delay history.
        for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        {
            engine->prepare(rate);
            for (int choice : {0,1,2,3,4,2,0,4})
            {
                engine->selectSize(choice);
                const int n=engine->getSize();
                require(n == (1 << (SpectralEngine::minOrder+choice)), "FFT size selection failed");
                SpectralEngine::Settings settings;
                std::vector<float> input(4*n);
                float maxError=0;
                for (int i=0;i<4*n;++i)
                {
                    input[static_cast<size_t>(i)]=0.2f*std::sin(0.17f*static_cast<float>(i));
                    float in[2]{input[static_cast<size_t>(i)],0}, wet[2]{}, dry[2]{};
                    engine->process(in,wet,dry,2,settings);
                    const float expected=i>=n ? input[static_cast<size_t>(i-n)] : 0;
                    maxError=std::max(maxError,std::abs(wet[0]-expected));
                    require(std::abs(dry[0]-expected)<1e-7f, "Variable-size dry latency mismatch");
                    require(std::abs(wet[1])<1e-7f, "Channel leakage after size change");
                }
                require(maxError<1e-5f,"Variable-size reconstruction failed");
                settings.delay=true; settings.feedback=.92f;
                for(int i=0;i<4*n;++i)
                {
                    float in[2]{.1f,.1f}, wet[2]{}, dry[2]{};
                    engine->process(in,wet,dry,2,settings);
                }
                engine->selectSize(choice);
                for(int i=0;i<4*n;++i)
                {
                    float in[2]{}, wet[2]{}, dry[2]{};
                    engine->process(in,wet,dry,2,settings);
                    require(wet[0]==0 && wet[1]==0,"Stale delay audio after FFT change");
                }
            }
        }
        std::cout << "PASS all FFT sizes, size changes, dry latency, stereo isolation and history reset at four sample rates\n";
        // LOW/HIGH must attenuate outside the band even with all effects off.
        auto measureFiltered = [](float frequency, float low, float high, bool enabled)
        {
            auto e = std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.low=low; settings.high=high; settings.bandFilter=enabled;
            double energy=0;
            for(int i=0;i<48000;++i)
            {
                float in[2]{.2f*std::sin(juce::MathConstants<float>::twoPi*frequency*static_cast<float>(i)/48000.0f),0},wet[2]{},dry[2]{};
                e->process(in,wet,dry,1,settings);
                if(i>24000) energy+=wet[0]*wet[0];
            }
            return energy;
        };
        // Closing HIGH must also reject FFT DC leakage and crossed LOW/HIGH values.
        for(int choice=0;choice<SpectralEngine::choices;++choice)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000,choice);
            SpectralEngine::Settings settings; settings.chain={};
            for(int mode=0;mode<4;++mode)
            {
                settings.low=mode==1 ? 1000.0f : mode==2 ? 20000.0f : 20.0f;
                settings.high=mode<2 ? 20.0f : 20000.0f;
                double wetEnergy=0,dryEnergy=0;
                for(int i=0;i<48000;++i)
                {
                    float in[2]{.1f+.2f*std::sin(juce::MathConstants<float>::twoPi*40*i/48000.0f)
                                     +.1f*std::sin(juce::MathConstants<float>::twoPi*440*i/48000.0f),0},wet[2]{},dry[2]{};
                    e->process(in,wet,dry,1,settings);
                    if(i>36000) { wetEnergy+=wet[0]*wet[0]; dryEnergy+=dry[0]*dry[0]; }
                }
                if(mode<3) require(wetEnergy<dryEnergy*1e-10,"Closed filter leaks audio or reopens with crossed cutoffs");
                else require(wetEnergy>dryEnergy*.99,"Filter fails to reopen after fully closing");
            }
        }
        std::cout << "PASS filter endpoints, crossed cutoffs and reopening at all FFT sizes\n";
        const auto filterReference=measureFiltered(1000,20,20000,true);
        require(measureFiltered(8000,20,3000,true)<filterReference*.001,"HIGH failed to reject high frequencies");
        require(measureFiltered(1000,3000,20000,true)<filterReference*.001,"LOW failed to reject low frequencies");
        require(measureFiltered(1000,20,3000,true)>filterReference*.95,"Low-pass damaged the passband");
        require(measureFiltered(8000,3000,20000,true)>filterReference*.95,"High-pass damaged the passband");
        require(measureFiltered(8000,20,3000,false)>filterReference*.95,"Filter bypass failed");
        std::cout << "PASS LOW/HIGH cutoffs, passband and FILTER bypass\n";
        const auto tone = [](float amplitude, float frequency, int t)
        { return amplitude*std::sin(juce::MathConstants<float>::twoPi*frequency*static_cast<float>(t)/48000.0f); };
        // DISPERSION spreads delays evenly in octaves across the band.
        require(SpectralEngine::bandPosition(20,20,20000)==0 && SpectralEngine::bandPosition(20000,20,20000)==1, "Band endpoints");
        require(std::abs(SpectralEngine::bandPosition(std::sqrt(20.0f*20000.0f),20,20000)-.5f)<1e-4f, "Dispersion is not logarithmic");
        require(std::abs(SpectralEngine::bandPosition(200,20,20000)-1.0f/3.0f)<1e-4f, "Dispersion octave spacing");
        std::cout << "PASS logarithmic delay dispersion\n";
        // A half-frame delay must not cancel a partial whose phase turns ~2/3 cycle per hop.
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.delay=true; settings.delayMix=1; settings.feedback=0; settings.spread=0;
            settings.delayMs=1000.0f*10.5f*SpectralEngine::defaultHop/48000.0f;
            constexpr int start=12000, end=30000, latency=SpectralEngine::defaultSize+10*SpectralEngine::defaultHop+SpectralEngine::defaultHop/2;
            double echo=0, error=0, expectedEnergy=0;
            for(int i=0;i<48000;++i)
            {
                float in[2]{i>=start && i<end ? tone(.2f,1000,i) : 0.0f,0}, wet[2]{}, dry[2]{};
                e->process(in,wet,dry,1,settings);
                if(i>=start+latency+2048 && i<end+latency-2048)
                {
                    const float expected=tone(.2f,1000,i-latency);
                    echo+=wet[0]*wet[0]; error+=(wet[0]-expected)*(wet[0]-expected); expectedEnergy+=expected*expected;
                }
            }
            require(echo>expectedEnergy*.9, "Fractional delay lost level");
            require(error<expectedEnergy*.01, "Fractional delay distorted the tone");
        }
        std::cout << "PASS fractional-frame delay without cancellation\n";
        // DAMPING shortens high repeats and leaves low repeats untouched.
        auto tailEnergy = [&](float frequency, float damping)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.delay=true; settings.delayMix=1; settings.feedback=.8f;
            settings.spread=0; settings.delayMs=100; settings.damping=damping;
            double energy=0;
            for(int i=0;i<96000;++i)
            {
                float in[2]{i<4800 ? tone(.2f,frequency,i) : 0.0f,0}, wet[2]{}, dry[2]{};
                e->process(in,wet,dry,1,settings);
                if(i>=48000) energy+=wet[0]*wet[0];
            }
            return energy;
        };
        require(tailEnergy(8000,1)<tailEnergy(8000,0)*.01, "DAMPING did not darken high repeats");
        require(tailEnergy(150,1)>tailEnergy(150,0)*.95, "DAMPING affected low repeats");
        std::cout << "PASS delay damping\n";
        // ATTACK and RELEASE are independent: slow release holds a tone that drops below threshold.
        auto gateWindow = [&](float attack, float release, bool rising)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.gate=true; settings.gateAttack=attack; settings.gateRelease=release;
            double energy=0;
            for(int i=0;i<36000;++i)
            {
                const bool loud = rising ? i>=24000 : i<24000;
                float in[2]{tone(loud ? .2f : .002f,1500,i),0}, wet[2]{}, dry[2]{};
                e->process(in,wet,dry,1,settings);
                if(i>=28096 && i<32896) energy+=wet[0]*wet[0];
            }
            return energy;
        };
        require(gateWindow(10,2000,false)>gateWindow(10,5,false)*10, "Gate RELEASE has no effect");
        require(gateWindow(1,100,true)>gateWindow(500,100,true)*2, "Gate ATTACK has no effect");
        std::cout << "PASS gate attack and release\n";
        // TILT lowers the threshold for highs: a quiet 8 kHz tone passes at 6 dB/oct.
        auto tiltEnergy = [&](bool gate, float tilt)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.gate=gate; settings.threshold=-30; settings.gateTilt=tilt;
            double energy=0;
            for(int i=0;i<48000;++i)
            {
                float in[2]{tone(.02f,8000,i),0}, wet[2]{}, dry[2]{};
                e->process(in,wet,dry,1,settings);
                if(i>24000) energy+=wet[0]*wet[0];
            }
            return energy;
        };
        const double tiltReference=tiltEnergy(false,0);
        require(tiltEnergy(true,0)<tiltReference*.001, "Untilted gate should suppress quiet highs");
        require(tiltEnergy(true,6)>tiltReference*.9, "Gate TILT did not protect quiet highs");
        std::cout << "PASS gate tilt\n";
        // SHIMMER adds a sustained +12 semitone voice to the captured sound.
        auto frozen = [&](float shimmer, int choice, std::vector<float>& out)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000, choice);
            SpectralEngine::Settings settings; settings.shimmer=shimmer;
            double energy=0;
            for(int i=0;i<60000;++i)
            {
                settings.freeze=i>=12000;
                float in[2]{i<18000 ? tone(.2f,440,i) : 0.0f,0}, wet[2]{}, dry[2]{};
                e->process(in,wet,dry,1,settings);
                require(std::isfinite(wet[0]), "Non-finite shimmer output");
                if(i>40000) { energy+=wet[0]*wet[0]; out.push_back(wet[0]); }
            }
            return energy;
        };
        for(int choice=0;choice<SpectralEngine::choices;++choice)
        {
            std::vector<float> still,legacy;
            frozen(0,choice,still);frozen(1,choice,legacy);
            require(still==legacy,"Retired Freeze Shimmer still affects audio");
        }
        std::cout<<"PASS retired Freeze Shimmer is inert at all FFT sizes\n";
        // A gate after the delay suppresses quiet repeats; before it, repeats survive.
        const auto orderedTail = [&](std::array<int,SpectralEngine::maxModules> chain)
        {
            auto engine=std::make_unique<SpectralEngine>(); engine->prepare(48000);
            SpectralEngine::Settings settings; settings.chain=chain;
            settings.bandFilter=false;
            settings.gate=true; settings.threshold=-20; settings.gateTilt=0;
            settings.gateAttack=1; settings.gateRelease=5;
            settings.delay=true; settings.delayMix=1; settings.delayMs=80; settings.spread=0; settings.feedback=.7f;
            double energy=0;
            for(int i=0;i<60000;++i)
            {
                float in[2]{i>=10000 && i<16000 ? tone(.4f,1500,i) : 0.0f,0}, wet[2]{},dry[2]{};
                engine->process(in,wet,dry,1,settings);
                if(i>42000) energy+=wet[0]*wet[0];
            }
            return energy;
        };
        const double gateBefore=orderedTail({0x22,0x33}), gateAfter=orderedTail({0x33,0x22});
        std::cout << "Order tail: " << gateBefore << " / " << gateAfter << '\n';
        require(gateBefore>gateAfter*2, "Module order did not change the audio path");
        // An empty strip must reconstruct the input even with every effect enabled.
        {
            auto engine=std::make_unique<SpectralEngine>(); engine->prepare(48000);
            SpectralEngine::Settings settings; settings.chain={}; settings.bandFilter=false; settings.freeze=true; settings.gate=true; settings.delay=true; settings.high=200;
            for(int i=0;i<20000;++i)
            {
                float in[2]{tone(.2f,3000,i),0},wet[2]{},dry[2]{};
                engine->process(in,wet,dry,1,settings);
                if(i>4096) require(std::abs(wet[0]-dry[0])<1e-5f,"Removed modules still process audio");
            }
        }
        std::cout << "PASS module order changes audio; empty strip is transparent\n";
        const auto duplicateGates=[&](bool second)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.useInstances=true; settings.bandFilter=false;
            settings.chain=second ? std::array<int,SpectralEngine::maxModules>{0x21,0x24} : std::array<int,SpectralEngine::maxModules>{0x21};
            settings.modules[0].gate=true; settings.modules[0].threshold=-90; settings.modules[0].gateTilt=0;
            settings.modules[3].gate=true; settings.modules[3].threshold=0; settings.modules[3].gateTilt=0;
            double energy=0;
            for(int i=0;i<30000;++i)
            { float in[2]{tone(.2f,1500,i),0},wet[2]{},dry[2]{}; e->process(in,wet,dry,1,settings); if(i>20000) energy+=wet[0]*wet[0]; }
            return energy;
        };
        require(duplicateGates(true)<duplicateGates(false)*.001,"Duplicate Gate parameters are shared");
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.chain={}; settings.high=200; settings.bandFilter=true;
            double energy=0;
            for(int i=0;i<30000;++i)
            { float in[2]{tone(.2f,3000,i),0},wet[2]{},dry[2]{}; e->process(in,wet,dry,1,settings); if(i>20000) energy+=wet[0]*wet[0]; }
            require(energy<.001,"Fixed filter disappeared with empty strip");
        }
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000);
            SpectralEngine::Settings settings; settings.useInstances=true; settings.chain={0x31,0x34};
            for(const int slot : {0,3})
            { auto& delay=settings.modules[static_cast<size_t>(slot)]; delay.delay=true; delay.delayMix=1; delay.feedback=0; delay.spread=0; }
            settings.modules[0].delayMs=1000.0f*6*SpectralEngine::defaultHop/48000;
            settings.modules[3].delayMs=1000.0f*10*SpectralEngine::defaultHop/48000;
            constexpr int latency=SpectralEngine::defaultSize+16*SpectralEngine::defaultHop;
            double error=0,energy=0,early=0;
            for(int i=0;i<50000;++i)
            {
                float in[2]{i>=12000 && i<30000 ? tone(.2f,1500,i) : 0.0f,0},wet[2]{},dry[2]{};
                e->process(in,wet,dry,1,settings);
                if(i>=12000+latency+4096 && i<30000+latency-4096)
                { const float expected=tone(.2f,1500,i-latency); error+=(wet[0]-expected)*(wet[0]-expected); energy+=expected*expected; }
                if(i>16000 && i<18000) early+=wet[0]*wet[0];
            }
            require(error<energy*.01 && early<.001,"Serial duplicate Delays share history or time");
        }
        std::cout << "PASS independent duplicate Gates/Delays and fixed filter with empty strip\n";
        const auto renderBlur=[&](float width,float amount,float mix,bool enabled,bool twice=false,int choice=2)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000,choice);
            SpectralEngine::Settings settings; settings.useInstances=true; settings.bandFilter=false;
            settings.chain=twice ? std::array<int,SpectralEngine::maxModules>{0x41,0x44} : std::array<int,SpectralEngine::maxModules>{0x41};
            for(const int slot : {0,3})
            { auto& blur=settings.modules[static_cast<size_t>(slot)]; blur.blur=enabled; blur.blurWidth=width; blur.blurAmount=amount; blur.blurMix=mix; }
            std::vector<float> out(48000);
            for(int i=0;i<48000;++i)
            {
                float in[2]{tone(.2f,1500,i),0},wet[2]{},dry[2]{}; e->process(in,wet,dry,1,settings);
                require(std::isfinite(wet[0]),"Blur produced a non-finite sample"); out[static_cast<size_t>(i)]=wet[0];
                if(i>24000 && (!enabled || amount==0 || mix==0)) require(std::abs(wet[0]-dry[0])<1e-5f,"Blur neutral path is not transparent");
            }
            return out;
        };
        const auto blurMetrics=[](const std::vector<float>& out)
        {
            juce::dsp::FFT fft(11); std::array<float,4096> data {};
            std::copy(out.end()-2048,out.end(),data.begin()); fft.performRealOnlyForwardTransform(data.data(),true);
            double total=0,variance=0;
            for(int k=1;k<1024;++k)
            {
                const double power=data[static_cast<size_t>(2*k)]*data[static_cast<size_t>(2*k)]+data[static_cast<size_t>(2*k+1)]*data[static_cast<size_t>(2*k+1)];
                const double distance=k*48000.0/2048-1500; total+=power; variance+=power*distance*distance;
            }
            return std::pair<double,double>{variance/std::max(1e-20,total),total};
        };
        const auto neutral=renderBlur(400,0,1,true);
        const auto medium=renderBlur(400,.6f,1,true), full=renderBlur(400,1,1,true);
        double mediumDifference=0,fullDifference=0;
        for(size_t i=24000;i<neutral.size();++i)
        {
            mediumDifference+=std::pow(medium[i]-neutral[i],2);
            fullDifference+=std::pow(full[i]-neutral[i],2);
        }
        const double strength=std::sqrt(mediumDifference/fullDifference);
        require(strength>.55 && strength<.65,"Blur Amount attenuates the transformation more than once");
        std::cout << "Blur Amount 0.6 relative transformation: " << strength << '\n';
        const auto clean=blurMetrics(neutral);
        renderBlur(400,1,0,true); renderBlur(400,1,1,false);
        const auto narrow=blurMetrics(renderBlur(100,1,1,true)),wide=blurMetrics(renderBlur(1800,1,1,true));
        const auto doubled=blurMetrics(renderBlur(100,1,1,true,true));
        std::cout << "Blur spread Hz: " << std::sqrt(narrow.first) << " / " << std::sqrt(wide.first) << "; twice " << std::sqrt(doubled.first) << "; power ratio " << wide.second/clean.second << '\n';
        require(narrow.first>clean.first+100,"Blur does not spread frequencies");
        require(wide.first>narrow.first*4,"Blur WIDTH does not control spectral spread");
        require(doubled.first>narrow.first*1.2,"Serial Blur instances do not accumulate");
        require(wide.second>clean.second*.25 && wide.second<clean.second*2,"Blur level is unstable");
        for(int choice=0;choice<SpectralEngine::choices;++choice) renderBlur(4000,1,1,true,true,choice);
        std::cout << "PASS Blur spread, width, bypass, serial copies and all FFT sizes\n";
        for(int choice=0;choice<SpectralEngine::choices;++choice)
        {
            auto e=std::make_unique<SpectralEngine>(); e->prepare(48000,choice);
            SpectralEngine::Settings settings; settings.useInstances=true; settings.chain={0x51}; settings.bandFilter=false;
            settings.modules[0].tremoloDepth=1;
            for(int mode=0;mode<5;++mode)
            {
                e->selectSize(choice);
                settings.chain=mode==1 ? std::array<int,SpectralEngine::maxModules>{0x51,0x52} : std::array<int,SpectralEngine::maxModules>{0x51};
                settings.modules[0].tremolo=mode!=2;
                settings.modules[0].tremoloDepth=mode==3 ? 0.0f : 1.0f;
                settings.modules[0].tremoloRate=mode==4 ? 20.0f : 4.0f;
                settings.modules[0].tremoloShape=mode==4 ? 1.0f : 0.0f;
                settings.modules[1]=settings.modules[0];
                float minimum=1,maximum=0; double error=0;
                std::vector<float> output(72000);
                for(int i=0;i<72000;++i)
                {
                    float in[2]{.2f,.2f},wet[2]{},dry[2]{}; e->process(in,wet,dry,2,settings);
                    require(std::isfinite(wet[0]) && std::abs(wet[0]-wet[1])<1e-6f,"Tremolo stereo or finite output failed");
                    output[static_cast<size_t>(i)]=wet[0];
                    if(i>=48000)
                    {
                        const double gain=.5+.5*std::cos(juce::MathConstants<double>::twoPi*4*(i-e->getSize()/4)/48000.0);
                        const double expected=mode==2 || mode==3 ? .2 : .2*(mode==1 ? gain*gain : gain);
                        if(mode<4) error=std::max(error,std::abs(wet[0]-expected));
                        else require(std::abs(wet[0]-output[static_cast<size_t>(i-2400)])<1e-4f,"Fast Tremolo RATE drifted at large FFT");
                        minimum=std::min(minimum,wet[0]); maximum=std::max(maximum,wet[0]);
                    }
                }
                require(error<2e-5,"Tremolo waveform, depth, bypass or serial copies failed");
                if(mode==4) require(minimum<.01f && maximum>.19f,"Fast Tremolo lost depth at large FFT");
            }
        }
        std::cout << "PASS Tremolo sine waveform, depth zero, bypass, serial copies, stereo and fast pulse at all FFT sizes\n";
        // Check the audible envelope, not merely that Shape changes a value.
        // A DC probe exposes gain directly without carrier/FFT measurement bias.
        for(int choice=0;choice<SpectralEngine::choices;++choice)
        {
            std::vector<float> previous;
            for(float shape : {0.0f,.25f,.5f,.75f,1.0f})
            {
                auto engine=std::make_unique<SpectralEngine>();engine->prepare(48000,choice);
                SpectralEngine::Settings s;s.useInstances=true;s.chain={0x51};s.bandFilter=false;
                s.modules[0].tremoloDepth=1;s.modules[0].tremoloShape=shape;s.modules[0].tremoloRate=4;
                std::vector<float> gain;int high=0,transitionSamples=0;double change=0;
                for(int i=0;i<96000;++i)
                {
                    float in[2]{.2f,.2f},wet[2]{},dry[2]{};engine->process(in,wet,dry,2,s);
                    if(i>=48000)
                    {
                        const float g=wet[0]*5;require(g>=-1e-5f && g<=1.00001f,"Tremolo Shape envelope exceeds unity or reverses polarity");
                        require(std::abs(wet[0]-wet[1])<1e-6f,"Tremolo Shape breaks stereo sync");
                        if(g>.5f)++high;if(g>.1f && g<.9f)++transitionSamples;
                        if(!previous.empty())change+=std::pow(g-previous[gain.size()],2);
                        gain.push_back(g);
                    }
                }
                const double duty=high/48000.0,edges=transitionSamples/48000.0;
                std::cout<<"Shape FFT "<<engine->getSize()<<" amount "<<shape<<" duty "<<duty<<" transition fraction "<<edges<<" change RMS "<<std::sqrt(change/48000)<<'\n';
                if(shape==0)require(edges>.5,"Shape zero is no longer a smooth sine");
                if(shape==.5f)require(std::abs(duty-.5)<.01 && edges<.05,"Shape midpoint is not a clear rounded square");
                if(shape==1)require(std::abs(duty-.125)<.01 && edges<.05,"Shape maximum does not produce short pulses");
                if(!previous.empty())require(std::sqrt(change/48000)>.06,"Shape quarter-turn is barely distinguishable");
                previous=std::move(gain);
            }
        }
        runProcessorTests();
        return 0;
    }
    catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
