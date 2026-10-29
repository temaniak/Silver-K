#pragma once
#include <cstdint>
namespace kstation {
// K image selection/copy and relocated overlay lifecycle. Logical cursors and
// typed live accesses are native; addresses, bus timing and caller projections
// belong to the board. Children must finish before the parent can advance.
class ResetImageTask final {
public:
 enum class Entry {Verify,RomCopy,Relocated};
 enum class Stage {Begin,Prepare,BindSignature,BindStoredSignature,CountSignature,ReadSignature,ReadStoredSignature,CompareSignature,SelectSignature,TickSignature,RepeatSignature,AdmitSignature,ReadVersion,ReadStoredVersion,CompareVersion,SelectVersion,BindImage,ReadSize,SelectSize,MarkEmpty,SelectEmpty,CountWords,ClearWordUpper,ClearSum,ReadWord,WriteWord,AddWord,TickWords,RepeatWords,ReadChecksum,WriteChecksum,CountTail,ClearTailValue,WriteTail,AdvanceTail,TickTail,RepeatTail,CompareChecksum,SelectChecksum,BindRom,ReadMarker,CompareMarker55,SelectMarker55,CompareMarkerAA,SelectMarkerAA,BindError,PublishError,RetainDelay,CountSettling,RetainInner,CountDelay,TickDelay,RepeatDelay,ReleaseInner,TickSettling,RepeatSettling,ReleaseDelay,Mask,BindTrampoline,BindTrampolineTarget,CountTrampoline,ReadTrampoline,WriteTrampoline,AdvanceTrampoline,TickTrampoline,RepeatTrampoline,EnterTrampoline,PrepareOverlay,PublishOverlay,PrepareAccessState,PublishAccessState,Restart,EnterWarm,ReadWarmMarker,CompareWarm55,SelectWarm55,CompareWarmAA,SelectWarmAA,ReturnWarm,BindWarmImage,ReadWarmSize,SelectWarmSize,MarkWarmEmpty,SelectWarmEmpty,BindUpdate,PublishUpdate,BindUpdateTarget,BindUpdateSource,UpdateFirmware,BindDone,PublishDone,HoldUpdate,Complete};
 enum class Read {Signature,StoredSignature,Version,StoredVersion,ImageSize,ImageWord,ExpectedChecksum,Marker,Trampoline};
 enum class Write {ImageWord,Checksum,Tail,Trampoline,Overlay,AccessState};
 enum class Child {None,ErrorMessage,UpdateMessage,FirmwareUpdate,DoneMessage};
 struct Credits {unsigned read=0,write=0,call=10,shortCall=5,ret=7,push=6,pop=6;};
 explicit ResetImageTask(Entry e=Entry::Verify):stage_(e==Entry::Verify?Stage::Begin:e==Entry::RomCopy?Stage::BindRom:Stage::PrepareOverlay){}
 Stage stage()const{return stage_;}Child child()const{return child_;}bool awaiting()const{return child_!=Child::None;}bool complete()const{return stage_==Stage::Complete;}bool held()const{return stage_==Stage::HoldUpdate;}
 bool resumeChild(){if(!awaiting())return false;child_=Child::None;return true;}
 uint32_t sourceBase()const{return source_;}uint32_t targetBase()const{return target_;}unsigned index()const{return index_;}unsigned targetIndex()const{return targetIndex_;}uint32_t remaining()const{return remaining_;}uint32_t datum()const{return data_;}uint32_t size()const{return size_;}uint32_t checksum()const{return sum_;}uint32_t expected()const{return expected_;}unsigned delay()const{return delay_;}unsigned settling()const{return settling_;}uint8_t marker()const{return marker_;}bool warmCopy()const{return warm_;}
 unsigned nextClocks(const Credits&c)const {if(awaiting()||complete())return 0;switch(stage_){
 case Stage::BindSignature:case Stage::BindStoredSignature:case Stage::BindImage:case Stage::BindRom:case Stage::BindError:case Stage::BindTrampoline:case Stage::BindTrampolineTarget:case Stage::BindWarmImage:case Stage::BindUpdate:case Stage::BindUpdateTarget:case Stage::BindUpdateSource:case Stage::BindDone:return 9;
 case Stage::CountSignature:case Stage::CountTrampoline:case Stage::CountDelay:case Stage::CountSettling:return 6;
 case Stage::ReadWord:return 5+c.read;
 case Stage::ReadSignature:case Stage::ReadStoredSignature:case Stage::ReadTrampoline:return 5+c.read;
 case Stage::ReadVersion:case Stage::ReadStoredVersion:return 9+c.read;
 case Stage::ReadSize:case Stage::ReadWarmSize:return 15+c.read;
 case Stage::WriteWord:return 12+c.write;
 case Stage::ReadChecksum:return 8+c.read;
 case Stage::WriteChecksum:return 15+c.write;
 case Stage::WriteTail:return 12+c.write;
 case Stage::ReadMarker:case Stage::ReadWarmMarker:return 9+c.read;
 case Stage::PublishError:case Stage::UpdateFirmware:case Stage::PublishUpdate:case Stage::PublishDone:return c.call;
 case Stage::RetainDelay:case Stage::RetainInner:return c.push;
 case Stage::ReleaseDelay:case Stage::ReleaseInner:return c.pop;
 case Stage::Mask:return 3;
 case Stage::SelectChecksum:return 8;
 case Stage::WriteTrampoline:return 3+c.write;
 case Stage::EnterTrampoline:case Stage::EnterWarm:return 8;
 case Stage::Restart:return 4;
 case Stage::PrepareOverlay:case Stage::PrepareAccessState:return 1;
 case Stage::PublishOverlay:case Stage::PublishAccessState:return 1+c.write;
 case Stage::ReturnWarm:return c.ret;
 default:return 3;
 }}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,const Credits&c,Reader&&read,Writer&&write){const auto cost=nextClocks(c);if(!cost||available<cost)return 0;auto next=Stage(unsigned(stage_)+1);switch(stage_){
 case Stage::BindSignature:index_=0;break;case Stage::BindStoredSignature:targetIndex_=0;break;case Stage::CountSignature:remaining_=40;break;
 case Stage::ReadSignature:signature_=uint8_t(read(Read::Signature,index_++));break;
 case Stage::ReadStoredSignature:data_=uint8_t(read(Read::StoredSignature,targetIndex_++));break;
 case Stage::CompareSignature:match_=signature_==uint8_t(data_);break;
 case Stage::SelectSignature:if(!match_)next=Stage::AdmitSignature;break;
 case Stage::TickSignature:--remaining_;break;
 case Stage::RepeatSignature:if(remaining_)next=Stage::ReadSignature;break;
 case Stage::AdmitSignature:if(!match_)next=Stage::BindRom;break;
 case Stage::ReadVersion:version_=uint16_t(read(Read::Version,0));break;
 case Stage::ReadStoredVersion:data_=uint16_t(read(Read::StoredVersion,0));break;
 case Stage::CompareVersion:match_=uint16_t(data_)>=version_;break;
 case Stage::SelectVersion:if(!match_)next=Stage::BindRom;break;
 case Stage::BindImage:source_=0x210000;target_=0x400000;index_=0;warm_=false;break;
 case Stage::ReadSize:case Stage::ReadWarmSize:size_=read(Read::ImageSize,0);break;
 case Stage::SelectSize:next=size_?Stage::CountWords:Stage::MarkEmpty;break;
 case Stage::MarkEmpty:match_=false;break;
 case Stage::SelectEmpty:next=source_?Stage::SelectChecksum:Stage::ReadMarker;break;
 case Stage::CountWords:remaining_=size_>>1;break;
 case Stage::ClearSum:sum_=0;break;
 case Stage::ReadWord:data_=uint16_t(read(Read::ImageWord,index_));index_+=2;break;
 case Stage::WriteWord:write(Write::ImageWord,index_-2,data_);break;
 case Stage::AddWord:sum_+=uint16_t(data_);break;
 case Stage::TickWords:--remaining_;break;
 case Stage::RepeatWords:if(remaining_)next=Stage::ReadWord;break;
 case Stage::ReadChecksum:expected_=read(Read::ExpectedChecksum,index_);index_+=4;break;
 case Stage::WriteChecksum:write(Write::Checksum,index_-4,sum_);break;
 case Stage::CountTail:tail_=128;break;
 case Stage::WriteTail:write(Write::Tail,index_,0);break;
 case Stage::AdvanceTail:index_+=2;break;
 case Stage::TickTail:--tail_;break;
 case Stage::RepeatTail:if(tail_)next=Stage::WriteTail;break;
 case Stage::CompareChecksum:match_=expected_==sum_;if(source_!=0x210000)next=warm_?Stage::BindUpdate:Stage::ReadMarker;break;
 case Stage::SelectChecksum:next=source_==0x210000?(match_?Stage::Mask:Stage::BindRom):warm_?Stage::BindUpdate:Stage::ReadMarker;break;
 case Stage::BindRom:source_=0;target_=0x400000;index_=0;warm_=false;next=Stage::ReadSize;break;
 case Stage::ReadMarker:case Stage::ReadWarmMarker:marker_=uint8_t(read(Read::Marker,0));break;
 case Stage::SelectMarker55:if(marker_==0x55)next=Stage::Mask;break;
 case Stage::SelectMarkerAA:if(marker_==0xaa)next=Stage::Mask;break;
 case Stage::PublishError:child_=Child::ErrorMessage;break;
 case Stage::CountSettling:settling_=500;break;
 case Stage::CountDelay:delay_=2663;break;
 case Stage::TickDelay:--delay_;break;
 case Stage::RepeatDelay:if(delay_)next=Stage::TickDelay;break;
 case Stage::TickSettling:--settling_;break;
 case Stage::RepeatSettling:if(settling_)next=Stage::RetainInner;break;
 case Stage::BindTrampoline:index_=0;break;
 case Stage::BindTrampolineTarget:targetIndex_=0;break;
 case Stage::CountTrampoline:remaining_=12;break;
 case Stage::ReadTrampoline:data_=uint8_t(read(Read::Trampoline,index_++));break;
 case Stage::WriteTrampoline:write(Write::Trampoline,targetIndex_,data_);break;
 case Stage::AdvanceTrampoline:++targetIndex_;break;
 case Stage::TickTrampoline:--remaining_;break;
 case Stage::RepeatTrampoline:if(remaining_)next=Stage::ReadTrampoline;break;
 case Stage::PublishOverlay:write(Write::Overlay,0,0);break;
 case Stage::PublishAccessState:write(Write::AccessState,0,0xfa);break;
 case Stage::SelectWarm55:if(marker_==0x55)next=Stage::BindUpdate;break;
 case Stage::SelectWarmAA:if(marker_==0xaa)next=Stage::BindWarmImage;break;
 case Stage::ReturnWarm:next=Stage::Complete;break;
 case Stage::BindWarmImage:source_=0x200000;target_=0x410000;index_=0;warm_=true;break;
 case Stage::SelectWarmSize:next=size_?Stage::CountWords:Stage::MarkWarmEmpty;break;
 case Stage::SelectWarmEmpty:next=Stage::BindUpdate;break;
 case Stage::PublishUpdate:child_=Child::UpdateMessage;break;
 case Stage::UpdateFirmware:child_=Child::FirmwareUpdate;break;
 case Stage::PublishDone:child_=Child::DoneMessage;break;
 case Stage::HoldUpdate:next=Stage::HoldUpdate;break;
 default:break;}stage_=next;return cost;}
private:
 Stage stage_;Child child_=Child::None;uint32_t source_=0,target_=0,index_=0,targetIndex_=0,remaining_=0,size_=0,data_=0,sum_=0,expected_=0;uint16_t version_=0,delay_=0,settling_=0;uint8_t signature_=0,marker_=0,tail_=0;bool match_=false,warm_=false;
};
}
