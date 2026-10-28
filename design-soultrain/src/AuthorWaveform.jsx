import React from 'react';

const shortNames = ['Sine','Triangle','Saw','Pulse','White','HP Noise','BP Noise','HP × BP','Organ','Harp','E.Piano','Slap Bass','Rhodes','Rh.Tine','Wurli','Clav','A.Bass','Kick 1','Kick 2','Kick 3','Snare 1','Snare 2','Snare 3','Tom','Rim','Sticks','Closed HH','Open HH','Tamb','Conga','Cowbell','Cymbal'];
const shapes = ['M2 13 C8 -1 14 -1 20 13 S32 27 38 13','M2 22 L11 4 L29 22 L38 4','M2 22 L38 4 V22','M2 22 V4 H20 V22 H38 V4'];
function WaveIcon({ value }) {
  return <svg className="a-wave-icon" viewBox="0 0 40 26" aria-hidden="true"><path d={shapes[value]} fill="none" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round"/></svg>;
}
export function AuthorWaveform({ model, number, spec, value, onChange, style }) {
  const name=`OSC ${number} Waveform`;
  if(model==='K')return <div className="a-wave-buttons" role="group" aria-label={name} data-param={spec.id} style={style}>
    {spec.choices.map((label,index)=><button key={label} aria-label={`OSC ${number} ${label} wave`} aria-pressed={value===index} tabIndex={value===index?0:-1} onClick={()=>onChange(index)} onKeyDown={event=>{
      const next={ArrowRight:(index+1)%4,ArrowDown:(index+1)%4,ArrowLeft:(index+3)%4,ArrowUp:(index+3)%4,Home:0,End:3}[event.key];
      if(next!==undefined){event.preventDefault();onChange(next);event.currentTarget.parentElement.children[next].focus();}
    }}><WaveIcon value={index}/><span>{label}</span></button>)}
  </div>;
  return <div className={`a-wave-display ${value<4?'a-wave-analog':''}`} data-param={spec.id} style={style}>
    <span className="a-wave-readout" aria-hidden="true">{value<4&&<WaveIcon value={value}/>}<span>{shortNames[value]}</span></span>
    <select className="a-wave-select" aria-label={name} title={spec.choices[value]} value={value} onChange={e=>onChange(Number(e.target.value))}>
      {spec.choices.map((label,index)=><option key={label} value={index}>{label}</option>)}
    </select>
  </div>;
}
