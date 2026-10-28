import React, { useEffect, useRef, useState } from 'react';
import catalog from './catalog.json';
import { Knob, Lamps, Menu, Fader, ExtraSlider } from './controls.jsx';
import { Performance } from './Performance.jsx';
const effects = [
  ['Delay', ['delay', 'delay_time', 'delay_feedback', 'delay_width', 'delay_wheel'], ['Level', 'Time', 'Feedback', 'Width', 'Mod wheel'], ['delay_ratio', 'delay_sync']],
  ['Reverb', ['reverb', 'reverb_decay', 'reverb_wheel'], ['Level', 'Decay', 'Wheel send'], ['reverb_type']],
  ['Chorus', ['chorus', 'chorus_rate', 'chorus_depth', 'chorus_feedback', 'chorus_centre', 'chorus_wheel'], ['Level', 'Rate', 'Depth', 'Feedback', 'Centre', 'Mod wheel'], ['chorus_type', 'chorus_sync', 'chorus_start']],
  ['Distortion', ['distortion_level', 'distortion_compensation', 'distortion_wheel'], ['Drive', 'Compensation', 'Wheel depth'], []],
  ['EQ', ['eq_level', 'eq_frequency', 'eq_rate', 'eq_depth'], ['Shape', 'Frequency', 'Rate', 'Depth'], ['eq_sync', 'eq_start']],
  ['Panning', ['pan_position', 'pan_rate', 'pan_depth'], ['Position', 'Rate', 'Depth'], ['pan_sync', 'pan_start']],
  ['Vocoder', ['vocoder_balance', 'vocoder_width', 'vocoder_sibilance'], ['Balance', 'Width', 'Sibilance'], ['vocoder_sibilance_type']],
];
const demoPrograms = ['Silver Init', 'Design Bass', 'Design Pad'];
const mixer = ['osc1_level', 'osc2_level', 'osc3_level', 'noise_level', 'ring_level'];
const filter = ['cutoff', 'resonance', 'filter_drive', 'filter_env', 'filter_lfo2', 'filter_keytrack'];
const filterLabels = ['Cutoff', 'Resonance', 'Overdrive', 'Env depth', 'LFO 2 depth', 'Key track'];
const oscSuffixes = ['semitone', 'fine', '', 'env_pitch', 'lfo1_pitch', 'bend_depth'];
const oscLabels = ['Semitone', 'Detune', 'PW amount', 'Mod env pitch', 'LFO 1 pitch', 'Pitch bend'];
const pwmSources = ['pulse_width', 'lfo2_pwm', 'env_pwm'];
const menuLabels = { delay_ratio: 'Ratio', delay_sync: 'Sync', reverb_type: 'Type', chorus_type: 'Type', chorus_sync: 'Sync', chorus_start: 'Sync start', eq_sync: 'Sync', eq_start: 'Start', pan_sync: 'Sync', pan_start: 'Start', vocoder_sibilance_type: 'Sibilance' };

export function App() {
  const [model, setModel] = useState('KS'), [part, setPart] = useState(1), [lfo, setLfo] = useState(1), [effect, setEffect] = useState(0);
  const [pwm, setPwm] = useState([0, 0, 0]), [values, setValues] = useState({}), [extra, setExtra] = useState(null);
  const [density, setDensity] = useState('compact');
  const [performanceReset, setPerformanceReset] = useState(0);
  const dimensions = density === 'compact' ? { width: 1280, height: 1000 } : { width: 1440, height: 1122 };
  const [grid, setGrid] = useState(false), [reference, setReference] = useState(false), [zoom, setZoom] = useState('fit'), [scale, setScale] = useState(1);
  const [display, setDisplay] = useState(null), [page, setPage] = useState(null), [program, setProgram] = useState('Silver Init');
  const container = useRef(null), specs = catalog[model], fx = effects[effect];
  const read = (id, owner = part) => values[`${model}:${owner}:${id}`] ?? (id === 'part_channel' ? owner - 1 : specs[id]?.default) ?? 0;
  const write = (id, v, owner = part) => {
    const s = specs[id]; if (!s) return;
    const value = Math.max(s.min, Math.min(s.max, Math.round(v)));
    setValues(current => ({ ...current, [`${model}:${owner}:${id}`]: value }));
    setDisplay(`${s.label}\n${s.choices.length ? s.choices[value] : value + s.offset}`);
  };
  useEffect(() => { if (!display) return; const timer = setTimeout(() => setDisplay(null), 1200); return () => clearTimeout(timer); }, [display]);
  useEffect(() => {
    const host = container.current;
    const update = () => {
      const heightScale = page === 'Perf' ? Math.max(.15, (window.innerHeight - document.querySelector('.stand-toolbar').offsetHeight - 34) / dimensions.height) : 1;
      setScale(zoom === 'fit' ? Math.min(1, heightScale, (host.clientWidth - 32) / dimensions.width) : Number(zoom));
    };
    const observer = new ResizeObserver(update); observer.observe(host); window.addEventListener('resize', update); update();
    return () => { observer.disconnect(); window.removeEventListener('resize', update); };
  }, [zoom, dimensions.width, dimensions.height, page]);
  const changeModel = next => { setModel(next); if (next === 'K') setPart(1); setDisplay(null); };
  const knob = (id, label, owner) => specs[id] && <Knob key={id} spec={specs[id]} value={read(id)} onChange={v => write(id, v)} label={label || specs[id].label} owner={owner} />;
  const menu = (id, label, owner, inline = false) => specs[id] && <Menu key={id} label={label || specs[id].label} choices={specs[id].choices} value={read(id)} onChange={v => write(id, v)} owner={owner} inline={inline} />;
  const stepProgram = direction => { setProgram(current => demoPrograms[(demoPrograms.indexOf(current) + direction + demoPrograms.length) % demoPrograms.length]); setDisplay(null); };
  const exposed = new Set(['arp_gate', 'arp_rhythm', 'arp_sync', 'arp_keysync', 'volume', 'arp_rate', 'arp_on', 'arp_latch', 'arp_pattern', 'arp_octaves', 'portamento_time', 'fm_attack', 'fm_decay', 'osc_sync', 'filter_slope', ...mixer, ...filter, ...fx[1], ...fx[3]]);
  if (model === 'KS') exposed.add('filter_type');
  for (let o = 1; o <= 3; o++) ['wave', 'octave', ...pwmSources, ...oscSuffixes.map(s => s || pwmSources[pwm[o - 1]])].forEach(s => exposed.add(`osc${o}_${s}`));
  for (const n of model === 'K' ? [1, 2] : [lfo]) {
    ['rate', 'delay', 'wave', 'sync', ...(model === 'KS' ? ['start_phase', 'velocity'] : [])].forEach(s => exposed.add(`lfo${n}_${s}`));
  }
  ['attack', 'decay', 'sustain', 'release', 'mod_attack', 'mod_decay', 'mod_sustain', 'mod_release'].forEach(id => exposed.add(id));
  const extraSpecs = domain => Object.values(specs).filter(s => {
    let include = false;
    if (domain.startsWith('osc')) include = s.section === `Oscillator ${domain.slice(3)}` || s.id === 'osc_key_phase' || (s.group === 'Modulation' && s.id.includes('_osc_'));
    if (domain === 'mixer') include = s.group === 'Mixer' || s.section === 'External input';
    if (domain === 'filter') include = s.group === 'Filter' || (s.group === 'Modulation' && s.id.includes('_filter_'));
    if (domain === 'envelopes') include = (s.group === 'Envelopes' && s.section !== 'FM') || (s.group === 'Modulation' && s.id.endsWith('_amplitude'));
    if (domain === 'lfo') include = model === 'K' ? /^lfo[12]_/.test(s.id) : s.id.startsWith(`lfo${lfo}_`);
    if (domain === 'voice') include = s.group === 'Voice' && s.section !== 'Glide';
    if (domain === 'arp') include = s.group === 'Arpeggiator' || (s.group === 'Voice' && s.section === 'Glide');
    if (domain === 'fm') include = s.id.startsWith('fm_');
    if (domain === 'fx') include = s.group === 'Effects' && s.section === (fx[0] === 'Chorus' ? 'Chorus / Phaser' : fx[0]);
    return include && !exposed.has(s.id);
  });
  const extraButton = (domain, label = 'Extra') => (domain !== 'fx' || fx[0] === 'Chorus') && extraSpecs(domain).length > 0 && <button className="extra-button" data-extra={domain} onClick={() => setExtra(extra === domain ? null : domain)}>{label}</button>;
  const extraLabel = s => {
    const label = `${s.id.startsWith('lfo') && model === 'K' ? `LFO ${s.id[3]} / ` : ''}${s.group === 'Modulation' || s.group === 'Envelopes' ? `${s.section} / ` : ''}${s.label}`;
    return density === 'compact' && extra?.startsWith('osc')
      ? label.replace('Oscillator pitch', 'Pitch').replace('Oscillator LFO1', 'LFO 1')
      : label;
  };
  const extraPanel = (domain, title) => extra === domain && <div className="extra-panel" data-extra-panel={domain} role="region" aria-label={`${title} Extra`}>
    <div className="extra-heading"><h2>{title}</h2><button autoFocus onClick={() => setExtra(null)}>Back</button></div>
    <div className="extra-content">{extraSpecs(domain).map(s => <div className="extra-row" key={s.id} data-extra-param={s.id}>
      <span title={`${s.group} / ${s.section} / ${s.label}`}>{extraLabel(s)}</span>
      {s.choices.length ? <select aria-label={`${title} ${extraLabel(s)}`} title={s.choices[read(s.id)]} value={read(s.id)} onChange={e => write(s.id, Number(e.target.value))}>{s.choices.map((t, i) => <option key={t} value={i}>{t}</option>)}</select>
        : domain === 'arp' || domain === 'voice'
          ? <Knob compact spec={s} value={read(s.id)} onChange={v => write(s.id, v)} label={extraLabel(s)} owner={title} />
          : <ExtraSlider spec={s} value={read(s.id)} onChange={v => write(s.id, v)} label={extraLabel(s)} owner={title} />}
    </div>)}</div>
  </div>;
  const envelope = (name, prefix) => <section className="envelope module" aria-label={name}><div inert={extra === 'envelopes'}><div className="module-heading"><h2>{name}</h2>{prefix === 'mod_' && extraButton('envelopes')}</div><div className="faders">{['attack', 'decay', 'sustain', 'release'].map(s => <Fader key={s} spec={specs[prefix + s]} value={read(prefix + s)} onChange={v => write(prefix + s, v)} label={s[0].toUpperCase() + s.slice(1)} owner={name} />)}</div></div></section>;
  return <>
    <div className="stand-toolbar"><span>Silver-K <strong>Design stand</strong><small>Локальные значения · без звука и прошивок</small></span><div>
      <label>Вариант <select aria-label="Плотность компоновки" value={density} onChange={e => setDensity(e.target.value)}><option value="compact">Компактный · 1280 × 1000</option><option value="original">Исходный · 1440 × 1122</option></select></label>
      <label>Масштаб <select aria-label="Масштаб стенда" value={zoom} onChange={e => setZoom(e.target.value)}><option value="fit">{page === 'Perf' ? 'Вписать экран' : 'Вписать по ширине'}</option><option value="0.75">75%</option><option value="1">100%</option><option value="1.25">125%</option></select></label>
      <button aria-pressed={grid} onClick={() => setGrid(!grid)}>Сетка</button><button aria-pressed={reference} onClick={() => setReference(!reference)}>Макет</button><button onClick={() => { setValues({}); setPerformanceReset(n => n + 1); setPwm([0, 0, 0]); setExtra(null); setDisplay(null); }}>Сбросить</button>
    </div></div>
    <main className="stand-canvas" ref={container}><div className="scale-box" style={{ width: dimensions.width * scale, height: dimensions.height * scale }}>
      <div className={`skin ${model === 'K' ? 'k-model' : 'ks-model'} ${grid ? 'show-grid' : ''} ${density === 'compact' ? 'compact-skin' : ''}`} style={{ transform: `scale(${scale})` }} data-model={model}>
        <header className="skin-header"><h1>Silver-K</h1><select className="model-picker" aria-label="Synth model" value={model} onChange={e => changeModel(e.target.value)}><option value="KS">KS</option><option value="K">K-Station</option></select>
          <div className="part-tabs" role="group" aria-label="Part">{[1, 2, 3, 4].map(n => <button key={n} disabled={model === 'K' && n !== 1} aria-pressed={part === n} onClick={() => setPart(n)}>{n}</button>)}</div>
          <nav>{['Main', 'Perf', 'Patches', 'Setup'].map(n => <button key={n} aria-current={(n === 'Main' && !page) || page === n ? 'page' : undefined} onClick={() => setPage(n === 'Main' ? null : n)}>{n}</button>)}</nav>
        </header>
        <div className="skin-body" hidden={page === 'Perf'}><div className="main-grid"><div className="oscillator-stack">{[1, 2, 3].map(o => <section className="oscillator module" key={o} aria-label={`Oscillator ${o}`}>
          <div inert={extra === `osc${o}` || (o === 2 && extra === 'fm')}><div className="module-heading"><h2>Oscillator {o}</h2>{extraButton(`osc${o}`)}</div>
          <div className="osc-toolbar">{menu(`osc${o}_wave`, 'Waveform', `OSC ${o}`)}{menu(`osc${o}_octave`, 'Octave', `OSC ${o}`)}<Lamps labels={['PW Position', 'LFO 2 PWM', 'Env PWM']} name={`OSC ${o} PWM source`} value={pwm[o - 1]} vertical onChange={n => setPwm(current => current.map((v, i) => i === o - 1 ? n : v))} /></div>
          <div className="six-knobs">{oscSuffixes.map((s, i) => knob(`osc${o}_${s || pwmSources[pwm[o - 1]]}`, oscLabels[i], `OSC ${o}`))}</div>
          </div>
          {o < 3 && <div className="seam">{o === 1 ? <Lamps labels={['Sync']} name="Oscillator sync" value={read('osc_sync') ? 0 : -1} onChange={() => write('osc_sync', read('osc_sync') ? 0 : 1)} /> : <button className="fm-button" aria-expanded={extra === 'fm'} onClick={() => setExtra(extra === 'fm' ? null : 'fm')}>FM</button>}</div>}
          {extraPanel(`osc${o}`, `Oscillator ${o}`)}{o === 2 && extraPanel('fm', 'FM settings')}
        </section>)}</div>
        <section className="mixer module" aria-label="Mixer"><div inert={extra === 'mixer'}><div className="module-heading"><h2>Mixer</h2>{extraButton('mixer')}</div>{mixer.map((id, i) => <div className="mixer-knob" key={id} style={{ top: density === 'compact' ? 84 + 112 * i : 86 + 128 * i }}>{knob(id, ['OSC 1', 'OSC 2', 'OSC 3', 'Noise', 'Ring 1 x 2'][i], 'Mixer')}</div>)}</div>{extraPanel('mixer', 'Mixer')}</section>
        <section className="filter module" aria-label="Filter"><div inert={extra === 'filter'}><div className="module-heading"><h2>Filter</h2>{extraButton('filter')}</div><div className="filter-switches">{model === 'KS' ? <Lamps labels={['LP', 'BP', 'HP']} name="Filter type" value={read('filter_type')} onChange={v => write('filter_type', v)} /> : <span className="fixed-filter">Low-pass</span>}<Lamps labels={['12 dB', '24 dB']} name="Filter slope" value={read('filter_slope')} onChange={v => write('filter_slope', v)} /></div><div className="six-knobs">{filter.map((id, i) => knob(id, filterLabels[i], 'Filter'))}</div></div>{extraPanel('filter', 'Filter')}</section>
        <div className="envelope-stack">{envelope('Mod Env', 'mod_')}{envelope('Amp Env', '')}{extraPanel('envelopes', 'Envelopes')}</div>
        <section className="fm-envelope module" aria-label="FM Env"><div className="module-heading"><h2>FM Env</h2></div><div className="two-knobs">{knob('fm_attack', 'Attack', 'FM Env')}{knob('fm_decay', 'Decay', 'FM Env')}</div></section>
        <section className="lfo module" aria-label="LFO"><div inert={extra === 'lfo'}><div className="module-heading"><h2>LFO</h2>{model === 'KS' && <Lamps labels={['1', '2']} name="Select LFO" value={lfo - 1} onChange={n => setLfo(n + 1)} />}{extraButton('lfo')}</div>
          {model === 'K' ? <div className="k-lfos">{[1, 2].map(n => <div className="k-lfo" key={n} role="group" aria-label={`LFO ${n}`}>
            <h3 className="k-lfo-title">LFO {n}</h3><div className="k-lfo-menus">{menu(`lfo${n}_wave`, 'Wave', `LFO ${n}`, true)}{menu(`lfo${n}_sync`, 'Sync', `LFO ${n}`, true)}</div>
            <div className="k-lfo-knobs">{knob(`lfo${n}_rate`, `Rate ${n}`, `LFO ${n}`)}{knob(`lfo${n}_delay`, `Delay ${n}`, `LFO ${n}`)}</div>
          </div>)}</div> : <>
            <div className="lfo-knobs">{knob(`lfo${lfo}_rate`, 'Rate', 'LFO')}{knob(`lfo${lfo}_delay`, 'Delay', 'LFO')}{knob(`lfo${lfo}_start_phase`, 'Start phase', 'LFO')}{knob(`lfo${lfo}_velocity`, 'Velocity depth', 'LFO')}</div>
            <div className="lfo-bottom">{menu(`lfo${lfo}_wave`, 'Wave', 'LFO', density === 'compact')}{menu(`lfo${lfo}_sync`, density === 'compact' ? 'Sync' : 'Sync rate', 'LFO', density === 'compact')}</div>
          </>}
        </div>{extraPanel('lfo', model === 'K' ? 'LFO 1 / 2' : `LFO ${lfo}`)}</section>
        </div>
        <div className="lower-panel">
          <section className="arp module master-module" aria-label="Volume / Arpeggiator / Glide"><div inert={extra === 'arp'}>
            <div className="module-heading"><h2>Volume / Arp / Glide</h2>{extraButton('arp')}</div>
            <div className="master-knobs">{knob('portamento_time', 'Glide time', 'Glide')}{knob('arp_rate', 'Tempo', 'ARP')}{knob('volume', 'Volume', 'Master')}{knob('arp_gate', 'Gate length', 'ARP')}</div>
            <div className="master-controls">
              <div className="master-switches"><Lamps labels={['On']} name="Arpeggiator on" value={read('arp_on') ? 0 : -1} onChange={() => write('arp_on', read('arp_on') ? 0 : 1)} /><Lamps labels={['Latch']} name="Arpeggiator latch" value={read('arp_latch') ? 0 : -1} onChange={() => write('arp_latch', read('arp_latch') ? 0 : 1)} /><Lamps labels={['K-sync']} name="Arpeggiator keysync" value={read('arp_keysync') ? 0 : -1} onChange={() => write('arp_keysync', read('arp_keysync') ? 0 : 1)} /></div>
              {menu('arp_sync', 'Sync rate', 'ARP', true)}{menu('arp_pattern', 'Pattern', 'ARP', true)}{menu('arp_octaves', 'Octaves', 'ARP', true)}
              {specs.arp_rhythm && <Menu label="Rhythm" choices={Array.from({ length: specs.arp_rhythm.max + 1 }, (_, n) => n ? String(n) : 'Off')} value={read('arp_rhythm')} onChange={v => write('arp_rhythm', v)} owner="ARP" inline />}
            </div>
          </div>{extraPanel('arp', 'Volume / Arp / Glide')}</section>
          <section className="program module" aria-label="Program"><div inert={extra === 'voice'}><div className="module-heading"><h2>Program</h2>{extraButton('voice', 'Options')}</div><div className="lcd" aria-live="polite">{display || `Preset\n${program}`}</div><div className="preset-nav"><button aria-label="Previous preset" onClick={() => stepProgram(-1)}>◀ Previous</button><button aria-label="Next preset" onClick={() => stepProgram(1)}>Next ▶</button></div><div className="program-actions"><button onClick={() => setPage('Patches')}>Patch browser</button><button onClick={() => { setDisplay('Panic\nDesign stand'); setExtra(null); }}>Panic</button></div></div>{extraPanel('voice', 'Options')}</section>
          <section className="effects module" aria-label="Effects"><div inert={extra === 'fx'}><div className="module-heading"><h2>Effects</h2><select aria-label="Effect" value={effect} onChange={e => { setEffect(Number(e.target.value)); setExtra(current => current === 'fx' ? null : current); }}>{effects.map((f, i) => <option key={f[0]} value={i}>{f[0]}</option>)}</select>{extraButton('fx')}</div><div className="fx-knobs">{Array.from({ length: 6 }, (_, i) => fx[1][i] ? knob(fx[1][i], fx[2][i], 'FX') : <div className="knob-control unused-effect" key={`empty-${i}`} aria-disabled="true" title="Нет дополнительного параметра"><div className="dial"><img src="/assets/knob.png" alt="" /></div><span className="knob-label">—</span><span className="knob-value">—</span></div>)}</div><div className="fx-menus" style={{ gridTemplateColumns: `repeat(${Math.max(1, fx[3].length)}, minmax(0, 1fr))` }}>{fx[3].map(id => menu(id, menuLabels[id], 'FX', true))}</div></div>{extraPanel('fx', `${fx[0]} / Extra`)}</section>
        </div>
        </div>
        <div hidden={page !== 'Perf'}><Performance key={performanceReset} model={model} visible={page === 'Perf'} selectedPart={part} onSelectPart={setPart} read={read} write={write} onEdit={(n, name) => { setPart(n); setProgram(name); setDisplay(null); setPage(null); }} /></div>
        {reference && <img className="reference-overlay" src="/reference.png" alt="Reference mockup" />}
        {page && page !== 'Perf' && <div className="demo-scrim"><section className="demo-page"><div className="extra-heading"><h2>{page}</h2><button onClick={() => setPage(null)}>Back</button></div>{page === 'Patches' ? <><p>Локальные подписи для проверки экрана программы.</p>{demoPrograms.map(name => <button className="demo-patch" key={name} onClick={() => { setProgram(name); setPage(null); }}>{name}</button>)}</> : page === 'Setup' ? <><p>Дизайн-стенд — только интерфейс, без ROM и DSP.</p><Menu label="Модель" choices={['KS', 'K-Station']} value={model === 'KS' ? 0 : 1} onChange={n => changeModel(n ? 'K' : 'KS')} /></> : <><p>Партии для проверки визуальных состояний.</p><div className="demo-parts">{[1, 2, 3, 4].map(n => <button key={n} disabled={model === 'K' && n !== 1} onClick={() => { setPart(n); setPage(null); }}>Part {n}</button>)}</div></>}</section></div>}
      </div>
    </div></main>
  </>;
}
