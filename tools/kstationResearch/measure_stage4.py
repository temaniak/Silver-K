"""Interleaved CPU cost after exact Timer1/RX stage validation. No trace targets."""
import argparse,hashlib,json,os,pathlib,statistics,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('firmware',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('phase',choices=['offline','paced']);a=p.parse_args();a.output.mkdir(exist_ok=False)
assert hashlib.sha256(a.firmware.read_bytes()).hexdigest()=='2ad5409f511e96fbd996901f993f8765ac226efc8930253a76f06edf012f2f07'
targets={'original':('kstation_stage4_original','original'),'stage3':('kstation_stage4_reference','serial'),'native':('kstation_stage4_candidate','timer_rx')}
identities={k:hashlib.sha256((a.build/v[0]).read_bytes()).hexdigest() for k,v in targets.items()}
fields=['frames','pcm_fnv','memory_fnv','flash_fnv','h8_instructions','h8_cycles','dsp_instructions','dsp_cycles','hdi_reads','hdi_writes','midi_received','midi_out_fnv','midi_out_count','dropped_frames','before_patch','after_patch','bank','program','before_clock_source','after_clock_source','before_clock_route','after_clock_route']
scenarios=['idle','held1','held8','notes','sweep','arp','clockext','clockjitter'] if a.phase=='offline' else ['held8','sweep','clockext'];runs=[];pairs=[]
for repeat in range(3):
 for scenario in scenarios if repeat%2==0 else list(reversed(scenarios)):
  modes=['original','stage3','native'] if a.phase=='offline' else ['stage3','native']
  if repeat%2:modes.reverse()
  group={}
  for controller in modes:
   target,mode=targets[controller];prefix=a.output/f'{repeat+1}-{scenario}-{controller}';env=dict(os.environ);env['K_STAGE1_MODE']=mode
   with prefix.with_suffix('.log').open('w') as log:r=subprocess.run([str(a.build/target),str(a.firmware),scenario,'12','off',a.phase,'256',str(prefix),'3'],stdout=log,stderr=subprocess.STDOUT,env=env)
   assert r.returncode==0,f'Failed {prefix}; retain failure, no automatic retry'
   d=json.loads(prefix.with_suffix('.json').read_text());d.update(repeat=repeat+1,controller=controller,executable_sha256=identities[controller],synthetic_transport_fixture=scenario.startswith('tx'));runs.append(d);group[controller]=d
   (a.output/'runs.json').write_text(json.dumps(runs,indent=2));print(repeat+1,scenario,controller,round(d['cpu_percent_one_core'],3),'CPU%',d['block_compute_over_budget'],'compute overruns',flush=True)
  ref=group['stage3']
  for k,d in group.items():assert all(d[f]==ref[f] for f in fields),f'Work/data mismatch {scenario}/{k}'
  assert group['native']['native_irq_steps']>0 and group['native']['interpreted_steps']>0
  assert group['native']['native_timer1_entries']>0 and group['native']['native_clock_steps']>0
  if scenario in ['clockext','clockjitter']:assert group['native']['native_rx_clock_requests']>0 and group['native']['native_clock_sync_updates']>0 and group['native']['after_clock_source']==2
  pairs.append({'repeat':repeat+1,'scenario':scenario,'costs':{k:d['cpu_percent_one_core'] for k,d in group.items()},'native_vs_stage3_percent':100*(group['native']['process_cpu_seconds']/ref['process_cpu_seconds']-1),'native_vs_original_percent':100*(group['native']['process_cpu_seconds']/group['original']['process_cpu_seconds']-1) if 'original' in group else None})
  (a.output/'pairs.json').write_text(json.dumps(pairs,indent=2))
assert identities=={k:hashlib.sha256((a.build/v[0]).read_bytes()).hexdigest() for k,v in targets.items()},'Executable changed during series'
summary=[]
for scenario in scenarios:
 row={'scenario':scenario,'controllers':{}}
 for controller in (['original','stage3','native'] if a.phase=='offline' else ['stage3','native']):
  sub=[d for d in runs if d['scenario']==scenario and d['controller']==controller];values=[d['cpu_percent_one_core'] for d in sub]
  row['controllers'][controller]={'cpu_median':statistics.median(values),'cpu_range':[min(values),max(values)],'compute_overruns':sum(d['block_compute_over_budget'] for d in sub),'max_compute_seconds':max(d['max_block_compute_seconds'] for d in sub),'native_ring_drops':sum(d['dropped_frames'] for d in sub)}
 delta=[d['native_vs_stage3_percent'] for d in pairs if d['scenario']==scenario];row['native_vs_stage3']={'paired_median_percent':statistics.median(delta),'range':[min(delta),max(delta)],'all_pairs_lower':all(x<0 for x in delta)}
 summary.append(row)
(a.output/'summary.json').write_text(json.dumps({'phase':a.phase,'scope':'getrusage process user+system,100%=oneCPU; native-rate ordinary-priority direct engine, not DAW; exclude boot/setup3swarm/IO. Offline triplets original/stage3/native; paced adjacent stage3/native AB/BA. Preserve all overruns, no realtime pass inferred.','synthetic_transport_fixtures':[],'identities':identities,'scenarios':summary},indent=2));print('CPU series complete',flush=True)
