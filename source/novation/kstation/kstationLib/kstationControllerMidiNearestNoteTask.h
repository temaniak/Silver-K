#pragma once
#include "kstationMidiEventTask.h"
#include <optional>
namespace kstation {
class ControllerMidiNearestNoteTask final {
 using T=MidiNearestNoteTask;using S=T::Stage;using Q=MidiSeekNoteTask;using QS=Q::Stage;
public:
 explicit ControllerMidiNearestNoteTask(uint8_t note):note_(note),task_(note){}bool complete()const{return task_.complete();}uint8_t result()const{return task_.result();}uint8_t velocity()const{return task_.velocity();}
 bool stackWrites()const{if(seek_)return false;auto s=task_.stage();return s==S::SaveWord0||s==S::SaveLong1||s==S::SaveLong3||s==S::SaveWord2||s==S::SeekUpper||s==S::SeekLower;}
 unsigned nextClocks()const{if(seek_){switch(seek_->stage()){case QS::Complete:return 0;case QS::ReadBitmap:return 6;case QS::Return:return 7;default:return 3;}}switch(task_.stage()){case S::Complete:case S::AwaitChild:return 0;case S::SaveWord0:case S::SaveWord2:case S::RestoreWord2:case S::RestoreWord0:return 6;case S::SaveLong1:case S::SaveLong3:case S::RestoreLong3:case S::RestoreLong1:return 10;case S::BindBitmap:case S::BindVelocity:return 9;case S::SeekUpper:case S::SeekLower:return 5;case S::ReadVelocity:return 6;case S::Return:return 7;default:return 3;}}
 template<class R>unsigned advanceBefore(unsigned budget,R&&read){auto cost=nextClocks();if(!cost||cost>budget)return 0;if(seek_){auto used=seek_->advance([&](Q::Read){return uint8_t(read(seek_->address()&0xffffff,1,cost));});if(!used)return 0;if(seek_->complete()){auto result=seek_->result();seek_.reset();task_.resume(task_.child(),result);}return cost;}bool used=task_.advance([&](T::Read){return uint8_t(read(0xcb5e + note_,1,cost));});if(!used)return 0;if(task_.awaitingChild())seek_.emplace(task_.child()==T::Child::Upper?Q::Kind::Upper:Q::Kind::Lower,note_,0xcbde);return cost;}
private:uint8_t note_;T task_;std::optional<Q>seek_{};
};
}
