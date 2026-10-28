import React from 'react';
import { Knob } from './AuthorControls.jsx';

export function ParamKnob({ id, label, specs, read, write, owner = 'SoulTrain' }) {
  const spec = specs[id];
  return spec ? <Knob spec={spec} value={read(id)} onChange={value => write(id, value)} label={label || spec.label} owner={owner}/> : null;
}
export function ParamMenu({ id, label, specs, read, write, labels, owner = '' }) {
  const spec = specs[id];
  if (!spec) return null;
  const entries = Array.from({ length: spec.max - spec.min + 1 }, (_, i) => i + spec.min);
  return <label className="a-param-menu" data-param={id}><span>{label || spec.label}</span><select aria-label={`${owner} ${label || spec.label}`.trim()} value={read(id)} onChange={e => write(id, Number(e.target.value))}>
    {entries.map((value, i) => <option key={value} value={value}>{labels?.[i] ?? spec.choices[i] ?? String(value + spec.offset)}</option>)}
  </select></label>;
}
export function ParamRange({ id, label, specs, read, write, owner = '' }) {
  const spec = specs[id];
  if (!spec) return null;
  return <label className="a-param-range" data-param={id}><span>{label || spec.label}<output>{read(id) + spec.offset}</output></span><input type="range" aria-label={`${owner} ${label || spec.label}`.trim()} min={spec.min} max={spec.max} value={read(id)} onChange={e => write(id, Number(e.target.value))}/></label>;
}
