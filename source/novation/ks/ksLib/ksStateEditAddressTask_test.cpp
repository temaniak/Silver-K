#include "ksStateEditAddressTask.h"
#include <iostream>
#include <stdexcept>

namespace {void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}}
int main(){try{
    using T=ks::StateEditAddressTask;
    uint64_t cases=0,intervals=0;
    for(unsigned kind=0;kind<3;++kind)for(unsigned selector=0;selector<256;++selector)for(unsigned index=0;index<256;++index){
        const uint32_t descriptor=selector&1?0xfffffff0u:0x21000u;
        const uint32_t indexBase=selector&1?0xffffffe0u:0x31000u,targetBase=index&1?0xfffffff0u:0x41000u;
        const auto offset=uint8_t(selector*7+index);T task(T::Kind(kind),descriptor);
        unsigned indexReads=0,targetReads=0,mappingReads=0;
        auto read=[&](T::Read field)->uint32_t{
            switch(field){
            case T::Read::Selector:return selector;
            case T::Read::IndexBase:++indexReads;return indexBase;
            case T::Read::Index:check(task.selectorAddress()==uint32_t(indexBase+selector),"Selector address differs");return index;
            case T::Read::MappedOffset:check(task.mappingAddress()==uint32_t(descriptor+index+3),"Mapped descriptor address differs");++mappingReads;return offset;
            case T::Read::TargetBase:case T::Read::DirectBase:++targetReads;return targetBase;
            case T::Read::DirectOffset:case T::Read::GlobalOffset:return offset;
            }return 0;
        };
        while(!task.complete()){
            const auto before=task.stage();const auto cost=task.nextClocks();
            check(cost&&!task.advanceBefore(cost-1,read)&&task.stage()==before,"Short edit deadline changed state");
            check(task.advanceBefore(cost,read)==cost,"Exact edit deadline refused");++intervals;
        }
        const bool indexed=kind==0&&selector!=255;
        check(task.target()==uint32_t((kind==2?0x168fcu:targetBase)+offset),"Resolved edit address differs");
        check(indexReads==unsigned(indexed)&&mappingReads==unsigned(indexed)&&targetReads==unsigned(kind!=2),"Edit observation policy differs");
        check(!task.advanceBefore(100,read),"Completed edit resolver ran again");++cases;
    }
    std::cout<<"PASS: "<<cases<<" edit descriptor policies / "<<intervals<<" intervals; all selector/index bytes, sentinel/direct/global, late target base, 32-bit wrap and deadlines\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
