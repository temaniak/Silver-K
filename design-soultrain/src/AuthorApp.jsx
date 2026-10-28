import React, { useState, useEffect, useRef } from 'react';
import catalog from './catalog.json';
import { Knob } from './AuthorControls.jsx';
import { IS_NATIVE, nativeSend } from './nativeBridge.js';
import { NativeBrowser } from './NativeBrowser.jsx';
import { AuthorOptionPanels, OPTION_BLOCKS } from './AuthorOptions.jsx';
import { AuthorExtraPage } from './AuthorPages.jsx';
import { AuthorEffects } from './AuthorEffects.jsx';
import { AuthorWaveform } from './AuthorWaveform.jsx';
import { AuthorBrowser, AuthorPerformance, AuthorSetup, DEMO_PATCHES } from './AuthorWorkflowPages.jsx';
import './author.css';
const base='/author-assets/';
export function AuthorApp(){
 const [view,setView]=useState('author'),[values,setValues]=useState({}),[part,setPart]=useState(1),[pwm,setPwm]=useState([0,0,0]),[pitch,setPitch]=useState([0,0,0]),[filterMod,setFilterMod]=useState(0),[lfoMod,setLfoMod]=useState([0,0]),[mixerSource,setMixerSource]=useState(0),[effect,setEffect]=useState(0),[message,setMessage]=useState(null),[scale,setScale]=useState(1);
 const [openOptions,setOpenOptions]=useState({}),[clockSource,setClockSource]=useState('Host'),[referencePage,setReferencePage]=useState('main'),[page,setPage]=useState('Main');
 const [model,storeModel]=useState('KS'),[performance,setPerformance]=useState(true),[enabled,setEnabled]=useState([true,true,true,true]),[partPatches,setPartPatches]=useState({});
 const [settings,setSettings]=useState({channel:'1',buffer:'Balanced',twoCore:false});
 const specs=catalog[model], localBlocks=OPTION_BLOCKS.filter(name=>name!=='Arpeggiator'||model==='KS');
 const [nativeState,setNativeState]=useState(null);const wasReady=useRef(undefined);
 const allOptions=localBlocks.every(name=>openOptions[name]);
 const setModel=next=>{if(IS_NATIVE){nativeSend({type:'native',action:`model:${next}`});return;}storeModel(next);setFilterMod(0);setPart(1);setOpenOptions({});setMessage(null);};
 const selectedPatch=partPatches[`${model}:${part}`]??(model==='KS'?'ks-init':'k-init');
 const patchName=DEMO_PATCHES.find(p=>p.id===selectedPatch)?.name??'Init';
 const read=(id,owner=part)=>values[`${model}:${owner}:${id}`]??(id==='part_channel'?owner-1:specs[id]?.default)??0;
 const write=(id,v,owner=part)=>{const s=specs[id];if(!s)return; const val=Math.round(Math.min(s.max,Math.max(s.min,v)));if(IS_NATIVE)nativeSend({type:'parameter',id,value:val,part:owner});setValues(c=>({...c,[`${model}:${owner}:${id}`]:val}));setMessage([s.label,s.choices[val]??String(val+s.offset)]);};
 const loadPatch=(patch,owner=part)=>{
   if(IS_NATIVE){if(patch)nativeSend({type:'browser',action:performance?'loadPart':'load',id:patch.id,part:owner});return;}
   if(!patch||patch.model!==model)return;
   setValues(current=>{const next={...current};for(const spec of Object.values(specs))if(spec.group!=='Part')next[`${model}:${owner}:${spec.id}`]=patch.values[spec.id]??spec.default;return next;});
   setPartPatches(current=>({...current,[`${model}:${owner}`]:patch.id}));setMessage(['Preset',patch.name]);
 };
 const stepPatch=direction=>{if(IS_NATIVE){nativeSend({type:'native',action:direction>0?'nextPreset':'previousPreset'});return;}const list=DEMO_PATCHES.filter(p=>p.model===model),index=list.findIndex(p=>p.id===selectedPatch);loadPatch(list[(index+direction+list.length)%list.length]);};
 const setPerformanceMode=active=>{if(IS_NATIVE)nativeSend({type:'performance',value:active});setPerformance(active);if(!active)setPart(1);};
 useEffect(()=>{if(!message)return;const t=setTimeout(()=>setMessage(null),1800);return()=>clearTimeout(t);},[message]);
 useEffect(()=>{const update=()=>setScale(Math.min(IS_NATIVE?2:1,(window.innerWidth-(IS_NATIVE?0:32))/1486));update();window.addEventListener('resize',update);return()=>window.removeEventListener('resize',update);},[]);
 const pos=(x,y,w,h)=>({left:x,top:y,...(w?{width:w}:{}),...(h?{height:h}:{})});
 const k=(id,label,x,y,dark=false)=>specs[id]&&<div className={`ak ${dark?'a-dark':''}`} style={pos(x,y)} key={id}><Knob spec={specs[id]} value={read(id)} onChange={v=>write(id,v)} label={label} owner="SoulTrain"/></div>;
 const label=(text,x,y,cls='')=><div className={`a-label ${cls}`} style={pos(x,y)}>{text}</div>;
 const lamp=(text,on,change,cls='')=><button className={`a-lamp ${on?'lit':''} ${cls}`} aria-pressed={on} onClick={change}><span className="a-lamp-art"><img src={base+(on?'7bdb9.svg':'b1b12.svg')} alt="" /></span><span>{text}</span></button>;
 const lamps=(id,names,x,y,vertical=false)=>specs[id]&&<div data-param={id} className={`a-lamps ${vertical?'column':''}`} style={pos(x,y)}>{names.map((t,i)=> <React.Fragment key={t}>{lamp(t,read(id)===i,()=>write(id,i))}</React.Fragment>)}</div>;
 const menu=(id,text,x,y,w=90,shown=null)=>specs[id]&&<label data-param={id} className="a-menu" style={pos(x,y,w)}><select aria-label={text} value={read(id)} onChange={e=>write(id,Number(e.target.value))}>{(specs[id].choices.length?specs[id].choices:Array.from({length:specs[id].max-specs[id].min+1},(_,i)=>String(i+specs[id].min))).map((t,i)=><option key={i} value={i+specs[id].min}>{shown?.[i]??t}</option>)}</select><span>{text}</span></label>;
 const gear=(name,x,y)=><button className={`a-gear ${openOptions[name]?'a-gear-open':''}`} aria-label={`${name} options`} title={`${name} options`} aria-pressed={Boolean(openOptions[name])} aria-expanded={Boolean(openOptions[name])} style={pos(x,y)} onClick={()=>setOpenOptions(current=>({...current,[name]:!current[name]}))}>⚙</button>;
 const env=(prefix,x)=>['attack','decay','sustain','release'].map((id,i)=>{const full=prefix+id,s=specs[full];return <label data-param={full} className="a-fader" style={pos(x+i*71,305.5)} key={full}><span className="a-fader-track"><img src={base+'6c27a.svg'} alt=""/><i/><img src={base+'6c27a.svg'} alt=""/></span><span className="a-fader-thumb" style={{top:7+(1-read(full)/127)*96}}/><input type="range" aria-label={`${prefix?'Mod':'Amp'} ${id}`} min={s.min} max={s.max} value={read(full)} onChange={e=>write(full,Number(e.target.value))}/><span className="a-fader-label">{id}</span></label>;});

 const selectPart=n=>{if(IS_NATIVE)nativeSend({type:'part',value:n});setPart(n);};
 const setPartEnabled=update=>{const next=typeof update==='function'?update(enabled):update;if(IS_NATIVE)next.forEach((value,index)=>{if(value!==enabled[index])nativeSend({type:'partActive',part:index+1,value});});setEnabled(next);};
 const updateSettings=update=>{const next=typeof update==='function'?update(settings):update;if(IS_NATIVE){if(next.buffer!==settings.buffer)nativeSend({type:'setting',name:'buffer',value:['Low latency','Balanced','Safe','Maximum reserve'].indexOf(next.buffer)});if(next.channel!==settings.channel)nativeSend({type:'setting',name:'channel',value:Number(next.channel)});if(next.twoCore!==settings.twoCore)nativeSend({type:'setting',name:'twoCore',value:next.twoCore});}setSettings(next);};
 useEffect(()=>{if(!IS_NATIVE)return;window.silverReceive=state=>setNativeState(old=>({...old,...state,browser:state.browser??old?.browser}));nativeSend({type:'hello'});return()=>{delete window.silverReceive;};},[]);
 useEffect(()=>{if(!nativeState)return;storeModel(nativeState.model);setPart(nativeState.part);setValues(nativeState.values);setPerformance(nativeState.performance);setEnabled(nativeState.enabled);setClockSource(['Host','Internal','MIDI'][nativeState.clock]);setSettings({channel:String(nativeState.channel),buffer:['Low latency','Balanced','Safe','Maximum reserve'][nativeState.buffer],twoCore:nativeState.twoCore});if(nativeState.ready===false&&wasReady.current!==false)setPage('Setup');else if(nativeState.ready===true&&wasReady.current===false)setPage('Main');wasReady.current=nativeState.ready;},[nativeState]);
 useEffect(()=>{if(IS_NATIVE)nativeSend({type:'page',value:page});},[page]);
 useEffect(()=>{if(!IS_NATIVE)return;let current=null;const end=()=>{if(current)nativeSend({type:'gestureEnd',...current});current=null;};const begin=event=>{const slider=event.target.closest('[role="slider"],input[type="range"]'),param=slider?.closest('[data-param]');if(!param)return;end();const match=slider.getAttribute('aria-label')?.match(/^Part (\d)/);current={id:param.dataset.param,part:match?Number(match[1]):part};nativeSend({type:'gestureBegin',...current});};document.addEventListener('pointerdown',begin);document.addEventListener('pointerup',end);document.addEventListener('pointercancel',end);return()=>{end();document.removeEventListener('pointerdown',begin);document.removeEventListener('pointerup',end);document.removeEventListener('pointercancel',end);};},[part]);
 const blocked=IS_NATIVE&&(!nativeState?.ready||nativeState.loading);
 return <><div className="review-toolbar"><span><b>SoulTrain × Silver-K</b><small>Local interface preview</small></span><div><label className="a-model-picker">Model <select aria-label="Synth model" value={model} onChange={e=>setModel(e.target.value)}><option value="KS">KS</option><option value="K">K-Station</option></select></label><button onClick={()=>{setPage('Main');setOpenOptions({});}}>Front</button><button onClick={()=>{setPage('Main');setOpenOptions(Object.fromEntries(localBlocks.map(name=>[name,true])));}}>Options</button></div></div>
 <div className="author-viewport" style={{height:942*scale+24}}><div className="author-frame" data-model={model} style={{transform:`scale(${scale})`}}>
 <div className="a-texture"/><img hidden={page!=='Main'} className="a-main-plate" src={base+'bf3f7.png'} alt=""/><img className={`a-watermark a-watermark-${model.toLowerCase()}`} data-model-mark={model} src={base+(model==='KS'?'2361c.svg':'model-watermark-k.svg')} alt="" aria-hidden="true"/>
 <div hidden={page!=='Main'}>{[264,405,547].map(y=><i className="a-line" style={pos(14,y,705,1)} key={y}/>)}<i className="a-line" style={pos(851,264,621,1)}/><i className="a-line" style={pos(851,452,621,1)}/><i className="a-line" style={pos(872,575,579,1)}/><i className="a-vline" style={pos(719,120,1,790)}/><i className="a-vline" style={pos(850,120,1,790)}/><i className="a-vline" style={pos(openOptions.Envelopes?1107:1159,264,1,188)}/><i className="a-vline" style={pos(337,687,1,223)}/>
 </div><img className="a-logo" src={base+'author-logo.png'} alt="Silver-K · Novation K-Station / KS emulation"/>
 <div className="a-lcd" aria-live="polite">{message?<>{message[0]}<br/>{message[1]}</>:<>{model==='KS'?'KS':'K-Station'}<br/>{IS_NATIVE?(nativeState?.name||'Load firmware'):patchName}</>}</div>
 <button className="a-nav a-prev" aria-label="Previous preset" onClick={()=>stepPatch(-1)}>▴</button><button className="a-nav a-next" aria-label="Next preset" onClick={()=>stepPatch(1)}>▾</button>
 <div className="a-parts">{[1,2,3,4].map(n=><button key={n} disabled={n!==1&&(model==='K'||!performance)} aria-pressed={part===n} onClick={()=>{selectPart(n);setMessage(['Part',String(n)]);}}>{n}</button>)}</div><div className="a-perf-indicator">{model==='KS'&&lamp(`performance ${performance?'on':'off'}`,performance,()=>setPerformanceMode(!performance))}</div>
 {IS_NATIVE&&nativeState?.drumBank>0&&<div className="a-drum-mode"><button aria-pressed={nativeState.drumEdit} onClick={()=>nativeSend({type:'drumEdit',value:!nativeState.drumEdit})}>Drum Edit {nativeState.drumEdit?'On':'Off'}</button><span>Bank {nativeState.drumBank} · MIDI {nativeState.drumKey}</span></div>}<button className="a-nav a-undo" disabled title="History is unavailable">↶</button><button className="a-nav a-redo" disabled title="History is unavailable">↷</button>
 <nav className="a-pages" aria-label="Skin pages">{['Main','Extra','Browser','Performance','Setup'].map(n=><button key={n} aria-current={page===n?'page':undefined} onClick={()=>setPage(n)}>{n}</button>)}</nav><div hidden={page!=='Main'} inert={blocked}>
 {[1,2,3].map((o,i)=>{const y=127+i*141;const pw=['pulse_width','lfo2_pwm','env_pwm'][pwm[i]],pi=['env_pitch','lfo1_pitch'][pitch[i]];return <React.Fragment key={o}>{label(`Oscillator ${o}${openOptions[`Oscillator ${o}`]?' / Extra':''}`,29,y,'heading')}{gear(`Oscillator ${o}`,690,y+3)}<div hidden={Boolean(openOptions[`Oscillator ${o}`])}><AuthorWaveform model={model} number={o} spec={specs[`osc${o}_wave`]} value={read(`osc${o}_wave`)} onChange={v=>write(`osc${o}_wave`,v)} style={pos(29,y+40,160,52)}/>{lamps(`osc${o}_octave`,['-1','0','+1','+2'],29,y+107)}{k(`osc${o}_semitone`,'semi',197,y+31)}{k(`osc${o}_fine`,'detune',281,y+31)}{k(`osc${o}_${pw}`,['pwm pos','lfo2 pwm','env pwm'][pwm[i]],365,y+31)}<div className="a-lamps column" style={pos(449,y+37)}>{['pwm pos','lfo2 pwm','env pwm'].map((t,j)=><React.Fragment key={t}>{lamp(t,pwm[i]===j,()=>setPwm(c=>c.map((n,l)=>l===i?j:n)))}</React.Fragment>)}</div>{k(`osc${o}_${pi}`,['pitch env','lfo1 pitch'][pitch[i]],533,y+31)}<div className="a-lamps column" style={pos(617,y+37)}>{['pitch env','lfo1 pitch'].map((t,j)=><React.Fragment key={j}>{lamp(t,pitch[i]===j,()=>setPitch(c=>c.map((n,l)=>l===i?j:n)))}</React.Fragment>)}</div><i className="a-bracket" style={pos(449,y+94,39,19)}/><i className="a-bracket" style={pos(617,y+94,39,19)}/></div></React.Fragment>;})}
 <div data-param="osc_sync" className="a-sync" style={pos(342,257)}>{lamp('sync',!!read('osc_sync'),()=>write('osc_sync',read('osc_sync')?0:1))}</div>
 {label('FM Settings / AD Envelope',29,551,'heading')}
 <div>
 {[['fm_level','fm level'],['fm_velocity','fm velocity'],['fm_lfo1','fm lfo1'],['fm_env_depth','fm env'],['fm_attack','attack'],['fm_decay','decay']].filter(([id])=>specs[id]).map(([id,t],i)=>k(id,t,29+i*84,582))}{lamps('fm_trigger',['single','multi'],533,599,true)}
 </div>
 {label('Mixer',761,127,'heading')}{gear('Mixer',823,130)}<div hidden={Boolean(openOptions.Mixer)}>{[['osc1_level','osc 1'],['osc2_level','osc 2'],['osc3_level','osc 3'],[['noise_level','ring_level','external_level'][mixerSource],['noise','ring 1×2','input'][mixerSource]]].map(([id,t],i)=>k(id,t,742,158+i*106))}<div className="a-lamps column" style={pos(742,590)}>{['noise','ring 1×2','input'].map((t,i)=><React.Fragment key={t}>{lamp(t,mixerSource===i,()=>setMixerSource(i))}</React.Fragment>)}</div></div><div data-param="external_to_fx" className="a-lamps" style={pos(742,657)}>{lamp('direct to fx',!!read('external_to_fx'),()=>write('external_to_fx',read('external_to_fx')?0:1))}</div>
 {label('Filter',869,127,'heading')}{lamps('filter_type',['LP','BP','HP'],965,133)}{lamps('filter_slope',['12dB','24dB'],1148,133)}
 {[['cutoff','cutoff'],['resonance','resonance'],['filter_env','mod env'],['filter_drive','overdrive'],['filter_normalise','q-norm'],[['filter_lfo2',...(model==='KS'?['filter_velocity']:[]),'filter_keytrack'][filterMod],['lfo2',...(model==='KS'?['velocity']:[]),'keytrack'][filterMod]]].map(([id,t],i)=>k(id,t,869+i*84,158))}<div className="a-lamps column" style={pos(1377,164)}>{['lfo2',...(model==='KS'?['velocity']:[]),'keytrack'].map((t,i)=><React.Fragment key={t}>{lamp(t,filterMod===i,()=>setFilterMod(i))}</React.Fragment>)}</div>
 {label('Mod Envelope',868,268,'heading')}{label('Amp Envelope',openOptions.Envelopes?1116:1178,268,'heading')}<div hidden={Boolean(openOptions.Envelopes)}>{env('mod_',866)}{env('',1169)}</div>{gear('Envelopes',1433,271)}
 {label('LFO 1 / LFO 2',874,460,'heading')}{gear('LFO',1433,463)}
 <div hidden={Boolean(openOptions.LFO)}>{[1,2].map((n,i)=><React.Fragment key={n}>
   {menu(`lfo${n}_wave`,`lfo${n} wave`,877,496+i*106,143)}{menu(`lfo${n}_sync`,`lfo${n} sync`,1045,496+i*106,70)}
   {k(`lfo${n}_rate`,'rate',1121,476+i*106)}{k(`lfo${n}_delay`,'delay',1205,476+i*106)}
   {model==='KS'&&<>
     {k(`lfo${n}_${lfoMod[i]?'velocity':'start_phase'}`,lfoMod[i]?'vel depth':'start phase',1289,476+i*106)}
     <div className="a-lamps column" style={pos(1377,493+i*106)}>{['s.phase','vel depth'].map((t,j)=><React.Fragment key={t}>{lamp(t,lfoMod[i]===j,()=>setLfoMod(c=>c.map((a,l)=>l===i?j:a)))}</React.Fragment>)}</div>
   </>}
 </React.Fragment>)}</div>
 <div className="a-bottom a-dark">{label('Arpeggiator',37,698,'heading')}<div data-param="arp_on" className="a-lamps" style={pos(153,701)}>{lamp('on/off',!!read('arp_on'),()=>write('arp_on',read('arp_on')?0:1))}</div>{model==='KS'&&gear('Arpeggiator',289,700)}<div hidden={Boolean(openOptions.Arpeggiator)}>{menu('arp_sync','sync',37,734,70)}{menu('arp_octaves','octaves',121,734,70)}{menu('arp_pattern','mode',205,734,105)}{k('arp_rate','tempo',29,804,true)}{k('arp_gate','gate time',113,804,true)}<div className="a-lamps column" style={pos(209,820)}>{[['arp_keysync','key sync'],['arp_latch','latch'],['arp_velocity','pattern velo']].filter(([id])=>specs[id]).map(([id,t])=><span data-param={id} key={id}>{lamp(t,!!read(id),()=>write(id,read(id)?0:1))}</span>)}</div>
</div>
 {label(`Voices / Glide${openOptions.Voices?' / Extra':''}`,365,698,'heading')}<button className="a-drift-link" onClick={()=>setPage('Extra')}>VCO Drift ↗</button>{gear('Voices',690,700)}<div hidden={Boolean(openOptions.Voices)}><div data-param="voice_mode" className="a-voices" style={pos(365,736)}>{['Mono','Mono AG','Poly 1','Poly 2'].map((t,i)=><button key={t} aria-pressed={read('voice_mode')===i} onClick={()=>write('voice_mode',i)}>{t}</button>)}</div>{k('volume','volume',365,804,true)}{k('portamento_time','glide time',449,804,true)}{k('preglide','preglide',533,804,true)}{lamps('portamento_mode',['exp','linear'],625,820,true)}</div>
 {label('Unison',750,698,'heading')}{menu('unison','voices',750,736,70)}{k('unison_detune','detune',742,804,true)}
 <AuthorEffects effect={effect} setEffect={setEffect} specs={specs} read={read} write={write}/>
 </div>
 <AuthorOptionPanels open={openOptions} read={read} write={write} knob={k} menu={menu} lamp={lamp} label={label} specs={specs} model={model}/></div>
 <div hidden={page!=='Extra'} inert={blocked}><AuthorExtraPage specs={specs} read={read} write={write} part={part} model={model}/></div>
 <div hidden={page!=='Browser'}>{IS_NATIVE?<NativeBrowser model={model} data={nativeState?.browser} status={nativeState?.status}/>:<AuthorBrowser model={model} loadPatch={loadPatch} selectedPatch={selectedPatch}/>}</div>
 <div hidden={page!=='Performance'}><AuthorPerformance model={model} specs={specs} readPart={read} writePart={write} selectedPart={part} selectPart={selectPart} active={performance} setActive={setPerformanceMode} enabled={enabled} setEnabled={setPartEnabled} partPatches={[1,2,3,4].map(n=>partPatches[`KS:${n}`]??'ks-init')} loadPatch={loadPatch} nativeState={nativeState} editPart={n=>{selectPart(n);setOpenOptions({});setPage('Main');}}/></div>
 <div hidden={page!=='Setup'}><AuthorSetup model={model} setModel={setModel} settings={settings} setSettings={updateSettings} clockSource={clockSource} setClockSource={value=>{setClockSource(value);if(IS_NATIVE)nativeSend({type:'clock',value:['Host','Internal','MIDI'].indexOf(value)});}} nativeState={nativeState}/></div>
 <footer className="a-footer"><span>Silver-K · {model==='KS'?'KS':'K-Station'} {IS_NATIVE?'· SoulTrain experimental':'preview · UI only'}</span><span>Skin design: SoulTrain</span></footer>

 </div></div><p className="review-note">Local block options stay with their controls. Extra contains Controllers and VCO Settings; Setup contains Clock and plugin settings. Only knob pointers rotate; the caps and13-dot arcs stay fixed.</p>
 </>;
}
