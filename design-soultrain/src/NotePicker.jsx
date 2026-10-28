import React, { useEffect, useRef } from 'react';
import { noteChoices, noteLabel } from './midiLearn.mjs';

export function NotePicker({ owner, label, value, open, status, onOpen, onClose, onChange, onLearn, onCancel }) {
  const root = useRef(null), trigger = useRef(null), manual = useRef(null);
  const callbacks = useRef({ onClose });
  callbacks.current = { onClose };
  const busy = status.phase === 'waiting' || status.phase === 'connecting';
  const dialogId = `note-${owner.replaceAll(' ', '-')}-${label.replaceAll(' ', '-')}`;
  useEffect(() => {
    if (!open) return;
    manual.current?.focus();
    const outside = event => { if (!root.current.contains(event.target)) callbacks.current.onClose(); };
    const escape = event => { if (event.key === 'Escape') { event.preventDefault(); callbacks.current.onClose(); trigger.current.focus(); } };
    document.addEventListener('pointerdown', outside);
    document.addEventListener('keydown', escape);
    return () => { document.removeEventListener('pointerdown', outside); document.removeEventListener('keydown', escape); };
  }, [open]);
  return <div className={`note-picker menu-field ${open ? 'note-picker-open' : ''}`} ref={root}>
    <span>{label}</span>
    <button className="note-trigger" ref={trigger} aria-label={`${owner} ${label}`} aria-description={`Текущая нота: ${noteLabel(value)}`} title={noteLabel(value)} aria-haspopup="dialog" aria-expanded={open} aria-controls={open ? dialogId : undefined} onClick={open ? onClose : onOpen}><span>{noteLabel(value)}</span><span aria-hidden="true">⌄</span></button>
    {open && <div className="note-popover" id={dialogId} role="dialog" aria-label={`${owner} ${label} selection`}>
      <div className="note-popover-heading"><h4>{owner} · {label}</h4><button aria-label="Close note selection" onClick={() => { onClose(); trigger.current.focus(); }}>×</button></div>
      <label className="menu-field"><span>Выбрать вручную</span><select ref={manual} aria-label={`${owner} ${label} manual`} value={value} onChange={event => onChange(Number(event.target.value))}>{noteChoices.map((text, n) => <option key={n} value={n}>{text}</option>)}</select></label>
      <div className="note-learn-separator">или нажать MIDI-клавишу</div>
      <button className={`note-learn ${busy ? 'learning' : ''}`} aria-pressed={busy} onClick={busy ? onCancel : onLearn}><i aria-hidden="true" />{busy ? 'Отмена обучения' : 'Learn MIDI'}</button>
      <p className={`note-learn-status ${status.phase}`} role="status">{status.message || 'Следующее нажатие задаст эту ноту.'}</p>
    </div>}
  </div>;
}
