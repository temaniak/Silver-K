#pragma once
#include <cstdint>

namespace ks {
// Store-page transitions and commit orchestration. Flash, serialization and
// engine refresh remain independently owned children; no CPU frame lives here.
class StateStoreUiTask final {
public:
    enum class Kind { CommitMulti, CommitSingle, BeginProgram, BeginName, BeginStore };
    enum class Stage {
        SerializeMulti, SaveMulti, SelectMultiMessage, DisplayMulti, ClearMultiPage,
        WriteMultiPage, FinishMulti, PrepareSingle, ReadCurrent, ReadCategory,
        WriteCategory, SaveSingle, ClearSinglePage, WriteSinglePage, ResetSinglePanel,
        SelectSingleMessage, ReadMode, SelectReload, ReloadMulti, ClearMultiDirty,
        WriteMultiDirty, RefreshMultiName, SelectReloadMessage, FinishSingle,
        CheckProtection, SelectAllowed, ReturnProtected, SetProgramPage, WriteProgramPage,
        ClearProgramDirty, WriteProgramDirty, RefreshProgramName, FinishProgram,
        SetNamePage, WriteNamePage, BeginNameEditor, FinishName,
        SetStorePage, WriteStorePage, FinishStore, AwaitChild, AwaitTail, Complete
    };
    enum class Read { Current, Category, Mode };
    enum class Write { Page, Dirty, Category };
    enum class Child { SerializeMulti, SaveMulti, DisplayMulti, PrepareSingle, SaveSingle, ResetPanel, ReloadMulti, RefreshName, CheckProtection, NameEditor };
    enum class Tail { ResetPanel, Display, FinishPage };
    explicit StateStoreUiTask(Kind kind):stage_(kind==Kind::CommitMulti?Stage::SerializeMulti:kind==Kind::CommitSingle?Stage::PrepareSingle:kind==Kind::BeginProgram?Stage::CheckProtection:kind==Kind::BeginName?Stage::SetNamePage:Stage::SetStorePage){}
    Stage stage()const{return stage_;}
    Stage resumeStage()const{return resume_;}
    bool complete()const{return stage_==Stage::Complete;}
    bool awaitingChild()const{return stage_==Stage::AwaitChild;}
    bool awaitingTail()const{return stage_==Stage::AwaitTail;}
    Child child()const{return child_;}
    Tail tail()const{return tail_;}
    uint32_t current()const{return current_;}
    uint8_t category()const{return category_;}
    uint8_t mode()const{return mode_;}
    bool resumeChild(uint8_t value=0){if(!awaitingChild())return false;if(child_==Child::CheckProtection)allowed_=value!=0;stage_=resume_;return true;}
    bool retireTail(){if(!awaitingTail())return false;stage_=Stage::Complete;return true;}
    unsigned nextClocks(unsigned storeClocks=3)const{
        switch(stage_){
        case Stage::SerializeMulti:case Stage::SaveMulti:case Stage::DisplayMulti:
        case Stage::PrepareSingle:case Stage::SaveSingle:case Stage::ResetSinglePanel:
        case Stage::ReloadMulti:case Stage::RefreshMultiName:case Stage::CheckProtection:
        case Stage::RefreshProgramName:case Stage::BeginNameEditor:return 20;
        case Stage::SelectMultiMessage:case Stage::SelectSingleMessage:case Stage::SelectReloadMessage:return 9;
        case Stage::ReadCurrent:return 18;case Stage::ReadCategory:case Stage::ReadMode:return 12;
        case Stage::WriteCategory:return 12+storeClocks;
        case Stage::WriteMultiPage:case Stage::WriteSinglePage:case Stage::WriteMultiDirty:
        case Stage::WriteProgramPage:case Stage::WriteProgramDirty:case Stage::WriteNamePage:case Stage::WriteStorePage:return 9+storeClocks;
        case Stage::FinishMulti:case Stage::SelectReload:case Stage::FinishSingle:case Stage::FinishProgram:return 8;
        case Stage::ReturnProtected:return 17;
        case Stage::AwaitChild:case Stage::AwaitTail:case Stage::Complete:return 0;
        default:return 3;
        }
    }
    template<class Reader,class Writer>
    unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned storeClocks=3){
        const auto cost=nextClocks(storeClocks);if(!cost||cost>available)return 0;
        switch(stage_){
        case Stage::SerializeMulti:await(Child::SerializeMulti,Stage::SaveMulti);break;
        case Stage::SaveMulti:await(Child::SaveMulti,Stage::SelectMultiMessage);break;
        case Stage::DisplayMulti:await(Child::DisplayMulti,Stage::ClearMultiPage);break;
        case Stage::WriteMultiPage:case Stage::WriteSinglePage:write(Write::Page,0);next();break;
        case Stage::FinishMulti:tail(Tail::ResetPanel);break;
        case Stage::PrepareSingle:await(Child::PrepareSingle,Stage::ReadCurrent);break;
        case Stage::ReadCurrent:current_=uint32_t(read(Read::Current));next();break;
        case Stage::ReadCategory:category_=uint8_t(read(Read::Category));next();break;
        case Stage::WriteCategory:write(Write::Category,category_);next();break;
        case Stage::SaveSingle:await(Child::SaveSingle,Stage::ClearSinglePage);break;
        case Stage::ResetSinglePanel:await(Child::ResetPanel,Stage::SelectSingleMessage);break;
        case Stage::ReadMode:mode_=uint8_t(read(Read::Mode));next();break;
        case Stage::SelectReload:if(mode_)stage_=Stage::ReloadMulti;else tail(Tail::Display);break;
        case Stage::ReloadMulti:await(Child::ReloadMulti,Stage::ClearMultiDirty);break;
        case Stage::WriteMultiDirty:case Stage::WriteProgramDirty:write(Write::Dirty,0);next();break;
        case Stage::RefreshMultiName:await(Child::RefreshName,Stage::SelectReloadMessage);break;
        case Stage::FinishSingle:tail(Tail::Display);break;
        case Stage::CheckProtection:await(Child::CheckProtection,Stage::SelectAllowed);break;
        case Stage::SelectAllowed:stage_=allowed_?Stage::SetProgramPage:Stage::ReturnProtected;break;
        case Stage::ReturnProtected:stage_=Stage::Complete;break;
        case Stage::WriteProgramPage:write(Write::Page,1);next();break;
        case Stage::RefreshProgramName:await(Child::RefreshName,Stage::FinishProgram);break;
        case Stage::FinishProgram:case Stage::FinishName:case Stage::FinishStore:tail(Tail::FinishPage);break;
        case Stage::WriteNamePage:write(Write::Page,2);next();break;
        case Stage::BeginNameEditor:await(Child::NameEditor,Stage::FinishName);break;
        case Stage::WriteStorePage:write(Write::Page,3);next();break;
        default:next();break;
        }return cost;
    }
private:
    void next(){stage_=Stage(unsigned(stage_)+1);}
    void await(Child child,Stage resume){child_=child;resume_=resume;stage_=Stage::AwaitChild;}
    void tail(Tail value){tail_=value;stage_=Stage::AwaitTail;}
    Stage stage_,resume_=Stage::Complete;Child child_=Child::SerializeMulti;Tail tail_=Tail::ResetPanel;
    uint32_t current_=0;uint8_t category_=0,mode_=0;bool allowed_=false;
};
}
