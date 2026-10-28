import React from 'react';
import { ParamKnob, ParamMenu } from './AuthorParameters.jsx';
const sources = [['wheel', 'Mod Wheel'], ['breath', 'Breath'], ['aftertouch', 'Aftertouch']];
const targets = [['osc_pitch', 'pitch · osc 1–3'], ['osc_lfo1', 'lfo1 → pitch'], ['filter_frequency', 'filter frequency'], ['filter_lfo2', 'lfo2 → filter'], ['amplitude', 'amp gain']];

export function AuthorExtraPage({ specs, read, write, part, model }) {
  const controls = { specs, read, write };
  return <section className="a-shared-page" aria-label="Extra page">
    <header className="a-shared-heading"><h2>Extra</h2><span>{model === 'KS' ? `Part ${part}` : 'K-Station'} · sound parameters</span></header>
    <div className="a-extra-grid">
      <section className="a-settings-card a-controllers" aria-label="Controllers"><h3>Controllers</h3>
        {sources.map(([prefix, title]) => <section className="a-controller-row" key={prefix} aria-label={`${title} assignments`}>
          <h4>{title}</h4><div className="a-page-knobs">{targets.map(([suffix, label]) => <ParamKnob key={suffix} id={`${prefix}_${suffix}`} label={label} {...controls}/>)}</div>
        </section>)}
      </section>
      <div className="a-extra-side">
        <section className="a-settings-card a-vco-settings" aria-label="VCO Settings"><h3>VCO Settings</h3><p>VCO Drift / Voice Drift controls oscillator1–3 tuning drift.</p>
          <div className="a-page-knobs"><ParamKnob id="random_detune" label="vco drift" {...controls}/>{specs.osc_key_phase.choices.length ? <ParamMenu id="osc_key_phase" label="start phase" {...controls}/> : <ParamKnob id="osc_key_phase" label="start phase" {...controls}/>}</div>
        </section>
        <section className="a-settings-card" aria-label="Live Controllers"><h3>Live Controllers</h3><div className="a-page-knobs"><ParamKnob id="modulation" label="mod wheel" {...controls}/><ParamKnob id="breath" label="breath" {...controls}/></div></section>
      </div>
    </div>
  </section>;
}
