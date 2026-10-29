"""Generate a diagnostic-only H8 copy. The production repository is read-only."""
import pathlib, sys
source, output = map(pathlib.Path, sys.argv[1:])
text = source.read_text()
text = text.replace('#include "kstationH8.h"', '#include "kstationH8.h"\nextern void kResearchStep(unsigned,unsigned long long,unsigned long long);\nextern void kResearchPort(unsigned,unsigned long long,unsigned,unsigned,bool);')
old = 'cpu->step();++instructions;timers->tick();tickSerial();'
if text.count(old) == 1:
    text = text.replace(old, 'const auto beforeResearch=cpu->cycles;cpu->step();kResearchStep(pc,beforeResearch,cpu->cycles);++instructions;timers->tick();tickSerial();')
else:
    begin = 'const auto pc=static_cast<uint32_t>(cpu->getPC());'
    assert text.count(begin) == 1 and text.count('++instructions;') == 1
    text = text.replace(begin, begin+'const auto beforeResearch=cpu->cycles;')
    text = text.replace('++instructions;', '++instructions;kResearchStep(pc,beforeResearch,cpu->cycles);')
old = 'if(address>=0x600000 && address<0x600010){++hostReads;return host.read(address-0x600000);}'
assert text.count(old) == 1
text = text.replace(old, 'if(address>=0x600000 && address<0x600010){++hostReads;const auto value=host.read(address-0x600000);kResearchPort(cpu->getPC(),cpu->cycles,address,value,false);return value;}')
old = '++hostWrites;\n                if(!host.write'
assert text.count(old) == 1
text = text.replace(old, '++hostWrites;kResearchPort(cpu->getPC(),cpu->cycles,address,value,true);\n                if(!host.write')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(text)
