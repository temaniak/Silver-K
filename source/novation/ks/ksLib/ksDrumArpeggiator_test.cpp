#include "command/ksDrumArpeggiator.h"

#include <iostream>
#include <stdexcept>
#include <vector>

using namespace novation::controller::command;

namespace {
constexpr KSDrumEvent pattern[]{
    {0, 0, KSDrumEventKind::NoteOn, 36, 100},
    {8, 0, KSDrumEventKind::NoteOff, 36, 0},
};

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

KSDrumSettings drumSettings(bool latch = false, bool keySync = false) {
    return {{pattern, 2, 16}, 0, true, keySync, latch, true, true};
}

MusicalClockSignal tick(uint64_t frame, uint64_t pulse, ClockAuthority source = ClockAuthority::Internal) {
    return {{frame}, pulse, 0, source, 16, true, true};
}

void expectSilent(KSDrumArpeggiator& drum, MusicalClockSignal signal) {
    check(drum.clock(signal) && !drum.nextDeadline() && !drum.pop(), "Clock alone must not start a drum pattern");
}

ArpNote noteOn(KSDrumArpeggiator& drum, MusicalClockSignal signal) {
    check(drum.clock(signal) && drum.takeDue(signal.at), "A triggered pattern must advance on the clock");
    const auto event = drum.pop();
    check(event && event->note.on && event->note.note == 36 && event->note.at == signal.at &&
          event->note.internal && event->note.midiOut, "First step must play its pattern note on both routes");
    return event->note;
}

void expectRelease(KSDrumArpeggiator& drum, ArpNote started, uint64_t frame) {
    const auto event = drum.pop();
    check(event && event->forcedOff && !event->note.on && event->note.generation == started.generation &&
          event->note.at == NativeFrame{frame} && !drum.nextDeadline(), "Stop must retire the sounding generation");
}

void sharedGridChecks() {
    KSUserDrumPattern grid;
    grid.steps = 5;
    for (uint8_t step = 0; step < grid.steps; ++step)
        grid.velocities[step] = uint8_t(70 + step);
    for (const auto source : {ClockAuthority::Internal, ClockAuthority::Host, ClockAuthority::Midi}) {
        KSDrumArpeggiator drum;
        auto kit = drumSettings();
        kit.sequence = drum.userSequence(grid);
        NativeArpeggiator bass;
        ArpSettings bassSettings;
        bassSettings.enabled = true;
        check(drum.configure({}, kit) && bass.configure({}, bassSettings), "Configure Performance layers");
        check(drum.press({33}, 36) && bass.press({33}, 60, 100), "Trigger both layers between sixteenths");
        unsigned steps = 0;
        for (uint64_t pulse = 3; pulse <= 42; ++pulse) {
            const auto signal = tick(pulse * 16, pulse, source);
            while (const auto due = drum.nextDeadline()) {
                if (!(*due < signal.at)) break;
                check(drum.takeDue(*due), "Drain drum gates before the next pulse");
                drum.pop();
            }
            while (const auto due = bass.nextDeadline()) {
                if (!(*due < signal.at)) break;
                check(bass.takeDue(*due), "Drain bass gates before the next pulse");
                bass.pop();
            }
            check(drum.clock(signal) && bass.clock(signal), "Feed both layers the same pulse");
            std::optional<ArpNote> drumHit, bassHit;
            while (const auto due = drum.nextDeadline()) {
                if (signal.at < *due) break;
                check(drum.takeDue(signal.at), "Advance drum step");
                if (const auto event = drum.pop(); event && event->note.on)
                    drumHit = event->note;
            }
            while (const auto* note = bass.front()) {
                if (note->on) bassHit = *note;
                bass.pop();
            }
            check(bool(drumHit) == bool(bassHit), "SEQ and the bass must start on the same sixteenth");
            if (drumHit) {
                check(drumHit->at == bassHit->at && drumHit->velocity == 70 + steps % grid.steps,
                      "Keep the first step and the phase of an odd-length sequence");
                ++steps;
            }
        }
        check(steps == 7, "Sound both layers across the SEQ loop");
    }
}

std::vector<ArpNote> collectNotes(KSDrumArpeggiator& drum,uint64_t firstPulse,uint64_t lastPulse,
                                 std::vector<ArpNote>* releases=nullptr) {
    std::vector<ArpNote> notes;
    for (auto pulse=firstPulse;pulse<=lastPulse;++pulse) {
        const auto frame=pulse*16;
        check(drum.clock(tick(frame,pulse,ClockAuthority::Host)), "Advance user-pattern clock");
        while (drum.nextDeadline() && drum.nextDeadline()->ticks<frame+16) {
            check(drum.takeDue({frame+15}), "Dispatch user-pattern deadline");
            while (const auto event=drum.pop()) {
                if (event->note.on) notes.push_back(event->note);
                else if (releases && !event->pressure) releases->push_back(event->note);
            }
        }
    }
    return notes;
}

void userPatternChecks() {
    KSUserDrumPatterns grids;
    auto& bank=grids[2];
    auto& grid=bank.patterns[0];
    bank.apply({KSDrumGridEdit::Kind::SelectUser,2,0,0,1});
    bank.apply({KSDrumGridEdit::Kind::Velocity,2,0,0,72});
    bank.apply({KSDrumGridEdit::Kind::Velocity,2,0,1,42});
    bank.apply({KSDrumGridEdit::Kind::AssignedKeys,2,0,0,1});
    check(bank.patternForKey(0)==0 && bank.patternForKey(127)==0 && bank.assignedNotes().all(),
          "A single unassigned sequence accepts the entire keyboard, including in Assigned Keys mode");
    bank.apply({KSDrumGridEdit::Kind::KeyLow,2,0,0,36});
    bank.apply({KSDrumGridEdit::Kind::KeyHigh,2,0,0,50});
    bank.apply({KSDrumGridEdit::Kind::KeyLow,2,1,0,62});
    bank.apply({KSDrumGridEdit::Kind::KeyLow,2,11,0,82});
    bank.apply({KSDrumGridEdit::Kind::Length,2,11,0,5});
    bank.apply({KSDrumGridEdit::Kind::Velocity,2,11,159,67});
    bank.apply({KSDrumGridEdit::Kind::Probability,2,11,159,55});
    bank.apply({KSDrumGridEdit::Kind::Ratchet,2,11,159,16});
    bank.apply({KSDrumGridEdit::Kind::SwitchAfter,2,0,0,KSDrumGridEdit::SwitchAtEnd});
    check(bank.patternForKey(36)==0 && bank.patternForKey(37)==0 && bank.patternForKey(50)==0 &&
          !bank.patternForKey(51) && !bank.patternForKey(61) && bank.patternForKey(62)==1 &&
          !bank.patternForKey(63) && bank.patternForKey(82)==11 && !bank.patternForKey(24),
          "Match inclusive ranges, disjoint ranges and a single key");
    bank.apply({KSDrumGridEdit::Kind::KeyHigh,2,2,0,45});
    check(bank.patternForKey(45)==0,"The first explicitly mapped sequence wins an overlap");
    bank.apply({KSDrumGridEdit::Kind::KeyLow,2,2,0,KSDrumGridEdit::Unassigned});
    check(!bank.keyRanges[2].assigned(),"Clear both endpoints in one edit");
    const KSDrumGridEdit selectLast{KSDrumGridEdit::Kind::SelectPattern,2,0,0,11};
    check(selectLast.valid(),"Accept manual selection of sequence 12");
    bank.apply(selectLast);
    check(bank.patternForKey(36)==0 && bank.patternForKey(62)==1,"Keep range routing while Assigned Keys is on");
    bank.apply({KSDrumGridEdit::Kind::AssignedKeys,2,0,0,0});
    check(bank.patternForKey(0)==11 && bank.patternForKey(36)==11 &&
          bank.patternForKey(62)==11 && bank.patternForKey(127)==11,
          "Use the selected sequence on every key while Assigned Keys is off");
    bank.apply({KSDrumGridEdit::Kind::AssignedKeys,2,0,0,1});
    check(bank.patternForKey(36)==0 && bank.patternForKey(62)==1 && bank.patternForKey(82)==11 &&
          !bank.patternForKey(127),"Reapply the same ranges when Assigned Keys is turned on");
    const auto versionFive=encodeKSUserDrums(grids);
    check(versionFive.front()==5 && decodeKSUserDrums(versionFive.data(),versionFive.size())[2].retrigger,
          "Keep Key Sync retriggering for existing grids");
    const KSDrumGridEdit disableRetrigger{KSDrumGridEdit::Kind::Retrigger,2,0,0,0};
    check(disableRetrigger.valid(),"Accept a per-part Retrigger edit");
    bank.apply(disableRetrigger);
    check(encodeKSUserDrums(grids,false)==versionFive,"Preserve the legacy drum grid when saving project state");
    const auto bytes=encodeKSUserDrums(grids);
    const auto restored=decodeKSUserDrums(bytes.data(),bytes.size());
    check(restored[2].selected && !restored[1].selected && restored[2].keyRanges==bank.keyRanges &&
          bytes.front()==6 && !restored[2].retrigger && restored[1].retrigger &&
          restored[2].assignedKeysOnly && restored[2].switchAfter==KSDrumGridEdit::SwitchAtEnd &&
          restored[2].patterns[0].velocities==grid.velocities &&
          restored[2].patterns[11].steps==5 && restored[2].patterns[11].velocities[159]==67 &&
          restored[2].patterns[11].probabilities[159]==55 && restored[2].patterns[11].ratchets[159]==16,
          "Round-trip Retrigger, ranges, timing, lengths and all three step parameters");
    auto malformed=bytes;
    constexpr size_t partBytes=(KSUserDrumBytes{}.size()-1)/KSDrumGridEdit::Slots;
    malformed[1+2*partBytes+3]=30;
    check(!decodeKSUserDrums(malformed.data(),malformed.size())[2].selected,
          "Reject inverted trigger endpoints");
    malformed=bytes;
    malformed[1+2*partBytes+1]=KSDrumGridEdit::SwitchAtEnd+1;
    check(!decodeKSUserDrums(malformed.data(),malformed.size())[2].selected,"Reject an invalid switch step");
    malformed=bytes;
    constexpr size_t oldPatternBytes=2+KSDrumGridEdit::Rows+KSDrumGridEdit::Cells;
    malformed[4+oldPatternBytes]=101;
    check(!decodeKSUserDrums(malformed.data(),malformed.size())[2].selected,"Reject an invalid probability");
    malformed=bytes;
    malformed[4+oldPatternBytes+KSDrumGridEdit::Cells]=0;
    check(!decodeKSUserDrums(malformed.data(),malformed.size())[2].selected,"Reject an invalid ratchet");

    constexpr size_t v4PartBytes=1+KSDrumGridEdit::Patterns*(oldPatternBytes+2*KSDrumGridEdit::Cells);
    std::array<uint8_t,1+KSDrumGridEdit::Slots*v4PartBytes> versionFour{};
    versionFour[0]=4;
    auto writer=versionFour.begin()+1;
    for (const auto& part:grids) {
        *writer++=uint8_t(part.selected | (part.assignedKeysOnly << 1));
        for (size_t i=0;i<part.patterns.size();++i) {
            const auto& pattern=part.patterns[i];
            *writer++=uint8_t(i==11?82:36+i);
            *writer++=pattern.steps;
            writer=std::copy(pattern.notes.begin(),pattern.notes.end(),writer);
            writer=std::copy(pattern.velocities.begin(),pattern.velocities.end(),writer);
            writer=std::copy(pattern.probabilities.begin(),pattern.probabilities.end(),writer);
            writer=std::copy(pattern.ratchets.begin(),pattern.ratchets.end(),writer);
        }
    }
    const auto migratedV4=decodeKSUserDrums(versionFour.data(),versionFour.size());
    check(migratedV4[2].selected && migratedV4[2].assignedKeysOnly && migratedV4[2].switchAfter==0 &&
          migratedV4[2].keyRanges[0]==KSDrumKeyRange{36,36} && migratedV4[2].keyRanges[11]==KSDrumKeyRange{82,82},
          "Preserve version-four single-note mappings and immediate switching");
    versionFour[1+2*v4PartBytes]|=4;
    check(decodeKSUserDrums(versionFour.data(),versionFour.size())[2].switchAfter==KSDrumGridEdit::DefaultSwitchStep,
          "Migrate the old synced setting to the new default step");

    auto versionThree=versionFour;
    versionThree[0]=3;
    for(size_t part=0;part<grids.size();++part)versionThree[1+part*v4PartBytes]&=1;
    const auto migratedV3=decodeKSUserDrums(versionThree.data(),versionThree.size());
    check(migratedV3[2].selected && !migratedV3[2].assignedKeysOnly &&
          migratedV3[2].switchAfter==KSDrumGridEdit::DefaultSwitchStep &&
          migratedV3[2].keyRanges==migratedV4[2].keyRanges &&
          migratedV3[2].patterns[11].probabilities[159]==55 && migratedV3[2].patterns[11].ratchets[159]==16,
          "Keep version-three sequences, keys, probability and ratchets");

    std::array<uint8_t,1+KSDrumGridEdit::Slots*(1+KSDrumGridEdit::Patterns*oldPatternBytes)> versionTwo{};
    versionTwo[0]=2;
    auto fromV3=versionThree.begin()+1;
    auto to=versionTwo.begin()+1;
    for (const auto& part:grids) {
        *to++=*fromV3++;
        for (size_t i=0;i<part.patterns.size();++i) {
            to=std::copy_n(fromV3,oldPatternBytes,to);
            fromV3+=oldPatternBytes+2*KSDrumGridEdit::Cells;
        }
    }
    const auto migratedV2=decodeKSUserDrums(versionTwo.data(),versionTwo.size());
    check(migratedV2[2].selected && migratedV2[2].keyRanges[11]==KSDrumKeyRange{82,82} &&
          migratedV2[2].patterns[11].velocities[159]==67 && migratedV2[2].patterns[11].steps==5 &&
          migratedV2[2].patterns[11].probabilities[159]==100 && migratedV2[2].patterns[11].ratchets[159]==1,
          "Keep version-two sequence data with retrigger and probability defaults");
    bank.apply({KSDrumGridEdit::Kind::Remove,2,11,159,0});
    check(!bank.patterns[11].velocities[159] && bank.patterns[11].probabilities[159]==100 &&
          bank.patterns[11].ratchets[159]==1, "Removing a hit clears its settings");

    std::array<uint8_t,1+5*(1+8*(1+16))> legacy{};
    legacy[0]=1;
    for (size_t i=0;i<5;++i) {
        auto* part=legacy.data()+1+i*(1+8*(1+16));
        part[0]=i==2;
        std::copy_n(grid.notes.begin(),8,part+1);
        part[9]=88;
        part[9+127]=57;
    }
    const auto migrated=decodeKSUserDrums(legacy.data(),legacy.size());
    check(migrated[2].selected && migrated[2].patternForKey(127)==0 &&
          migrated[2].patterns[0].steps==16 && migrated[2].patterns[0].velocities[0]==88 &&
          migrated[2].patterns[0].velocities[127]==57 && !migrated[2].patterns[0].velocities[128] &&
          !migrated[2].patterns[1].velocities[0],"Migrate the old grid to Sequence 1 of each part");

    KSDrumArpeggiator drum;
    auto settings=drumSettings();
    settings.sequence=drum.userSequence(grid);
    check(drum.configure({},settings) && drum.press({},60),"Enable programmed grid");
    const auto first=collectNotes(drum,0,0);
    check(first.size()==1 && first[0].note==36 && first[0].velocity==72,"Play the first programmed hit");
    bank.apply({KSDrumGridEdit::Kind::Note,2,0,0,44});
    bank.apply({KSDrumGridEdit::Kind::Velocity,2,0,0,98});
    settings.sequence=drum.userSequence(grid);
    check(drum.configure({16},settings,true),"Queue a grid edit");
    bank.apply({KSDrumGridEdit::Kind::Velocity,2,0,0,115});
    settings.sequence=drum.userSequence(grid);
    check(drum.configure({17},settings,true),"Replace the queued grid edit");
    const auto next=collectNotes(drum,1,96);
    check(next.size()==2 && next[0].note==36 && next[0].velocity==42 && next[0].at.ticks==96 &&
          next[1].note==44 && next[1].velocity==115 && next[1].at.ticks==1536,
          "Use the old step before the seam and the latest grid at the next bar");
    settings=drumSettings();
    check(drum.configure({1552},settings,true),"Queue the factory rhythm");
    const auto returned=collectNotes(drum,97,192);
    check(returned.size()==2 && returned.back().at.ticks==3072 &&
          returned.back().note==36 && returned.back().velocity==100,"Return to the factory rhythm at the seam");

    KSDrumArpeggiator cancelled;
    check(cancelled.configure({},drumSettings()) && cancelled.press({},60),"Trigger the factory rhythm");
    auto initial=noteOn(cancelled,tick(0,0));
    settings.sequence=cancelled.userSequence(grid);
    check(cancelled.configure({5},settings,true) && cancelled.configure({6},drumSettings()),
          "Cancel a pending User switch");
    check(cancelled.takeDue({8}),"Keep the factory gate deadline after cancellation");
    const auto release=cancelled.pop();
    check(release && !release->forcedOff && release->note.generation==initial.generation,
          "Cancelling a switch must leave the sounding note intact");

    KSDrumArpeggiator silent;
    grid.velocities.fill(0);
    settings.sequence=silent.userSequence(grid);
    check(silent.configure({},settings) && silent.press({},60) && collectNotes(silent,0,10).empty(),
          "Triggering an empty grid is silent");
    grid.velocities[0]=100;
    settings.sequence=silent.userSequence(grid);
    check(silent.configure({176},settings,true) && collectNotes(silent,11,96).size()==1,
          "A running empty grid accepts hits at its next seam");
    grid.velocities.fill(127);
    KSUserDrumSequence dense;
    dense.compile(grid);
    check(dense.count==2*KSDrumGridEdit::Cells,"One trigger and gate per cell by default");
    grid.ratchets.fill(KSDrumGridEdit::MaxRatchet);
    dense.compile(grid);
    check(dense.count==dense.events.size() && dense.events.back().tick<dense.sequence().length,
          "A full grid at the maximum ratchet fits bounded playback storage");
}

void ratchetChecks() {
    KSUserDrumPattern grid;
    grid.steps=1;
    grid.velocities[0]=63;
    grid.probabilities[0]=0;
    grid.ratchets[0]=16;
    grid.velocities[KSDrumGridEdit::Steps]=72;
    grid.ratchets[KSDrumGridEdit::Steps]=3;
    grid.velocities[2*KSDrumGridEdit::Steps]=100;
    grid.probabilities[2*KSDrumGridEdit::Steps]=50;
    grid.ratchets[2*KSDrumGridEdit::Steps]=16;
    KSDrumArpeggiator drum;
    auto settings=drumSettings();
    settings.sequence=drum.userSequence(grid);
    check(drum.configure({},settings) && drum.press({},36),"Start the probability and ratchet test grid");
    constexpr size_t repeats=24;
    constexpr auto step=KSUserDrumSequence::StepTicks;
    std::vector<ArpNote> releases;
    const auto hits=collectNotes(drum,0,repeats*step/16-1,&releases);
    std::array<size_t,repeats> always{}, sometimes{};
    for(const auto& note:hits) {
        const auto loop=note.at.ticks/step;
        const auto offset=note.at.ticks%step;
        check(note.note!=36 && loop<repeats,"Zero-percent cells never play, including their ratchets");
        if(note.note==38) {
            check(offset==always[loop]*32 && note.velocity==72,"Three equally spaced hits retain velocity");
            ++always[loop];
        }else{
            check(note.note==42 && offset==sometimes[loop]*6 && note.velocity==100,
                  "Sixteen equally spaced hits retain velocity");
            ++sometimes[loop];
        }
        const auto gate=std::find_if(releases.begin(),releases.end(),[&](const auto& release) {
            return release.generation==note.generation;
        });
        check(gate!=releases.end() && gate->at.ticks==note.at.ticks+(note.note==38?24:4),
              "Each played ratchet has a matching gate before the next repeat");
    }
    check(hits.size()==releases.size(),"Skipped cells generate no unmatched note offs");
    check(std::all_of(always.begin(),always.end(),[](auto count){return count==3;}),
          "Full probability plays every loop");
    const auto skipped=std::count(sometimes.begin(),sometimes.end(),0);
    check(skipped>0 && skipped<repeats &&
          std::all_of(sometimes.begin(),sometimes.end(),[](auto count){return count==0 || count==16;}),
          "Intermediate probability decides once for each full ratcheted step on every loop");
}

void keyedPatternChecks() {
    KSUserDrumPattern four, five;
    four.steps=4;
    four.velocities[0]=72;
    four.velocities[4]=100;
    five.steps=5;
    five.notes[9]=51;
    five.velocities[9*16]=48;
    KSDrumArpeggiator drum;
    auto settings=drumSettings(false,true);
    settings.retriggerUserSequence=false;
    settings.sequence=drum.userSequence(four,1);
    check(drum.configure({},settings) && drum.press({},36),"Start the four-step sequence");
    check(drum.playhead()==NativeDrumPlayhead{},"The playhead starts on a clock pulse");
    auto notes=collectNotes(drum,3,28);
    check(notes.size()==2 && notes[0].at.ticks==48 && notes[1].at.ticks==432,
          "Loop an off-beat start every four steps and ignore a hit outside the length");
    check(drum.playhead()==NativeDrumPlayhead{1,0},"Track the sounding sequence through its loop");
    check(drum.press({449},82,drum.userSequence(five,7),KSDrumGridEdit::SwitchAtEnd) &&
          drum.releaseKey({450},36) && drum.press({451},82),
          "Queue the second sequence while the first is sounding");
    check(collectNotes(drum,29,50).empty() && drum.playhead()==NativeDrumPlayhead{1,3},
          "The playhead tracks empty columns of the sounding sequence while a switch waits");
    notes=collectNotes(drum,51,81);
    check(notes.size()==2 && notes[0].note==51 && notes[0].velocity==48 && notes[0].at.ticks==816 &&
          notes[1].note==51 && notes[1].at.ticks==1296,
          "Switch at the end of the sounding sequence and loop the new one every five steps");
    check(drum.playhead()==NativeDrumPlayhead{7,0},"Show the new sequence once its switch takes effect");
    check(drum.releaseKey({1297},82),"Release the last trigger key");
    while(drum.pop()) {}
    expectSilent(drum,tick(1312,82,ClockAuthority::Host));
    check(drum.playhead()==NativeDrumPlayhead{},"Clear the playhead when playback stops");

    KSDrumArpeggiator immediate;
    settings.sequence=immediate.userSequence(four);
    check(immediate.configure({},settings) && immediate.press({},36),"Start the unsynced switch test");
    notes=collectNotes(immediate,0,3);
    check(notes.size()==1 && immediate.clock(tick(64,4,ClockAuthority::Host)) &&
          immediate.press({65},82,immediate.userSequence(five),false),
          "An unsynced key can change the running sequence");
    expectRelease(immediate,notes[0],65);
    notes=collectNotes(immediate,5,5);
    check(notes.size()==1 && notes[0].note==51 && notes[0].at.ticks==80,
          "Start on the next pulse with immediate switching");

    KSDrumArpeggiator restricted;
    check(restricted.configure({},drumSettings()) && restricted.press({},24) && restricted.press({},36),
          "Both notes started holding the sequence before restriction");
    const auto held=noteOn(restricted,tick(0,0));
    KSUserDrumBank assigned;
    assigned.keyRanges[0]={36,40};
    check(restricted.keepAssignedKeys({1},assigned.assignedNotes()) &&
          restricted.releaseKey({2},36),"Retire the unassigned held key when restriction is enabled");
    expectRelease(restricted,held,2);
    expectSilent(restricted,tick(16,1));

    const auto checkBoundary=[&](uint8_t length,uint8_t after,uint64_t request,uint64_t expected) {
        KSDrumArpeggiator player;
        auto first=four;
        first.steps=length;
        first.velocities[4]=0;
        settings.sequence=player.userSequence(first,1);
        check(player.configure({},settings) && player.press({},36),"Start an off-beat test sequence");
        collectNotes(player,3,request);
        check(player.press({request*16+15},82,player.userSequence(five,7),after),"Queue a step-boundary switch");
        const auto waiting=collectNotes(player,request+1,expected-1);
        check(std::none_of(waiting.begin(),waiting.end(),[](const auto& hit){return hit.note==51;}),
              "Keep the outgoing sequence until the chosen boundary");
        const auto switched=collectNotes(player,expected,expected);
        check(switched.size()==1 && switched[0].note==51 && switched[0].at.ticks==expected*16 &&
              player.playhead()==NativeDrumPlayhead{7,0},"Start the incoming sequence on step one at the boundary");
    };
    checkBoundary(16,1,4,9);
    checkBoundary(16,8,4,51);
    checkBoundary(16,8,54,147);
    checkBoundary(5,8,4,33);
    checkBoundary(7,KSDrumGridEdit::SwitchAtEnd,4,45);
    checkBoundary(16,16,4,99);
}

void retriggerChecks() {
    KSUserDrumPattern sequence;
    sequence.steps=4;
    sequence.velocities[0]=100;
    for (const bool latch : {false,true}) {
        KSDrumArpeggiator drum;
        auto settings=drumSettings(latch,true);
        settings.sequence=drum.userSequence(sequence,1);
        check(drum.configure({},settings) && drum.press({33},36),"Trigger the user sequence");
        auto notes=collectNotes(drum,3,9);
        check(notes.size()==1 && notes[0].at.ticks==48 && drum.playhead()==NativeDrumPlayhead{1,1},
              "Start on the next pulse with Key Sync and reach the second step");

        drum.setUserSequenceRetrigger(false);
        if (latch) check(drum.releaseKey({145},36),"Release the trigger before playing a detached note");
        check(drum.press({147},38) && drum.playhead()==NativeDrumPlayhead{1,1},
              "A second key for the same sequence must preserve the playhead");
        check(drum.releaseKey({148},latch?38:36),"Keep sounding through overlap or a latched gap");
        notes=collectNotes(drum,10,27);
        check(notes.size()==1 && notes[0].at.ticks==432,
              "Keep the original loop boundary with Retrigger off and Key Sync on");

        if (latch) drum.setUserSequenceRetrigger(true);
        else {
            check(drum.releaseKey({433},38),"Stop when the last key is released without Latch");
            while(drum.pop()) {}
        }
        check(drum.press({434},40) && drum.playhead()==NativeDrumPlayhead{},
              "Start from step one after a stop or when Retrigger is reenabled");
        while(drum.pop()) {}
        notes=collectNotes(drum,28,28);
        check(notes.size()==1 && notes[0].at.ticks==448,"Hear the restarted first step");
    }
}
}

int main() {
    try {
        sharedGridChecks();
        for (const auto source : {ClockAuthority::Internal, ClockAuthority::Host, ClockAuthority::Midi}) {
            KSDrumArpeggiator drum;
            check(drum.configure({}, drumSettings()), "Load drum settings");
            auto start = tick(0, 0, source);
            start.reset = true;
            expectSilent(drum, start);
            expectSilent(drum, tick(16, 1, source));
            check(drum.press({17}, 60) && drum.press({18}, 64), "Trigger two routed keys");
            for(uint64_t pulse=2;pulse<6;++pulse)expectSilent(drum,tick(pulse*16,pulse,source));
            const auto started = noteOn(drum, tick(96, 6, source));
            check(drum.releaseKey({97}, 60) && !drum.pop() && drum.nextDeadline(),
                  "Releasing one key must keep the other key's pattern");
            check(drum.releaseKey({98}, 64), "Release the last key");
            expectRelease(drum, started, 98);
            expectSilent(drum, tick(112, 7, source));
        }

        KSDrumArpeggiator latched;
        check(latched.configure({}, drumSettings(true, true)), "Load a latched key-sync pattern");
        expectSilent(latched, tick(0, 0));
        check(latched.press({1}, 60), "Trigger the latched pattern");
        auto started = noteOn(latched, tick(16, 1));
        check(latched.releaseKey({18}, 60) && latched.nextDeadline() && !latched.pop(),
              "Latch must continue after key release");
        latched.setUserSequenceRetrigger(false);
        check(latched.press({19}, 64), "Key Sync still restarts a factory pattern with user Retrigger off");
        expectRelease(latched, started, 19);
        started = noteOn(latched, tick(32, 2));
        check(latched.clear({41}), "Scene change must cancel a pending step");
        expectRelease(latched, started, 41);
        check(latched.configure({41}, drumSettings(true, true)), "Reload the same pattern");
        expectSilent(latched, tick(48, 3, ClockAuthority::Host));
        check(latched.releaseKey({49}, 64), "Release a key from the retired scene");
        expectSilent(latched, tick(64, 4, ClockAuthority::Host));
        check(latched.press({65}, 64), "New note after the scene change");
        noteOn(latched, tick(80, 5, ClockAuthority::Host));

        KSDrumArpeggiator sustained;
        check(sustained.configure({}, drumSettings()) && sustained.pedal({}, true), "Load with sustain down");
        expectSilent(sustained, tick(0, 0));
        check(sustained.press({1}, 60), "Start the sustained pattern");
        started = noteOn(sustained, tick(96, 6));
        check(sustained.releaseKey({98}, 60) && sustained.nextDeadline() && !sustained.pop(),
              "Sustain must keep an already triggered pattern");
        check(sustained.pedal({99}, false), "Release sustain");
        expectRelease(sustained, started, 99);
        expectSilent(sustained, tick(112, 7));
        userPatternChecks();
        keyedPatternChecks();
        retriggerChecks();
        ratchetChecks();
        std::cout << "PASS: factory drums, sequence switching, retrigger, variable loops, probability, ratchets and legacy grids\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
