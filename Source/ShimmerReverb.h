#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

// Per-channel, modulated eight-line feedback delay network with octave feedback.
// Storage is allocated in prepare; reset invalidates history without clearing it.
class ShimmerReverb
{
    struct Delay
    {
        std::vector<float> data;
        int write=0,valid=0;
        void prepare(int n) { data.resize(static_cast<size_t>(n)); reset(); }
        void reset() { write=valid=0; }
        float read(float delay) const
        {
            delay=std::clamp(delay,1.0f,static_cast<float>(data.size()-2));
            const int a=static_cast<int>(delay); const float fraction=delay-a;
            const auto sample=[&](int age) { return age<=valid ? data[static_cast<size_t>((write-age+static_cast<int>(data.size()))%static_cast<int>(data.size()))] : 0.0f; };
            return sample(a)+(sample(a+1)-sample(a))*fraction;
        }
        void push(float x) { data[static_cast<size_t>(write)]=x; write=(write+1)%static_cast<int>(data.size());valid=std::min(valid+1,static_cast<int>(data.size())-1); }
    };
    std::array<Delay,8> lines;
    std::array<Delay,4> diffusers;
    struct Octave
    {
        Delay buffer;
        float phase=0,lp1=0,lp2=0,previous=0,hp=0;
        void prepare(int n) { buffer.prepare(n); reset(); }
        void reset() { buffer.reset();phase=lp1=lp2=previous=hp=0; }
        float process(float input,float span,float filter,float pole)
        {
            lp1+=filter*(input-lp1);lp2+=filter*(lp1-lp2);
            hp=lp2-previous+pole*hp;previous=lp2;buffer.push(hp);
            phase+=1/span;if(phase>=1)phase-=1;
            float out=0;
            for(int head=0;head<2;++head)
            {
                float p=phase+head*.5f;if(p>=1)p-=1;
                out+=(.5f-.5f*std::cos(6.2831853f*p))*buffer.read(2+(1-p)*span);
            }
            return out;
        }
    };
    std::array<Octave,2> inputPitch,loopPitch;
    std::array<float,8> low {},phase {};
    float sr=48000,side=0,decay=6,size=.6f,shimmer=.45f,tone=.55f,mix=.35f;
    float targetDecay=6,targetSize=.6f,targetShimmer=.45f,targetTone=.55f,targetMix=.35f;
    float smooth=0;
    float damping=0,pitchFilter=0,dcPole=0;
    std::array<float,8> lengths {},feedback {};
    int controlCounter=0;
public:
    void prepare(double rate,int channel)
    {
        sr=static_cast<float>(rate);side=static_cast<float>(channel);
        for(auto& d:lines)d.prepare(static_cast<int>(sr*.30f)+8);
        for(auto& d:diffusers)d.prepare(static_cast<int>(sr*.04f)+8);
        for(auto& p:inputPitch)p.prepare(static_cast<int>(sr*.10f)+8);
        for(auto& p:loopPitch)p.prepare(static_cast<int>(sr*.10f)+8);
        smooth=1-std::exp(-1/(sr*.05f));
        pitchFilter=1-std::exp(-6.2831853f*std::min(8000.0f,sr*.18f)/sr);
        dcPole=std::exp(-6.2831853f*60/sr);reset();
    }
    void reset()
    {
        for(auto& d:lines)d.reset();for(auto& d:diffusers)d.reset();
        for(auto& p:inputPitch)p.reset();for(auto& p:loopPitch)p.reset();
        low.fill(0);for(int j=0;j<8;++j)phase[static_cast<size_t>(j)]=.7f*j+.31f*side;
        controlCounter=0;
        decay=targetDecay;size=targetSize;shimmer=targetShimmer;tone=targetTone;mix=targetMix;
    }
    void set(float d,float s,float sh,float t,float m)
    {targetDecay=d;targetSize=s;targetShimmer=sh;targetTone=t;targetMix=m;}
    float process(float input)
    {
        decay+=smooth*(targetDecay-decay);size+=smooth*(targetSize-size);
        shimmer+=smooth*(targetShimmer-shimmer);tone+=smooth*(targetTone-tone);mix+=smooth*(targetMix-mix);
        static constexpr float times[8]={.0311f,.0371f,.0419f,.0479f,.0533f,.0617f,.0713f,.0797f};
        if(controlCounter--<=0)
        {
            controlCounter=31;damping=1-std::exp(-6.2831853f*(700+std::pow(tone,2.0f)*14500)/sr);
            for(int j=0;j<8;++j)
            {const auto k=static_cast<size_t>(j);lengths[k]=sr*times[j]*(.55f+size*2.0f)*(1+side*.023f);feedback[k]=std::pow(.001f,lengths[k]/(sr*std::max(.5f,decay)));}
        }
        float diffuse=input;
        for(int j=0;j<4;++j)
        {
            auto& d=diffusers[static_cast<size_t>(j)];const float delayed=d.read(sr*(.0041f+j*.0023f)*(1+side*.031f));
            const float next=diffuse+.65f*delayed;d.push(next);diffuse=delayed-.65f*next;
        }
        std::array<float,8> values {};std::array<float,2> send {};float wet=0,meanFeedback=0;
        for(int j=0;j<8;++j)
        {
            const auto k=static_cast<size_t>(j);phase[k]+=6.2831853f*(.071f+.019f*j)/sr;if(phase[k]>6.2831853f)phase[k]-=6.2831853f;
            const float value=lines[k].read(lengths[k]+sr*.00025f*std::sin(phase[k]));
            low[k]+=damping*(value-low[k]);values[k]=low[k]*feedback[k];
            send[0]+=values[k]*.35355339f;send[1]+=values[k]*(j%2 ? -.35355339f : .35355339f);
            meanFeedback+=feedback[k]*.125f;wet+=value*(j%2 ? -.25f : .25f);
        }
        // Independent grain lengths keep the two octave voices from sharing
        // the same cancellation frequencies. Excite the tank with the octave
        // directly, so audibility does not require sacrificing its decay.
        std::array<float,2> shifted {},excitation {};
        for(int voice=0;voice<2;++voice)
        {
            const auto k=static_cast<size_t>(voice);
            const float span=sr*(voice==0 ? .0437f : .0613f)*(1+side*.013f);
            shifted[k]=loopPitch[k].process(send[k],span,pitchFilter,dcPole);
            excitation[k]=inputPitch[k].process(diffuse,span,pitchFilter,dcPole);
        }
        // Normalised Hadamard scattering. Budget the pitch return against the
        // requested decay loss instead of removing 45% of the tank per pass.
        // The two return directions are orthogonal unit vectors.
        for(int step=1;step<8;step*=2)for(int start=0;start<8;start+=step*2)for(int j=0;j<step;++j)
        {const auto a=static_cast<size_t>(start+j),b=static_cast<size_t>(start+j+step);const float x=values[a],y=values[b];values[a]=x+y;values[b]=x-y;}
        const float pitchAmount=std::min(.12f,(1-meanFeedback)*.2f)*shimmer;
        for(int j=0;j<8;++j)
        {
            const float octaveReturn=(shifted[0]+(j%2 ? -shifted[1] : shifted[1]))*.35355339f;
            const float feedbackValue=(1-pitchAmount)*values[static_cast<size_t>(j)]*.35355339f+pitchAmount*octaveReturn;
            const float next=diffuse*.25f+shimmer*.35f*excitation[static_cast<size_t>(j%2)]+feedbackValue;
            // Smooth saturation only on excessive internal feedback, not on the dry path.
            const float bounded=std::abs(next)<=1.5f ? next : std::copysign(1.5f+.5f*std::tanh((std::abs(next)-1.5f)*2),next);
            lines[static_cast<size_t>(j)].push(bounded);
        }
        return input+mix*(wet-input);
    }
};
