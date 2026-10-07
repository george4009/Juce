#include "PluginEditor.h"
namespace { const juce::Colour bg(0xff10171d), panel(0xff1a252e), accent(0xff73e3bd), muted(0xff8fa5b4); }
SpectralStripEditor::SpectralStripEditor(SpectralStripProcessor& p) : AudioProcessorEditor(p), owner(p)
{
    look.setColour(juce::Slider::rotarySliderFillColourId, accent);
    look.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff34434e));
    look.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    look.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    look.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    look.setColour(juce::ToggleButton::tickColourId, accent);
    setLookAndFeel(&look);
    addKnob("smear", "SMEAR", ""); addKnob("threshold", "THRESHOLD", " dB");
    addKnob("delayMs", "TIME", " ms"); addKnob("spread", "DISPERSION", "");
    addKnob("feedback", "FEEDBACK", ""); addKnob("delayMix", "DELAY MIX", "");
    addKnob("low", "LOW", " Hz"); addKnob("high", "HIGH", " Hz");
    addKnob("mix", "DRY / WET", ""); addKnob("output", "OUTPUT", " dB");
    addSwitch("freezeModule", "Enabled"); addSwitch("freeze", "FREEZE");
    addSwitch("gate", "Enabled"); addSwitch("invert", "Invert gate"); addSwitch("delay", "Enabled");
    fftSizeLabel.setText("FFT SIZE", juce::dontSendNotification);
    fftSizeLabel.setColour(juce::Label::textColourId, muted);
    fftSizeChoice.addItemList({"512", "1024", "2048", "4096", "8192"}, 1);
    fftSizeChoice.setColour(juce::ComboBox::backgroundColourId, panel);
    fftSizeChoice.setTooltip("Larger FFT: finer frequency resolution, longer latency. Changing size clears Freeze and Delay memory.");
    addAndMakeVisible(fftSizeLabel); addAndMakeVisible(fftSizeChoice);
    fftSizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.state, "fftSize", fftSizeChoice);
    addSwitch("bandFilter", "FILTER");
    switches.back()->button.setTooltip("Filters the wet signal. Set DRY / WET to 1.00 for complete cutoff.");
    addKnob("shimmer", "SHIMMER", ""); addKnob("gateAttack", "ATTACK", " ms");
    addKnob("gateRelease", "RELEASE", " ms"); addKnob("gateTilt", "TILT", " dB/oct");
    addKnob("damping", "DAMPING", "");
    addAndMakeVisible(addModule);
    addModule.setTextWhenNothingSelected("+ ADD MODULE");
    addModule.addItemList({"Freeze / Smear", "Gate", "Spectral Delay", "Blur", "Tremolo", "Shimmer Reverb"},1);
    addModule.onChange = [this]
    {
        const int id=addModule.getSelectedId(); if (!id) return;
        owner.addInstance(id);
        int count=0; for(const int entry : owner.getChain()) if(entry) ++count;
        firstVisible=std::max(0,count-4);
        addModule.setSelectedId(0,juce::dontSendNotification); resized(); repaint();
    };
    addAndMakeVisible(stripScroll);
    stripScroll.addListener(this);
    const char* ids[]={"smear","threshold","delayMs","spread","feedback","delayMix","shimmer","gateAttack","gateRelease","gateTilt","damping"};
    const char* labels[]={"SMEAR","THRESHOLD","TIME","DISPERSION","FEEDBACK","DELAY MIX","SHIMMER","ATTACK","RELEASE","TILT","DAMPING"};
    const char* units[]={""," dB"," ms","","","",""," ms"," ms"," dB/oct",""};
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        for(int i=0;i<11;++i)
        {
            addKnob(SpectralStripProcessor::moduleParamID(slot,ids[i]).toRawUTF8(),labels[i],units[i]);
            if(i==0)knobs.back()->slider.setTooltip("Smooths spectral amplitudes before Freeze capture. Slower attack and longer tail up to a 1.5 s time constant. Does not change an already frozen snapshot.");
        }
        for(const char* id : {"freezeModule","freeze","gate","invert","delay"})
            addSwitch(SpectralStripProcessor::moduleParamID(slot,id).toRawUTF8(),juce::String(id)=="freeze" ? "FREEZE" : juce::String(id)=="invert" ? "Invert gate" : "Enabled");
    }
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        for (const int local : {2, 3, 4, 5, 10})
            knobs[static_cast<size_t>(15+slot*11+local)]->slider.getProperties().set("largeDelayKnob", true);
        addKnob(SpectralStripProcessor::moduleParamID(slot,"blurAmount").toRawUTF8(),"AMOUNT","");
        addKnob(SpectralStripProcessor::moduleParamID(slot,"blurWidth").toRawUTF8(),"WIDTH"," Hz");
        addKnob(SpectralStripProcessor::moduleParamID(slot,"blurMix").toRawUTF8(),"MIX","");
        addSwitch(SpectralStripProcessor::moduleParamID(slot,"blur").toRawUTF8(),"Enabled");
    }
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        addKnob(SpectralStripProcessor::moduleParamID(slot,"tremoloRate").toRawUTF8(),"RATE"," Hz");
        knobs.back()->slider.textFromValueFunction=[](double v) { return juce::String(v,2); };
        knobs.back()->slider.updateText();
        addKnob(SpectralStripProcessor::moduleParamID(slot,"tremoloDepth").toRawUTF8(),"DEPTH","");
        addKnob(SpectralStripProcessor::moduleParamID(slot,"tremoloShape").toRawUTF8(),"SHAPE","");
        knobs.back()->slider.setTooltip("0: smooth sine. 0.5: rounded square. 1: short pulses (12.5% duty). Higher Depth makes the contrast more audible.");
        addSwitch(SpectralStripProcessor::moduleParamID(slot,"tremolo").toRawUTF8(),"Enabled");
    }
    for(int slot=0;slot<SpectralEngine::maxModules;++slot)
    {
        const char* reverbIds[]={"reverbDecay","reverbSize","reverbShimmer","reverbTone","reverbMix"};
        const char* reverbLabels[]={"DECAY","SIZE","SHIMMER","TONE","MIX"};
        for(int i=0;i<5;++i)
        {
            addKnob(SpectralStripProcessor::moduleParamID(slot,reverbIds[i]).toRawUTF8(),reverbLabels[i],i==0 ? " s" : "");
            knobs.back()->slider.getProperties().set("largeDelayKnob",true);
            if(i==2)knobs.back()->slider.setTooltip("Amount of +12 semitone excitation and feedback in the reverb. Works without Freeze.");
        }
        addSwitch(SpectralStripProcessor::moduleParamID(slot,"reverb").toRawUTF8(),"Enabled");
    }
    for (int i=0; i<SpectralEngine::maxModules; ++i)
    {
        auto& header=moduleHeaders[static_cast<size_t>(i)];
        header.setColour(juce::TextButton::buttonColourId,juce::Colours::transparentBlack);
        header.setColour(juce::TextButton::textColourOffId,accent);
        header.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        header.setTooltip("Drag to reorder"); addAndMakeVisible(header);
        header.begin=[this](const juce::MouseEvent& e) { mouseDown(e.getEventRelativeTo(this)); };
        header.move=[this](const juce::MouseEvent& e) { mouseDrag(e.getEventRelativeTo(this)); };
        header.end=[this](const juce::MouseEvent& e) { mouseUp(e.getEventRelativeTo(this)); };
        auto& button = removeModule[static_cast<size_t>(i)];
        button.setButtonText("x"); button.setTooltip("Remove module"); addAndMakeVisible(button);
        button.onClick = [this, i]
        { auto chain = owner.getChain(); chain[static_cast<size_t>(i)] = 0; owner.setChain(chain); resized(); repaint(); };
    }
    display.fill(-90);
    setSize(1040, 800);
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(1040.0 / 800.0);
    setResizeLimits(780, 600, 1560, 1200);
    startTimerHz(30);
}
SpectralStripEditor::~SpectralStripEditor() { stopTimer(); stripScroll.removeListener(this); setLookAndFeel(nullptr); }
void SpectralStripEditor::addKnob(const char* id, const char* name, const char* suffix)
{
    auto k = std::make_unique<Knob>();
    k->slider.setName(name);
    k->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 110, 23);
    if(juce::String(name)=="TIME") k->slider.setTooltip("Delay time; the shortest effective delay is one FFT hop (FFT size / 4 samples).");
    if(juce::String(name)=="SHIMMER") k->slider.setTooltip("Adds a +12 semitone voice to captured audio. Requires FREEZE on; 1.00 blends equal parts original and octave.");
    k->slider.setTextValueSuffix(suffix); k->slider.setNumDecimalPlacesToDisplay(2);
    k->slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k->slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->label.setText(name, juce::dontSendNotification); k->label.setJustificationType(juce::Justification::centred);
    k->label.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(k->slider); addAndMakeVisible(k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(owner.state, id, k->slider);
    // Set after the attachment, which would otherwise show the parameter's 0.001 step.
    const juce::String unit(suffix);
    const int decimals = unit.contains("Hz") || unit.contains("ms") ? 0 : unit.contains("dB") ? 1 : 2;
    k->slider.textFromValueFunction = [decimals](double v) { return juce::String(v, decimals); };
    k->slider.updateText();
    knobs.push_back(std::move(k));
}
void SpectralStripEditor::addSwitch(const char* id, const char* name)
{
    auto s = std::make_unique<Switch>(); s->button.setButtonText(name); addAndMakeVisible(s->button);
    s->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(owner.state, id, s->button);
    switches.push_back(std::move(s));
}
void SpectralStripEditor::timerCallback()
{
    if (visibleChain != owner.getChain()) { resized(); repaint(); }
    for (int i = 0; i < SpectralEngine::displayBins; ++i)
    { const float next = owner.engine.spectrum[static_cast<size_t>(i)].load(std::memory_order_relaxed); display[static_cast<size_t>(i)] += 0.35f * (next-display[static_cast<size_t>(i)]); }
    const auto scale = juce::AffineTransform::scale(getWidth() / 1040.0f, getHeight() / 800.0f);
    repaint(juce::Rectangle<float>(24, 88, 992, 182).transformedBy(scale).getSmallestIntegerContainer());
    repaint(juce::Rectangle<float>(24, 772, 992, 28).transformedBy(scale).getSmallestIntegerContainer());
}
void SpectralStripEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.addTransform(juce::AffineTransform::scale(getWidth() / 1040.0f, getHeight() / 800.0f));
    g.setColour(juce::Colours::white); g.setFont(30.0f);
    g.drawText("SPECTRAL / STRIP", 28, 18, 600, 42, juce::Justification::centredLeft);
    g.setColour(accent); g.setFont(13.0f);
    g.drawText("MODULAR SPECTRAL PROCESSOR", 28, 60, 500, 18, juce::Justification::centredLeft);
    const juce::Rectangle<float> scope(24, 88, 992, 182);
    g.setColour(panel); g.fillRoundedRectangle(scope, 12);
    auto plot = scope.reduced(18, 24);
    const auto mapFrequency = [&](float f) { return plot.getX() + plot.getWidth() * std::log(f/20.0f)/std::log(1000.0f); };
    float lo = owner.state.getRawParameterValue("low")->load(), hi = owner.state.getRawParameterValue("high")->load();
    if (lo > hi) std::swap(lo, hi);
    g.setColour(accent.withAlpha(0.07f));
    g.fillRect(juce::Rectangle<float>(mapFrequency(lo), plot.getY(), mapFrequency(hi)-mapFrequency(lo), plot.getHeight()));
    g.setColour(muted.withAlpha(0.15f));
    for (float f : {100.0f, 1000.0f, 10000.0f}) g.drawVerticalLine(static_cast<int>(mapFrequency(f)), plot.getY(), plot.getBottom());
    juce::Path path;
    for (int i = 0; i < SpectralEngine::displayBins; ++i)
    {
        const float x = plot.getX() + plot.getWidth()*static_cast<float>(i)/(SpectralEngine::displayBins-1);
        const float y = plot.getBottom() - plot.getHeight()*juce::jlimit(0.0f, 1.0f, (display[static_cast<size_t>(i)]+90)/90);
        if (i == 0) path.startNewSubPath(x,y); else path.lineTo(x,y);
    }
    g.setColour(accent); g.strokePath(path, juce::PathStrokeType(1.7f));
    g.setColour(muted); g.setFont(11.0f);
    for (float frequency : {20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f})
    {
        const auto text = frequency < 1000 ? juce::String(frequency, 0) + " Hz"
                                          : juce::String(frequency / 1000, 0) + " kHz";
        const int x = juce::jlimit(42, 940, static_cast<int>(mapFrequency(frequency)) - 30);
        g.drawText(text, x, 248, 60, 17, juce::Justification::centred);
    }
    const auto chain = owner.getChain();
    for (int i=0; i<SpectralEngine::maxModules; ++i)
    {
        const int x=24+(i-firstVisible)*248, module=chain[static_cast<size_t>(i)] >> 4;
        if(i<firstVisible || i>=firstVisible+4) continue;
        g.setColour(panel); g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(x),286,236,350),12);
        g.setColour(module ? accent : muted); g.setFont(15.0f);
        if (!module) g.drawText("EMPTY SLOT", x+12,300,194,25,juce::Justification::centredLeft);
        if (!module)
        { g.setColour(muted); g.setFont(13.0f); g.drawText("Add an effect with +",x+12,440,212,30,juce::Justification::centred); }
        if (draggedModule && i==dropSlot)
        { g.setColour(accent); g.drawRect(x,286,236,350,2); }
    }
    g.setColour(panel); g.fillRoundedRectangle(juce::Rectangle<float>(24,652,992,120),12);
    g.setColour(muted); g.setFont(11.0f);
    g.drawText("FILTER / FX BAND",40,665,135,22,juce::Justification::centredLeft);
    g.drawText("FFT " + juce::String(owner.activeFFTSize.load()) + " / OVERLAP 4x   |   Latency: " + juce::String(1000.0 * owner.getLatencySamples() / std::max(1.0, owner.getSampleRate()), 1) + " ms   |   v0.7.4", 28, 775, 955, 20, juce::Justification::centredRight);
}
void SpectralStripEditor::resized()
{
    visibleChain = owner.getChain();
    const char* names[]={"","FREEZE / SMEAR","GATE","SPECTRAL DELAY","BLUR","TREMOLO","SHIMMER REVERB"};
    int count=0; for(const int entry : visibleChain) if(entry) ++count;
    firstVisible=juce::jlimit(0,std::max(0,count-4),firstVisible);
    for(int id=1;id<=6;++id) addModule.setItemEnabled(id,count<SpectralEngine::maxModules);
    stripScroll.setRangeLimits(0,std::max(4,count),juce::dontSendNotification);
    stripScroll.setCurrentRange(firstVisible,4,juce::dontSendNotification);
    stripScroll.setVisible(count>4); stripScroll.setBounds(24,637,992,12);
    fftSizeLabel.setBounds(590,28,75,24); fftSizeChoice.setBounds(665,25,100,30);
    addModule.setBounds(825,25,185,30);
    for (auto& knob : knobs) { knob->label.setVisible(false); knob->slider.setVisible(false); }
    for (auto& toggle : switches) toggle->button.setVisible(false);
    auto place = [&](int index, int x, int y, int w, int h)
    {
        auto& knob = *knobs[static_cast<size_t>(index)];
        knob.label.setVisible(true); knob.slider.setVisible(true);
        const int labelHeight = h<100 ? 16 : 20;
        knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,h<100 ? 96 : 110,h<100 ? 18 : 23);
        knob.label.setBounds(x,y,w,labelHeight); knob.slider.setBounds(x,y+labelHeight,w,h-labelHeight);
    };
    auto toggle = [&](int index, int x, int y, int w)
    { auto& toggleButton=switches[static_cast<size_t>(index)]->button; toggleButton.setVisible(true); toggleButton.setBounds(x,y,w,26); };
    for (int i=0; i<SpectralEngine::maxModules; ++i)
    {
        const int entry=visibleChain[static_cast<size_t>(i)], module=entry >> 4,slot=(entry & 15)-1;
        const int x=24+(i-firstVisible)*248;
        const bool visible=entry && i>=firstVisible && i<firstVisible+4;
        auto& header=moduleHeaders[static_cast<size_t>(i)];
        header.setVisible(visible); header.setBounds(x+8,300,198,25);
        header.setButtonText(juce::String(i+1)+" "+names[module]+" #"+juce::String(slot+1));
        removeModule[static_cast<size_t>(i)].setVisible(visible);
        removeModule[static_cast<size_t>(i)].setBounds(x+208,300,22,25);
        if(!visible) continue;
        const auto instanceKnob=[&](int local,int px,int py,int w,int h) { place(15+slot*11+local,px,py,w,h); };
        const auto instanceToggle=[&](int local,int px,int py,int w) { toggle(6+slot*5+local,px,py,w); };
        switch(module)
        {
        case 1:
            instanceToggle(0,x+10,335,100); instanceToggle(1,x+110,335,116);
            instanceKnob(0,x+55,410,126,140); break;
        case 2:
            instanceToggle(2,x+10,335,96); instanceToggle(3,x+106,335,125);
            instanceKnob(1,x+10,370,103,120); instanceKnob(9,x+123,370,103,120);
            instanceKnob(7,x+10,500,103,120); instanceKnob(8,x+123,500,103,120); break;
        case 3:
            instanceToggle(4,x+10,335,110);
            instanceKnob(2,x+10,364,103,88); instanceKnob(3,x+123,364,103,88);
            instanceKnob(4,x+10,452,103,88); instanceKnob(10,x+123,452,103,88);
            instanceKnob(5,x+65,540,103,88); break;
        case 4:
            toggle(6+SpectralEngine::maxModules*5+slot,x+10,335,110);
            place(15+SpectralEngine::maxModules*11+slot*3,x+10,370,103,120);
            place(16+SpectralEngine::maxModules*11+slot*3,x+123,370,103,120);
            place(17+SpectralEngine::maxModules*11+slot*3,x+65,500,103,120); break;
        case 5:
            toggle(6+SpectralEngine::maxModules*6+slot,x+10,335,110);
            place(15+SpectralEngine::maxModules*14+slot*3,x+10,370,103,120);
            place(16+SpectralEngine::maxModules*14+slot*3,x+123,370,103,120);
            place(17+SpectralEngine::maxModules*14+slot*3,x+65,500,103,120); break;
        case 6:
            toggle(6+SpectralEngine::maxModules*7+slot,x+10,335,110);
            place(15+SpectralEngine::maxModules*17+slot*5,x+10,364,103,88);
            place(16+SpectralEngine::maxModules*17+slot*5,x+123,364,103,88);
            place(17+SpectralEngine::maxModules*17+slot*5,x+10,452,103,88);
            place(18+SpectralEngine::maxModules*17+slot*5,x+123,452,103,88);
            place(19+SpectralEngine::maxModules*17+slot*5,x+65,540,103,88); break;
        default: break;
        }
    }
    toggle(5,40,701,125); place(6,182,658,135,107); place(7,337,658,135,107);
    place(8,651,658,135,107); place(9,827,658,135,107);
    const auto scale = juce::AffineTransform::scale(getWidth()/1040.0f,getHeight()/800.0f);
    fftSizeLabel.setTransform(scale); fftSizeChoice.setTransform(scale); addModule.setTransform(scale); stripScroll.setTransform(scale);
    for (auto& button : removeModule) button.setTransform(scale);
    for (auto& header : moduleHeaders) header.setTransform(scale);
    for (auto& knob : knobs) { knob->label.setTransform(scale); knob->slider.setTransform(scale); }
    for (auto& switchControl : switches) switchControl->button.setTransform(scale);
}
void SpectralStripEditor::mouseDown(const juce::MouseEvent& e)
{
    const float x=e.position.x*1040.0f/getWidth(), y=e.position.y*800.0f/getHeight();
    if (y<300 || y>328 || x<24 || x>=1004) return;
    dropSlot=juce::jlimit(0,SpectralEngine::maxModules-1,firstVisible+static_cast<int>((x-24)/248));
    draggedModule=owner.getChain()[static_cast<size_t>(dropSlot)];
}
void SpectralStripEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!draggedModule) return;
    const float x=e.position.x*1040.0f/getWidth();
    int count=0; for(const int entry : owner.getChain()) if(entry) ++count;
    if(x>1010 && firstVisible<std::max(0,count-4)) { ++firstVisible; resized(); }
    if(x<20 && firstVisible>0) { --firstVisible; resized(); }
    dropSlot=juce::jlimit(0,std::max(0,count-1),firstVisible+static_cast<int>((x-24)/248));
    repaint();
}
void SpectralStripEditor::mouseUp(const juce::MouseEvent& e)
{
    if (draggedModule && e.mouseWasDraggedSinceMouseDown())
    {
        auto chain=owner.getChain();
        const auto source=std::find(chain.begin(),chain.end(),draggedModule);
        const int from=static_cast<int>(std::distance(chain.begin(),source));
        if (source!=chain.end())
        {
            if (from<dropSlot) for (int i=from;i<dropSlot;++i) chain[static_cast<size_t>(i)]=chain[static_cast<size_t>(i+1)];
            else for (int i=from;i>dropSlot;--i) chain[static_cast<size_t>(i)]=chain[static_cast<size_t>(i-1)];
            chain[static_cast<size_t>(dropSlot)]=draggedModule;
            owner.setChain(chain);
        }
    }
    draggedModule=0; dropSlot=-1; resized(); repaint();
}

void SpectralStripEditor::scrollBarMoved(juce::ScrollBar*,double start)
{ firstVisible=static_cast<int>(std::round(start)); resized(); repaint(); }
