import React, { useEffect, useRef } from 'react';
const clamp = (n, lo, hi) => Math.max(lo, Math.min(hi, n));
export function Knob({ spec, value, onChange, label, owner = '', compact = false }) {
  const dial = useRef(null), live = useRef(null), drag = useRef(null);
  live.current = { spec, value, onChange };
  const shown = value + spec.offset;
  const angle = -135 + (value - spec.min) / (spec.max - spec.min || 1) * 270;
  useEffect(() => {
    const el = dial.current;
    const wheel = e => { e.preventDefault(); const { spec: s, value: v, onChange: change } = live.current; change(clamp(v - Math.sign(e.deltaY), s.min, s.max)); };
    el.addEventListener('wheel', wheel, { passive: false });
    return () => el.removeEventListener('wheel', wheel);
  }, []);
  return <div className={`knob-control ${compact ? 'trimmer-control' : ''}`} data-param={spec.id}>
    <div ref={dial} className="dial" role="slider" tabIndex={0} aria-label={`${owner} ${label}`} aria-valuemin={spec.min + spec.offset} aria-valuemax={spec.max + spec.offset} aria-valuenow={shown}
      title={`${label}: ${shown} · drag, scroll or arrow keys`}
      onPointerDown={e => { e.currentTarget.setPointerCapture(e.pointerId); drag.current = { y: e.clientY, value }; }}
      onPointerMove={e => { if (drag.current) onChange(clamp(Math.round(drag.current.value + (drag.current.y - e.clientY) * (e.shiftKey ? .12 : .65)), spec.min, spec.max)); }}
      onPointerUp={() => { drag.current = null; }} onPointerCancel={() => { drag.current = null; }} onDoubleClick={() => onChange(spec.default)}
      onKeyDown={e => { const step = e.shiftKey ? 10 : 1; const next = ({ ArrowUp: value + step, ArrowRight: value + step, ArrowDown: value - step, ArrowLeft: value - step, Home: spec.min, End: spec.max })[e.key]; if (next !== undefined) { e.preventDefault(); onChange(clamp(next, spec.min, spec.max)); } }}>
      <img src={compact ? '/assets/mini-knob.png' : '/assets/knob.png'} alt="" draggable="false" style={{ transform: `rotate(${angle}deg)` }} />
    </div>
    {!compact && <label htmlFor={`value-${owner}-${spec.id}`} className="knob-label">{label}</label>}
    {compact ? <output className="trimmer-value" aria-label={`${owner} ${label} value`}>{shown}</output> : <input id={`value-${owner}-${spec.id}`} className="knob-value" type="number" aria-label={`${owner} ${label} value`} min={spec.min + spec.offset} max={spec.max + spec.offset} value={shown} onChange={e => onChange(clamp(Number(e.target.value) - spec.offset, spec.min, spec.max))} />}
  </div>;
}
export function ButtonCap({ lit = false }) {
  return <img className="hardware-button-cap" src={`/assets/novation-button-${lit ? 'on' : 'off'}.png`} alt="" draggable="false" />;
}
export function Lamps({ labels, value, onChange, name, vertical = false }) {
  return <div className={`lamps ${vertical ? 'vertical' : ''}`} role="group" aria-label={name}>{labels.map((text, i) => <button key={text} className={`lamp-button ${value === i ? 'selected' : ''}`} aria-pressed={value === i} onClick={() => onChange(i)} title={`${name}: ${text}`}><ButtonCap lit={value === i} /><span>{text}</span></button>)}</div>;
}
export function Menu({ label, choices, value, onChange, owner = '', inline = false }) {
  return <label className={`menu-field ${inline ? 'inline' : ''}`}><span>{label}</span><select aria-label={`${owner} ${label}`.trim()} value={value} onChange={e => onChange(Number(e.target.value))}>{choices.map((choice, i) => <option value={i} key={choice}>{choice}</option>)}</select></label>;
}
export function Fader({ spec, value, onChange, label, owner }) {
  const ratio = 1 - (value - spec.min) / (spec.max - spec.min || 1);
  const position = `calc(${ratio * 100}% + ${5 - ratio * 11}px)`;
  return <div className="fader-control" data-param={spec.id}><label htmlFor={`fader-${owner}-${spec.id}`}>{label}</label><div className="fader-body"><img src="/assets/fader-track.png" className="fader-track" alt="" /><img src="/assets/fader-cap.png" className="fader-cap" alt="" style={{ top: position }} /><input id={`fader-${owner}-${spec.id}`} type="range" aria-label={`${owner} ${label}`} min={spec.min} max={spec.max} value={value} onChange={e => onChange(Number(e.target.value))} /></div><output>{value + spec.offset}</output></div>;
}

export function ExtraSlider({ spec, value, onChange, label, owner }) {
  const shown = value + spec.offset;
  return <div className="extra-slider">
    <input type="range" aria-label={`${owner} ${label}`} min={spec.min + spec.offset} max={spec.max + spec.offset} value={shown} onChange={e => onChange(Number(e.target.value) - spec.offset)} />
    <output className="trimmer-value" aria-label={`${owner} ${label} value`}>{shown}</output>
  </div>;
}
