import React, { useState } from 'react';
import { ParamKnob, ParamMenu, ParamRange } from './AuthorParameters.jsx';

// Synthetic UI fixtures, never the user's private banks or firmware.
export const DEMO_PATCHES = [
  { id:'ks-init',model:'KS',name:'Silver Init',type:'Init',tags:'clean',bank:'A',number:'001',source:'Demo Factory',values:{} },
  { id:'ks-bass',model:'KS',name:'Silver Bass',type:'Bass',tags:'mono, warm',bank:'A',number:'002',source:'Demo Factory',values:{cutoff:68,resonance:22,voice_mode:0,osc2_level:76} },
  { id:'ks-pad',model:'KS',name:'Soul Pad',type:'Pad',tags:'wide, soft',bank:'A',number:'003',source:'Demo Factory',values:{attack:78,release:90,chorus:65,random_detune:18} },
  { id:'ks-arp',model:'KS',name:'Night Sequence',type:'Arp',tags:'rhythmic',bank:'B',number:'001',source:'Demo User',values:{arp_on:1,arp_pattern:2,arp_rhythm:8} },
  { id:'k-init',model:'K',name:'K Init',type:'Init',tags:'clean',bank:'A',number:'001',source:'Demo Factory',values:{} },
  { id:'k-bass',model:'K',name:'K Round Bass',type:'Bass',tags:'mono, round',bank:'A',number:'002',source:'Demo Factory',values:{cutoff:60,voice_mode:0,osc2_level:50} },
  { id:'k-pad',model:'K',name:'K Air Pad',type:'Pad',tags:'wide, slow',bank:'A',number:'003',source:'Demo Factory',values:{attack:80,release:88,chorus:45,random_detune:12} },
  { id:'k-lead',model:'K',name:'K Soft Lead',type:'Lead',tags:'glide',bank:'B',number:'001',source:'Demo User',values:{voice_mode:1,portamento_time:32} },
];
const notes = Array.from({length:128},(_,n)=>`${['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'][n%12]}${Math.floor(n/12)-1} · ${n}`);

export function AuthorBrowser({ model, loadPatch, selectedPatch }) {
  const [query,setQuery] = useState(''),[source,setSource] = useState('All'),[type,setType] = useState('All'),[favouritesOnly,setFavouritesOnly] = useState(false),[favourites,setFavourites] = useState({});
  const rows=DEMO_PATCHES.filter(p=>p.model===model&&(source==='All'||source===p.source)&&(type==='All'||type===p.type)&&(!favouritesOnly||favourites[p.id])&&`${p.name} ${p.tags}`.toLowerCase().includes(query.toLowerCase()));
  return <section className="a-shared-page a-browser-page" aria-label="Browser page">
    <header className="a-shared-heading"><h2>Browser</h2><span>Демонстрационная библиотека · {model==='KS'?'KS':'K-Station'}</span></header>
    <div className="a-browser-filters"><label>Search<input aria-label="Search patches" placeholder="Название или тег" value={query} onChange={e=>setQuery(e.target.value)}/></label>
      <label>Source<select aria-label="Patch source" value={source} onChange={e=>setSource(e.target.value)}>{['All','Demo Factory','Demo User'].map(x=><option key={x}>{x}</option>)}</select></label>
      <label>Type<select aria-label="Patch type" value={type} onChange={e=>setType(e.target.value)}>{['All','Init','Bass','Pad','Arp','Lead'].map(x=><option key={x}>{x}</option>)}</select></label>
      <button aria-pressed={favouritesOnly} onClick={()=>setFavouritesOnly(v=>!v)}>★ Favourites</button><span>{rows.length} patches</span>
    </div>
    <div className="a-patch-table-scroll"><table className="a-patch-table"><thead><tr>{['Favourite','Name','Model','Type','Tags','Source','Bank','No.',''].map((x,i)=><th key={i}>{x}</th>)}</tr></thead><tbody>
      {rows.map(p=><tr key={p.id} className={selectedPatch===p.id?'a-selected-row':''}><td><button aria-label={`Favourite ${p.name}`} aria-pressed={Boolean(favourites[p.id])} onClick={()=>setFavourites(v=>({...v,[p.id]:!v[p.id]}))}>{favourites[p.id]?'★':'☆'}</button></td><td>{p.name}</td><td>{p.model}</td><td>{p.type}</td><td>{p.tags}</td><td>{p.source}</td><td>{p.bank}</td><td>{p.number}</td><td><button onClick={()=>loadPatch(p)}>Load {p.name}</button></td></tr>)}
    </tbody></table>{!rows.length&&<p className="a-empty-results">Ничего не найдено. Измените поиск или фильтры.</p>}</div>
    <p className="a-browser-caption">Load применяет демонстрационный звук к выбранной партии. Исходные банки и ROM в этот стенд не загружаются.</p>
  </section>;
}

export function AuthorPerformance({ model, specs, readPart, writePart, selectedPart, selectPart, active, setActive, enabled, setEnabled, partPatches, loadPatch, editPart }) {
  const supported=model==='KS';
  return <section className="a-shared-page a-performance-page" aria-label="Performance page">
    <header className="a-shared-heading"><h2>Performance</h2><div><span>{supported?'4 parts · демонстрация':'K-Station · один тембр'}</span><button disabled={!supported} aria-pressed={supported&&active} onClick={()=>setActive(!active)}>Performance {supported&&active?'On':'Off'}</button></div></header>
    {!supported?<div className="a-settings-card"><h3>K-Station</h3><p>Четырёхчастный Performance доступен в модели KS. K-Station редактируется как один звук.</p></div>:<div className="a-performance-grid">
      {[1,2,3,4].map(n=>{
        const controls={specs,read:id=>readPart(id,n),write:(id,v)=>writePart(id,v,n),owner:`Part ${n}`};
        const rangeWrite=(id,v)=>{writePart(id,v,n);if(id==='part_low'&&v>readPart('part_high',n))writePart('part_high',v,n);if(id==='part_high'&&v<readPart('part_low',n))writePart('part_low',v,n);};
        return <section key={n} className={`a-settings-card a-part-channel ${selectedPart===n?'a-part-selected':''}`} aria-label={`Part ${n}`}>
          <header><button className="a-part-heading" disabled={!active} onClick={()=>selectPart(n)}>Part {n}</button><button aria-label={`Part ${n} active`} disabled={!active} aria-pressed={enabled[n-1]} onClick={()=>setEnabled(v=>v.map((on,i)=>i===n-1?!on:on))}>{enabled[n-1]?'On':'Off'}</button></header>
          <fieldset disabled={!active||!enabled[n-1]} inert={!active||!enabled[n-1]}>
            <label className="a-param-menu"><span>Preset</span><select aria-label={`Part ${n} preset`} value={partPatches[n-1]} onChange={e=>loadPatch(DEMO_PATCHES.find(p=>p.id===e.target.value),n)}>{DEMO_PATCHES.filter(p=>p.model==='KS').map(p=><option key={p.id} value={p.id}>{p.name}</option>)}</select></label>
            <h4>Routing</h4><div className="a-part-pair"><ParamMenu id="part_channel" label="MIDI channel" {...controls}/><ParamMenu id="part_output" label="Output" {...controls}/></div>
            <h4>Keyboard range</h4><div className="a-part-pair"><ParamMenu id="part_low" label="Low note" labels={notes} {...controls} write={(id,v)=>rangeWrite(id,v)}/><ParamMenu id="part_high" label="High note" labels={notes} {...controls} write={(id,v)=>rangeWrite(id,v)}/></div>
            <div className="a-note-range"><i style={{left:`${readPart('part_low',n)/128*100}%`,width:`${(readPart('part_high',n)-readPart('part_low',n)+1)/128*100}%`}}/></div>
            <h4>Tuning</h4><div className="a-page-knobs"><ParamKnob id="part_transpose" label="transpose" {...controls}/><ParamKnob id="part_fine" label="fine tune" {...controls}/></div>
            <ParamMenu id="part_velocity" label="Velocity curve" {...controls}/><ParamRange id="volume" label="Volume" {...controls}/>
            <button className="a-edit-part" onClick={()=>editPart(n)}>Edit sound · Part {n}</button>
          </fieldset>
        </section>;
      })}
    </div>}
  </section>;
}

export function AuthorSetup({ model, setModel, settings, setSettings, clockSource, setClockSource }) {
  const setting=(key,value)=>setSettings(v=>({...v,[key]:value}));
  return <section className="a-shared-page a-setup-page" aria-label="Setup page">
    <header className="a-shared-heading"><h2>Setup</h2><span>Демонстрационный режим · без ROM и аудио</span></header>
    <div className="a-setup-grid">
      <section className="a-settings-card"><h3>Synth & MIDI</h3>
        <label className="a-param-menu"><span>Model</span><select aria-label="Setup model" value={model} onChange={e=>setModel(e.target.value)}><option value="KS">KS</option><option value="K">K-Station</option></select></label>
        <label className="a-param-menu"><span>Clock source</span><select aria-label="Clock source" value={clockSource} onChange={e=>setClockSource(e.target.value)}>{['Host','Internal','MIDI'].map(v=><option key={v}>{v}</option>)}</select></label>
        <label className="a-param-menu"><span>Control MIDI channel</span><select aria-label="Control MIDI channel" value={settings.channel} onChange={e=>setting('channel',e.target.value)}>{Array.from({length:16},(_,i)=><option key={i+1}>{i+1}</option>)}</select></label>
        <p>Источник Clock общий для экземпляра. Деления LFO, эффектов и арпеджиатора задаются в их блоках.</p>
      </section>
      <section className="a-settings-card"><h3>Audio & Processing</h3>
        <label className="a-param-menu"><span>Internal audio buffer</span><select aria-label="Audio buffer profile" value={settings.buffer} onChange={e=>setting('buffer',e.target.value)}>{['Low latency','Balanced','Safe','Maximum reserve'].map(v=><option key={v}>{v}</option>)}</select></label>
        <label className="a-check-setting"><input type="checkbox" disabled={model!=='KS'} checked={model==='KS'&&settings.twoCore} onChange={e=>setting('twoCore',e.target.checked)}/>KS: two CPU cores · experimental</label>
        <p>В веб-стенде сохраняется только выбор. Настройки звука применяются в нативном плагине.</p>
      </section>
      <section className="a-settings-card"><h3>Data & Skin</h3><dl className="a-data-locations"><dt>ROMs</dt><dd>{model==='KS'?'Firmware · Wave ROM · совместимые Data MIDI':'Firmware K-Station'}</dd><dt>Patches</dt><dd>Пользовательские библиотеки</dd><dt>Skins</dt><dd>SoulTrain · Inter / Pixelmix</dd></dl><p>Файловые операции, загрузка ROM и диагностика доступны в нативном плагине.</p></section>
    </div>
  </section>;
}
