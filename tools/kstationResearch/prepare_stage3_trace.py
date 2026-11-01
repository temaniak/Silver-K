"""Diagnostic-only HDI + SCI byte-access tapes and CPU slice frames."""
from pathlib import Path
import subprocess,sys
source,output=map(Path,sys.argv[1:])
subprocess.run([sys.executable,str(Path(__file__).with_name('prepare_stage1_trace.py')),str(source),str(output)],check=True)
s=output.read_text()
read='return regs[r];';write='const auto old=regs[r];'
assert s.count(read)==1 and s.count(write)==1
s=s.replace(read,'if(r>=0xb0&&r<=0xb5)kStage1Port(*cpu,address,regs[r],false,true);'+read)
s=s.replace(write,'if(r>=0xb0&&r<=0xb5)kStage1Port(*cpu,address,value,true,true);'+write)
output.write_text(s)
