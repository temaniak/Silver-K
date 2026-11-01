#include "ksExceptionReturnCalls.h"
#include "ksControllerBoundary.h"
#include <stdexcept>
namespace ks{
bool ExceptionReturnCalls::step(ControllerCpu&h){if(unsigned(h.getPC())!=0x170)return false;auto before=h.cycles;controllerDetail::Boundary f(h,0x170,2);unsigned cost=5,sp=h.getSP();int previous=h.lastread;for(unsigned i=0;i<(h.interruptMode2?6u:4u);++i){unsigned at=(sp+i)&0xffffff;if(at<0x400000){if((previous&1)||at!=unsigned(previous|1))cost+=3;previous=int(at);}else if(at>=0xfffd10&&at<0xffffa0){if((previous&1)||at!=unsigned(previous|1))++cost;previous=int(at);}else{cost+=at>=0xffffa0?1:3;previous=-1;}}ExceptionReturnTask task;++started;if(task.advanceBefore(cost,cost)!=cost||!task.complete())throw std::runtime_error("Exception return admission mismatch");--h.indent;h.clockI(2);if(h.interruptMode2)h.exr=uint8_t(h.popW()>>8);h.pc=h.makepc(h.pcoff(h.popPC(true)));++completed;if(h.cycles-before!=cost)throw std::runtime_error("Exception return clocks differ");return true;}}
