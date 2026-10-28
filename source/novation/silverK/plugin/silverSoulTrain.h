#pragma once
#include "silverProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <cmath>
namespace silverK {
class SoulTrainWeb final : public juce::Component, private juce::Timer {
public:
 struct Callbacks {std::function<juce::var()> browserSnapshot;std::function<void(const juce::var&)> browserAction;std::function<void(const juce::String&)> nativeAction;std::function<juce::String()> nativeStatus;};
 SoulTrainWeb(Processor&,Callbacks,juce::File resourceRoot={});
 ~SoulTrainWeb()override;
 void resized()override;
 void receive(const juce::var&);
 void loaded();
 void evaluate(const juce::String&,std::function<void(juce::var)> completion = {});
private:
 struct Impl;std::unique_ptr<Impl> impl;
 Processor& processor;Callbacks callbacks;
 bool ready=false,includeLibrary=false;int tick=0;
 juce::String lastSnapshot,lastLibrary,error;
 std::map<juce::String,juce::RangedAudioParameter*> gestures;
 juce::RangedAudioParameter* parameter(const juce::String&,int);
 juce::String key(const juce::String&,int)const;
 juce::var snapshot(bool);
 void timerCallback()override;
};
}
