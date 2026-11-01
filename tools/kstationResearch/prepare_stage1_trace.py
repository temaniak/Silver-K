"""Create diagnostic-only source copies with ordered HDI and slice recording."""
from pathlib import Path
import sys
source,output=map(Path,sys.argv[1:]);text=source.read_text()
declarations='''
extern void kStage1Port(const h8state&,unsigned,unsigned,bool,bool);
extern void kStage1Slice(const h8state&,int,int,unsigned long long);
'''
marker='#include "h8s/h8sdevices.hpp"'
assert text.count(marker)==1
text=text.replace(marker,marker+declarations)
if 'std::unique_ptr<H8SEmulator>' in text:
    text=text.replace(marker,marker+'''
namespace { class TraceCpu final:public H8SEmulator {
public:int readHistory()const{return lastread;}int writeHistory()const{return lastwrite;}
}; }
''')
    text=text.replace('std::unique_ptr<H8SEmulator> cpu=std::make_unique<H8SEmulator>();','std::unique_ptr<TraceCpu> cpu=std::make_unique<TraceCpu>();')
read='if(address>=0x600000 && address<0x600010){++hostReads;return host.read(address-0x600000);}'
assert text.count(read)==1
text=text.replace(read,'if(address>=0x600000 && address<0x600010){++hostReads;const auto value=host.read(address-0x600000);kStage1Port(*cpu,address,value,false,true);return value;}')
write='if(!host.write(address-0x600000,value))throw std::runtime_error("H8 wrote a busy DSP HDI08 host port");'
assert text.count(write)==1
text=text.replace(write,'const bool accepted=host.write(address-0x600000,value);kStage1Port(*cpu,address,value,true,accepted);if(!accepted)throw std::runtime_error("H8 wrote a busy DSP HDI08 host port");')
end='return cpu->cycles-begin;'
assert text.count(end)==1
text=text.replace(end,'kStage1Slice(*cpu,cpu->readHistory(),cpu->writeHistory(),begin);'+end)
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(text)
