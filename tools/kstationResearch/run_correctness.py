import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(sys.argv[1]);firmware=pathlib.Path(sys.argv[2]);out=root/'evidence/correctness';out.mkdir(exist_ok=False)
assert hashlib.sha256(firmware.read_bytes()).hexdigest()=='2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
fields=['frames','pcm_fnv','h8_instructions','h8_cycles','dsp_instructions','dsp_cycles','hdi_reads','hdi_writes','midi_received','dropped_frames','flash_fnv','memory_fnv','midi_out_count','midi_out_fnv','before_patch','after_patch','bank','program']
results=[]
scenarios=['idle','held1','held8','notes','sweep','arp']
if '--include-program' in sys.argv[3:]:scenarios.append('program')
for scenario in scenarios:
 runs=[]
 for exe,mode in [('clean','capture'),('observe','observe'),('clean','detailed')]:
  prefix=out/f'{scenario}-{mode}'
  with prefix.with_suffix('.log').open('w') as log:
   p=subprocess.run([str(root/f'build/kstation_research_{exe}'),str(firmware),scenario,'2',mode,'offline','256',str(prefix),'2'],stdout=log,stderr=subprocess.STDOUT)
  d=json.loads(prefix.with_suffix('.json').read_text());d['exit_code']=p.returncode;runs.append(d)
 diffs=[{'mode':d['mode'],'fields':[k for k in fields if d[k]!=runs[0][k]]} for d in runs[1:]]
 raw={}
 for suffix in ['pcm','memory','flash']:
  a=out/f'{scenario}-capture.{suffix}';b=out/f'{scenario}-observe.{suffix}'
  raw[suffix]={'byte_exact':a.read_bytes()==b.read_bytes(),'bytes':a.stat().st_size,'baseline_sha256':hashlib.sha256(a.read_bytes()).hexdigest(),'observer_sha256':hashlib.sha256(b.read_bytes()).hexdigest()}
 patch=runs[0]['before_patch'];settings_ok=patch[37]==127 and patch[38:43]==[0]*5 and patch[62]==0 and patch[64]==127 and patch[65]==0 and patch[100]==0 and patch[107]==0 and patch[110]==0 and bool(patch[88]&4)==(scenario=='arp')
 passed=not any(d['fields'] for d in diffs) and all(v['byte_exact'] for v in raw.values()) and all(d['exit_code']==0 for d in runs) and settings_ok
 item={'scenario':scenario,'passed':passed,'settings_ok':settings_ok,'diffs':diffs,'raw':raw,'runs':runs};results.append(item)
 (out/'summary.json').write_text(json.dumps(results,indent=2))
 print(scenario,'PASS' if passed else 'FAIL',diffs,'settings',settings_ok,flush=True)
assert all(x['passed'] for x in results),'Preserve failed results and diagnose before measurements'
