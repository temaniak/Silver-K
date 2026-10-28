import React, { useEffect, useRef, useState } from 'react';
import { ButtonCap, Knob, Menu, Fader } from './controls.jsx';
import catalog from './catalog.json';
import { NotePicker } from './NotePicker.jsx';
import { createMidiLearner, noteLabel, noteRangeChanges } from './midiLearn.mjs';

const specs = catalog.KS;
const parts = [1, 2, 3, 4];
const patches = ['Silver Init', 'Design Bass', 'Design Pad'];
const channels = Array.from({ length: 16 }, (_, n) => String(n + 1));
const curves = Array.from({ length: 16 }, (_, n) => String(n));

export function Performance({ model, visible, selectedPart, onSelectPart, read, write, onEdit }) {
  const [enabled, setEnabled] = useState([true, true, true, true]);
  const [active, setActive] = useState(true);
  const [programs, setPrograms] = useState(['Design Bass', 'Design Pad', 'Silver Init', 'Silver Init']);
  const [noteEditor, setNoteEditor] = useState(null);
  const [learnStatus, setLearnStatus] = useState({ phase: 'idle', message: '' });
  const learnerRef = useRef(null);
  if (!learnerRef.current) learnerRef.current = createMidiLearner({
    requestAccess: () => {
      if (!navigator.requestMIDIAccess) throw new DOMException('Web MIDI unavailable', 'NotSupportedError');
      return navigator.requestMIDIAccess({ sysex: false });
    },
    onStatus: setLearnStatus,
  });
  const learner = learnerRef.current;
  const closeNote = () => { learner.cancel(); setNoteEditor(null); };
  const openNote = (part, id) => { learner.cancel(); setLearnStatus({ phase: 'idle', message: '' }); setNoteEditor({ part, id }); };
  const changeNote = (part, id, note) => {
    for (const [param, value] of noteRangeChanges(id, note, read('part_low', part), read('part_high', part))) write(param, value, part);
  };
  useEffect(() => {
    if (!visible || model !== 'KS') { learner.cancel(); setNoteEditor(null); }
  }, [visible, model, learner]);
  useEffect(() => {
    const hidden = () => { if (document.hidden) { learner.cancel(); setNoteEditor(null); } };
    document.addEventListener('visibilitychange', hidden);
    return () => { learner.cancel(); document.removeEventListener('visibilitychange', hidden); };
  }, [learner]);
  const supported = model === 'KS';
  const setProgram = (n, value) => setPrograms(current => current.map((p, i) => i === n - 1 ? value : p));
  const stepProgram = (n, delta) => setProgram(n, patches[(patches.indexOf(programs[n - 1]) + delta + patches.length) % patches.length]);
  const togglePart = n => setEnabled(current => current.map((v, i) => i === n - 1 ? !v : v));

  return <section className="performance-page" aria-label="Performance mixer">
    <div className="performance-heading module">
      <div><h2>Performance</h2><span>{supported ? '4 parts' : 'Четыре части доступны в режиме KS'}</span></div>
      <button className="performance-power" disabled={!supported} aria-pressed={supported && active} onClick={() => setActive(v => !v)}><ButtonCap lit={supported && active} />Performance {active && supported ? 'On' : 'Off'}</button>
    </div>
    <div className="performance-channels">
      {parts.map(n => {
        const owner = `Part ${n}`;
        const value = id => read(id, n);
        const change = (id, v) => write(id, v, n);
        const menu = (id, label, choices = specs[id].choices) => <Menu label={label} choices={choices} value={value(id)} onChange={v => change(id, v)} owner={owner} />;
        return <section className={`performance-channel module ${selectedPart === n ? 'selected-part' : ''} ${enabled[n - 1] && active ? '' : 'part-off'}`} aria-label={owner} key={n} tabIndex={supported && active ? 0 : -1} onPointerDownCapture={e => { if (e.button === 0 && supported && active) onSelectPart(n); }} onKeyDown={e => { if (e.target === e.currentTarget && (e.key === 'Enter' || e.key === ' ') && supported && active) { e.preventDefault(); onSelectPart(n); } }}>
          <div className="channel-heading"><h3 className="channel-title">{owner}</h3><button className="part-power" aria-label={`${owner} active`} aria-pressed={enabled[n - 1]} disabled={!supported} onClick={() => togglePart(n)}><ButtonCap lit={enabled[n - 1]} />{enabled[n - 1] ? 'On' : 'Off'}</button></div>
          <fieldset className="channel-controls" disabled={!supported} inert={!supported}>
            <div className="channel-preset"><label>Preset<select aria-label={`${owner} preset`} value={programs[n - 1]} onChange={e => setProgram(n, e.target.value)}>{patches.map(p => <option key={p}>{p}</option>)}</select></label><div className="channel-preset-nav"><button aria-label={`${owner} previous preset`} onClick={() => stepProgram(n, -1)}>◀</button><button aria-label={`${owner} next preset`} onClick={() => stepProgram(n, 1)}>▶</button></div></div>
            <div className="channel-section channel-routing"><h3>Routing</h3><div className="channel-pair">{menu('part_channel', 'MIDI channel', channels)}{menu('part_output', 'Output')}</div></div>
            <div className="channel-section channel-range"><h3>Keyboard range</h3><div className="channel-pair">{['part_low', 'part_high'].map((id, i) => <NotePicker key={id} owner={owner} label={i ? 'High note' : 'Low note'} value={value(id)} open={noteEditor?.part === n && noteEditor.id === id} status={learnStatus} onOpen={() => openNote(n, id)} onClose={closeNote} onChange={note => { learner.cancel(); changeNote(n, id, note); setLearnStatus({ phase: 'idle', message: `Выбрано: ${noteLabel(note)}` }); }} onLearn={() => learner.begin(note => changeNote(n, id, note))} onCancel={() => { learner.cancel(); setLearnStatus({ phase: 'idle', message: 'Обучение отменено.' }); }} />)}</div><div className="note-range" aria-hidden="true"><span style={{ left: `${value('part_low') / 128 * 100}%`, width: `${(value('part_high') - value('part_low') + 1) / 128 * 100}%` }} /></div></div>
            <div className="channel-section channel-tuning"><h3>Tuning</h3><div className="channel-pair">{['part_transpose', 'part_fine'].map((id, i) => <Knob key={id} spec={specs[id]} value={value(id)} onChange={v => change(id, v)} label={i ? 'Fine tuning' : 'Transpose'} owner={owner} />)}</div></div>
            <div className="channel-velocity">{menu('part_velocity', 'Velocity curve', curves)}</div>
            <div className="channel-level"><div className="channel-level-scale" aria-hidden="true">{[127, 96, 64, 32, 0].map(v => <span key={v}>{v}</span>)}</div><Fader spec={specs.volume} value={value('volume')} onChange={v => change('volume', v)} label="Volume" owner={owner} /></div>
            <button className="channel-edit" onClick={() => onEdit(n, programs[n - 1])}>Edit sound</button>
          </fieldset>
        </section>;
      })}
    </div>
  </section>;
}
