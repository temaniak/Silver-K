"""Serial fixed-work measurements; no profiling/builds during clean CPU phase."""
import argparse,hashlib,json,pathlib,statistics,subprocess,time
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('firmware',type=pathlib.Path);p.add_argument('phase',choices=['detailed','cpu','offline','stacks']);p.add_argument('--run-id',default='');a=p.parse_args()
assert hashlib.sha256(a.firmware.read_bytes()).hexdigest()=='2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
exe=a.root/'build/kstation_research_clean';identity=hashlib.sha256(exe.read_bytes()).hexdigest();out=a.root/f'evidence/{a.phase}{"-"+a.run_id if a.run_id else ""}';out.mkdir(exist_ok=False)
scenarios=['idle','held1','held8','notes','sweep','arp']
if a.phase=='stacks':
 for scenario in ['idle','held8','arp']:
  prefix=out/scenario
  with prefix.with_suffix('.log').open('w') as log:
   task=subprocess.Popen([str(exe),str(a.firmware),scenario,'60','off','offline','256',str(prefix),'3'],stdout=log,stderr=subprocess.STDOUT)
   time.sleep(3)
   sample=subprocess.run(['/usr/bin/sample',str(task.pid),'6','2','-file',str(prefix)+'.sample.txt'],capture_output=True,text=True)
   (out/f'{scenario}-sample-status.json').write_text(json.dumps({'returncode':sample.returncode,'stdout':sample.stdout,'stderr':sample.stderr},indent=2))
   code=task.wait()
  print(scenario,'sample',sample.returncode,'render',code,flush=True)
  if code:raise RuntimeError('Profile render failed')
else:
 allruns=[]
 for repeat in range(3):
  order=scenarios if repeat%2==0 else list(reversed(scenarios))
  for scenario in order:
   prefix=out/f'{repeat+1}-{scenario}'
   mode='detailed' if a.phase=='detailed' else 'off'
   pacing='paced' if a.phase=='cpu' else 'offline'
   seconds='12';warm='3'
   with prefix.with_suffix('.log').open('w') as log:
    task=subprocess.run([str(exe),str(a.firmware),scenario,seconds,mode,pacing,'256',str(prefix),warm],stdout=log,stderr=subprocess.STDOUT)
   d=json.loads(prefix.with_suffix('.json').read_text());d.update(repeat=repeat+1,executable_sha256=identity,exit_code=task.returncode)
   allruns.append(d);(out/'runs.json').write_text(json.dumps(allruns,indent=2))
   print(repeat+1,scenario,task.returncode,round(d['cpu_percent_one_core'],3),'CPU%',flush=True)
   if task.returncode:raise RuntimeError('Preserve failed run; no retry')
 assert hashlib.sha256(exe.read_bytes()).hexdigest()==identity,'Executable changed during series'
 summary=[]
 for scenario in scenarios:
  cases=[d for d in allruns if d['scenario']==scenario];cost=[d['cpu_percent_one_core'] for d in cases]
  item={'scenario':scenario,'runs':len(cases),'cpu_median_percent_one_core':statistics.median(cost),'cpu_range':[min(cost),max(cost)],'pcm_hash_repeat_exact':len(set(d['pcm_fnv'] for d in cases))==1,'guest_counters_repeat_exact':all(len(set(d[k] for d in cases))==1 for k in ['frames','h8_cycles','h8_instructions','dsp_cycles','dsp_instructions','hdi_reads','hdi_writes','midi_received','flash_fnv','memory_fnv','midi_out_fnv']),'dropped_frames':sum(d['dropped_frames'] for d in cases),'block_compute_over_budget':sum(d['block_compute_over_budget'] for d in cases)}
  if a.phase=='detailed':
   shares=[100*d['h8_sampled_wall_ns']/(d['h8_sampled_wall_ns']+d['dsp_sampled_wall_ns']) for d in cases]
   item.update(h8_sampled_wall_share_median=statistics.median(shares),h8_sampled_wall_share_range=[min(shares),max(shares)],h8_samples=sum(d['h8_sample_count'] for d in cases),dsp_samples=sum(d['dsp_sample_count'] for d in cases))
  summary.append(item)
 (out/'summary.json').write_text(json.dumps(summary,indent=2))
 print(json.dumps(summary,indent=2),flush=True)
