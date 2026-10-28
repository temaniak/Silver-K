import React from 'react';
import { ParamKnob, ParamMenu } from './AuthorParameters.jsx';

// One home for every effect parameter, using the same IDs as the native catalog.
export const EFFECTS = [
  { name: 'Delay', knobs: [['delay','send level'],['delay_time','time'],['delay_feedback','feedback'],['delay_width','width'],['delay_wheel','mod wheel']], menus: [['delay_ratio','ratio'],['delay_sync','sync']] },
  { name: 'Reverb', knobs: [['reverb','send level'],['reverb_decay','decay'],['reverb_wheel','mod wheel']], menus: [['reverb_type','type']] },
  { name: 'Chorus', knobs: [['chorus','send level'],['chorus_rate','rate'],['chorus_depth','depth'],['chorus_feedback','feedback'],['chorus_centre','centre'],['chorus_wheel','mod wheel']], menus: [['chorus_type','type'],['chorus_sync','sync'],['chorus_start','sync start']] },
  { name: 'Distortion', short: 'Dist', knobs: [['distortion_level','drive'],['distortion_compensation','compensation'],['distortion_wheel','mod wheel']], menus: [] },
  { name: 'EQ', knobs: [['eq_level','shape'],['eq_frequency','frequency'],['eq_rate','rate'],['eq_depth','depth']], menus: [['eq_sync','sync'],['eq_start','sync start']] },
  { name: 'Panning', knobs: [['pan_position','position'],['pan_rate','rate'],['pan_depth','depth']], menus: [['pan_sync','sync'],['pan_start','sync start']] },
  { name: 'Vocoder', knobs: [['vocoder_balance','balance'],['vocoder_width','width'],['vocoder_sibilance','sibilance']], menus: [['vocoder_sibilance_type','sibilance type']] },
];
export function AuthorEffects({ effect, setEffect, specs, read, write }) {
  const current = EFFECTS[effect], controls = { specs, read, write };
  return <section className="a-effects-complete a-dark" aria-label="Effects">
    <h2>Effects <small>{current.name}</small></h2>
    <div className="a-fx-tabs" role="tablist" aria-label="Effect selection">{EFFECTS.map((fx, i) => <button key={fx.name} role="tab" aria-selected={effect === i} aria-pressed={effect === i} onClick={() => setEffect(i)}>{fx.short || fx.name}</button>)}</div>
    <div className="a-fx-menus">{current.menus.map(([id,label]) => <ParamMenu key={id} id={id} label={label} owner={current.name} {...controls}/>)}</div>
    <div className="a-fx-knobs">{current.knobs.map(([id,label]) => <ParamKnob key={id} id={id} label={label} owner={current.name} {...controls}/>)}</div>
  </section>;
}
