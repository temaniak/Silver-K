"""Diagnostic source copy: ordered HDI, all mapped MCU IO, timers and slices."""
from pathlib import Path
import subprocess,sys
source,output=map(Path,sys.argv[1:])
subprocess.run([sys.executable,str(Path(__file__).with_name('prepare_stage1_trace.py')),str(source),str(output)],check=True)
s=output.read_text();marker='namespace kstation\n{'
wrapper='''
class KStage4TapeDevice final:public H8SDevice {
    H8SDevice* delegate_=nullptr;
public:
    H8SDevice* bind(H8SDevice* d,h8state* cpu){delegate_=d;d->setState(cpu);return this;}
    uint8_t read(uint32_t a)override{const auto v=delegate_->read(a);kStage1Port(*state,a,v,false,true);return v;}
    void write(uint32_t a,uint8_t v)override{delegate_->write(a,v);kStage1Port(*state,a,v,true,true);}
};
'''
assert s.count(marker)==1;s=s.replace(marker,wrapper+marker)
field='std::unique_ptr<KStationTimers> timers;';assert s.count(field)==1
s=s.replace(field,field+'\n        KStage4TapeDevice timerTape,ioTape;')
for old,new in [('cpu->memmap(this,0xffff1c,0xe4);','cpu->memmap(ioTape.bind(this,cpu.get()),0xffff1c,0xe4);'),('cpu->memmap(timers.get(),0xffff60,64);','cpu->memmap(timerTape.bind(timers.get(),cpu.get()),0xffff60,64);')]:
 assert s.count(old)==1;s=s.replace(old,new)
s=s.replace('cpu->memmap(nativeTimers.get(),0xffff60,64);','cpu->memmap(timerTape.bind(nativeTimers.get(),cpu.get()),0xffff60,64);')
output.write_text(s)
