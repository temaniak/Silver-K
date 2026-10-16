#include "ksPanelPollTask.h"
#include <stdexcept>
#include <iostream>
int main(){using T=ks::PanelPollTask;using S=T::Stage;unsigned cases=0;for(unsigned busy=0;busy<256;++busy)for(unsigned ready=0;ready<128;++ready){T t;unsigned reads=0,used=0;while(t.stage()!=S::Complete){if(t.awaiting()){bool final=t.stage()==S::AwaitFinal;uint8_t reply=final?uint8_t(ready^0x5a):reads<busy?uint8_t(ready|128):uint8_t(ready);++reads;if(!t.resumeChild(reply))throw std::runtime_error("resume");continue;}auto cost=t.nextClocks();if(t.advanceBefore(cost-1))throw std::runtime_error("early");used+=t.advanceBefore(cost);}if(reads!=busy+2||t.reply()!=uint8_t(ready^0x5a)||used!=21*(busy+1)+44)throw std::runtime_error("poll clocks/reply/count");++cases;}std::cout<<"PASS: "<<cases<<" pure panel-poll properties\n";}
