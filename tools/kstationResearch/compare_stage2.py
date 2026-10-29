"""Compare IRQ-stage against both frozen original H8 and frozen native stage1."""
import argparse,hashlib,json,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('firmware',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);a=p.parse_args()
assert hashlib.sha256(a.firmware.read_bytes()).hexdigest()=='2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
a.output.mkdir(exist_ok=False)
fields=['frames','pcm_fnv','memory_fnv','flash_fnv','h8_instructions','h8_cycles','dsp_instructions','dsp_cycles','hdi_reads','hdi_writes','midi_received','midi_out_fnv','midi_out_count','dropped_frames','before_patch','after_patch','bank','program']
def digest(q):
 h=hashlib.sha256()
 with q.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def byte_equal(x,y):
 if x.stat().st_size!=y.stat().st_size:return False
 with x.open('rb') as f,y.open('rb') as g:
  while True:
   b=f.read(1024*1024)
   if b!=g.read(1024*1024):return False
   if not b:return True
results=[]
for scenario in ['idle','held1','held8','notes','sweep','arp','clock','irq','sustain','program','irq2','scierror']:
 runs={}
 for controller,target,mode in [('original','kstation_stage1_reference_trace','original'),('stage1','kstation_stage2_reference_trace','hdi'),('control','kstation_stage2_candidate','hdi'),('irq','kstation_stage2_candidate_trace','irq')]:
  prefix=a.output/f'{scenario}-{controller}';env=dict(os.environ);env['K_STAGE1_MODE']=mode
  with prefix.with_suffix('.log').open('w') as log:r=subprocess.run([str(a.build/target),str(a.firmware),scenario,'2','capture','offline','256',str(prefix),'1'],stdout=log,stderr=subprocess.STDOUT,env=env)
  assert r.returncode==0,f'{scenario}/{controller} failed; preserve log'
  runs[controller]=json.loads(prefix.with_suffix('.json').read_text())
 comparisons=[]
 for controller in ['stage1','control','irq']:
  diff=[k for k in fields if runs[controller][k]!=runs['original'][k]];raw={}
  for suffix in ['pcm','memory','flash']+(['ports','slices'] if controller!='control' else []):
   x=a.output/f'{scenario}-original.{suffix}';y=a.output/f'{scenario}-{controller}.{suffix}'
   raw[suffix]={'byte_exact':byte_equal(x,y),'bytes':x.stat().st_size,'reference_sha256':digest(x),'candidate_sha256':digest(y)}
  comparisons.append({'controller':controller,'report_differences':diff,'raw':raw})
  assert not diff and all(v['byte_exact'] for v in raw.values()),f'{scenario}/{controller} mismatch'
 assert runs['irq']['native_irq_steps']>0 and runs['irq']['native_irq_entries']>0 and runs['irq']['native_irq_returns']>0
 assert runs['irq']['interpreted_steps']>0,'This remains a partial controller'
 if scenario in ['irq2','scierror']:assert runs['irq']['native_serial_error_steps']>0
 results.append({'scenario':scenario,'passed':True,'runs':runs,'comparisons':comparisons});(a.output/'summary.json').write_text(json.dumps(results,indent=2))
 print(scenario,'PASS IRQsteps',runs['irq']['native_irq_steps'],'entries',runs['irq']['native_irq_entries'],'returns',runs['irq']['native_irq_returns'],flush=True)
print('All original/stage1/IRQ-stage data and timed traces matched',flush=True)
