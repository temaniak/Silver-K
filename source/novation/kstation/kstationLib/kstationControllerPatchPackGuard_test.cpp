#include "kstationControllerPatchPackTask.h"
#include <cstdint>
#include <cstdio>
int main(){kstation::ControllerPatchPackTask task;unsigned writes=0,steps=0;for(;steps<64&&!task.unsupportedTarget();++steps){auto clocks=task.nextClocks();if(!clocks)return 1;auto used=task.advanceBefore(clocks,[](uint32_t,unsigned width,unsigned){return width==4?uint32_t(0xdeadbeef):0u;},[&](uint32_t,uint32_t,unsigned,unsigned){++writes;});if(used!=clocks)return 2;}if(!task.unsupportedTarget()||task.complete()||task.nextClocks()||writes)return 3;std::printf("{\"passed\":true,\"unsupported_target\":true,\"phases\":%u}\n",steps);}
