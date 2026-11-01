#pragma once
#include <cstdint>
namespace ks {
// Original byte packing recipes, including rotations of out-of-range field
// values; reads remain separate observations rather than a patch snapshot.
class StatePackFieldsTask final {
public:enum class Kind {Oscillators,Unison,Lfo,Filter,Envelope,ProgramMode};enum class Stage {ReadFirst,RotateBefore,RotateBeforeExtra,ReadContributor,MergeContributor,RotateAfter,RotateFinalExtra,SetOffset,ReturnPacked,ShiftSignedFirst,ShiftSignedLast,MaskSigned,Complete};
 explicit StatePackFieldsTask(Kind k):kind_(k){}Kind kind()const{return kind_;}Stage stage()const{return phase_;}bool complete()const{return phase_==Stage::Complete;}unsigned piece()const{return piece_;}uint8_t value()const{return value_;}unsigned targetOffset()const{constexpr unsigned v[]{0,88,121,122,127,126};return v[unsigned(kind_)];}
 unsigned sourceOffset()const{constexpr unsigned values[6][5]={{210,209,211,0,0},{214,215,212,213,216},{217,218,0,0,0},{219,220,221,0,0},{222,223,224,225,0},{226,227,0,0,0}};return values[unsigned(kind_)][phase_==Stage::ReadFirst?0:piece_];}
 unsigned rotation()const{return phase_==Stage::RotateBeforeExtra?1:phase_==Stage::RotateFinalExtra?(kind_==Kind::Filter?2:1):phase_==Stage::RotateAfter&&kind_==Kind::Unison&&piece_<4?1:2;}
 bool rotateLeft()const{return kind_==Kind::Lfo&&(phase_==Stage::RotateAfter||phase_==Stage::RotateFinalExtra);}
 unsigned nextClocks()const{return phase_==Stage::ReadFirst||phase_==Stage::ReadContributor?15:phase_==Stage::SetOffset?9:phase_==Stage::ReturnPacked?17:complete()?0:3;}
 template<class Reader>unsigned advanceBefore(unsigned available,Reader&& read){unsigned used=0;while(const auto cost=nextClocks()){if(cost>available-used)break;switch(phase_){case Stage::ReadFirst:value_=uint8_t(read());piece_=1;phase_=kind_==Kind::ProgramMode?Stage::ReadContributor:Stage::RotateBefore;break;
 case Stage::RotateBefore:rotate();phase_=kind_==Kind::Oscillators||kind_==Kind::Lfo?Stage::RotateBeforeExtra:Stage::ReadContributor;break;case Stage::RotateBeforeExtra:rotate();phase_=Stage::ReadContributor;break;
 case Stage::ReadContributor:contributor_=uint8_t(read());phase_=kind_==Kind::ProgramMode?Stage::ShiftSignedFirst:Stage::MergeContributor;break;
 case Stage::MergeContributor:value_|=contributor_;phase_=kind_==Kind::ProgramMode?Stage::SetOffset:Stage::RotateAfter;break;
 case Stage::RotateAfter:rotate();if(piece_+1<count())++piece_,phase_=Stage::ReadContributor;else phase_=kind_==Kind::Envelope?Stage::SetOffset:Stage::RotateFinalExtra;break;
 case Stage::RotateFinalExtra:rotate();phase_=Stage::SetOffset;break;case Stage::SetOffset:phase_=Stage::ReturnPacked;break;case Stage::ReturnPacked:phase_=Stage::Complete;break;
 case Stage::ShiftSignedFirst:contributor_=uint8_t(contributor_*4);phase_=Stage::ShiftSignedLast;break;case Stage::ShiftSignedLast:contributor_=uint8_t(contributor_*2);phase_=Stage::MaskSigned;break;case Stage::MaskSigned:contributor_&=127;phase_=Stage::MergeContributor;break;default:break;
 }used+=cost;}return used;}
private:unsigned count()const{constexpr unsigned n[]{3,5,2,3,4,2};return n[unsigned(kind_)];}void rotate(){const auto n=rotation();value_=uint8_t(rotateLeft()?(unsigned(value_)<<n)|(unsigned(value_)>>(8-n)):(unsigned(value_)>>n)|(unsigned(value_)<<(8-n)));}Kind kind_;Stage phase_=Stage::ReadFirst;unsigned piece_=0;uint8_t value_=0,contributor_=0;
};}
