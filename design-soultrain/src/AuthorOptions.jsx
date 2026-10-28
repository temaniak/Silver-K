import React from 'react';

export const OPTION_BLOCKS = ['Oscillator 1', 'Oscillator 2', 'Oscillator 3', 'Mixer', 'Envelopes', 'LFO', 'Arpeggiator', 'Voices'];
const pos = (left, top, width, height) => ({ left, top, ...(width ? { width } : {}), ...(height ? { height } : {}) });

function OptionRegion({ name, rect, dark = false, children }) {
  return <section className={`a-option-section ${dark ? 'a-dark' : ''}`} aria-label={`${name} options`} style={pos(...rect)}>
    <div className="a-option-coordinates" style={{ left: -rect[0], top: -rect[1] }}>{children}</div>
  </section>;
}

export function AuthorOptionPanels({ open, read, write, knob: k, menu, lamp, label, specs, model }) {
  const choices = (id, names, x, y, owner) => specs[id] && <div data-param={id} className="a-lamps column" role="group" aria-label={`${owner} selection`} style={pos(x, y)}>
    {names.map((text, value) => <React.Fragment key={text}>{lamp(text, read(id) === value, () => write(id, value))}</React.Fragment>)}
  </div>;
  const toggle = (id, text) => specs[id] && <span data-param={id}>{lamp(text, Boolean(read(id)), () => write(id, read(id) ? 0 : 1))}</span>;
  return <>
    {[1, 2, 3].map((n, i) => {
      const name = `Oscillator ${n}`, y = 158 + i * 141;
      if (!open[name]) return null;
      const modulation = n === 1 ? [['osc1_level_mod', 'lfo1 level'], ['osc1_lfo2_level', 'lfo2 level']]
        : n === 2 ? [['osc2_level_mod', 'lfo2 level']] : [['osc3_level_mod', 'a/d env level']];
      return <OptionRegion key={name} name={name} rect={[14, 127 + i * 141, 705, 122]}>
        {k(`osc${n}_bend_depth`, 'pitch bend', 29, y)}
        {modulation.map(([id, text], j) => k(id, text, 197 + j * 168, y))}
      </OptionRegion>;
    })}
    {open.Mixer && <OptionRegion name="Mixer" rect={[720, 158, 130, 485]}>
      {menu('noise_type', 'noise type', 742, 165, 86, ['white', 'hp', 'bp', 'hp × bp'])}
      {k('noise_level_mod', 'noise lfo1', 742, 248)}
      {k('ring_level_mod', 'ring lfo1', 742, 354)}
      <div className="a-lamps column" role="group" aria-label="External input triggering" style={pos(733, 480)}>{toggle('external_trigger', 'env trigger')}</div>
    </OptionRegion>}
    {open.Envelopes && <OptionRegion name="Envelopes" rect={[851, 298, 621, 154]}>
      {k('mod_velocity', 'mod velocity', 869, 300)}
      {choices('mod_trigger', ['single', 'multi'], 961, 316, 'Mod trigger')}
      {k('amp_velocity', 'amp velocity', 1121, 300)}
      {choices('amp_trigger', ['single', 'multi'], 1213, 316, 'Amp trigger')}
    </OptionRegion>}
    {open.LFO && <OptionRegion name="LFO" rect={[851, 487, 621, 199]}>
      {[1, 2].map((n, i) => {
        const y = 497 + i * 106;
        return <React.Fragment key={n}>
          {choices(`lfo${n}_delay_multi`, ['single', 'multi'], 872, y, `LFO ${n} delay trigger`)}
          {label(`lfo${n} delay trig`, 872, y + 52)}
          {choices(`lfo${n}_lock`, ['per voice', 'common'], 976, y, `LFO ${n} phase lock`)}
          {label('phase lock', 976, y + 52)}
          {menu(`lfo${n}_delay_sync`, `LFO ${n} delay sync`, 1080, y - 1, 78)}
          <div className="a-lamps column" role="group" aria-label={`LFO ${n} modes`} style={pos(1189, y + 3)}>{toggle(`lfo${n}_phase`, model==='KS'?'one-shot':'keysync phase')}{toggle(`lfo${n}_keysync`, 'keysync')}</div>
          <div className="a-lamps column" role="group" aria-label={`LFO ${n} polarity`} style={pos(1320, y + 3)}>{toggle(`lfo${n}_unipolar`, 'unipolar')}</div>
        </React.Fragment>;
      })}
    </OptionRegion>}
    {model==='KS' && open.Arpeggiator && <OptionRegion name="Arpeggiator" rect={[14, 734, 323, 172]} dark>
      {menu('arp_rhythm', 'rhythm', 37, 736, 110, Array.from({ length: 33 }, (_, i) => i ? String(i) : 'off'))}
    </OptionRegion>}
    {open.Voices && <OptionRegion name="Voices" rect={[338, 734, 381, 172]} dark>
      {model==='KS'?k('voice_level', 'program boost', 365, 804, true):k('voice_level', 'voice level', 365, 746, true)}
      {k('fixed_note', 'fixed note', 449, 804, true)}
    </OptionRegion>}
  </>;
}
