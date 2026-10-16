#pragma once
#include <array>
#include <cstdint>
namespace ks {
// Decode six packed patch bytes into nineteen original expanded engine fields.
// A full patch refresh performs the same pass twice, rereading every source.
class StateParameterExpandTask final {
public:enum class Entry {Refresh,Fields};enum class Stage {ReadCurrentBase,CaptureCurrent,DispatchFirst,ReadLiveBase,ReadPacked,ShiftPacked,ShiftPackedAgain,CopyPiece,MaskPiece,WritePiece,ShiftSigned,ArithmeticSignedFirst,ArithmeticSignedSecond,WriteSigned,RetainRequest,ReadMode,AdmitMode,CaptureSigned,DispatchSigned,RestoreRequest,ReturnExpansion,AwaitSigned,Complete};enum class Read {CurrentBase,LiveBase,Packed,Mode};
 explicit StateParameterExpandTask(Entry e):phase_(e==Entry::Refresh?Stage::ReadCurrentBase:Stage::ReadPacked),pass_(e==Entry::Refresh?0:2){}
 Stage stage()const{return phase_;}bool complete()const{return phase_==Stage::Complete;}bool awaitingSigned()const{return phase_==Stage::AwaitSigned;}bool resumeSigned(){if(!awaitingSigned())return false;phase_=Stage::RestoreRequest;return true;}unsigned pass()const{return pass_;}unsigned output()const{return output_;}uint8_t signedMode()const{return packed_;}
 unsigned sourceOffset()const{constexpr unsigned v[]{88,88,88,88,88,0,0,0,121,121,122,122,122,127,127,127,127,126,126};return v[output_];}
 unsigned targetOffset()const{constexpr unsigned v[]{214,215,212,213,216,210,209,211,217,218,219,220,221,222,223,224,225,226,227};return v[output_];}
 unsigned nextClocks(unsigned storeClocks=3)const{switch(phase_){case Stage::ReadCurrentBase:case Stage::ReadLiveBase:return 18;case Stage::DispatchFirst:case Stage::ReadPacked:return 15;case Stage::WritePiece:case Stage::WriteSigned:return 12+storeClocks;case Stage::RetainRequest:case Stage::RestoreRequest:return 11;case Stage::ReadMode:return 12;case Stage::DispatchSigned:return 20;case Stage::ReturnExpansion:return 17;case Stage::AwaitSigned:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned storeClocks=3){unsigned used=0;while(const auto cost=nextClocks(storeClocks)){if(cost>available-used)break;switch(phase_){case Stage::ReadCurrentBase:(void)read(Read::CurrentBase);next();break;case Stage::DispatchFirst:phase_=Stage::ReadPacked;break;case Stage::ReadLiveBase:(void)read(Read::LiveBase);output_=0;phase_=Stage::ReadPacked;break;case Stage::ReadPacked:packed_=uint8_t(read(Read::Packed));phase_=Stage::CopyPiece;break;
 case Stage::ShiftPacked:{constexpr unsigned shifts[]{0,2,1,1,1,0,2,2,0,2,0,2,2,0,2,2,2,0,0};packed_>>=shifts[output_];phase_=output_==6||output_==9?Stage::ShiftPackedAgain:output_==16?Stage::MaskPiece:Stage::CopyPiece;break;}case Stage::ShiftPackedAgain:packed_>>=1;phase_=Stage::CopyPiece;break;
 case Stage::CopyPiece:piece_=packed_;next();break;case Stage::MaskPiece:{constexpr unsigned masks[]{3,1,1,1,3,7,3,3,7,3,3,3,3,3,3,3,1,7,255};if(output_==16){packed_&=masks[output_];piece_=packed_;}else piece_&=masks[output_];next();break;}
 case Stage::WritePiece:write(targetOffset(),piece_);++output_;phase_=output_==18?Stage::ShiftSigned:output_==5||output_==8||output_==10||output_==13||output_==17?Stage::ReadPacked:Stage::ShiftPacked;break;
 case Stage::ShiftSigned:packed_=uint8_t(packed_*2);next();break;case Stage::ArithmeticSignedFirst:case Stage::ArithmeticSignedSecond:packed_=uint8_t((packed_>>2)|(packed_&128?192:0));next();break;case Stage::WriteSigned:write(targetOffset(),packed_);next();break;
 case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));next();break;case Stage::AdmitMode:phase_=mode_?Stage::RestoreRequest:Stage::CaptureSigned;break;case Stage::DispatchSigned:phase_=Stage::AwaitSigned;break;
 case Stage::ReturnExpansion:if(pass_==0){pass_=1;phase_=Stage::ReadLiveBase;}else phase_=Stage::Complete;break;default:next();break;}used+=cost;}return used;}
private:void next(){phase_=Stage(unsigned(phase_)+1);}Stage phase_;unsigned pass_=0,output_=0;uint8_t packed_=0,piece_=0,mode_=0;
};}
