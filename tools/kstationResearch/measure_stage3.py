"""Interleaved CPU cost after exact serial stage validation. No trace targets."""
import argparse,hashlib,json,os,pathlib,statistics,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('firmware',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('phase',choices=['offline','paced']);a=p.parse_args();a.output.mkdir(exist_ok=False)
assert hashlib.sha256(a.firmware.read_bytes()).hexdigest()=='2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
targets={'original':('kstation_stage3_original','original'),'stage2':('kstation_stage3_reference','irq'),'serial':('kstation_stage3_candidate','serial')}
identities={k:hashlib.sha256((a.build/v[0]).read_bytes()).hexdigest() for k,v in targets.items()}
fields=['frames','pcm_fnv','memory_fnv','flash_fnv','h8_instructions','h8_cycles','dsp_instructions','dsp_cycles','hdi_reads','hdi_writes','midi_received','midi_out_fnv','midi_out_count','dropped_frames','before_patch','after_patch','bank','program']
scenarios=['idle','held1','held8','notes','sweep','arp','txclock','txqueue'] if a.phase=='offline' else ['held8','sweep'];runs=[];pairs=[]
for repeat in range(3):
 for scenario in scenarios if repeat%2==0 else list(reversed(scenarios)):
  modes=['original','stage2','serial'] if a.phase=='offline' else ['stage2','serial']
  if repeat%2:modes.reverse()
  group={}
  for controller in modes:
   target,mode=targets[controller];prefix=a.output/f'{repeat+1}-{scenario}-{controller}';env=dict(os.environ);env['K_STAGE1_MODE']=mode
   with prefix.with_suffix('.log').open('w') as log:r=subprocess.run([str(a.build/target),str(a.firmware),scenario,'12','off',a.phase,'256',str(prefix),'3'],stdout=log,stderr=subprocess.STDOUT,env=env)
   assert r.returncode==0,f'Failed {prefix}; retain failure, no automatic retry'
   d=json.loads(prefix.with_suffix('.json').read_text());d.update(repeat=repeat+1,controller=controller,executable_sha256=identities[controller],synthetic_transport_fixture=scenario.startswith('tx'));runs.append(d);group[controller]=d
   (a.output/'runs.json').write_text(json.dumps(runs,indent=2));print(repeat+1,scenario,controller,round(d['cpu_percent_one_core'],3),'CPU%',d['block_compute_over_budget'],'compute overruns',flush=True)
  ref=group['stage2']
  for k,d in group.items():assert all(d[f]==ref[f] for f in fields),f'Work/data mismatch {scenario}/{k}'
  assert group['serial']['native_irq_steps']>0 and group['serial']['interpreted_steps']>0
  if scenario.startswith('tx'):assert group['serial']['native_serial_tx_steps']>0 and group['serial']['midi_out_count']>0
  pairs.append({'repeat':repeat+1,'scenario':scenario,'costs':{k:d['cpu_percent_one_core'] for k,d in group.items()},'serial_vs_stage2_percent':100*(group['serial']['process_cpu_seconds']/ref['process_cpu_seconds']-1),'serial_vs_original_percent':100*(group['serial']['process_cpu_seconds']/group['original']['process_cpu_seconds']-1) if 'original' in group else None})
  (a.output/'pairs.json').write_text(json.dumps(pairs,indent=2))
assert identities=={k:hashlib.sha256((a.build/v[0]).read_bytes()).hexdigest() for k,v in targets.items()},'Executable changed during series'
summary=[]
for scenario in scenarios:
 row={'scenario':scenario,'controllers':{}}
 for controller in (['original','stage2','serial'] if a.phase=='offline' else ['stage2','serial']):
  sub=[d for d in runs if d['scenario']==scenario and d['controller']==controller];values=[d['cpu_percent_one_core'] for d in sub]
  row['controllers'][controller]={'cpu_median':statistics.median(values),'cpu_range':[min(values),max(values)],'compute_overruns':sum(d['block_compute_over_budget'] for d in sub),'max_compute_seconds':max(d['max_block_compute_seconds'] for d in sub),'native_ring_drops':sum(d['dropped_frames'] for d in sub)}
 delta=[d['serial_vs_stage2_percent'] for d in pairs if d['scenario']==scenario];row['serial_vs_stage2']={'paired_median_percent':statistics.median(delta),'range':[min(delta),max(delta)],'all_pairs_lower':all(x<0 for x in delta)}
 summary.append(row)
(a.output/'summary.json').write_text(json.dumps({'phase':a.phase,'scope':'getrusage process user+system,100%=oneCPU; native-rate ordinary-priority direct engine, not DAW; exclude boot/setup3swarm/IO. Offline triplets original/stage2/serial; paced adjacent stage2/serial AB/BA. Preserve all overruns, no realtime pass inferred.','synthetic_transport_fixtures':['txclock','txqueue'] if a.phase=='offline' else [],'identities':identities,'scenarios':summary},indent=2));print('CPU series complete',flush=True)
