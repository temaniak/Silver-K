"""Count exclusive samples once, inheriting H8 ownership for all descendants.

Unknown JIT frames remain unknown except when nested beneath DSP::step. The
parser never sums recursive inclusive counts. All-thread sums are reported.
"""
import argparse,collections,json,pathlib,re
parser=argparse.ArgumentParser();parser.add_argument('root',type=pathlib.Path);parser.add_argument('--overlap-scenarios',nargs='*',default=[]);args=parser.parse_args()
root=args.root;results=[]
for file in sorted(root.glob('*.sample.txt')):
 text=file.read_text();body=text.split('Call graph:\n',1)[1].split('Total number in stack',1)[0]
 nodes=[];stack=[]
 for line in body.splitlines():
  m=re.match(r'^([ +!:|]*)(\d+) (.*)',line)
  if not m:continue
  depth=len(m[1]);count=int(m[2]);name=m[3]
  while stack and nodes[stack[-1]]['depth']>=depth:stack.pop()
  parent=stack[-1] if stack else None
  node={'depth':depth,'count':count,'self':count,'name':name,'parent':parent,'kind':nodes[parent]['kind'] if parent is not None else 'other'}
  if 'kstation::H8::Impl::exec(' in name or 'kstation::H8::exec(' in name:node['kind']='h8_inclusive'
  elif 'kstation::DSP::step(' in name:node['kind']='dsp_identified'
  elif '???' in name and node['kind']=='other':node['kind']='unresolved_jit_or_other'
  if parent is not None:nodes[parent]['self']-=count
  nodes.append(node);stack.append(len(nodes)-1)
 assert all(n['self']>=0 for n in nodes),file
 total=sum(n['count'] for n in nodes if n['parent'] is None)
 bins=collections.Counter();timers=0
 for i,n in enumerate(nodes):
  bins[n['kind']]+=n['self']
  if n['self']:
   chain=[];cursor=i
   while cursor is not None:chain.append(nodes[cursor]['name']);cursor=nodes[cursor]['parent']
   if n['kind']=='h8_inclusive' and any('Timers::tick()' in name for name in chain):timers+=n['self']
 assert sum(bins.values())==total
 scenario=file.stem.split('.')[0]
 overlap=scenario in args.overlap_scenarios
 results.append({'scenario':scenario,'total_thread_samples':total,'exclusive_samples_by_attribution':dict(bins),'h8_inclusive_percent':100*bins['h8_inclusive']/total,'h8_timer_tick_percent_of_total':100*timers/total,'h8_timer_tick_percent_of_h8':100*timers/bins['h8_inclusive'],'source':str(file),'other_profile_overlap_possible':overlap,'limitation':'6s offline sample; symbols for JIT may be unresolved, inclusive H8 roots exclude caller self.'+(' Some final diagnostic-only third-repeat cases overlapped: corroborative only.' if overlap else '')})
(root/'attribution.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
