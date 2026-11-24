#include "silverBrowser.h"
#include "silverPatch.h"
#include "silverProcessor.h"
#include "silverSkin.h"
#include "silverSoulTrainJuce.h"
#include "soultrain/WaveIndicator.h"
#include "../../controller/command/controllerArchive.h"

#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace silverK;
using juce::String;
using juce::var;

void require(bool result,const String& detail) {
    if (!result) throw std::runtime_error(detail.toStdString());
}

void pump(double seconds=.12) {
    const auto end=juce::Time::getMillisecondCounterHiRes()+seconds*1000;
    while (juce::Time::getMillisecondCounterHiRes()<end)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,true);
}

var object(std::initializer_list<std::pair<String,var>> fields) {
    auto* result=new juce::DynamicObject;
    for (const auto& [key,value]:fields) result->setProperty(key,value);
    return result;
}

juce::Component* find(juce::Component& root,const String& value,bool byId,bool visible=true) {
    if ((byId?root.getComponentID():root.getName())==value && (!visible || root.isVisible()))
        return &root;
    for (auto* child:root.getChildren())
        if (auto* result=find(*child,value,byId,visible)) return result;
    return nullptr;
}

void press(juce::Component& root,const String& title) {
    auto* button=dynamic_cast<juce::Button*>(find(root,title,false));
    require(button && button->isEnabled() && button->onClick,"Unavailable native button "+title);
    button->onClick();
    pump();
}

juce::Rectangle<int> choiceMenuBounds(const SoulTrainJuce& ui,const juce::Component& picker) {
    juce::Rectangle<int> content;
    for (const auto* child:picker.getChildren()) content=content.getUnion(child->getBounds());
    return ui.getLocalArea(&picker,content.expanded(6));
}

int checkComboMenuWidth(SoulTrainJuce& ui,const juce::Component& trigger) {
    auto* picker=find(ui,"soulTrainPicker",true);
    require(picker && picker->getNumChildComponents()>0,"Open the menu for "+trigger.getName());
    const auto frame=choiceMenuBounds(ui,*picker);
    const auto bounds=ui.getLocalArea(&trigger,trigger.getLocalBounds());
    require(frame.getWidth()>=bounds.getWidth(),"Keep the menu at least as wide as "+trigger.getName());
    return frame.getWidth();
}

void checkChoiceFrameStates(SoulTrainJuce& ui,juce::Component& trigger,const juce::File& output) {
    auto* button=dynamic_cast<juce::Button*>(&trigger);
    require(button && button->onClick,"Find the Clock source menu control");
    const auto bounds=ui.getLocalArea(button,button->getLocalBounds());
    const auto capture=[&]{return ui.createComponentSnapshot(bounds);};
    const auto top=[](const juce::Image& image){return image.getPixelAt(image.getWidth()/2,1);};
    const auto distance=[](juce::Colour a,juce::Colour b){
        return std::max({std::abs(int(a.getRed())-b.getRed()),std::abs(int(a.getGreen())-b.getGreen()),
                         std::abs(int(a.getBlue())-b.getBlue())});
    };
    button->giveAwayKeyboardFocus();
    button->setState(juce::Button::buttonNormal);
    const auto idle=capture();
    const auto bottom=idle.getPixelAt(1,idle.getHeight()*3/4);
    require(distance(bottom,juce::Colour(0xff3e4d60))<=3 && top(idle).getRed()>bottom.getRed()+24,
            "Use the supplied SVG colours and gradient in the resting choice frame");
    button->setState(juce::Button::buttonOver);
    const auto hover=capture();
    require(distance(top(hover),top(idle).interpolatedWith(juce::Colours::white,.15f))<=3,
            "Blend 15% white into the hovered choice frame");

    button->setState(juce::Button::buttonDown);
    const auto pressed=capture();
    require(distance(top(pressed),juce::Colour(0xff92a8bf))<=4 &&
            top(pressed).getRed()>top(hover).getRed()+12,"Use the lighter open reference on mouse-down");
    int strongerGlow=0;
    for (auto y=5;y<idle.getHeight()-5;++y)
        for (auto x=5;x<idle.getWidth()-5;++x)
            if (int(pressed.getPixelAt(x,y).getBlue())>int(idle.getPixelAt(x,y).getBlue())+2) ++strongerGlow;
    require(strongerGlow>20,"Increase the caption glow inside the pressed box");

    button->onClick();
    button->setState(juce::Button::buttonNormal);
    pump();
    auto* picker=find(ui,"soulTrainPicker",true);
    require(picker!=nullptr,"Open the menu to check its frame");
    const auto open=capture();
    const auto menuBounds=choiceMenuBounds(ui,*picker);
    require(menuBounds.getHorizontalRange()==bounds.getHorizontalRange(),
            "Match Clock source field and popup width and alignment exactly");
    const auto menu=ui.createComponentSnapshot(menuBounds);
    require(distance(top(open),top(pressed))<=2 && distance(top(open),top(menu))<=2,
            "Use the identical frame on the open choice and its menu");

    require(picker->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Select the current menu row");
    for (int i=0;i<20 && find(ui,"soulTrainPicker",true);++i) pump(.025);
    require(find(ui,"soulTrainPicker",true)==nullptr,"Dismiss the menu after selection");
    // Exercise the focus event even when another app owns the native window focus.
    trigger.focusGained(juce::Component::focusChangedDirectly);
    button->setState(juce::Button::buttonNormal);
    require(distance(top(capture()),top(idle))<=2,"Restore the default frame outside the control after selection");

    const auto stride=idle.getWidth()+12;
    juce::Image states(juce::Image::RGB,stride*2+menu.getWidth(),open.getHeight()+4+menu.getHeight(),true);
    juce::Graphics g(states);
    g.fillAll(juce::Colour(0xff1e1e1e));
    g.drawImageAt(idle,0,0);
    g.drawImageAt(hover,stride,0);
    g.drawImageAt(open,stride*2,0);
    g.drawImageAt(menu,stride*2,open.getHeight()+4);
    auto png=output.getChildFile(ui.currentScene()+"-choice-states.png").createOutputStream();
    require(png && juce::PNGImageFormat().writeImageToStream(states,*png),"Save the rendered choice states");
    std::cout<<"PASS: SVG choice gradient, 15% hover, matched open frames, glow and reset after selection\n";
}

void checkWaveMenuWidth(SoulTrainJuce& ui,bool kStation) {
    auto* wave=dynamic_cast<soultrain::WaveIndicator*>(find(ui,"OSC 1 Waveform",false));
    require(wave!=nullptr,"Find the native oscillator waveform control");
    auto* trigger=dynamic_cast<juce::Button*>(find(*wave,"soultrain-wave-trigger",true));
    require(trigger && trigger->isEnabled() && trigger->onClick,"Find the enabled oscillator waveform menu trigger");
    const auto face=ui.getLocalArea(wave,wave->faceBounds()).toNearestIntEdges();
    trigger->onClick();
    pump();
    auto* menu=find(ui,"soultrain-wave-menu",true);
    require(menu!=nullptr,"Open the oscillator waveform menu");
    const auto frame=choiceMenuBounds(ui,*menu);
    require(frame.getX()==face.getX() && frame.getWidth()>=face.getWidth(),
            "Align the oscillator menu with the full frame");
    if (kStation) require(frame.getWidth()==face.getWidth(),"Fit the K-Station waves at the field width");
    require(menu->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Dismiss the oscillator menu");
    pump();
}

var browserData(Browser& browser) {
    browser.pollLibrary();
    return browser.webSnapshot();
}

var snapshot(Processor& processor,Browser& browser,const String& status) {
    const String model=processor.isKStation()?"K":"KS";
    juce::Array<var> enabled, names, firmwares;
    const auto partNames=processor.partNames();
    for (unsigned part=0;part<partNames.size();++part) {
        enabled.add((processor.activeParts()&(1u<<part))!=0);
        names.add(partNames[part]);
    }
    for (size_t i=0;i<processor.firmwares.size();++i) {
        const auto& item=processor.firmwares[i];
        firmwares.add(object({{"value",int(i)},{"label",item.file.getFileName()},
            {"model",item.kStation?"K":"KS"},{"selected",item.file==processor.osFile}}));
    }
    var values(new juce::DynamicObject);
    for (const auto& spec:processor.visibleControls())
        for (unsigned part=0;part<(processor.isKStation()?1u:4u);++part) {
            const auto id=processor.isKStation()?"k_"+String(spec.id):Processor::parameterId(spec,part);
            auto* target=processor.parameters.getParameter(id);
            require(target!=nullptr,"Missing tested automation parameter "+id);
            values.getDynamicObject()->setProperty(model+":"+String(part+1)+":"+semanticId(spec.id),
                       juce::roundToInt(target->convertFrom0to1(target->getValue())));
        }
    return object({{"model",model},{"part",int(processor.currentPart()+1)},
        {"ready",processor.firmwareReady()},{"loading",false},{"performance",processor.performanceMode()},
        {"name",processor.currentName()},{"clock",int(processor.clockSource())},{"status",status},
        {"dataFolder",processor.dataFolder.getFullPathName()},{"dataFolderStatus",processor.dataFolderStatus()},
        {"firmwareSwitch",int(processor.firmwareSwitch())},
        {"buffer",processor.bufferProfile()},{"channel",int(processor.editorChannel()+1)},
        {"uiScale",processor.editorScale.load()},{"twoCore",processor.ksTwoCoreProcessing()},{"partNames",names},{"enabled",enabled},
        {"firmwares",firmwares},{"values",values},{"browser",browserData(browser)}});
}

void capture(SoulTrainJuce& ui,Processor& processor,Browser& browser,const juce::File& output,
             const String& page,int effect,const String& variant={},const juce::Array<var>& panels={}) {
    const auto name=ui.currentScene()+variant;
    const auto data=object({{"page",page},{"effect",effect},{"panels",panels},{"state",snapshot(processor,browser,"")}});
    require(output.getChildFile(name+".json").replaceWithText(juce::JSON::toString(data)),"Write matching WebKit fixture");
    const auto image=ui.createComponentSnapshot(ui.getLocalBounds(),true,2);
    require(image.isValid() && image.getWidth()==2972 && image.getHeight()==1884,"Render native interface");
    auto out=output.getChildFile(name+".png").createOutputStream();
    require(out && juce::PNGImageFormat().writeImageToStream(image,*out),"Write native snapshot");
    std::cout<<name<<'\n';
}

void flush(Processor& processor,int blocks=24) {
    juce::AudioBuffer<float> audio(2,256);
    juce::MidiBuffer midi;
    for (int i=0;i<blocks;++i) {
        audio.clear();
        processor.processBlock(audio,midi);
    }
    processor.refreshControls();
    pump();
}

void checkPerformancePartEditing(SoulTrainJuce& ui, Processor& processor) {
    require(ui.showPage("Main"),"Open Main before entering Performance");
    press(ui,"performance");
    flush(processor,96);
    require(processor.performanceMode() && processor.currentName().startsWith("New Performance"),
            "Open New Performance in Main");

    const auto select=[&](unsigned part) {
        press(ui,String(part+1));
        require(processor.currentPart()==part,"Select the matching Performance part");
    };
    const auto knob=[&](unsigned part,const char* id) -> juce::Slider& {
        auto* control=dynamic_cast<juce::Slider*>(find(ui,String(part+1)+":"+id,true));
        require(control && control->isEnabled(),"Find the selected part's Main control");
        return *control;
    };
    const auto changeChannel=[&](int channel) {
        auto* parameter=processor.uiParameter("part_channel");
        require(parameter!=nullptr,"Find the selected part's MIDI channel");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(float(channel-1)));
        flush(processor);
        require(processor.midiChannel()==channel-1,"Display the channel the controller uses");
    };
    const auto verifyPatch=[&](juce::MemoryBlock& state,const std::array<int,4>& cutoff,
                               const std::array<int,4>& fm,const std::array<int,4>& channels) {
        processor.getStateInformation(state);
        const auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
        juce::MemoryBlock program;
        require(xml && program.fromBase64Encoding(xml->getStringAttribute("program")) &&
                program.getSize()==1038,"Read the engine's Performance");
        const auto* data=static_cast<const uint8_t*>(program.getData())+13;
        for(size_t part=0;part<cutoff.size();++part) {
            const auto* patch=data+256*part;
            require(patch[46]==cutoff[part] && patch[56]==fm[part] && patch[181]==channels[part]-1,
                    "Commit the two Main controls and routing to the intended part "+String(part+1));
        }
    };

    constexpr std::array<int,4> fm{17,38,59,80};
    std::array<int,4> cutoff{29,56,83,111};
    select(0);
    changeChannel(2);
    for(unsigned part=0;part<cutoff.size();++part) {
        select(part);
        knob(part,"cutoff").setValue(cutoff[part],juce::sendNotificationSync);
        knob(part,"fm_level").setValue(fm[part],juce::sendNotificationSync);
    }
    flush(processor);
    juce::MemoryBlock project;
    verifyPatch(project,cutoff,fm,{2,2,3,4});
    for(unsigned part=0;part<cutoff.size();++part) {
        select(part);
        require(knob(part,"cutoff").getValue()==cutoff[part] && knob(part,"fm_level").getValue()==fm[part],
                "Recall both controls on selecting part "+String(part+1));
    }

    select(0);
    changeChannel(8);
    cutoff[0]=103;
    knob(0,"cutoff").setValue(cutoff[0],juce::sendNotificationSync);
    flush(processor);
    verifyPatch(project,cutoff,fm,{8,2,3,4});
    juce::AudioBuffer<float> audio(2,256);
    juce::MidiBuffer midi;
    cutoff[1]=61;
    midi.addEvent(juce::MidiMessage::controllerEvent(2,105,cutoff[1]),0);
    processor.processBlock(audio,midi);
    flush(processor);
    verifyPatch(project,cutoff,fm,{8,2,3,4});

    require(ui.showPage("Perf"),"Open the Performance mixer");
    press(ui,"Enable Part 2");
    flush(processor,96);
    require(processor.activeParts()==13 && ui.showPage("Main"),"Edit a muted part from Main");
    select(1);
    cutoff[1]=73;
    knob(1,"cutoff").setValue(cutoff[1],juce::sendNotificationSync);
    flush(processor);
    verifyPatch(project,cutoff,fm,{8,2,3,4});
    processor.setStateInformation(project.getData(),int(project.getSize()));
    flush(processor,96);
    require(processor.activeParts()==13 && processor.currentPart()==1 &&
            knob(1,"cutoff").getValue()==cutoff[1],"Restore the selected part and its edited controls");
    select(0);
    require(processor.midiChannel()==7 && knob(0,"cutoff").getValue()==cutoff[0],
            "Restore the other part and its live MIDI channel");
    std::cout<<"PASS: Main displays and edits each KS Performance part with shared channels, rerouting, mute and recall\n";
}

void checkDrumLoadTiming(Processor& processor) {
    using juce::Time;
    using namespace pluginLib::patchDB;
    juce::MemoryBlock project,flash;
    processor.getStateInformation(project);
    const auto xml=juce::AudioProcessor::getXmlFromBinary(project.getData(),int(project.getSize()));
    require(xml && flash.fromBase64Encoding(xml->getStringAttribute("userFlash")),"Read prepared KS Flash");
    constexpr size_t offset=0x20000-0x1fc00+400*256;
    require(flash.getSize()>=offset+50*256,"Fixture has a complete Drum Map");
    const auto* stored=static_cast<const uint8_t*>(flash.getData())+offset;
    Data kit;
    for(uint8_t i=0;i<50;++i) {
        const uint8_t header[]{0xf0,0,0x20,0x29,1,0x42,0x7f,1,1,0x11,2,5,i};
        kit.insert(kit.end(),std::begin(header),std::end(header));
        kit.insert(kit.end(),stored+size_t(i)*256,stored+size_t(i+1)*256);
        kit.push_back(0xf7);
    }
    const auto factoryName=String::fromUTF8(reinterpret_cast<const char*>(stored+151),16).trim();
    const String importedName="Grid timing test";
    auto imported=kit;
    std::copy_n(importedName.toRawUTF8(),16,imported.begin()+13+151);
    const String performanceName="Grid performance";
    auto performance=kit;
    for(size_t n=0;n<50;++n)performance[n*270+11]=6;
    std::copy_n(performanceName.toRawUTF8(),16,performance.begin()+13+151);
    juce::AudioBuffer<float> audio(2,256);
    juce::MidiBuffer midi;
    const auto block=[&]{audio.clear();processor.processBlock(audio,midi);};
    const auto measure=[&](Data next,const String& name,unsigned part,const char* label,bool pause){
        auto patch=processor.patchLibrary.createPatch(std::move(next),{});
        require(patch!=nullptr,"Prepare Drum Map timing fixture");
        String error;
        const auto before=Time::getMillisecondCounterHiRes();
        require(processor.activatePatchForPart(patch,part,error),error);
        const auto submission=Time::getMillisecondCounterHiRes()-before;
        require(processor.drumMapLoadStatus()==Processor::DrumLoadStatus::Loading,
                "Publish a loading status before the next host block");
        block();
        if(pause) {
            const auto lookup=Time::getMillisecondCounterHiRes();
            processor.drumSequencerInfo();
            const auto lookupMs=Time::getMillisecondCounterHiRes()-lookup;
            std::cout<<"DRUM_STATUS_LOOKUP_MS "<<lookupMs<<'\n';
            require(lookupMs<50,"Read the warmed drum names while the host is idle");
        }
        int blocks=1;
        while(processor.partNames()[part]!=name && blocks<47) { block(); ++blocks; }
        require(processor.partNames()[part]==name,"Drum Map activation completed. "+processor.status());
        require(processor.drumMapLoadStatus()==Processor::DrumLoadStatus::Applied &&
                processor.drumSequencerInfo().names[0]==name,"Publish the committed bank and name together");
        const auto audioMs=1000.0*blocks*256/48000;
        std::cout<<label<<" submit_ms="<<submission
                 <<" completion_audio_ms="<<audioMs<<'\n';
        require(audioMs<250,"Kit selection must finish without a full MIDI-port transfer");
    };
    measure(std::move(kit),factoryName,0,"FACTORY",false);
    measure(std::move(imported),importedName,0,"IMPORT",true);

    String error;
    require(processor.setPerformanceMode(true,error),error);
    flush(processor,160);
    require(processor.performanceMode(),"Enter Performance before importing a Drum Map");
    constexpr unsigned part=2;
    processor.selectPart(part);
    auto names=processor.partNames();
    measure(std::move(performance),performanceName,part,"PERFORMANCE_IMPORT",false);
    names[part]=performanceName;
    require(processor.partNames()==names,"Import into the chosen Performance part");

    processor.getStateInformation(project);
    processor.setStateInformation(project.getData(),int(project.getSize()));
    flush(processor,56);
    const auto restored=processor.drumSequencerInfo();
    require(processor.performanceMode() && processor.partNames()==names &&
            restored.bank==6 && restored.names[0]==performanceName,"Restore the imported Drum Map from the project");
}

void checkDrumSequencer(SoulTrainJuce& ui,Processor& processor,const juce::File& output) {
    String error;
    require(ui.showPage("Main"),"Open Main with a regular KS program");
    press(ui,"Arpeggiator options");
    require(!find(ui,"drumVoiceEdit",true),"Hide Drum Edit without a Drum Map");
    press(ui,"Arpeggiator options");
    const auto channel=processor.editorChannel();
    require(processor.sendShortMidi(uint8_t(0xb0|channel),32,37) &&
            processor.sendShortMidi(uint8_t(0xc0|channel),0),"Select KS Drum Map bank 5");
    flush(processor,128);
    require(processor.drumSequencerInfo().bank==5,"Load factory Drum Map for the grid. "+processor.status());
    processor.setClockSource(ClockSource::Internal);
    const auto click=[&](const String& id){
        auto* button=dynamic_cast<juce::Button*>(find(ui,id,true));
        require(button && button->isEnabled() && button->onClick,"Find enabled grid control "+id);
        button->onClick();
        pump(.06);
    };
    const auto chooseList=[&](const String& id,int row,int count){
        click(id);
        auto* list=dynamic_cast<juce::ListBox*>(find(ui,"soulTrainPickerList",true));
        require(list && list->getModel() && list->getModel()->getNumRows()==count,"Grid menu rows "+id);
        list->selectRow(row);
        require(list->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Select grid menu row");
        pump(.08);
    };
    const auto openGrid=[&](const String& id,int count) -> juce::Component& {
        click(id);
        auto* picker=find(ui,"soulTrainPicker",true);
        require(picker && !find(*picker,"soulTrainPickerList",true) &&
                picker->getNumChildComponents()==count,"Show every choice at once for "+id);
        checkComboMenuWidth(ui,*find(ui,id,true));
        return *picker;
    };
    const auto chooseRhythm=[&](const String& label){
        auto& picker=openGrid("arpRhythmPicker",34);
        press(picker,label);
    };
    const auto audition=[&](uint8_t note){
        juce::AudioBuffer<float> audio(2,256);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(processor.editorChannel()+1,note,uint8_t(90)),0);
        float peak=0;
        for(int block=0;block<94;++block) {
            audio.clear();
            processor.processBlock(audio,midi);
            peak=std::max(peak,audio.getMagnitude(0,audio.getNumSamples()));
        }
        midi.addEvent(juce::MidiMessage::noteOff(processor.editorChannel()+1,note),0);
        processor.processBlock(audio,midi);
        flush(processor,80);
        return peak;
    };
    require(ui.showPage("Main"),"Open KS Main");
    require(!find(ui,"drumSequencerToggle",true),"Remove the top-bar sequencer button");
    auto* mode=processor.uiParameter("arp_pattern");
    require(mode,"Find KS Arpeggiator Mode");
    mode->setValueNotifyingHost(mode->convertTo0to1(0));
    flush(processor,36);
    press(ui,"Arpeggiator options");
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000;
    while(!find(ui,"arpRhythmPicker",true) && juce::Time::getMillisecondCounterHiRes()<deadline)
        pump(.03);
    require(find(ui,"arpRhythmPicker",true) && !find(ui,"drumSequencerEdit",true),
            "Only show Edit after choosing SEQ");
    auto& melodic=openGrid("arpRhythmPicker",33);
    require(find(melodic,"Off",false) && find(melodic,"32",false) && !find(melodic,"SEQ",false),
            "Melodic rhythms show Off and 1–32");
    press(melodic,"Off");
    mode->setValueNotifyingHost(mode->convertTo0to1(7));
    flush(processor,36);
    auto& factoryPatterns=openGrid("arpRhythmPicker",34);
    require(find(factoryPatterns,"1",false) && find(factoryPatterns,"33",false) && !find(factoryPatterns,"Off",false),
            "Drum mode shows factory tracks 1–33 and SEQ");
    press(factoryPatterns,"SEQ");
    const auto* rhythm=find(ui,"arpRhythmPicker",true);
    const auto* edit=find(ui,"drumSequencerEdit",true);
    require(edit && edit->getName()=="Seq","Label the sequencer button Seq");
    const auto* mono=find(ui,"Mono",false);
    const auto* monoGlide=find(ui,"Mono AG",false);
    const auto* unison=find(ui,"voices",false);
    require(edit && rhythm && mono && monoGlide && unison,"Find the Arpeggiator, Voices and Unison controls");
    require(rhythm->getWidth()==unison->getWidth() && rhythm->getHeight()==unison->getHeight(),
            "Match Rhythm to the Unison choice box");
    const auto buttonGap=monoGlide->getX()-mono->getRight();
    require(rhythm->getX()==37 && edit->getX()==148 && edit->getX()-rhythm->getRight()>buttonGap &&
            edit->getBounds().getCentreY()==rhythm->getBounds().getCentreY() &&
            edit->getWidth()==mono->getWidth() && edit->getHeight()==mono->getHeight(),
            "Keep Edit in place with more space beside the narrower Rhythm box");
    auto* voiceEdit=dynamic_cast<juce::Button*>(find(ui,"drumVoiceEdit",true));
    require(voiceEdit && voiceEdit->getX()-edit->getRight()==buttonGap &&
            voiceEdit->getBounds().getCentreY()==edit->getBounds().getCentreY() &&
            voiceEdit->getWidth()==mono->getWidth() && voiceEdit->getHeight()==mono->getHeight(),
            "Match the size and spacing of Voices / Glide buttons");
    click("drumVoiceEdit");
    require(processor.drumEditEnabled() && voiceEdit->getToggleState(),"Enable Drum Edit with SEQ selected");
    const auto previewPeak=audition(38);
    const auto drum=processor.drumEditInfo();
    require(drum.editing && drum.note==38 && drum.name==processor.drumSequencerInfo().names[2],
            "Follow the actual MIDI note and the Drum Map name");
    require(previewPeak>0.001f,"Hear the mapped drum with Drum Edit active and an empty SEQ grid");
    {
        auto png=output.getChildFile("KS-drum-voice-edit.png").createOutputStream();
        const auto image=ui.createComponentSnapshot(ui.getLocalBounds());
        require(png && image.isValid() && juce::PNGImageFormat().writeImageToStream(image,*png),"Capture Drum Edit and the MIDI readout");
    }
    click("drumVoiceEdit");
    require(!processor.drumEditEnabled() && !voiceEdit->getToggleState(),"Disable Drum Edit");
    {
        const auto image=ui.createComponentSnapshot({24,682,802,109},true,2);
        auto png=output.getChildFile("KS-seq-edit.png").createOutputStream();
        require(image.isValid() && png && juce::PNGImageFormat().writeImageToStream(image,*png),"Capture the standard Edit button");
    }
    press(ui,"Voices options");
    require(find(ui,"drumSequencerEdit",true)!=nullptr,"Keep the SEQ button with both options panels open");
    press(ui,"Voices options");
    auto& drums=openGrid("arpRhythmPicker",34);
    for(int number=1;number<=33;++number)
        require(find(drums,String(number),false),"Find visible factory rhythm "+String(number));
    require(find(drums,"SEQ",false) && !find(drums,"Off",false),"List factory Drum numbers and SEQ");
    {
        const auto image=ui.createComponentSnapshot(ui.getLocalBounds());
        auto png=output.getChildFile("KS-rhythm-menu.png").createOutputStream();
        require(image.isValid() && png && juce::PNGImageFormat().writeImageToStream(image,*png),"Capture the full Rhythm menu");
    }
    press(drums,"SEQ");
    click("drumSequencerEdit");
    for (int row=1;row<=10;++row) {
        const auto* sound=find(ui,"drumSound"+String(row),true);
        require(sound && sound->getHeight()==36,"Match the drum selector to the standard box height");
    }
    auto* assignedKeys=dynamic_cast<juce::Button*>(find(ui,"drumAssignedKeys",true));
    auto* retrigger=dynamic_cast<juce::Button*>(find(ui,"drumRetrigger",true));
    const auto* switchAfter=find(ui,"drumSwitchAfter",true);
    require(assignedKeys && switchAfter && !assignedKeys->getToggleState() && switchAfter->getHeight()==36,
            "Show the assigned-keys LED and the standard switch timing ComboBox");
    require(retrigger && retrigger->getToggleState() && !retrigger->getBounds().intersects(assignedKeys->getBounds()),
            "Show the default-on Retrigger control next to Assigned Keys");
    click("drumRetrigger");
    require(!retrigger->getToggleState() && !processor.drumSequencerInfo().sequences.retrigger,
            "Turn off Retrigger for this Drum SEQ");
    const auto option=[&](const String& label){press(openGrid("drumSwitchAfter",18),label);};
    {
        auto& menu=openGrid("drumSwitchAfter",18);
        require(find(menu,"Immediate",false) && find(menu,"End of sequence",false),
                "Offer immediate and end-of-sequence switching");
        for(int step=1;step<=16;++step)
            require(find(menu,"After step "+String(step),false),"Show every switch step");
        const auto image=ui.createComponentSnapshot(ui.getLocalBounds());
        auto png=output.getChildFile("KS-seq-switch-menu.png").createOutputStream();
        require(image.isValid() && png && juce::PNGImageFormat().writeImageToStream(image,*png),"Capture the timing menu");
        press(menu,"After step 16");
        require(processor.drumSequencerInfo().sequences.switchAfter==16,"Select the last numbered step");
        option("After step 8");
    }
    for(int row=1;row<=10;++row)
        for(int step=1;step<=16;++step)
            require(find(ui,"drumStep"+String(row)+":"+String(step),true),"Find each of the 160 native pads");
    for(int sequence=1;sequence<=12;++sequence)
        require(find(ui,"drumSequence"+String(sequence),true),"Find each of the 12 sequences");
    flush(processor,36);
    require(processor.drumSequencerInfo().sequences.selected,"Enable SEQ as the user rhythm");
    require(audition(36)<1e-6f,"An empty sequence should be silent");
    chooseList("drumSound1",2,49);
    click("drumStep1:1");
    auto* stepValue=dynamic_cast<juce::Slider*>(find(ui,"drumStepValue",true));
    require(stepValue && stepValue->isEnabled(),"Edit the selected step");
    const auto setValue=[&](int value){stepValue->setValue(value,juce::sendNotificationSync);};
    const auto selectParameter=[&](const String& label){press(openGrid("drumStepParameter",3),label);};
    setValue(107);
    click("drumStep1:1");
    require(stepValue->isEnabled() && stepValue->getValue()==107,"Click an active step to select it");
    selectParameter("Probability");
    require(stepValue->getMinimum()==0 && stepValue->getValue()==100,"New steps play by default");
    setValue(0);
    selectParameter("Ratchet");
    require(stepValue->getMaximum()==16 && stepValue->getValue()==1,"Choose the number of hits per step");
    setValue(16);
    flush(processor,48);
    require(audition(36)<1e-6f,"Zero percent mutes the whole ratcheted step");
    selectParameter("Probability");
    require(stepValue->getValue()==0,"Ratchet leaves probability intact");
    setValue(100);
    click("drumAssignedKeys");
    require(assignedKeys->getToggleState() && processor.drumSequencerInfo().sequences.assignedKeysOnly,
            "Restrict playback to the assigned MIDI notes");
    flush(processor,48);
    require(audition(24)>0.001f && audition(96)>0.001f,
            "A single sequence with no explicit range plays across the keyboard");
    press(openGrid("drumSequenceLowNote",129),String::fromUTF8("C2 · 36"));
    flush(processor,36);
    require(audition(24)<1e-6f && audition(37)<1e-6f,
            "C2–C2 is playable on one key only");
    require(audition(36)>0.001f && !processor.underruns(),"Play the 16-hit sequence on C2");
    openGrid("drumSequenceHighNote",129);
    {
        juce::AudioBuffer<float> audio(2,256);
        juce::MidiBuffer midi;
        const auto channel=processor.editorChannel()+1;
        midi.addEvent(juce::MidiMessage::noteOn(channel,50,uint8_t(90)),0);
        midi.addEvent(juce::MidiMessage::noteOff(channel,50),32);
        processor.processBlock(audio,midi);
    }
    flush(processor,36);
    require(!find(ui,"soulTrainPicker",true) &&
            processor.drumSequencerInfo().sequences.keyRanges[0]==
                novation::controller::command::KSDrumKeyRange{36,50},
            "Learn the upper sequence note from MIDI");
    require(audition(42)>0.001f && audition(50)>0.001f && audition(51)<1e-6f,
            "C2–D3 includes interior notes and D3, and rejects the next semitone");
    click("drumAssignedKeys");
    flush(processor,36);
    require(audition(24)>0.001f,"With restriction off, an unassigned note can start the current sequence");
    click("drumAssignedKeys");
    option("Immediate");
    require(assignedKeys->getToggleState() && processor.drumSequencerInfo().sequences.switchAfter==0,
            "Choose immediate switching independently from Assigned Keys");
    click("drumStep1:1");
    setValue(75);
    selectParameter("Velocity");
    require(stepValue->getValue()==107,"Probability and Ratchet leave velocity intact");
    click("drumStep10:1");
    selectParameter("Ratchet");
    setValue(9);
    selectParameter("Probability");
    setValue(42);
    auto* doubleTapped=find(ui,"drumStep10:1",true);
    require(doubleTapped,"Find a pad to double click");
    const auto centre=doubleTapped->getLocalBounds().getCentre().toFloat();
    const auto now=juce::Time::getCurrentTime();
    const auto doubleClick=[&](juce::ModifierKeys modifiers){
        doubleTapped->mouseDoubleClick({juce::Desktop::getInstance().getMainMouseSource(),centre,modifiers,
            1.f,0.f,0.f,0.f,0.f,doubleTapped,doubleTapped,now,centre,now,2,false});
    };
    doubleClick(juce::ModifierKeys::rightButtonModifier);
    require(stepValue->isEnabled() && stepValue->getValue()==42,"Keep the hit after a right-button double click");
    doubleClick(juce::ModifierKeys::leftButtonModifier);
    require(!stepValue->isEnabled() && !find(ui,"drumRemoveStep",true)->isEnabled() &&
            processor.drumSequencerInfo().sequences.patterns[0].velocities[9*16]==0,
            "Double-click the pad to remove the step");
    click("drumStep9:1");
    click("drumRemoveStep");
    require(!stepValue->isEnabled() && !find(ui,"drumRemoveStep",true)->isEnabled(),"Clear the selected step with the cross");
    selectParameter("Velocity");
    click("drumStep1:3");
    setValue(45);
    click("drumStep2:5");
    setValue(81);
    click("drumStep10:4");
    setValue(65);
    press(openGrid("drumSequenceLength",16),"4");
    require(!find(ui,"drumStep2:5",true)->isEnabled(),"Disable pads beyond the selected length");

    click("drumSequence2");
    press(openGrid("drumSequenceLowNote",129),String::fromUTF8("D4 · 62"));
    press(openGrid("drumSequenceLength",16),"5");
    flush(processor,48);
    require(audition(62)<1e-6f,"The reassigned key selects the empty second sequence");
    click("drumStep3:1");
    setValue(91);
    flush(processor,48);
    const auto sequences=processor.drumSequencerInfo().sequences;
    const auto& pattern=sequences.patterns[0];
    require(pattern.notes[0]==38 && pattern.velocities[0]==107 && pattern.velocities[2]==45 &&
            pattern.probabilities[0]==75 && pattern.ratchets[0]==16 &&
            !pattern.velocities[9*16] && pattern.probabilities[9*16]==100 && pattern.ratchets[9*16]==1 &&
            pattern.velocities[20]==81 && pattern.velocities[9*16+3]==65 && pattern.steps==4 &&
            sequences.patterns[1].steps==5 && sequences.patterns[1].velocities[2*16]==91 &&
            sequences.keyRanges[0]==novation::controller::command::KSDrumKeyRange{36,50} &&
            sequences.keyRanges[1]==novation::controller::command::KSDrumKeyRange{62,62},
            "Retain independent lengths, trigger ranges, drum sounds and velocities");
    require(audition(36)>0.001f && audition(62)>0.001f,"Hear both sequences from their assigned keys");
    click("drumSequence1");
    click("drumStep1:1");
    selectParameter("Ratchet");
    option("End of sequence");
    require(!find(ui,"drumPlayhead",true),"Hide the playhead while idle");
    juce::AudioBuffer<float> playheadAudio(2,256);
    juce::MidiBuffer playheadMidi;
    const auto render=[&]{
        playheadAudio.clear();
        processor.processBlock(playheadAudio,playheadMidi);
    };
    const auto playTo=[&](Processor::DrumPlayhead expected){
        for(int block=0;block<260 && processor.drumPlayhead(0)!=expected;++block) render();
        require(processor.drumPlayhead(0)==expected,"Observe the played column at the audio output");
        pump(.12);
    };
    playheadMidi.addEvent(juce::MidiMessage::noteOn(processor.editorChannel()+1,36,uint8_t(90)),0);
    playTo({0,0});
    const auto* firstColumn=find(ui,"drumPlayhead",true);
    require(firstColumn,"Show the playing first column");
    const auto firstColumnX=firstColumn->getX();
    playTo({0,1});
    const auto* secondColumn=find(ui,"drumPlayhead",true);
    require(secondColumn && secondColumn->getX()>firstColumnX,"The playhead moves to an empty second column");
    click("drumSequence2");
    require(!find(ui,"drumPlayhead",true),"Only show a column on the sounding sequence");
    click("drumSequence1");
    require(find(ui,"drumPlayhead",true),"Show the current column when returning to the sounding sequence");
    auto image=ui.createComponentSnapshot(ui.getLocalBounds(),true,1.0f);
    auto png=output.getChildFile("KS-drum-sequencer.png").createOutputStream();
    require(image.isValid() && png && juce::PNGImageFormat().writeImageToStream(image,*png),"Capture native drum grid");
    png.reset();
    playheadMidi.addEvent(juce::MidiMessage::noteOn(processor.editorChannel()+1,62,uint8_t(90)),0);
    playTo({0,2});
    playTo({0,3});
    playTo({1,0});
    playheadMidi.addEvent(juce::MidiMessage::noteOff(processor.editorChannel()+1,36),0);
    playheadMidi.addEvent(juce::MidiMessage::noteOff(processor.editorChannel()+1,62),1);
    playTo({});
    require(!find(ui,"drumPlayhead",true),"Clear the playing column after release");
    click("closeDrumSequencer");
    require(!find(ui,"drumSequencer",true),"Close the sequencer without losing the grid");

    juce::MemoryBlock project;
    processor.getStateInformation(project);
    processor.setStateInformation(project.getData(),int(project.getSize()));
    flush(processor,56);
    const auto restored=processor.drumSequencerInfo().sequences;
    require(restored.selected && restored.assignedKeysOnly && !restored.retrigger &&
            restored.switchAfter==Processor::DrumGridEdit::SwitchAtEnd &&
            restored.keyRanges==sequences.keyRanges && restored.patterns[0].notes==pattern.notes &&
            restored.patterns[0].velocities==pattern.velocities && restored.patterns[0].steps==4 &&
            restored.patterns[0].probabilities==pattern.probabilities && restored.patterns[0].ratchets==pattern.ratchets &&
            restored.patterns[1].velocities==sequences.patterns[1].velocities && restored.patterns[1].steps==5 &&
            audition(62)>0.001f,"Restore both sequences, ranges, switch timing and Retrigger with the project");

    using DrumEdit=Processor::DrumGridEdit;
    const auto densePattern=uint8_t(DrumEdit::Patterns-1);
    require(processor.editDrumGrid({DrumEdit::Kind::KeyLow,0,densePattern,0,90},error),error);
    require(processor.editDrumGrid({DrumEdit::Kind::Length,0,densePattern,0,1},error),error);
    for(uint8_t row=0;row<DrumEdit::Rows;++row) {
        const auto pad=uint8_t(row*DrumEdit::Steps);
        require(processor.editDrumGrid({DrumEdit::Kind::Velocity,0,densePattern,pad,90},error),error);
        require(processor.editDrumGrid({DrumEdit::Kind::Ratchet,0,densePattern,pad,DrumEdit::MaxRatchet},error),error);
    }
    flush(processor,36);
    require(audition(90)>0.001f && !processor.underruns(),
            "Play all ten rows with 16 ratchets through the native controller. "+processor.status());
    require(find(ui,"drumSequencerEdit",true),"Restore Edit when the project selects SEQ");
    chooseRhythm("33");
    flush(processor,36);
    auto* patternParameter=processor.uiParameter("arp_rhythm");
    require(patternParameter && patternParameter->convertFrom0to1(patternParameter->getValue())==32,
            "Displayed rhythm 33 uses the original index 32");
    require(!processor.drumSequencerInfo().sequences.selected && !find(ui,"drumSequencerEdit",true),
            "Hide Edit when a factory rhythm is selected");
    auto* arpSwitch=processor.uiParameter("arp_on");
    require(arpSwitch && arpSwitch->getValue()==1.0f,"Factory rhythm can play with Arpeggiator On");
    arpSwitch->setValueNotifyingHost(0.f);
    flush(processor,36);
    require(arpSwitch->getValue()==0.0f && audition(38)>0.001f,
            "Arpeggiator Off disables the factory sequence while allowing a drum key to play");

    const auto performances=processor.performances();
    int performance=-1;
    unsigned drumPart=0;
    for(size_t index=0;index<performances.size() && performance<0;++index) {
        const auto& data=performances[index].data;
        for(unsigned part=1;part<ks::PartCount;++part) {
            const auto bank=data[13+part*256+187];
            if(bank>=5 && bank<=8 && (data[13+190]&(1u<<part))) {
                performance=int(index);
                drumPart=part;
                break;
            }
        }
    }
    require(performance>=0 && processor.selectPerformance(performance,error),"Load a Performance with a Drum Map on another part");
    flush(processor,180);
    processor.selectPart(drumPart);
    flush(processor,24);
    const auto partGrid=processor.drumSequencerInfo();
    require(partGrid.slot==drumPart+1 && partGrid.bank && !partGrid.sequences.patterns[0].velocities[0],
            "The Performance part starts with its own sequences");
    auto* partMode=processor.uiParameter("arp_pattern");
    require(partMode!=nullptr,"Find the active part's Arpeggiator Mode");
    partMode->setValueNotifyingHost(partMode->convertTo0to1(7));
    flush(processor,36);
    chooseRhythm("SEQ");
    click("drumSequencerEdit");
    click("drumStep1:1");
    flush(processor,48);
    click("closeDrumSequencer");
    processor.getStateInformation(project);
    auto xml=juce::AudioProcessor::getXmlFromBinary(project.getData(),int(project.getSize()));
    juce::MemoryBlock gridBytes;
    require(xml && gridBytes.fromBase64Encoding(xml->getStringAttribute("ksUserDrums")),"Decode saved Performance grids");
    const auto grids=novation::controller::command::decodeKSUserDrums(
        static_cast<const uint8_t*>(gridBytes.getData()),gridBytes.getSize());
    require(!grids[0].selected && grids[0].patterns[0].velocities[0]==107 && grids[0].keyRanges[1].low==62 &&
            grids[drumPart+1].selected && grids[drumPart+1].patterns[0].velocities[0]==100 &&
            !grids[drumPart+1].keyRanges[1].assigned(),
            "Select User on a Performance part while retaining the independent Single grid");
    std::cout<<"PASS: KS Rhythm menu, playhead, trigger ranges, switch timing, double-click deletion and recall\n";
}

void installSyntheticLibrary(Processor& processor,Browser& browser,const juce::File& data) {
    std::vector<uint8_t> raw;
    for (int number=0;number<40;++number) {
        std::vector<uint8_t> patch(270);
        const uint8_t header[]{0xf0,0,0x20,0x29,1,0x42,0x7f,1,0,0x11,2,1,uint8_t(number)};
        std::copy(std::begin(header),std::end(header),patch.begin());
        patch.back()=0xf7;
        for (const auto& c:ksPlugin::controls) {
            if (c.patchOffset<0) continue;
            auto& byte=patch[13+c.patchOffset];
            const auto value=String(c.id)=="cutoff"?32+number:int(c.defaultValue);
            byte=uint8_t((byte&~c.patchMask)|((value<<c.patchShift)&c.patchMask));
        }
        const auto name="Test Preset "+String(number).paddedLeft('0',2);
        std::fill_n(patch.begin()+164,16,' ');
        std::copy_n(name.toRawUTF8(),name.length(),patch.begin()+164);
        raw.insert(raw.end(),patch.begin(),patch.end());
    }
    const auto file=data.getChildFile("Native-Performance-fixture.syx");
    require(file.replaceWithData(raw.data(),raw.size()),"Write synthetic test bank");
    processor.patchLibrary.addPath(file);
    int source=0;
    for (int i=0;i<150 && !source;++i) {
        const auto snap=browserData(browser);
        if (const auto* options=snap["sources"].getArray())
            for (const auto& option:*options) if (option["label"].toString().contains(file.getFileName()))
                source=int(option["value"]);
        pump(.03);
    }
    require(source>1,"Synthetic source missing");
    browser.webAction(object({{"action","source"},{"value",source}}));
    for (int i=0;i<150 && browserData(browser)["rows"].size()!=40;++i) pump(.02);
    require(browserData(browser)["rows"].size()==40,"Synthetic Performance library count");
}

void checkSkinSwitch() {
    Processor processor;
    require(processor.skinChoice==Processor::defaultSkin,"Use a fresh profile for the default-skin check");
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto* front=dynamic_cast<SoulTrainJuce*>(find(*editor,"soulTrainNative",true));
    require(front && front->currentScene().contains("-main-"),"Open the native editor on Main before firmware is ready");
    require(find(*editor,"surface",true,false)==nullptr,"Native startup must not construct the legacy editor");
    const juce::Component::SafePointer<SoulTrainJuce> previousNative(front);
    press(*front,"Setup");
    press(*front,"Skin");
    auto* picker=find(*editor,"soulTrainPicker",true);
    require(picker && find(*picker,"SoulTrain (Default)",false) && find(*picker,"Experimental grey",false),"Built-in skins missing from Setup");
    press(*picker,"Old version skin");
    require(processor.skinChoice=="Silver-K","Select the original Silver-K skin");
    require(find(*editor,"surface",true)!=nullptr && !previousNative,
            "The old skin runs only after selection and releases SoulTrain");
    const auto choose=[&](int id){
        auto* menu=dynamic_cast<juce::ComboBox*>(find(*editor,"skinSelect",true));
        require(menu!=nullptr,"Skin selector unavailable");
        menu->setSelectedId(id,juce::sendNotificationSync);
        pump(.35);
    };
    choose(3);
    auto* surface=find(*editor,"surface",true);
    require(processor.skinChoice=="Silver Flat" && surface && skinIsFlat(*surface),"Select the experimental grey skin");
    const juce::Component::SafePointer<juce::Component> previousLegacy(surface);
    {Processor reopened;require(reopened.skinChoice=="Silver Flat","Grey selection did not survive reopening");}
    choose(1);
    require(processor.skinChoice==Processor::defaultSkin && find(*editor,"soulTrainNative",true),"Return to native SoulTrain");
    require(!previousLegacy && find(*editor,"surface",true,false)==nullptr,
            "Returning to SoulTrain must destroy the legacy editor");
    std::cout<<"PASS: mutually exclusive editors on startup and skin changes, including grey skin persistence\n";
}

void checkMenuBackdrop(SoulTrainJuce& ui) {
    require(ui.showPage("Main"),"Open Main for LFO waveform");
    const juce::Rectangle<int> outsideMenu(40,740,100,80);
    const auto before=ui.createComponentSnapshot(outsideMenu);
    press(ui,"lfo1 wave");
    auto* picker=find(ui,"soulTrainPicker",true);
    require(picker!=nullptr,"Open LFO waveform picker");
    const auto during=ui.createComponentSnapshot(outsideMenu);
    require(before.isValid() && before.getBounds()==during.getBounds(),"Capture the panel outside the picker");
    for (int y=0;y<before.getHeight();++y)
        for (int x=0;x<before.getWidth();++x)
            require(before.getPixelAt(x,y)==during.getPixelAt(x,y),"Picker changed pixels outside its menu");
    require(picker->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Dismiss the LFO picker");
    for(int i=0;i<30 && find(ui,"soulTrainPicker",true);++i) pump(.03);
    require(find(ui,"soulTrainPicker",true)==nullptr,"LFO picker stayed open");
}

void checkHighlightBounds(SoulTrainJuce& ui,juce::Button& button,juce::Rectangle<int> allowed,
                          juce::Rectangle<int> previousChoice={}) {
    const auto hit=ui.getLocalArea(&button,button.getLocalBounds());
    const auto frame=hit.getUnion(allowed).getUnion(previousChoice).expanded(4);
    button.giveAwayKeyboardFocus();
    button.setState(juce::Button::buttonNormal);
    const auto idle=ui.createComponentSnapshot(frame);
    require(idle.isValid(),"Capture idle "+button.getName());
    const auto check=[&](const juce::Image& state,const String& name,bool previewsSelection=false){
        require(state.isValid() && state.getBounds()==idle.getBounds(),"Capture "+name+" for "+button.getName());
        bool changed=false;
        for (int y=0;y<idle.getHeight();++y)
            for (int x=0;x<idle.getWidth();++x) if (idle.getPixelAt(x,y)!=state.getPixelAt(x,y)) {
                const auto point=frame.getPosition()+juce::Point<int>(x,y);
                require(allowed.contains(point) || (previewsSelection && previousChoice.contains(point)),
                        name+" escaped "+button.getName());
                require(allowed!=hit || !hit.contains(point) || hit.reduced(1).contains(point),
                        name+" painted over the outside border of "+button.getName());
                changed=true;
            }
        require(changed,name+" did not light "+button.getName());
    };

    button.setState(juce::Button::buttonOver);
    const auto hover=ui.createComponentSnapshot(frame);
    check(hover,"Hover");
    button.setState(juce::Button::buttonDown);
    const auto pressed=ui.createComponentSnapshot(frame);
    check(pressed,"Press",true);
    bool distinct=false;
    for (int y=0;y<hover.getHeight();++y)
        for (int x=0;x<hover.getWidth();++x) distinct|=hover.getPixelAt(x,y)!=pressed.getPixelAt(x,y);
    require(distinct,"Show a distinct pressed state for "+button.getName());

    button.setState(juce::Button::buttonNormal);
    button.grabKeyboardFocus();
    require(button.hasKeyboardFocus(false),"Focus "+button.getName()+" for the keyboard check");
    check(ui.createComponentSnapshot(frame),"Keyboard focus");
    button.giveAwayKeyboardFocus();
    button.setState(juce::Button::buttonNormal);
}

juce::Rectangle<int> indicatorLens(const SoulTrainJuce& ui,const juce::Button& button) {
    const auto hit=ui.getLocalArea(&button,button.getLocalBounds());
    return {hit.getX(),hit.getCentreY()-6,12,12};
}

bool indicatorIsRed(juce::Colour pixel) {
    return int(pixel.getRed())>int(pixel.getGreen())+80;
}

void checkExclusiveLedSwitch(SoulTrainJuce& ui,Processor& processor) {
    auto* first=dynamic_cast<juce::Button*>(find(ui,"12dB",false));
    auto* second=dynamic_cast<juce::Button*>(find(ui,"24dB",false));
    auto* slope=processor.uiParameter("filter_slope");
    require(first && second,"Find both filter slope buttons in "+ui.currentScene());
    require(first->isEnabled() && second->isEnabled(),"Enable both filter slope buttons in "+ui.currentScene());
    require(slope!=nullptr,"Find the filter slope parameter in "+ui.currentScene());
    const auto before=slope->getValue();
    const auto firstLens=indicatorLens(ui,*first),secondLens=indicatorLens(ui,*second);
    const auto bounds=firstLens.getUnion(secondLens);
    const auto choiceIsLit=[&](bool firstOn) {
        const auto image=ui.createComponentSnapshot(bounds);
        const auto red=[&](juce::Rectangle<int> lens) {
            const auto centre=lens.getCentre()-bounds.getPosition();
            return indicatorIsRed(image.getPixelAt(centre.x,centre.y));
        };
        return red(firstLens)==firstOn && red(secondLens)!=firstOn;
    };

    first->onClick();
    require(choiceIsLit(true),"Update both slope lamps on the same click");
    const auto firstValue=slope->getValue();
    second->setState(juce::Button::buttonDown);
    require(choiceIsLit(false) && slope->getValue()==firstValue,"Preview only the pressed lamp before applying the click");
    second->setState(juce::Button::buttonNormal);
    require(choiceIsLit(true) && slope->getValue()==firstValue,"Restore both lamps when cancelling a press");

    second->setState(juce::Button::buttonDown);
    second->onClick();
    require(second->isDown() && choiceIsLit(false) && slope->getValue()!=firstValue,
            "Show the committed lamp during a residual JUCE press");
    first->setState(juce::Button::buttonDown);
    require(choiceIsLit(true),"A residual press cannot keep the previous lamp lit");
    first->setState(juce::Button::buttonNormal);
    second->setState(juce::Button::buttonNormal);
    require(choiceIsLit(false),"Return to the last committed slope");
    slope->setValueNotifyingHost(before);
    pump();
}

void checkTwoCoreSwitches(SoulTrainJuce& ui,Processor& processor,Browser& browser,const juce::File& output,
                          const juce::File& firmware,const juce::File& flash,const juce::File& wave) {
    String error;
    Processor reference;
    reference.setNonRealtime(true);
    reference.prepareToPlay(48000,256);
    require(reference.setFlashRom(flash,error) && reference.setWaveRom(wave,error) &&
            reference.loadFirmware(firmware,error),error);
    for (auto* instrument:{&reference,&processor}) {
        require(instrument->setKSTwoCoreProcessing(false,error),error);
        require(instrument->selectPreset(0,error),error);
        flush(*instrument,96);
    }

    require(ui.showPage("Setup"),"Open KS Setup for the two-core check");
    const auto title=String::fromUTF8("KS: two CPU cores · experimental");
    const auto* toggle=dynamic_cast<juce::Button*>(find(ui,title,false));
    require(toggle && toggle->isEnabled(),"Find the experimental KS worker switch");
    const auto lens=indicatorLens(ui,*toggle);
    const auto revision=processor.firmwareLoadRevision();
    const auto latency=processor.getLatencySamples();
    juce::AudioBuffer<float> expected(2,256),actual(2,256);
    float peak=0.f;
    for (int block=0;block<96;++block) {
        if (block==4 || block==12 || block==20 || block==32 || block==40 || block==56) {
            const bool enabled=block==4 || block==20 || block==40;
            press(ui,title);
            const auto report=juce::JSON::parse(processor.performanceReport());
            const auto image=ui.createComponentSnapshot(lens);
            require(processor.ksTwoCoreProcessing()==enabled &&
                    report["dsp_execution_backend"].toString()==(enabled?"coordinator-A-helper-B-slice-barrier":"sequential") &&
                    indicatorIsRed(image.getPixelAt(6,6))==enabled,
                    "Setup and the worker must agree after each on/off click");
            require(processor.firmwareLoadRevision()==revision && processor.getLatencySamples()==latency,
                    "Switch workers without a firmware or latency change");
            require(!reference.ksTwoCoreProcessing(),"Keep the other instance sequential");
            if (block==4 || block==56)
                capture(ui,processor,browser,output,"Setup",0,enabled?"-two-core-on":"-two-core-off");
        }
        juce::MidiBuffer midi;
        if(block==0) midi.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(100)),0);
        if(block==16) midi.addEvent(juce::MidiMessage::noteOn(1,67,uint8_t(90)),0);
        if(block==64) midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
        if(block==65) midi.addEvent(juce::MidiMessage::noteOff(1,67),0);
        auto copy=midi;
        expected.clear();
        actual.clear();
        reference.processBlock(expected,copy);
        processor.processBlock(actual,midi);
        for (int channel=0;channel<actual.getNumChannels();++channel) {
            for (int sample=0;sample<actual.getNumSamples();++sample) {
                const auto value=actual.getSample(channel,sample);
                require(std::isfinite(value) && value==expected.getSample(channel,sample),
                        "Live two-core switches preserve held-note PCM at block "+String(block));
                peak=std::max(peak,std::abs(value));
            }
        }
    }
    require(peak>1e-5f && processor.underruns()==0 && reference.underruns()==0,
            "Compare audible audio without missing frames");
    std::cout<<"PASS: six Setup on/off switches, matching worker and LED state, and 49152 exact audible-stream samples\n";
}

void checkIndicatorGlow(SoulTrainJuce& ui,juce::Button& button) {
    const auto lens=indicatorLens(ui,button);
    const auto halo=lens.expanded(17);
    checkHighlightBounds(ui,button,halo);
    const auto resting=ui.createComponentSnapshot(halo);
    button.setState(juce::Button::buttonDown);
    const auto held=ui.createComponentSnapshot(halo);
    button.setState(juce::Button::buttonNormal);
    require(indicatorIsRed(resting.getPixelAt(23,23))!=indicatorIsRed(held.getPixelAt(23,23)),
            "Show the Figma LED state immediately on press");
    bool outerGlow=false;
    for (int y=0;y<halo.getHeight();++y)
        for (int x=0;x<halo.getWidth();++x)
            if (!lens.expanded(2).contains(x+halo.getX(),y+halo.getY()) &&
                resting.getPixelAt(x,y)!=held.getPixelAt(x,y)) outerGlow=true;
    require(outerGlow,"Draw the Figma LED halo beyond its lens");
}

void checkPerformanceLamp(SoulTrainJuce& ui,Processor& processor) {
    for (const auto* page:{"Main","Perf"}) {
        require(ui.showPage(page),"Open page for the Performance LED");
        const String name="performance";
        require(!find(ui,"Enable Performance",false),"Remove the duplicate Performance LED");
        const auto lamp=[&]() -> juce::Button& {
            auto* button=dynamic_cast<juce::Button*>(find(ui,name,false));
            require(button && button->isEnabled() && button->onClick,"Missing Performance LED on "+String(page));
            return *button;
        };
        for (int toggle=0;toggle<2;++toggle) {
            auto& button=lamp();
            button.giveAwayKeyboardFocus();
            button.setState(juce::Button::buttonNormal);
            const auto before=processor.performanceMode();
            const auto halo=indicatorLens(ui,button).expanded(17);
            const auto snapshot=[&](const String& phase){
                const auto image=ui.createComponentSnapshot(halo);
                require(image.isValid(),"Capture Performance LED on "+String(page));
                require(indicatorIsRed(image.getPixelAt(23,23))==processor.performanceMode(),
                        "Performance LED disagrees with the engine on "+String(page)+" during "+phase);
                return image;
            };
            const auto unchanged=[&](const juce::Image& reference,const String& phase){
                const auto image=snapshot(phase);
                for (int y=0;y<reference.getHeight();++y)
                    for (int x=0;x<reference.getWidth();++x)
                        require(image.getPixelAt(x,y)==reference.getPixelAt(x,y),
                                "Performance LED brightness changed on "+String(page)+" during "+phase);
            };
            const auto idle=snapshot("idle");
            button.setState(juce::Button::buttonDown);
            unchanged(idle,"mouse-down");
            button.setState(juce::Button::buttonOver);
            unchanged(idle,"mouse-up before the click");
            button.setState(juce::Button::buttonNormal);
            button.grabKeyboardFocus();
            unchanged(idle,"keyboard focus");
            button.giveAwayKeyboardFocus();
            button.setState(juce::Button::buttonNormal);

            const auto click=button.onClick;
            click();
            if (processor.performanceMode()==before) unchanged(idle,"the queued mode change");
            flush(processor,1);
            snapshot("the first processed block");
            flush(processor,223);
            require(processor.performanceMode()!=before,"Performance LED did not toggle the mode on "+String(page));
            const auto committed=snapshot("the confirmed mode change");
            lamp().setState(juce::Button::buttonOver);
            unchanged(committed,"hover after confirmation");
            lamp().setState(juce::Button::buttonNormal);
        }
    }
    std::cout<<"PASS: Main and Perf LEDs follow both confirmed transitions without a press or hover flash\n";
}

void checkPerformanceResponses(SoulTrainJuce& ui,Processor& processor,Browser& browser,const juce::File& output) {
    String error;
    require(processor.setPerformanceMode(true,error),error);
    flush(processor,96);
    require(processor.performanceMode() && processor.activeParts()==15 && ui.showPage("Perf"),
            "Open a four-part Performance for the switching check");
    const bool oneSwitch=find(ui,"performance",false) && !find(ui,"Enable Performance",false);
    const auto lamp=[&](int part) -> juce::Button& {
        auto* button=dynamic_cast<juce::Button*>(find(ui,"Enable Part "+String(part),false));
        require(button && button->isEnabled() && button->onClick,"Find active part switch");
        return *button;
    };
    const auto lit=[&](int part) {
        const auto image=ui.createComponentSnapshot(indicatorLens(ui,lamp(part)));
        return indicatorIsRed(image.getPixelAt(image.getWidth()/2,image.getHeight()/2));
    };
    const auto channel=[&](int part) {
        auto* control=find(ui,"Part "+String(part)+" MIDI channel",false);
        require(control!=nullptr,"Find the part's MIDI channel selector");
        return control;
    };
    juce::Component::SafePointer<juce::Button> unchangedLamp(&lamp(4));
    juce::Component::SafePointer<juce::Component> unchangedChannel(channel(2));
    juce::Component::SafePointer<juce::Component> transpose(find(ui,"2:part_transpose",true));
    require(transpose && transpose->getAlpha()==1.f,"Find the active Part 2 transpose knob");
    juce::AudioBuffer<float> audio(2,256);
    juce::MidiBuffer midi;
    const auto untilMask=[&](uint8_t expected) {
        int blocks=0;
        for (;blocks<180 && processor.activeParts()!=expected;++blocks) {
            audio.clear();
            processor.processBlock(audio,midi);
            processor.refreshControls();
        }
        pump();
        return blocks;
    };
    bool immediate=true;
    for (const bool enabled:{false,true}) {
        auto& toggle=lamp(2);
        toggle.setState(juce::Button::buttonDown);
        const bool holdsPrevious=lit(2)!=enabled;
        toggle.setState(juce::Button::buttonOver);
        const auto begin=juce::Time::getMillisecondCounterHiRes();
        toggle.onClick();
        const auto clickMs=juce::Time::getMillisecondCounterHiRes()-begin;
        const bool visual=lit(2)==enabled && channel(2)->isEnabled()==enabled && transpose &&
            transpose->isEnabled()==enabled && (transpose->getAlpha()==1.f)==enabled;
        const auto confirmed=untilMask(enabled?15:13);
        std::cout<<"PART_SWITCH enabled="<<enabled<<" press_stable="<<holdsPrevious
                 <<" immediate="<<visual<<" callback_ms="<<clickMs<<" confirmed_blocks="<<confirmed<<'\n';
        require(confirmed<=24,"Apply a desktop part switch without a Performance SysEx round trip");
        immediate&=holdsPrevious && visual;
        require(processor.activeParts()==(enabled?15:13) && lit(2)==enabled &&
                channel(2)->isEnabled()==enabled,"Confirm the part switch in the engine and mixer");
    }
    const bool retained=unchangedLamp && unchangedChannel;
    lamp(2).onClick();
    lamp(3).onClick();
    const bool bothOff=!lit(2) && !lit(3);
    untilMask(9);
    const auto rapidMask=processor.activeParts();
    require(!lit(2) && !lit(3),"Both part indicators agree with the controller");
    lamp(2).onClick();
    lamp(2).onClick();
    lamp(2).onClick();
    const bool rapidFeedback=bothOff && lit(2) && !lit(3) && lit(1) && lit(4);
    flush(processor,180);
    std::cout<<"PART_SWITCH one_header="<<oneSwitch<<" controls_retained="<<retained
             <<" rapid_feedback="<<rapidFeedback<<" rapid_mask="<<int(rapidMask)<<'\n';
    require(oneSwitch,"Keep just the header Performance switch");
    require(immediate,"Show an accepted part switch and update its controls before an audio block");
    require(retained,"Switching a part must retain the live mixer controls");
    require(rapidFeedback && rapidMask==9 && processor.activeParts()==11,
            "Rapid clicks must preserve independent part changes and reversals");
    capture(ui,processor,browser,output,"Perf",0,"-part-switch");
    require(processor.setPerformanceMode(false,error),error);
    flush(processor,96);
    require(!processor.performanceMode(),"Return to Single after the switching check");
    std::cout<<"PASS: responsive independent part switches and one Performance toggle\n";
}

void checkGearArtwork(SoulTrainJuce& ui,const juce::File& assets,const String& title,
                      const String& panelControl,bool byId) {
    const auto gear=[&]() -> juce::Button& {
        auto* button=dynamic_cast<juce::Button*>(find(ui,title,false));
        require(button && button->isEnabled(),"Find "+title);
        return *button;
    };
    const auto& button=gear();
    const auto hit=ui.getLocalArea(&button,button.getLocalBounds());
    require(hit.getWidth()==21 && hit.getHeight()==21,"Keep the 21×21 hit area for "+title);
    const auto rest=juce::ImageFileFormat::loadFrom(assets.getChildFile("sprites/art-0043.png"));
    const auto lit=juce::ImageFileFormat::loadFrom(assets.getChildFile("sprites/art-0163.png"));
    require(rest.getWidth()==42 && rest.getHeight()==42 && lit.getWidth()==90 && lit.getHeight()==90,
            "Load the authored gear sprites");
    const auto area=hit.expanded(12);
    const auto capture=[&]{return ui.createComponentSnapshot(area,true,2);};
    const auto panelOpen=[&]{return find(ui,panelControl,byId)!=nullptr;};
    const auto distance=[](juce::Colour a,juce::Colour b) {
        return std::max({std::abs(int(a.getRed())-b.getRed()),std::abs(int(a.getGreen())-b.getGreen()),
                         std::abs(int(a.getBlue())-b.getBlue())});
    };
    const auto matchesSolidPixels=[&](const juce::Image& actual,const juce::Image& art,int offset) {
        require(actual.isValid() && actual.getWidth()==90 && actual.getHeight()==90,
                "Capture "+title+" and its halo");
        for (int y=0;y<art.getHeight();++y)
            for (int x=0;x<art.getWidth();++x) {
                const auto expected=art.getPixelAt(x,y);
                if (expected.isOpaque())
                    require(distance(actual.getPixelAt(x+offset,y+offset),expected)<=3,
                            "Keep the authored sprite pixels of "+title);
            }
    };
    const juce::Rectangle<int> core{24,24,42,42};
    const auto changed=[&](const juce::Image& a,const juce::Image& b,bool outside) {
        for (int y=0;y<a.getHeight();++y)
            for (int x=0;x<a.getWidth();++x)
                if (outside!=core.contains(x,y) && distance(a.getPixelAt(x,y),b.getPixelAt(x,y))>3)
                    return true;
        return false;
    };

    gear().giveAwayKeyboardFocus();
    gear().setState(juce::Button::buttonNormal);
    const auto idle=capture();
    require(!panelOpen(),"Start with "+title+" closed");
    matchesSolidPixels(idle,rest,24);
    gear().setState(juce::Button::buttonOver);
    const auto hover=capture();
    require(changed(idle,hover,false) && !changed(idle,hover,true),"Keep the compact hover for "+title);
    gear().setState(juce::Button::buttonDown);
    const auto held=capture();
    matchesSolidPixels(held,lit,0);
    require(changed(idle,held,true),"Show the authored halo outside the hit area for "+title);
    gear().setState(juce::Button::buttonNormal);
    auto& focusTarget=static_cast<juce::Component&>(gear());
    focusTarget.focusGained(juce::Component::focusChangedByTabKey);
    require(changed(idle,capture(),false),"Show keyboard focus for "+title);
    focusTarget.focusLost(juce::Component::focusChangedByTabKey);

    press(ui,title);
    require(panelOpen(),"Open the panel from "+title);
    matchesSolidPixels(capture(),lit,0);
    press(ui,title);
    require(!panelOpen(),"Close the panel from "+title);
    matchesSolidPixels(capture(),rest,24);

    const juce::KeyPress enter(juce::KeyPress::returnKey);
    const auto activateFromKeyboard=[&]{return static_cast<juce::Component&>(gear()).keyPressed(enter);};
    require(activateFromKeyboard(),"Open "+title+" with Enter");
    pump();
    require(panelOpen(),"Show the keyboard-opened panel for "+title);
    require(activateFromKeyboard(),"Close "+title+" with Enter");
    pump();
    require(!panelOpen(),"Close the keyboard-opened panel for "+title);
    matchesSolidPixels(capture(),rest,24);
}

void checkButtonHighlights(SoulTrainJuce& ui,Processor& processor,const juce::File& assets,const juce::File& output) {
    const bool k=processor.isKStation();
    const String model=k?"K":"KS";
    const auto button=[&](const String& name,bool byId=false) -> juce::Button& {
        auto* control=dynamic_cast<juce::Button*>(find(ui,name,byId));
        require(control && control->isEnabled(),"Find enabled button "+name);
        return *control;
    };
    const auto area=[&](const juce::Component& control){return ui.getLocalArea(&control,control.getLocalBounds());};
    const auto screenshot=[&](const String& name,juce::Rectangle<int> bounds){
        const auto image=ui.createComponentSnapshot(bounds,true,2);
        const auto stream=output.getChildFile(model+"-"+name+".png").createOutputStream();
        require(image.isValid() && stream && stream->setPosition(0) && stream->truncate().wasOk() &&
                juce::PNGImageFormat().writeImageToStream(image,*stream),"Save "+name+" review image");
    };

    require(ui.showPage("Main"),"Open Main to review button highlights");
    checkExclusiveLedSwitch(ui,processor);
    auto& extra=button("Extra");
    checkHighlightBounds(ui,extra,area(extra),area(button("Main")));
    extra.setState(juce::Button::buttonOver);
    screenshot("menu-button-highlight",area(extra).getUnion(area(button("Main"))).expanded(10));
    extra.setState(juce::Button::buttonNormal);
    auto& preset=button("Previous preset");
    checkHighlightBounds(ui,preset,area(preset));
    press(ui,"Poly 1");
    flush(processor);
    auto& mono=button("Mono");
    checkHighlightBounds(ui,mono,area(mono),area(button("Poly 1")));
    const auto caption=area(mono).reduced(18,8);
    const auto brightGlyphs=[&] {
        const auto image=ui.createComponentSnapshot(caption,true,2);
        int count=0;
        for (int y=0;y<image.getHeight();++y)
            for (int x=0;x<image.getWidth();++x) {
                const auto pixel=image.getPixelAt(x,y);
                count+=pixel.getBlue()>214 && int(pixel.getBlue())>int(pixel.getRed())+50;
            }
        return count;
    };
    const auto idleGlyphs=brightGlyphs();
    press(ui,"Mono");
    flush(processor);
    require(brightGlyphs()>idleGlyphs,"Draw the active voice mode caption in blue");
    auto& poly=button("Poly 1");
    poly.setState(juce::Button::buttonOver);
    screenshot("button-highlight",{350,720,362,66});
    poly.setState(juce::Button::buttonNormal);

    require(!find(ui,"All effects On/Off",false,false),"Remove the Effects master switch");
    press(ui,"Delay");
    auto* delay=processor.uiParameter(k?"delay_level":"delay");
    require(delay!=nullptr,"Find the Delay level for the FX tab LED");
    const auto originalDelay=delay->getValue();
    delay->setValueNotifyingHost(.5f);
    flush(processor);
    auto& reverb=button("Reverb");
    checkHighlightBounds(ui,reverb,area(reverb),area(button("Delay")));
    const auto delayBounds=area(button("Delay"));
    const auto lampPoint=delayBounds.getCentre().withX(delayBounds.getX()+15);
    const auto lampState=ui.createComponentSnapshot({lampPoint.x,lampPoint.y,1,1});
    require(lampState.isValid() && indicatorIsRed(lampState.getPixelAt(0,0)),"Keep the individual Delay LED");
    reverb.setState(juce::Button::buttonOver);
    screenshot("effect-buttons",delayBounds.getUnion(area(button("Vocoder"))).expanded(5,37));
    reverb.setState(juce::Button::buttonNormal);
    delay->setValueNotifyingHost(originalDelay);
    flush(processor);

    press(ui,"Vocoder");
    auto& sibilance=button("Vocoder sibilance type");
    const auto chooseSibilance=[&](const String& option,float value,bool captureMenu) {
        press(ui,sibilance.getName());
        auto* picker=find(ui,"soulTrainPicker",true);
        require(picker && find(*picker,"Noise",false) && find(*picker,"High pass",false),
                "List Noise and the full High pass label");
        if (captureMenu)
            screenshot("vocoder-menu",choiceMenuBounds(ui,*picker).getUnion(area(sibilance)).expanded(8));
        press(*picker,option);
        flush(processor);
        auto* parameter=processor.uiParameter("vocoder_sibilance_type");
        require(parameter && std::abs(parameter->getValue()-value)<.01f,"Apply Vocoder "+option);
    };
    chooseSibilance("Noise",1.f,false);
    chooseSibilance("High pass",0.f,true);
    screenshot("vocoder-hp",area(sibilance).expanded(10));

    checkGearArtwork(ui,assets,"Voices options","linear",false);
    if (!k) {
        checkGearArtwork(ui,assets,"Arpeggiator options","arpRhythmPicker",true);
        String error;
        require(processor.setPerformanceMode(true,error),error);
        flush(processor,96);
        require(processor.performanceMode(),"Enable KS Performance for the Part buttons");
        press(ui,"1");
        auto& second=button("2");
        checkHighlightBounds(ui,second,area(second),area(button("1")));
        second.setState(juce::Button::buttonOver);
        screenshot("part-buttons",area(button("1")).getUnion(area(button("4"))).expanded(7));
        second.setState(juce::Button::buttonNormal);

        require(ui.showPage("Perf"),"Open the Performance part headings");
        for (int part=1;part<=4;++part) {
            const auto title="Part "+String(part);
            const auto heading=ui.createComponentSnapshot(area(button(title)));
            require(heading.isValid() && heading.getPixelAt(0,0).getBrightness()>.55f,
                    "Leave "+title+" on the light panel without a button frame");
        }
        screenshot("performance-headings",area(button("Part 1")).getUnion(area(button("Part 4"))).expanded(10));
        press(ui,"Part 3");
        require(processor.currentPart()==2,"The Part 3 heading still selects its part");
        press(ui,"Part 1");
        require(ui.showPage("Main"),"Return to the Main performance switch");
        require(processor.setPerformanceMode(false,error),error);
        flush(processor,96);
        require(processor.selectDrumRhythm(processor.drumEditorSlot(),Processor::UserDrumRhythm,error),error);
        press(ui,"Arpeggiator options");
        flush(processor);
        auto& edit=button("drumSequencerEdit",true);
        checkHighlightBounds(ui,edit,area(edit));
        edit.setState(juce::Button::buttonOver);
        screenshot("seq-edit-highlight",area(edit).expanded(215,18).withLeft(37));
        edit.setState(juce::Button::buttonNormal);
    }

    require(ui.showPage("Setup"),"Open Setup to review its action buttons");
    auto& rescanFolders=button("Rescan folders");
    checkHighlightBounds(ui,rescanFolders,area(rescanFolders));
    require(ui.showPage("Browser"),"Open Browser to review its action buttons");
    auto& columns=button("Columns");
    checkHighlightBounds(ui,columns,area(columns));

    require(ui.showPage("Extra"),"Open Extra to review the LFO LED");
    require(!find(ui,"All effects On/Off",false,false),"Remove the Effects master switch in Extra");
    auto* sync=processor.uiParameter("lfo1_keysync");
    require(sync!=nullptr,"Find LFO 1 Key Sync");
    if (sync->getValue()<.5f) press(ui,"keysync");
    flush(processor);
    auto& lamp=button("keysync");
    const auto hit=area(lamp);
    checkIndicatorGlow(ui,lamp);
    screenshot("led-glow",hit.expanded(77,36));
    checkMenuBackdrop(ui);
    std::cout<<"PASS: "<<model<<" shared button states and fixed border, focus, LEDs and Vocoder selection\n";
}

void checkFirmwareOptions(SoulTrainJuce& ui,Processor& processor,const juce::File& output) {
    require(ui.showPage("Main"),"Open Main before Setup");
    require(!find(ui,"On firmware change",false),"Firmware switch mode escaped Setup");
    require(ui.showPage("Setup"),"Open Setup for the firmware switch mode");
    for(int row:{1,0}) {
        press(ui,"On firmware change");
        auto* picker=find(ui,"soulTrainPicker",true);
        require(picker && picker->getNumChildComponents()==2,"Firmware Options must offer the two switch modes");
        press(*picker,Processor::firmwareSwitchLabels[size_t(row)]);
        for(int i=0;i<30 && int(processor.firmwareSwitch())!=row;++i) pump(.03);
        require(int(processor.firmwareSwitch())==row,"Firmware Options did not change the processor");
        if(row==1) {
            const auto image=ui.createComponentSnapshot({38,169,462,127});
            auto file=output.getChildFile(processor.isKStation()?"K-Firmware-Setup.png":"KS-Firmware-Setup.png");
            auto stream=file.createOutputStream();
            require(stream && stream->setPosition(0) && stream->truncate().wasOk() &&
                    juce::PNGImageFormat().writeImageToStream(image,*stream),"Capture the firmware switch control");
        }
    }
    require(ui.showPage("Main"),"Return from Setup");
    std::cout<<"PASS: both switch modes in "<<(processor.isKStation()?"K-Station":"KS")<<" Setup\n";
}

void checkEnvelopeFader(SoulTrainJuce& ui,Processor& processor) {
    require(ui.showPage("Main"),"Open Main for the envelope fader test");
    auto* fader=dynamic_cast<juce::Slider*>(find(ui,"1:attack",true));
    auto* attack=processor.parameters.getParameter(processor.isKStation()?"k_amp_attack":"attack");
    require(fader && fader->isEnabled() && attack,"Find the Amp Attack fader and parameter");
    const auto saved=attack->getValue();
    attack->setValueNotifyingHost(attack->convertTo0to1(91));
    pump();
    require(fader->getValue()==91,"Host automation reached the Amp Attack fader");
    {
        const juce::Slider::ScopedDragNotification drag(*fader);
        fader->setValue(43,juce::sendNotificationSync);
        require(attack->convertFrom0to1(attack->getValue())==43,"Amp Attack fader sent the value to the host");
    }
    attack->setValueNotifyingHost(saved);
    flush(processor);
    std::cout<<"PASS: Amp Attack automation and dragging in "<<(processor.isKStation()?"K-Station":"KS")<<'\n';
}

void checkHeldKnobReadout(SoulTrainJuce& ui,Processor& processor) {
    require(ui.showPage("Extra"),"Open KS Extra for Ring LFO1");
    auto* knob=dynamic_cast<juce::Slider*>(find(ui,"1:ring_level_mod",true));
    require(knob && knob->onDragStart && knob->onDragEnd,"Find the Ring LFO1 gesture callbacks");
    const juce::Rectangle<int> lcd(390,20,281,75);
    const auto equal=[](const juce::Image& a,const juce::Image& b) {
        if(a.getBounds()!=b.getBounds()) return false;
        for(int y=0;y<a.getHeight();++y)
            for(int x=0;x<a.getWidth();++x)
                if(a.getPixelAt(x,y)!=b.getPixelAt(x,y)) return false;
        return true;
    };
    const auto idle=ui.createComponentSnapshot(lcd);
    knob->onDragStart();
    knob->setValue(knob->getValue()==97?101:97,juce::sendNotificationSync);
    flush(processor);
    pump(2.);
    const auto held=ui.createComponentSnapshot(lcd);
    require(!equal(idle,held),"Held knob showed no LCD readout");
    knob->onDragEnd();
    pump();
    require(equal(held,ui.createComponentSnapshot(lcd)),"LCD dropped the readout on release after a long hold");
    pump(1.85);
    require(equal(idle,ui.createComponentSnapshot(lcd)),"LCD did not return to the preset after the hold");
    std::cout<<"PASS: Ring LFO1 readout stays on the LCD after a long gesture and expires\n";
}

void checkRecallBrowser(SoulTrainJuce& ui,Processor& processor,Browser& browser,
                        const juce::File& k,const juce::File& ks) {
    String error;
    const auto load=[&](const juce::File& file) {
        require(processor.loadFirmware(file,error),error);
        flush(processor,128);
        browser.pollLibrary();
        ui.refreshBrowser();
    };
    const auto choose=[&](const String& name) {
        const auto data=browserData(browser);
        const auto* rows=data["rows"].getArray();
        require(rows!=nullptr,"Synthetic preset source missing after firmware change");
        for(const auto& row:*rows) {
            if(row["name"].toString()!=name) continue;
            browser.webAction(object({{"action","load"},{"id",row["id"]}}));
            flush(processor,128);
            return;
        }
        require(false,"No row for "+name);
    };
    const auto check=[&](const String& name) {
        require(processor.currentName()==name,"Expected LCD "+name+", got "+processor.currentName());
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+3000.;
        do {
            const auto data=browserData(browser);
            if (const auto* rows=data["rows"].getArray())
                for(const auto& row:*rows) if (bool(row["selected"])) {
                    require(row["name"].toString()==name,"Wrong selected row after recall: "+row["name"].toString());
                    return;
                }
            pump(.05);
        } while (juce::Time::getMillisecondCounterHiRes()<deadline);
        require(false,"Browser did not select the current preset "+name);
    };

    require(ui.showPage("Browser"),"Open Browser for Recall test");
    processor.setFirmwareSwitch(Processor::FirmwareSwitch::RecallLast);
    choose("Test Preset 02");
    load(k);
    choose("Test Preset 08");
    load(ks);
    check("Test Preset 02");
    require(browser.stepPatch(1,&error),error);
    flush(processor,128);
    check("Test Preset 03");
    load(k);
    check("Test Preset 08");
    require(browser.stepPatch(-1,&error),error);
    flush(processor,128);
    check("Test Preset 07");
    load(ks);
    check("Test Preset 03");
    std::cout<<"PASS: Recall restores the Browser selection and the next/previous preset in both models\n";
}

void checkBrowserSort(SoulTrainJuce& ui,Processor& processor,Browser& browser,const juce::File& output) {
    require(ui.showPage("Browser"),"Open the Browser header");
    auto* name=find(ui,"Sort by Name",false);
    require(name && name->getWidth()>150 && name->hitTest(name->getWidth()-2,name->getHeight()/2),
            "The whole Name cell should accept a click");
    browser.webAction(object({{"action","sort"},{"column",7},{"ascending",true}}));
    pump();
    const auto checkOrder=[&](bool ascending){
        press(ui,"Sort by Name");
        const auto data=browserData(browser);
        require(int(data["sortColumn"])==1 && bool(data["sortAscending"])==ascending,
                "Sort direction must follow each click on Name");
        require(data["rows"][0]["name"].toString()==(ascending?"Test Preset 00":"Test Preset 39"),
                "Browser table rows did not follow the new sort direction");
        ui.refreshBrowser();
    };
    checkOrder(true);
    capture(ui,processor,browser,output,"Browser",0,"-sort-ascending");
    checkOrder(false);
    capture(ui,processor,browser,output,"Browser",0,"-sort-descending");
    press(ui,"Columns");
    press(ui,"Tags");
    require(find(ui,"Sort by Tags",false)==nullptr,"Hidden column retained its sort target");
    require(find(ui,"soulTrainPicker",true)!=nullptr,"Columns closes while toggling a checkbox");
    checkOrder(true);
    press(ui,"Tags");
    require(find(ui,"Sort by Tags",false)!=nullptr,"Toggling restores the column and its sort target");
    auto* columns=find(ui,"soulTrainPicker",true);
    require(columns && columns->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Dismiss the Columns menu");
    pump();
    if(!processor.isKStation()) {
        auto* grip=find(ui,"presetColumnGrip:name",true);
        require(grip,"Name has a draggable divider");
        const auto size=name->getWidth();
        const auto now=juce::Time::getCurrentTime();
        const auto mouse=[&](juce::Component& component,juce::Point<float> position,int clicks) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),position,
                juce::ModifierKeys::leftButtonModifier,1.f,0.f,0.f,0.f,0.f,&component,&component,
                now,position,now,clicks,false);
        };
        grip->mouseDown(mouse(*grip,{4.f,10.f},1));
        grip->mouseDrag(mouse(*grip,{32.f,10.f},1));
        require(name->getWidth()>size+20,"Moving the divider widens Name");
        require(browserData(browser)["columnLayout"]["widths"]["name"].isDouble(),"Column widths enter saved Browser state");

        auto* list=dynamic_cast<juce::ListBox*>(find(ui,"soulTrainPresetList",true));
        auto* tags=find(ui,"Sort by Tags",false);
        require(list && list->getListBoxModel() && tags,"Tags column and list exist");
        const auto x=list->getLocalPoint(tags,tags->getLocalBounds().getCentre()).x;
        const auto y=list->getRowPosition(0,true).getCentreY();
        list->getListBoxModel()->listBoxItemDoubleClicked(0,mouse(*list,{float(x),float(y)},2));
        auto* field=dynamic_cast<juce::TextEditor*>(find(ui,"soulTrainTagCellEditor",true));
        require(field && field->onReturnKey,"Double-click creates a Tags cell editor");
        require(field->findColour(juce::TextEditor::textColourId)==juce::Colour(0xff101011) &&
                field->findColour(juce::TextEditor::highlightedTextColourId)==juce::Colour(0xff101011),
                "Inline tag text remains dark on the light background, including selection");
        field->setText("Live Test, Bright",false);
        field->onReturnKey();
        for(int i=0;i<100 && !browserData(browser)["rows"][0]["tags"].toString().contains("Bright");++i)pump(.02);
        require(browserData(browser)["rows"][0]["tags"].toString().contains("Live Test"),"Inline tags persisted");
        capture(ui,processor,browser,output,"Browser",0,"-tags-edited");
        std::cout<<"PASS: Browser column drag, persistent layout and inline Tags editor\n";
    }
    std::cout<<"PASS: flat Browser header sorts both ways in "<<(processor.isKStation()?"K-Station":"KS")
             <<" with all columns and with a hidden column\n";
}

void checkBrowserControls(SoulTrainJuce& ui,Processor& processor,Browser& browser,const juce::File& output) {
    require(ui.showPage("Browser"),"Open the Browser controls");
    for (const auto* name:{"Search patches","Filter tags"}) {
        auto* field=dynamic_cast<juce::TextEditor*>(find(ui,name,false));
        require(field && field->getLeftIndent()==7,"Find the padded Browser filter");
        const auto previous=field->getText();
        field->setText("Test",false);
        const auto caret=field->getCaretRectangleForCharIndex(4);
        require(std::abs(caret.getCentreY()-field->getHeight()/2)<=2,"Vertically centre Browser text and caret");
        field->setText(previous,false);
    }

    const auto areaOf=[&](juce::Component& component){return ui.getLocalArea(&component,component.getLocalBounds());};
    auto* rescan=dynamic_cast<juce::Button*>(find(ui,"Rescan",false));
    require(rescan!=nullptr,"Find the Browser Rescan button");
    rescan->setState(juce::Button::buttonNormal);
    const auto resting=ui.createComponentSnapshot(areaOf(*rescan));
    rescan->setState(juce::Button::buttonDown);
    const auto pressed=ui.createComponentSnapshot(areaOf(*rescan));
    rescan->setState(juce::Button::buttonNormal);
    require(resting.isValid() && pressed.isValid() &&
            resting.getPixelAt(resting.getWidth()/2,5)!=pressed.getPixelAt(pressed.getWidth()/2,5),
            "Show a pressed state before activating the Browser action");

    auto* instrument=dynamic_cast<juce::Button*>(find(ui,"Instrument",false));
    require(instrument && instrument->onClick,"Find the Browser Instrument menu");
    const juce::Rectangle<int> outside(1180,675,120,46);
    const auto base=ui.createComponentSnapshot(outside);
    const auto start=juce::Time::getMillisecondCounterHiRes();
    instrument->onClick();
    auto* picker=find(ui,"soulTrainPicker",true);
    const auto ready=juce::Time::getMillisecondCounterHiRes()-start;
    require(picker && picker->getWidth()>=instrument->getWidth() && picker->getHeight()<ui.getHeight()/2,
            "Show a local Browser menu at least as wide as its box");
    const auto withPopup=ui.createComponentSnapshot(outside);
    for (int y=0;y<base.getHeight();++y)
        for (int x=0;x<base.getWidth();++x)
            require(base.getPixelAt(x,y)==withPopup.getPixelAt(x,y),"Keep pixels outside the Browser menu unchanged");
    capture(ui,processor,browser,output,"Browser",0,"-instrument-menu");
    require(picker->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Dismiss the Browser Instrument menu");
    pump();

    auto* exportButton=dynamic_cast<juce::Button*>(find(ui,String::fromUTF8("Export .syx…"),false));
    require(exportButton!=nullptr,"Find the SysEx export button");
    const auto snapshot=browserData(browser);
    require(snapshot["rows"].size()>0,"Use a populated library for export");
    browser.webAction(object({{"action","load"},{"id",snapshot["rows"][0]["id"]}}));
    flush(processor,160);
    ui.refreshBrowser();
    require(exportButton->isEnabled(),"Enable the SysEx export action for the selected patch");
    const auto* tags=dynamic_cast<juce::TextEditor*>(find(ui,"Selected patch tags",false));
    require(tags && tags->findColour(juce::TextEditor::textColourId)==juce::Colour(0xff101011) &&
            tags->findColour(juce::TextEditor::highlightedTextColourId)==juce::Colour(0xff101011),
            "The selected patch's tag field uses the browser's dark text");
    capture(ui,processor,browser,output,"Browser",0,"-review");
    std::cout<<"PASS: centred filter fields, immediate press state, local Instrument menu and available SysEx export. Menu construction ms: "<<ready<<'\n';
}

void checkPartPopup(SoulTrainJuce& ui,Processor& processor,Browser& browser,unsigned part,int row) {
    const auto before=processor.partNames();
    press(ui,"Part "+String(part)+" preset");
    auto* list=dynamic_cast<juce::ListBox*>(find(ui,"soulTrainPickerList",true));
    require(list && list->getModel() && list->getModel()->getNumRows()==40,"Scrollable native preset list");
    const auto expected=browserData(browser)["rows"][row]["name"].toString();
    list->selectRow(row);
    require(list->getSelectedRow()==row,"Native preset list navigation");
    if (part==2) {
        list->scrollToEnsureRowIsOnscreen(row);
        const auto centre=list->getRowPosition(row,true).getCentre();
        require(list->getVerticalPosition()>0 && list->getLocalBounds().contains(centre) &&
                list->getRowContainingPosition(centre.x,centre.y)==row,"Scrolled preset resolves to the clicked row");
        const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(),centre.toFloat(),
            juce::ModifierKeys::leftButtonModifier,0.f,0.f,0.f,0.f,0.f,
            list,list,juce::Time::getCurrentTime(),centre.toFloat(),
            juce::Time::getCurrentTime(),1,false);
        list->getModel()->listBoxItemClicked(row,click);
    } else {
        require(list->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Enter selected a native preset");
    }
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+6000.;
    auto after=processor.partNames();
    do {
        flush(processor,160);
        after=processor.partNames();
    } while (after[part-1]!=expected && juce::Time::getMillisecondCounterHiRes()<deadline);
    std::cerr<<"Performance before";
    for (const auto& value:before) std::cerr<<" ["<<value<<']';
    std::cerr<<" after";
    for (const auto& value:after) std::cerr<<" ["<<value<<']';
    std::cerr<<"\n";
    for (unsigned i=0;i<4;++i)
        require(i==part-1?after[i]==expected:after[i]==before[i],
                "Selecting part "+String(part)+" expected "+(i==part-1?expected:before[i])+
                " in part "+String(i+1)+", observed "+after[i]+"; "+browserData(browser)["status"].toString());
}

void checkFirmwareTabs(Processor& processor,const juce::File& k,const juce::File& ks,
                       const juce::File& output) {
    processor.firmwares={{k,true},{ks,false}};
    processor.skinChoice=Processor::defaultSkin;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    juce::DocumentWindow window("Silver-K firmware tabs",juce::Colours::black,0);
    window.setContentNonOwned(editor.get(),true);
    window.setVisible(true);
    auto* ui=dynamic_cast<SoulTrainJuce*>(find(*editor,"soulTrainNative",true));
    require(ui!=nullptr,"Open the actual SoulTrain editor");
    int step=0;
    juce::AudioBuffer<float> audio(2,256);
    juce::MidiBuffer midi;
    const auto waitForSwitch=[&](uint64_t revision,bool wantK){
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
        while(processor.firmwareLoadRevision()==revision){
            require(juce::Time::getMillisecondCounterHiRes()<deadline,"Firmware switch timed out");
            audio.clear();
            processor.processBlock(audio,midi);
            pump(.01);
        }
        pump(.2);
        require(processor.firmwareReady() && processor.isKStation()==wantK,processor.status());
    };
    const auto snapshot=[&](juce::Component& component,const String& scene){
        const auto file=output.getChildFile(String(++step).paddedLeft('0',2)+"-"+scene+".png");
        auto stream=file.createOutputStream();
        const auto image=component.createComponentSnapshot(component.getLocalBounds());
        require(image.isValid() && stream && juce::PNGImageFormat().writeImageToStream(image,*stream),
                "Capture "+scene);
        std::cout<<file.getFileName()<<'\n';
    };
    const auto chooseFirmware=[&](bool wantK,const String& expectedPage){
        if(processor.firmwareReady() && processor.isKStation() && !wantK){
            require(processor.sendShortMidi(uint8_t(0xb0|processor.editorChannel()),7,127),
                    "Set K-Station volume before returning to KS");
            flush(processor,64);
        }
        const auto revision=processor.firmwareLoadRevision();
        press(*ui,"Select firmware");
        auto* list=dynamic_cast<juce::ListBox*>(find(*ui,"soulTrainPickerList",true));
        require(list!=nullptr,"Open the firmware picker from the current page");
        list->selectRow(wantK?0:1);
        require(list->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)),"Select firmware using the picker");
        waitForSwitch(revision,wantK);
        const auto scene=String(wantK?"K-":"KS-")+expectedPage.toLowerCase()+"-";
        snapshot(*ui,ui->currentScene());
        require(ui->currentScene().startsWith(scene),"Expected "+scene+" after switch, saw "+ui->currentScene());
    };
    chooseFirmware(false,"Setup");
    press(*ui,"Browser");
    for(const auto mode:{Processor::FirmwareSwitch::KeepPatch,Processor::FirmwareSwitch::RecallLast}){
        processor.setFirmwareSwitch(mode);
        for(int i=0;i<2;++i){
            chooseFirmware(true,"Browser");
            chooseFirmware(false,"Browser");
        }
    }
    for(const auto* page:{"Extra","Main","Setup"}){
        press(*ui,page);
        chooseFirmware(true,page);
        chooseFirmware(false,page);
    }
    press(*ui,"Perf");
    chooseFirmware(true,"Main");
    chooseFirmware(false,"Perf");
    window.clearContentComponent();
    editor.reset();

    processor.skinChoice="Silver-K";
    editor.reset(processor.createEditor());
    window.setContentNonOwned(editor.get(),true);
    auto* patches=dynamic_cast<juce::TextButton*>(find(*editor,"pagePatches",true));
    require(patches && patches->onClick,"Open Patches in the original skin");
    patches->onClick();
    auto* firmware=dynamic_cast<juce::ComboBox*>(find(*editor,"firmwareSelect",true));
    require(firmware!=nullptr,"Open the original skin's firmware control");
    const auto revision=processor.firmwareLoadRevision();
    firmware->setSelectedId(1,juce::sendNotificationSync);
    waitForSwitch(revision,true);
    require(find(*editor,"patchBrowser",true)!=nullptr,"Old skin left Patches after switching firmware");
    snapshot(*editor,"K-original-Patches");
    window.clearContentComponent();
    std::cout<<"PASS: firmware swaps retain the selected tab in SoulTrain and the original skin\n";
}

void checkControlSizes(SoulTrainJuce& ui,Processor& processor,Browser& browser,const juce::File& assets,
                       const juce::File& output) {
    const auto checkHeight=[&](const char* name,int height){
        const auto* control=find(ui,String::fromUTF8(name),false);
        require(control && control->getHeight()==height,"Match the standard control height for "+String::fromUTF8(name));
    };
    const auto open=[&](const String& page){
        require(ui.showPage(page),"Open "+page+" for the sizing review");
        const auto scene=juce::JSON::parse(assets.getChildFile(ui.currentScene()+".json"));
        const auto* widgets=scene["widgets"].getArray();
        require(widgets!=nullptr,"Read the authored positions for "+page);
        for (const auto& widget:*widgets) {
            if (widget["visualType"]!="header-button" && widget["visualType"]!="preset") continue;
            const auto name=widget["label"].toString();
            const auto* control=find(ui,name,false);
            const auto area=widget["rect"];
            const juce::Rectangle<float> authored{float(area[0]),float(area[1]),float(area[2]),float(area[3])};
            require(control && control->getBounds()==authored.getSmallestIntegerContainer(),
                    "Keep the authored size and position of "+name);
        }
    };
    require(ui.showPage("Setup"),"Refresh controls after loading firmware");
    open("Main");
    checkWaveMenuWidth(ui,processor.isKStation());
    checkHeight("Mono",34);
    checkHeight("voices",36);
    checkHeight("lfo1 wave",36);
    open("Extra");
    checkHeight("Mono",34);
    checkHeight("voices",36);
    open("Browser");
    checkHeight("Columns",34);
    checkHeight("Export .syx…",34);
    checkHeight("Instrument",36);
    checkHeight("Source",36);
    open("Setup");
    checkHeight("Rescan folders",34);
    checkHeight("Panic",34);
    checkHeight("Firmware",36);
    checkHeight("On firmware change",36);
    checkHeight("Audio buffer profile",36);
    const auto control=[&](const String& name) -> juce::Component& {
        auto* item=find(ui,name,false);
        require(item!=nullptr,"Find "+name+" in Setup");
        return *item;
    };
    const auto model=control("Setup model").getBounds();
    const auto clock=control("Clock source").getBounds();
    const auto channel=control("Control MIDI channel").getBounds();
    const auto audio=control("Audio buffer profile").getBounds();
    const auto data=control("Interface size").getBounds();
    require(model.getY()==clock.getY() && clock.getY()==channel.getY() &&
            model.getRight()<clock.getX() && clock.getRight()<channel.getX(),
            "Place Model, Clock source and Control MIDI channel in one row");
    require(audio.getX()==model.getX() && audio.getY()>channel.getBottom(),
            "Place audio processing below Synth and MIDI");
    require(data.getX()>channel.getRight() && data.getRight()<1000 &&
            control("Skins folder").getX()<data.getX(),
            "Move Data and Skin into the next column");
    capture(ui,processor,browser,output,"Setup",0,"-compact");
    checkChoiceFrameStates(ui,control("Clock source"),output);
    for (const auto* name:{"Setup model","Clock source","Control MIDI channel",
                          "Firmware","On firmware change","Audio buffer profile","Interface size"}) {
        press(ui,name);
        const auto triggerWidth=control(name).getWidth();
        const auto menuWidth=checkComboMenuWidth(ui,control(name));
        if (String(name)=="Firmware" || String(name)=="Audio buffer profile")
            require(menuWidth==triggerWidth,"Match the menu width when all rows fit");
        if (String(name)=="Clock source") capture(ui,processor,browser,output,"Setup",0,"-clock-menu");
        auto* picker=find(ui,"soulTrainPicker",true);
        require(picker && picker->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Dismiss "+String(name));
        pump();
    }
    const auto longName=String::repeatedString("Extended firmware name ",6)+"KS.syx";
    processor.firmwares.push_back({output.getChildFile(longName),processor.isKStation()});
    press(ui,"Firmware");
    require(checkComboMenuWidth(ui,control("Firmware"))>control("Firmware").getWidth(),
            "Widen a single-column firmware menu for a long filename");
    auto* firmwarePicker=find(ui,"soulTrainPicker",true);
    require(firmwarePicker && firmwarePicker->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),
            "Dismiss the wide firmware menu");
    pump();
    processor.firmwares.pop_back();
    std::cout<<"PASS: equal field and menu widths, oscillator alignment and widening for long text\n";
    if (processor.isKStation()) {
        std::cout<<"PASS: standard K-Station control heights and authored header geometry\n";
        return;
    }

    String error;
    require(processor.setPerformanceMode(true,error),error);
    flush(processor,224);
    require(processor.performanceMode(),"Enter KS Performance to check the note menus");
    open("Perf");
    checkHeight("Part 1 preset",36);
    checkHeight("Part 1 MIDI channel",36);
    checkHeight("Part 1 Low note",36);
    checkHeight("Part 1 High note",36);
    checkHeight("Edit sound · Part 1",34);
    const auto* canvas=find(ui,"soulTrainJuce",true);
    require(canvas!=nullptr,"Find the Performance surface");
    std::array<int,2> navigationCounts{};
    const auto navSize=control("Prev").getLocalBounds();
    for (const auto* child:canvas->getChildren()) {
        const auto name=child->getName();
        if (name!="Prev" && name!="Next") continue;
        require(child->getLocalBounds()==navSize && child->getHeight()==34,
                "Match Prev and Next sizes across all four parts");
        ++navigationCounts[size_t(name=="Next")];
    }
    require(navigationCounts==std::array<int,2>{4,4},"Find Prev and Next in every Performance part");
    press(ui,"Part 1 MIDI channel");
    checkComboMenuWidth(ui,control("Part 1 MIDI channel"));
    auto* channelPicker=find(ui,"soulTrainPicker",true);
    auto* channelRow=channelPicker?dynamic_cast<juce::Button*>(find(*channelPicker,"8",false)):nullptr;
    require(channelRow!=nullptr,"Find an unselected channel row");
    const auto row=ui.getLocalArea(channelRow,channelRow->getLocalBounds());
    const auto sample=juce::Rectangle<int>(row.getX()+4,row.getCentreY(),1,1);
    const auto idle=ui.createComponentSnapshot(sample).getPixelAt(0,0);
    const auto currentChannel=processor.midiChannel();
    channelRow->setState(juce::Button::buttonOver);
    require(ui.createComponentSnapshot(sample).getPixelAt(0,0).getBrightness()>idle.getBrightness() &&
            processor.midiChannel()==currentChannel,"Highlight an unselected Performance row on hover without applying it");
    capture(ui,processor,browser,output,"Perf",0,"-channel-hover");
    require(channelPicker->keyPressed(juce::KeyPress(juce::KeyPress::downKey)) &&
            ui.createComponentSnapshot(sample).getPixelAt(0,0)==idle,
            "Move the highlight away from the hovered row with the keyboard");
    require(channelPicker->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Dismiss the hovered channel menu");
    pump();
    const auto selectNote=[&](const char* title,const char* parameter,const String& label,int note){
        press(ui,title);
        require(checkComboMenuWidth(ui,control(title))>control(title).getWidth(),"Keep the wider Performance note menu");
        auto* picker=find(ui,"soulTrainPicker",true);
        require(picker && picker->getNumChildComponents()==128,"Keep the complete Performance note grid");
        for (const auto* sharp:{"C#4 · 61","D#4 · 63","F#4 · 66","G#4 · 68","A#4 · 70"})
            require(find(*picker,String::fromUTF8(sharp),false),"Use standard sharps in the note menu");
        press(*picker,label);
        auto* value=processor.uiParameter(parameter);
        require(value && juce::roundToInt(value->convertFrom0to1(value->getValue()))==note,
                "Preserve the MIDI number selected in "+String(title));
    };
    selectNote("Part 1 Low note","part_low",String::fromUTF8("C#4 · 61"),61);
    selectNote("Part 1 High note","part_high",String::fromUTF8("A#4 · 70"),70);
    const auto readNote=[&](unsigned part,const char* id) {
        const auto* specification=processor.spec(id);
        require(specification!=nullptr,"Find the note parameter");
        auto* value=processor.parameters.getParameter(Processor::parameterId(*specification,part));
        require(value!=nullptr,"Find the selected part's note parameter");
        return juce::roundToInt(value->convertFrom0to1(value->getValue()));
    };
    const auto playMidi=[&](std::initializer_list<juce::MidiMessage> messages) {
        juce::AudioBuffer<float> audio(2,256);
        juce::MidiBuffer midi;
        int offset=0;
        for (const auto& message:messages) {
            midi.addEvent(message,offset);
            offset+=8;
        }
        processor.processBlock(audio,midi);
        flush(processor,48);
    };
    press(ui,"Part 2 Low note");
    checkComboMenuWidth(ui,control("Part 2 Low note"));
    playMidi({juce::MidiMessage::noteOff(13,44),juce::MidiMessage::noteOn(13,45,uint8_t(0))});
    require(find(ui,"soulTrainPicker",true)!=nullptr,"Keep learning through Note Off and zero velocity");
    playMidi({juce::MidiMessage::noteOn(13,63,uint8_t(80)),juce::MidiMessage::noteOn(4,84,uint8_t(90)),
              juce::MidiMessage::noteOff(13,63),juce::MidiMessage::noteOff(4,84)});
    require(!find(ui,"soulTrainPicker",true) && readNote(1,"part_low")==63 && readNote(0,"part_low")==61,
            "Learn the first MIDI note for Part 2 Low note");
    press(ui,"Part 2 High note");
    playMidi({juce::MidiMessage::noteOn(8,66,uint8_t(90)),juce::MidiMessage::noteOff(8,66)});
    require(!find(ui,"soulTrainPicker",true) && readNote(1,"part_high")==66 && readNote(0,"part_high")==70,
            "Learn Part 2 High note independently");
    press(ui,"Part 2 High note");
    auto* cancelled=find(ui,"soulTrainPicker",true);
    require(cancelled && cancelled->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Cancel MIDI Learn");
    playMidi({juce::MidiMessage::noteOn(2,72,uint8_t(90)),juce::MidiMessage::noteOff(2,72)});
    require(!find(ui,"soulTrainPicker",true) && readNote(1,"part_high")==66,"Ignore notes after dismissing the menu");
    flush(processor,100);
    capture(ui,processor,browser,output,"Perf",0,"-note-values");
    press(ui,"Part 1 Low note");
    capture(ui,processor,browser,output,"Perf",0,"-note-menu");
    auto* picker=find(ui,"soulTrainPicker",true);
    require(picker && picker->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Dismiss Performance notes");
    pump();
    require(processor.setPerformanceMode(false,error),error);
    flush(processor,224);
    require(!processor.performanceMode(),"Return to single Program mode for the drum grid checks");
    std::cout<<"PASS: KS control sizes, equal Prev and Next, manual notes and automatic per-part MIDI Learn\n";
}

void checkKnobAssignments(SoulTrainJuce& ui,Processor& processor,const juce::File& assets,const juce::File& output) {
    require(ui.showPage("Setup") && ui.showPage("Main"),"Open the assignable knobs with the loaded firmware");
    const bool k=processor.isKStation();
    const String model=k?"K":"KS";
    auto layout=juce::JSON::parse(assets.getChildFile(ui.currentScene()+".json"));
    auto* canvas=find(ui,"soulTrainJuce",true);
    require(canvas && layout["widgets"].isArray(),"Read the authored assignment choices");
    const auto select=[&](const String& group,const String& title,const String& id) {
        const auto* widgets=layout["widgets"].getArray();
        const auto authored=std::find_if(widgets->begin(),widgets->end(),[&](const var& widget){
            return widget["group"].toString()==group && widget["label"].toString()==title;
        });
        require(authored!=widgets->end(),"Find the WebUI assignment "+group+" "+title);
        const auto rect=(*authored)["rect"];
        const juce::Point<int> position{int(float(rect[0])+float(rect[2])*.5f),int(float(rect[1])+float(rect[3])*.5f)};
        for (auto* child:canvas->getChildren()) {
            auto* button=dynamic_cast<juce::Button*>(child);
            if (!button || !button->isVisible() || button->getName()!=title || !button->getBounds().contains(position)) continue;
            require(button->isEnabled() && button->onClick,"Find the active assignment choice "+group+" "+title);
            button->onClick();
            for (int attempt=0;attempt<20 && !find(ui,"1:"+id,true);++attempt) pump(.035);
            if (!find(ui,"1:"+id,true)) {
                std::cout<<"Assigned controls in "<<ui.currentScene()<<":";
                for (auto* control:canvas->getChildren()) {
                    const auto assigned=control->getComponentID();
                    if (assigned.startsWith("1:osc") || assigned.startsWith("1:filter") ||
                        assigned.startsWith("1:ring") || assigned.startsWith("1:external")) std::cout<<" "<<assigned;
                }
                std::cout<<'\n';
            }
            require(find(ui,"1:"+id,true),"Bind "+title+" to "+id);
            return;
        }
        require(false,"Missing assignment control "+title);
    };
    for (int oscillator=1;oscillator<=3;++oscillator) {
        const auto part="Oscillator "+String(oscillator);
        const auto id="osc"+String(oscillator)+"_";
        select(part+" PWM source","lfo2 pwm",id+"lfo2_pwm");
        select(part+" PWM source","pwm pos",id+"pulse_width");
        select(part+" PWM source","env pwm",id+"env_pwm");
        select(part+" knob mode","lfo1 pitch",id+"lfo1_pitch");
        select(part+" knob mode","pitch env",id+"env_pitch");
        const auto level=std::array<const char*,3>{"lfo1 level","lfo2 level","a/d env level"}[size_t(oscillator-1)];
        select(part+" knob mode",k?"pitch bend":level,id+(k?"bend_depth":"level_mod"));
    }
    if (!k) select("Filter knob mode","velocity","filter_velocity");
    select("Filter knob mode","lfo2","filter_lfo2");
    select("Filter knob mode","keytrack","filter_keytrack");
    const auto capture=[&](const String& area){
        const auto image=ui.createComponentSnapshot(ui.getLocalBounds());
        auto png=output.getChildFile(model+"-assigned-knobs-"+area+".png").createOutputStream();
        require(png && image.isValid() && juce::PNGImageFormat().writeImageToStream(image,*png),"Capture assigned knob captions");
    };
    capture("main");
    if (!k) {
        require(ui.showPage("Extra"),"Open the mixer input assignment");
        layout=juce::JSON::parse(assets.getChildFile(ui.currentScene()+".json"));
        select("Mixer knob mode","input","external_level");
        capture("mixer");
        select("Mixer knob mode","ring lfo1","ring_level_mod");
    }
    std::cout<<"PASS: "<<model<<" WebUI knob assignments and caption captures\n";
}

bool checkLfoControls(SoulTrainJuce& ui,Processor& processor,Browser& browser,const juce::File& assets,
                      const juce::File& output) {
    require(ui.showPage("Extra"),"Open the LFO controls in Extra");
    auto* canvas=find(ui,"soulTrainJuce",true);
    const auto layout=juce::JSON::parse(assets.getChildFile(ui.currentScene()+".json"));
    const auto* widgets=layout["widgets"].getArray();
    require(canvas && widgets,"Find the authored LFO controls");
    const auto model=processor.isKStation()?String("K-Station"):
        processor.performanceMode()?String("KS Performance part ")+String(processor.currentPart()+1):String("KS Program");
    const auto widget=[&](const String& id,const String& label){
        for (const auto& w:*widgets)
            if (w["param"].toString()==id && (label.isEmpty() || w["label"].toString()==label)) return w;
        throw std::runtime_error(("Missing LFO widget "+id+" "+label).toStdString());
    };
    const auto rect=[](const var& data){return juce::Rectangle<float>{float(data[0]),float(data[1]),float(data[2]),float(data[3])};};
    const auto pressWidget=[&](const var& w){
        const auto centre=rect(w["rect"]).getCentre().roundToInt();
        for (auto* child:canvas->getChildren()) {
            if (!child->isVisible() || !child->getBounds().contains(centre) || child->getName()!=w["label"].toString()) continue;
            auto* button=dynamic_cast<juce::Button*>(child);
            require(button && button->isEnabled() && button->onClick,"Find enabled "+w["label"].toString());
            button->onClick();
            return;
        }
        throw std::runtime_error(("Missing LFO button "+w["label"].toString()).toStdString());
    };
    const auto lampOn=[&](const var& w){
        const auto centre=ui.getLocalPoint(canvas,rect(w["lampArt"]).getCentre().roundToInt());
        const auto pixel=ui.createComponentSnapshot({centre.x,centre.y,1,1}).getPixelAt(0,0);
        return int(pixel.getRed())>int(pixel.getGreen())+45;
    };
    const auto program=[&]{
        juce::MemoryBlock data,raw;
        processor.getStateInformation(data);
        const auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
        require(xml!=nullptr,"Read the controller state for the LFO comparison");
        if (processor.isKStation()) {
            require(raw.fromBase64Encoding(xml->getStringAttribute("kCommandState")),"Read the K-Station controller archive");
            const auto bytes=static_cast<const uint8_t*>(raw.getData());
            auto state=novation::controller::command::decodeControllerArchive(std::vector<uint8_t>(bytes,bytes+raw.getSize()));
            require(state && state->activeProgram.size()==142,"Read the K-Station edit buffer");
            return std::move(state->activeProgram);
        }
        require(raw.fromBase64Encoding(xml->getStringAttribute("program")) &&
                raw.getSize()==(processor.performanceMode()?1038u:270u),"Read the KS edit buffer");
        const auto bytes=static_cast<const uint8_t*>(raw.getData());
        return std::vector<uint8_t>(bytes,bytes+raw.getSize());
    };
    const auto part=processor.currentPart();
    const auto readValue=[&](const std::vector<uint8_t>& data,const String& id,unsigned usingPart){
        const auto* spec=processor.spec(id);
        require(spec && spec->patchOffset>=0,"Find packed LFO parameter "+id);
        return int(ksPlugin::decode(*spec,data.data()+13+usingPart*256));
    };
    juce::StringArray ids{"external_trigger","external_to_fx"};
    for (const auto* lfo:{"lfo1_","lfo2_"}) {
        for (const auto* field:{"phase","keysync","delay_multi","lock"}) ids.add(String(lfo)+field);
        if (!processor.isKStation()) {
            ids.add(String(lfo)+"unipolar");
            ids.add(String(lfo)+"delay_sync");
        }
    }
    auto before=program();
    bool passed=true;
    const auto check=[&](const String& id,int expected,bool lamps){
        const auto after=program();
        auto* parameter=processor.uiParameter(id);
        require(parameter!=nullptr,"Find the live LFO parameter "+id);
        const auto live=juce::roundToInt(parameter->convertFrom0to1(parameter->getValue()));
        const auto applied=readValue(after,id,part);
        bool isolated=true;
        for (unsigned target=0;target<(processor.performanceMode()?4u:1u);++target)
            for (const auto& other:ids) if (other!=id || target!=part)
                isolated&=readValue(before,other,target)==readValue(after,other,target);
        if (applied!=expected || live!=expected || !lamps || !isolated) {
            passed=false;
            std::cout<<"FAIL: "<<model<<" "<<id<<" expected="<<expected<<" editor="<<live
                     <<" firmware="<<applied<<" lamps="<<lamps<<" isolated="<<isolated<<'\n';
        }
        before=after;
    };
    const auto toggle=[&](const String& id){
        const auto w=widget(id,{});
        const auto original=readValue(before,id,part);
        for (const int expected:{1-original,original}) {
            pressWidget(w);
            flush(processor,80);
            check(id,expected,lampOn(w)==(expected==1));
        }
    };
    const auto pair=[&](const String& id,const char* first,const char* second){
        const std::array<var,2> buttons{widget(id,first),widget(id,second)};
        const auto original=readValue(before,id,part);
        for (const int expected:{1-original,original}) {
            pressWidget(buttons[size_t(expected)]);
            flush(processor,80);
            check(id,expected,lampOn(buttons[0])==(expected==0) && lampOn(buttons[1])==(expected==1));
        }
    };
    for (const auto& id:ids) {
        if (id.endsWith("_delay_multi")) pair(id,"single","multi");
        else if (id.endsWith("_lock")) pair(id,"per voice","common");
        else if (id.endsWith("_delay_sync")) {
            const auto w=widget(id,{});
            for (const auto& [name,number]:{std::pair{"1/16",4},std::pair{"Off",0}}) {
                pressWidget(w);
                auto* picker=find(ui,"soulTrainPicker",true);
                require(picker,"Open the LFO Delay Sync menu");
                press(*picker,name);
                flush(processor,80);
                check(id,number,true);
            }
        } else toggle(id);
    }
    capture(ui,processor,browser,output,"Extra",0,processor.performanceMode()?"-lfo-performance":"-lfo");
    if (passed) std::cout<<"PASS: "<<model<<" LFO and Mixer input controls, lamps, packed state and independent parameters\n";
    return passed;
}

int run(char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File assets(argv[1]),output(argv[2]),k(argv[3]),ks(argv[4]),flash(argv[5]),wave(argv[6]);
    const juce::File data(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",""));
    require(data!=juce::File{} && data.createDirectory().wasOk(),"Set isolated SILVER_K_DATA");
    require(output.createDirectory().wasOk(),"Create capture output");
    if(juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_DRUM_COLD_START",{})=="1") {
        const auto roms=data.getChildFile("Silver-K/ROMs");
        require(!data.getChildFile("settings.json").exists() && roms.createDirectory().wasOk(),"Use a fresh startup profile");
        for(const auto& asset:{ks,flash,wave})
            require(asset.copyFileTo(roms.getChildFile(asset.getFileName())),"Install isolated KS startup fixtures");
        const auto start=juce::Time::getMillisecondCounterHiRes();
        Processor processor;
        processor.prepareToPlay(48000,256);
        processor.setNonRealtime(true);
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        auto* front=dynamic_cast<SoulTrainJuce*>(find(*editor,"soulTrainNative",true));
        require(front && front->currentScene().contains("-main-"),"Open Main while the detected firmware is starting");
        while((!processor.firmwareReady() || processor.isFirmwareLoading()) &&
              juce::Time::getMillisecondCounterHiRes()-start<15000)
            pump(.025);
        require(processor.firmwareReady() && !processor.isFirmwareLoading() &&
                !processor.isKStation(),"Automatically start the discovered KS firmware. "+processor.status());
        require(front->currentScene().contains("-main-"),"Stay on Main after firmware startup");
        std::cout<<"DISCOVERED_KS_READY_MS "<<juce::Time::getMillisecondCounterHiRes()-start<<'\n';
        checkDrumLoadTiming(processor);
        return 0;
    }
    if(juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_FIRMWARE_TABS",{})=="1"){
        Processor processor;
        processor.prepareToPlay(48000,256);
        processor.setNonRealtime(true);
        String error;
        require(processor.setFlashRom(flash,error),error);
        require(processor.setWaveRom(wave,error),error);
        checkFirmwareTabs(processor,k,ks,output);
        return 0;
    }
    const auto drumOnly=juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_DRUM_SEQ_TEST",{})=="1";
    const auto performanceOnly=juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_PERF_EDIT_TEST",{})=="1";
    const auto lfoOnly=juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_LFO_REVIEW_ONLY",{})=="1";
    const auto highlightOnly=juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_HIGHLIGHT_REVIEW_ONLY",{})=="1";
    const auto workerOnly=juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_DSP_SWITCH_REVIEW_ONLY",{})=="1";
    if(!drumOnly && !performanceOnly && !lfoOnly && !highlightOnly && !workerOnly)checkSkinSwitch();
    Processor processor;
    processor.prepareToPlay(48000,256);
    processor.setNonRealtime(true);
    Browser browser(processor);
    juce::DocumentWindow window("Silver-K native capture",juce::Colours::black,0);
    String setupAction;
    SoulTrainJuce ui(processor, { [&]{return browserData(browser);}, [&](const var& m){browser.webAction(m);},
        [&](const String& action){setupAction=action;}, []{return String{};} },assets);
    window.setContentNonOwned(&ui,false);
    window.setBounds(0,0,1486,942);
    window.setVisible(true);
    ui.setBounds(0,0,1486,942);
    String error;
    require(processor.setFlashRom(flash,error),error);
    require(processor.setWaveRom(wave,error),error);
    if (workerOnly) {
        require(processor.loadFirmware(ks,error),error);
        checkTwoCoreSwitches(ui,processor,browser,output,ks,flash,wave);
        window.clearContentComponent();
        return 0;
    }
    if (highlightOnly) {
        window.toFront(true);
        for (const auto& firmware:{k,ks}) {
            require(processor.loadFirmware(firmware,error),error);
            flush(processor);
            require(ui.showPage("Setup"),"Rebind the controls after loading firmware for button review");
            checkButtonHighlights(ui,processor,assets,output);
        }
        checkPerformanceLamp(ui,processor);
        window.clearContentComponent();
        return 0;
    }
    if (lfoOnly) {
        bool passed=true;
        for (const auto& firmware:{k,ks}) {
            require(processor.loadFirmware(firmware,error),error);
            flush(processor);
            checkKnobAssignments(ui,processor,assets,output);
            passed=checkLfoControls(ui,processor,browser,assets,output) && passed;
        }
        require(processor.setPerformanceMode(true,error),error);
        flush(processor,224);
        require(processor.performanceMode(),"Enter KS Performance for isolated LFO edits");
        processor.selectPart(1);
        pump();
        passed=checkLfoControls(ui,processor,browser,assets,output) && passed;
        window.clearContentComponent();
        return passed?0:1;
    }
    if(performanceOnly) {
        require(processor.loadFirmware(ks,error),error);
        flush(processor);
        checkPerformanceResponses(ui,processor,browser,output);
        checkPerformancePartEditing(ui,processor);
        window.clearContentComponent();
        return 0;
    }
    if(drumOnly) {
        const bool timingOnly=juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_DRUM_TIMING",{})=="1";
        require(processor.loadFirmware(k,error),error);
        flush(processor);
        require(ui.showPage("Main") && !find(ui,"drumSequencerEdit",true),"Keep the sequencer exclusive to KS");
        if (!timingOnly) checkControlSizes(ui,processor,browser,assets,output);
        require(processor.loadFirmware(ks,error),error);
        flush(processor);
        if(timingOnly)
            checkDrumLoadTiming(processor);
        else {
            checkControlSizes(ui,processor,browser,assets,output);
            checkDrumSequencer(ui,processor,output);
        }
        window.clearContentComponent();
        return 0;
    }
    static const juce::StringArray effects{"Delay","Reverb","Chorus","Dist","EQ","Panning","Vocoder"};
    const auto browserOnly=juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_BROWSER_REVIEW_ONLY",{})=="1";
    const auto quick=browserOnly || juce::SystemStats::getEnvironmentVariable("SILVER_NATIVE_UI_FAST",{})=="1";
    if (quick) installSyntheticLibrary(processor,browser,data);
    for (const auto& firmware:{k,ks}) {
        require(processor.loadFirmware(firmware,error),error);
        flush(processor);
        if (browserOnly) {
            checkBrowserSort(ui,processor,browser,output);
            checkBrowserControls(ui,processor,browser,output);
            continue;
        }
        checkEnvelopeFader(ui,processor);
        checkMenuBackdrop(ui);
        checkFirmwareOptions(ui,processor,output);
        require(ui.showPage("Setup"),"Open combined folder Setup");
        press(ui,String::fromUTF8("Choose Silver-K folder…"));
        require(setupAction=="dataFolder","Folder button did not request the root chooser");
        press(ui,"Rescan folders");
        require(setupAction=="rescanData","Rescan button did not request a complete scan");
        for(const auto* removed:{"Load firmware…","Wave ROM…","KS Data…","Load skin…","Demo song"})
            require(!find(ui,String::fromUTF8(removed),false),"Old individual file chooser in Setup");
        capture(ui,processor,browser,output,"Setup",0,"-data-folder");
        if (quick) checkBrowserSort(ui,processor,browser,output);
        if (quick && firmware==k) continue;
        if (quick) {
            checkHeldKnobReadout(ui,processor);
            checkRecallBrowser(ui,processor,browser,k,ks);
        }
        if (!processor.isKStation()) {
            require(processor.setPerformanceMode(true,error),error);
            flush(processor,224);
            require(processor.performanceMode() && processor.activeParts()==15,"Stable four-part Performance");
            const auto names=processor.partNames();
            flush(processor,180);
            require(names==processor.partNames(),"Performance continued changing before the picker was used");
        }
        if (quick) {
            require(ui.showPage("Perf"),"Open populated Performance");
            checkPartPopup(ui,processor,browser,1,16);
            checkPartPopup(ui,processor,browser,2,29);
            checkPartPopup(ui,processor,browser,3,38);
            checkPartPopup(ui,processor,browser,4,0);
            const auto programs=browser.filteredPrograms();
            const auto previous=processor.partNames();
            require(browser.activateProgram(0,programs[4],error),error);
            require(browser.activateProgram(1,programs[9],error),error);
            flush(processor,320);
            const auto loaded=processor.partNames();
            require(loaded[0]==patchName(*programs[4]) && loaded[1]==patchName(*programs[9]) &&
                    loaded[2]==previous[2] && loaded[3]==previous[3],
                    "Two preset loads queued before one render must update both parts independently");
            unsigned accepted=0;
            for(int click=0;click<48;++click)if(browser.stepProgram(0,1,error))++accepted;
            require(accepted>0 && accepted<48,"Desktop preset requests use bounded admission");
            flush(processor,400);
            require(browser.stepProgram(0,-1,error),error);
            flush(processor,160);
            require(processor.ksPresetLoadStatus()==Processor::DrumLoadStatus::Applied && processor.partNames()[1]==loaded[1],
                    "Rapid navigation must leave the engine usable and preserve the other part");
            std::cout<<"PASS: two queued Browser presets update only their respective parts\n";
            const auto selectedName=[&] {
                const auto data=browserData(browser);
                for(const auto& row:*data["rows"].getArray())
                    if(bool(row["selected"]))return row["name"].toString();
                return String{};
            };
            processor.selectPart(1);
            require(browser.stepProgram(1,1,error),error);
            flush(processor,160);
            require(selectedName()==processor.partNames()[1],"Browser selection follows the Part 2 arrow");
            processor.selectPart(0);
            require(selectedName()==processor.partNames()[0],"Browser selection follows the selected part without reloading sounds");
            std::cout<<"PASS: Browser selection follows preset arrows on the selected Performance part\n";
            capture(ui,processor,browser,output,"Perf",0,"-presets");
            checkPerformanceLamp(ui,processor);
            const juce::ScopedValueSetter<String> useNativeSkin(processor.skinChoice,Processor::defaultSkin);
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            auto* status=dynamic_cast<juce::Label*>(find(*editor,"setupStatus",true,false));
            require(status && find(*editor,"soulTrainJuce",true),"Find the native editor and its underlying status label");
            status->setText("Loading firmware...",juce::dontSendNotification);
            require(processor.beginFirmwareLoad(k),"Start an asynchronous KS to K firmware handoff");
            const auto deadline=juce::Time::getMillisecondCounterHiRes()+8000.;
            while((processor.isFirmwareLoading() || status->getText()=="Loading firmware...") &&
                  juce::Time::getMillisecondCounterHiRes()<deadline)pump(.03);
            require(processor.isKStation() && !processor.isFirmwareLoading() && status->getText()==processor.status(),
                    "Hidden native controls must still publish the finished firmware status to SoulTrain");
            std::cout<<"PASS: SoulTrain Setup receives the asynchronous firmware result\n";
            break;
        }
        for (const String& page:juce::StringArray{"Main","Extra","Browser","Setup","Perf"}) {
            if (page=="Perf" && processor.isKStation()) continue;
            require(ui.showPage(page),"Navigate to "+page);
            const auto withEffects=page=="Main" || page=="Extra";
            for (int i=0;i<(withEffects?effects.size():1);++i) {
                if (withEffects) press(ui,effects[i]);
                capture(ui,processor,browser,output,page,i);
            }
        }
        require(ui.showPage("Main"),"Open Main automation check");
        auto* knob=dynamic_cast<juce::Slider*>(find(ui,"1:cutoff",true));
        auto* cutoff=processor.parameters.getParameter(processor.isKStation()?"k_cutoff":"cutoff");
        require(knob && cutoff,"Find native cutoff");
        cutoff->setValueNotifyingHost(cutoff->convertTo0to1(73));
        pump();
        require(knob->getValue()==73,"Host automation reached the native knob");
        knob->setValue(51,juce::sendNotificationSync);
        require(juce::roundToInt(cutoff->convertFrom0to1(cutoff->getValue()))==51,"Native knob updated the automatable parameter");
        press(ui,"Delay");
        press(ui,"Voices options");
        capture(ui,processor,browser,output,"Main",0,"-voices",{"Voices options"});
        press(ui,"Voices options");
        if (!processor.isKStation()) {
            press(ui,"Arpeggiator options");
            capture(ui,processor,browser,output,"Main",0,"-arp",{"Arpeggiator options"});
            press(ui,"Arpeggiator options");
            installSyntheticLibrary(processor,browser,data);
            require(ui.showPage("Perf"),"Open populated Performance");
            checkPartPopup(ui,processor,browser,2,29);
            checkPartPopup(ui,processor,browser,4,0);
            capture(ui,processor,browser,output,"Perf",0,"-presets");
            require(ui.showPage("Browser"),"Open populated Browser");
            capture(ui,processor,browser,output,"Browser",0,"-presets");
            checkPerformanceLamp(ui,processor);
        }
    }
    window.clearContentComponent();
    std::cout<<"PASS: native skin switching, sliders, automation and per-part presets\n";
    return 0;
}
}

int main(int argc,char** argv) {
    if (argc!=7) {
        std::cerr<<"Pass the skin assets, capture folder, K firmware, KS firmware, KS Data, and waves.\n";
        return 2;
    }
    try { return run(argv); }
    catch (const std::exception& error) {
        std::cerr<<"FAIL: "<<error.what()<<'\n';
        return 1;
    }
}
