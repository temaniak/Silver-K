import React, { useEffect, useState, useRef } from 'react';
import { nativeSend } from './nativeBridge.js';
const send=(action,extra={})=>nativeSend({type:'browser',action,...extra});
function FilterInput({label,value,action,placeholder}) {
 const [text,setText]=useState(value??'');const input=useRef(null);
 useEffect(()=>{if(document.activeElement!==input.current)setText(value??'');},[value]);
 return <label>{label}<input ref={input} aria-label={label} placeholder={placeholder} value={text} onChange={e=>{setText(e.target.value);send(action,{value:e.target.value});}}/></label>;
}
export function NativeBrowser({model,data,status}) {
 const [selected,setSelected]=useState(''),[tags,setTags]=useState(''),[offset,setOffset]=useState(0);const scroll=useRef(null);
 const rows=data?.rows??[],row=rows.find(p=>p.id===selected)??rows.find(p=>p.selected),rowHeight=38;
 useEffect(()=>{if(row){setSelected(row.id);setTags(row.tags);}},[row?.id,row?.tags]);
 useEffect(()=>{setOffset(0);if(scroll.current)scroll.current.scrollTop=0;},[data?.query,data?.model,data?.source,data?.bank,data?.tagFilter,data?.favouritesOnly,data?.allSlots]);
 const load=p=>{if(!p)return;setSelected(p.id);send('load',{id:p.id});};
 const field=(key,label)=><label>{label}<select aria-label={label} value={data?.[key]??1} onChange={e=>send(key,{value:Number(e.target.value)})}>{(data?.[key==='model'?'models':key==='source'?'sources':'banks']??[]).map(x=><option key={x.value} value={x.value}>{x.label}</option>)}</select></label>;
 const columns=[['name','Name',1],['model','Instrument',2],['type','Type',3],['tags','Tags',4],['compatibility','Compatibility',5],['source','Source',6],['bank','Bank',8],['number','No.',7]];
 const first=Math.max(0,Math.floor(offset/rowHeight)-6),visible=rows.slice(first,first+26),last=first+visible.length;
 return <section className="a-shared-page a-browser-page a-native-browser" aria-label="Browser page">
 <header className="a-shared-heading"><h2>Browser</h2><span>{data?.loading?'Updating library…':`${rows.length} patches`} · {model==='KS'?'KS':'K-Station'}</span></header>
 <div className="a-browser-filters"><FilterInput label="Search patches" placeholder="Patch name" value={data?.query} action="query"/>{field('model','Instrument')}{field('source','Source')}<FilterInput label="Filter tags" value={data?.tagFilter} action="tagFilter"/></div>
 <div className="a-browser-bank-row">{model==='KS'&&field('bank','KS bank')}<button aria-pressed={data?.favouritesOnly??false} onClick={()=>send('favouritesOnly',{value:!data?.favouritesOnly})}>★ Favourites</button><label className="a-inline-check"><input type="checkbox" checked={data?.allSlots??false} onChange={e=>send('allSlots',{value:e.target.checked})}/>Show all bank slots</label></div>
 <div className="a-browser-actions">{[['addFile','Add file…'],['addFolder','Add folder…'],['rescan','Rescan'],['removeSource','Remove source'],['saveCurrent','Save current…']].map(([action,label])=><button key={action} disabled={action==='removeSource'&&!(data?.source>1)} onClick={()=>send(action)}>{label}</button>)}</div>
 <div className="a-patch-table-scroll" ref={scroll} onScroll={e=>setOffset(e.currentTarget.scrollTop)} tabIndex={0} aria-label="Patch list" onKeyDown={e=>{if(e.target!==e.currentTarget)return;const at=rows.findIndex(p=>p.id===row?.id);if(e.key==='ArrowDown'||e.key==='ArrowUp'){e.preventDefault();const n=Math.min(rows.length-1,Math.max(0,at+(e.key==='ArrowDown'?1:-1)));load(rows[n]);scroll.current.scrollTop=Math.max(0,n*rowHeight-4*rowHeight);}if(e.key==='Enter')load(row);}}>
 <table className="a-patch-table"><thead><tr><th>★</th>{columns.map(([key,label,id])=><th key={key}><button aria-label={`Sort by ${label}`} onClick={()=>send('sort',{column:id,ascending:data?.sortColumn===id?!data.sortAscending:true})}>{label}{data?.sortColumn===id?(data.sortAscending?' ▴':' ▾'):''}</button></th>)}</tr></thead><tbody>
 {first>0&&<tr aria-hidden="true"><td colSpan={9} style={{height:first*rowHeight,padding:0}}/></tr>}
 {visible.map(p=><tr key={p.id} className={row?.id===p.id?'a-selected-row':''} style={{height:rowHeight}} onClick={()=>load(p)}><td><button aria-label={`Favourite ${p.name}`} aria-pressed={p.favourite} onClick={e=>{e.stopPropagation();send('favourite',{id:p.id,value:!p.favourite});}}>{p.favourite?'★':'☆'}</button></td>{columns.map(([key])=><td title={p[key]} key={key}>{p[key]}</td>)}</tr>)}
 {last<rows.length&&<tr aria-hidden="true"><td colSpan={9} style={{height:(rows.length-last)*rowHeight,padding:0}}/></tr>}
 </tbody></table>{!rows.length&&<p className="a-empty-results">{data?.loading?'Loading library…':'No patches match the filters.'}</p>}</div>
 <div className="a-browser-selected"><strong>{row?.name??'Select a patch'}</strong><label>Tags<input aria-label="Selected patch tags" value={tags} disabled={!row} onChange={e=>setTags(e.target.value)} onKeyDown={e=>{if(e.key==='Enter'&&row)send('tags',{id:row.id,value:tags});}}/></label><button disabled={!row} onClick={()=>send('tags',{id:row.id,value:tags})}>Save tags</button><button disabled={!row} onClick={()=>send('export',{id:row.id})}>Export .syx…</button></div>
 <p className="a-browser-caption" role="status">{data?.status||status||'Choose a patch to load it into the selected part.'}</p>
 </section>;
}
