"""Byte-exact cold/startup/steady engine comparison against frozen original H8.

Build all four stage1 targets first; baseline source directory must have been
frozen before edits. All trace files remain private, and existing runs refuse
overwrite. CPU measurements use other, uninstrumented targets after this gate.
"""
import argparse,hashlib,json,os,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('firmware',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);a=p.parse_args()
assert hashlib.sha256(a.firmware.read_bytes()).hexdigest()=='2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
a.output.mkdir(exist_ok=False)
fields=['frames','pcm_fnv','memory_fnv','flash_fnv','h8_instructions','h8_cycles','dsp_instructions','dsp_cycles','hdi_reads','hdi_writes','midi_received','midi_out_fnv','midi_out_count','dropped_frames','before_patch','after_patch','bank','program']
def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()
def byte_equal(x,y):
 if x.stat().st_size!=y.stat().st_size:return False
 with x.open('rb') as f,y.open('rb') as g:
  while True:
   b=f.read(1024*1024)
   if b!=g.read(1024*1024):return False
   if not b:return True
results=[]
for scenario in ['idle','held1','held8','notes','sweep','arp','clock','irq','sustain','program']:
 runs={}
 for controller in ['reference','original','timers','hdi']:
  # Full traces for original reference and final combined native stage. The
  # two secondary controls compare raw audio/RAM/Flash and guest counters.
  trace=controller in ['reference','hdi']
  name='reference' if controller=='reference' else 'candidate'
  exe=a.build/f'kstation_stage1_{name}{"_trace" if trace else ""}'
  prefix=a.output/f'{scenario}-{controller}';env=dict(os.environ);env['K_STAGE1_MODE']=controller
  cmd=[str(exe),str(a.firmware),scenario,'2','capture','offline','256',str(prefix),'1']
  with prefix.with_suffix('.log').open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,env=env)
  assert r.returncode==0,f'{controller}/{scenario} rejected: preserve log'
  d=json.loads(prefix.with_suffix('.json').read_text());runs[controller]=d
  assert d['dropped_frames']==0
 ref=runs['reference'];comparisons=[]
 for controller in ['original','timers','hdi']:
  d=runs[controller];diff=[f for f in fields if d[f]!=ref[f]];raw={}
  for suffix in ['pcm','memory','flash']+(['ports','slices'] if controller=='hdi' else []):
   x=a.output/f'{scenario}-reference.{suffix}';y=a.output/f'{scenario}-{controller}.{suffix}'
   raw[suffix]={'byte_exact':byte_equal(x,y),'bytes':x.stat().st_size,'reference_sha256':digest(x),'candidate_sha256':digest(y)}
  comparisons.append({'controller':controller,'report_differences':diff,'raw':raw})
  assert not diff and all(x['byte_exact'] for x in raw.values()),f'{scenario}/{controller} mismatch; preserve all files'
 assert runs['hdi']['native_wait_steps']>0 and runs['hdi']['interpreted_steps']>0,'First stage remains hybrid, both paths must be observed'
 results.append({'scenario':scenario,'passed':True,'runs':runs,'comparisons':comparisons})
 (a.output/'summary.json').write_text(json.dumps(results,indent=2));print(scenario,'PASS',runs['hdi']['native_wait_steps'],'native wait phases',flush=True)
print('All cold/startup/steady PCM, RAM, Flash, HDI timing and full slice frames matched',flush=True)
