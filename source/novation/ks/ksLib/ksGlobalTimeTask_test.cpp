#include "ksGlobalTimeTask.h"
#include <iostream>
#include <stdexcept>
static void check(bool b){if(!b)throw std::runtime_error("Global time semantic property failed");}
int main(){using T=ks::GlobalTimeTask;uint64_t cases=0;
 auto verify=[&](unsigned kind,unsigned mode,unsigned time,unsigned divisor){T t{T::Kind(kind)};unsigned calls=0,guard=0,tableReads=0,timeReads=0;auto read=[&](T::Read r)->uint32_t{switch(r){case T::Read::Aux:return 213;case T::Read::Mode:return mode;case T::Read::Time:++timeReads;return time;case T::Read::Divisor:return divisor;case T::Read::Table:++tableReads;check(t.index()==uint8_t((mode-1)*4));return 0x87654321u^t.index();}return 0;};
 while(t.stage()!=T::Stage::Complete){check(++guard<48);auto c=t.child();if(c!=T::Child::None){if(c==T::Child::Time){auto q=uint16_t(divisor?0x28a1ea0u/divisor:0);check(t.scaled()==uint32_t(q)*(time*time));}if(c==T::Child::Table)check(t.table()==(0x87654321u^uint8_t((mode-1)*4)));++calls;check(t.resumeChild());continue;}auto cost=t.nextClocks(),s=unsigned(t.stage());check(cost);check(t.advanceBefore(cost-1,read)==0&&unsigned(t.stage())==s);check(t.advanceBefore(cost,read)==cost);}
 check(calls==2&&tableReads==unsigned(mode!=0)&&timeReads==unsigned(mode==0));++cases;};
 for(unsigned kind=0;kind<2;++kind){for(unsigned mode=0;mode<256;++mode)for(unsigned time=0;time<256;++time)verify(kind,mode,time,44100);for(unsigned divisor=0;divisor<65536;++divisor)for(unsigned time:{0u,127u,255u})verify(kind,0,time,divisor);}
 std::cout<<"PASS: "<<cases<<" pure time-pair properties; both kinds all mode/time pairs, all word divisors including0 with time edges, wrapped table index, exact unsigned quadratic packet, insufficient-budget no progress, 2child lifetime\n";
}
