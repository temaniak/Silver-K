#include "ksControllerState.h"
namespace ks {
namespace {ControllerReferenceFactory referenceFactory=nullptr;}
void setControllerReferenceFactoryForTests(ControllerReferenceFactory f){referenceFactory=f;}
bool controllerReferenceAvailable(){return referenceFactory!=nullptr;}
uint8_t ControllerReference::mappedRead(ControllerCpu& h,uint32_t a){return h.bus_?h.bus_->read(a):h.memory[a];}
void ControllerReference::mappedWrite(ControllerCpu& h,uint32_t a,uint8_t v){if(h.bus_)h.bus_->write(a,v);else h.memory[a]=v;}
ControllerCpu::ControllerCpu()=default;
ControllerCpu::~ControllerCpu()=default;
void ControllerCpu::memmap(ControllerBus* device,int start,int length){
    if(start!=0 || length!=(1<<24))throw std::invalid_argument("KS requires one explicit full-board bus");
    attachResourceBus(device);if(device)device->setState(this);
}
void ControllerCpu::step(){
    if(!interpreterAllowed)throw std::logic_error("H8 interpreter called after native KS handoff");
    if(!reference){if(!referenceFactory)throw std::logic_error("KS H8 is available only in comparison tests");reference=referenceFactory();}
    ++interpretedSteps;reference->step(*this);
}
}
