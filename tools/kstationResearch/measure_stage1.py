"""Serial interleaved CPU comparison, after compare_stage1.py admission."""
import argparse,hashlib,json,os,pathlib,statistics,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('firmware',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('phase',choices=['offline','paced']);a=p.parse_args()
a.output.mkdir(exist_ok=False)
assert hashlib.sha256(a.firmware.read_bytes()).hexdigest()=='2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
exes={s:a.build/f'kstation_stage1_{s}' for s in ['reference','candidate']};identities={s:hashlib.sha256(q.read_bytes()).hexdigest() for s,q in exes.items()}
fields=['frames','pcm_fnv','memory_fnv','flash_fnv','h8_instructions','h8_cycles','dsp_instructions','dsp_cycles','hdi_reads','hdi_writes','midi_received','midi_out_fnv','midi_out_count','dropped_frames','before_patch','after_patch','bank','program']
scenarios=['idle','held1','held8','notes','sweep','arp'] if a.phase=='offline' else ['held8','sweep']
runs=[];pairs=[]
for repeat in range(3):
 for scenario in (scenarios if repeat%2==0 else list(reversed(scenarios))):
  modes=['reference','timers','hdi'] if a.phase=='offline' else ['reference','hdi']
  if repeat%2:modes.reverse()
  group={}
  for controller in modes:
   name='reference' if controller=='reference' else 'candidate';prefix=a.output/f'{repeat+1}-{scenario}-{controller}';env=dict(os.environ);env['K_STAGE1_MODE']=controller
   with prefix.with_suffix('.log').open('w') as log:r=subprocess.run([str(exes[name]),str(a.firmware),scenario,'12','off',a.phase,'256',str(prefix),'3'],stdout=log,stderr=subprocess.STDOUT,env=env)
   assert r.returncode==0,f'Failed run {prefix}: preserve evidence, no automatic retry'
   d=json.loads(prefix.with_suffix('.json').read_text());d.update(repeat=repeat+1,controller=controller,executable_sha256=identities[name]);runs.append(d);group[controller]=d
   (a.output/'runs.json').write_text(json.dumps(runs,indent=2));print(repeat+1,scenario,controller,round(d['cpu_percent_one_core'],3),'CPU%',d['block_compute_over_budget'],'compute overruns',flush=True)
  ref=group['reference']
  for mode,d in group.items():
   assert all(d[f]==ref[f] for f in fields),f'Work/audio/state differs for {scenario}/{mode}'
   assert d['dropped_frames']==0 and d['peak']>=0
   if mode=='hdi':assert d['native_wait_steps']>0 and d['interpreted_steps']>0
  pairs.append({'repeat':repeat+1,'scenario':scenario,'costs':{k:v['cpu_percent_one_core'] for k,v in group.items()},'relative_percent':{k:100*(v['process_cpu_seconds']/ref['process_cpu_seconds']-1) for k,v in group.items() if k!='reference'},'byte_hashes_and_guest_counters_match':True})
  (a.output/'pairs.json').write_text(json.dumps(pairs,indent=2))
assert identities=={s:hashlib.sha256(q.read_bytes()).hexdigest() for s,q in exes.items()},'Measured executable changed'
summary=[]
for scenario in scenarios:
 row={'scenario':scenario,'controllers':{},'comparisons':{}}
 for mode in (['reference','timers','hdi'] if a.phase=='offline' else ['reference','hdi']):
  subset=[d for d in runs if d['scenario']==scenario and d['controller']==mode];values=[d['cpu_percent_one_core'] for d in subset]
  row['controllers'][mode]={'runs':len(subset),'cpu_median_percent_one_core':statistics.median(values),'cpu_range':[min(values),max(values)],'block_compute_over_budget':sum(d['block_compute_over_budget'] for d in subset),'max_block_compute_seconds':max(d['max_block_compute_seconds'] for d in subset),'dropped_native_frames':sum(d['dropped_frames'] for d in subset)}
  if mode!='reference':
   changes=[d['relative_percent'][mode] for d in pairs if d['scenario']==scenario];row['comparisons'][mode]={'paired_relative_percent_median':statistics.median(changes),'paired_relative_percent_range':[min(changes),max(changes)],'all_pairs_lower_cpu':all(v<0 for v in changes)}
 summary.append(row)
(a.output/'summary.json').write_text(json.dumps({'phase':a.phase,'cpu_convention':'100%=one logical CPU; getrusage user+system per same12s native-rate audio, boot/setup3swarm excluded','scope':'Direct engine, not DAW/driver/plugin worker. Paced block-compute overruns preclude realtime admission; retain cost data descriptively.','executable_sha256':identities,'scenarios':summary},indent=2))
print('Comparison complete',flush=True)
