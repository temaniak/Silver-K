const names = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
export const noteLabel = n => `${names[n % 12]}${Math.floor(n / 12) - 1} · ${n}`;
export const noteChoices = Array.from({ length: 128 }, (_, n) => noteLabel(n));

export function noteFromMessage(data) {
  if (!data || data.length !== 3) return null;
  const [status, note, velocity] = data;
  return Number.isInteger(status) && status >= 0x90 && status <= 0x9f
    && Number.isInteger(note) && note >= 0 && note <= 127
    && Number.isInteger(velocity) && velocity > 0 && velocity <= 127 ? note : null;
}

// The played note is kept exactly, even if the opposite zone edge must move.
export function noteRangeChanges(id, note, low, high) {
  return id === 'part_low'
    ? [...(note > high ? [['part_high', note]] : []), [id, note]]
    : [...(note < low ? [['part_low', note]] : []), [id, note]];
}

export function createMidiLearner({ requestAccess, onStatus }) {
  let generation = 0, access = null, release = () => {};
  const cancel = () => { generation++; release(); release = () => {}; };
  const begin = async onNote => {
    cancel();
    const token = generation;
    onStatus({ phase: 'connecting', message: 'Подключение MIDI… Разрешите MIDI-доступ, если браузер его запрашивает.' });
    try {
      const result = access || await requestAccess();
      if (token !== generation) return;
      access = result;
      const bound = new Map(), ready = new Set(), failed = new Set();
      const publish = () => {
        if (token !== generation) return;
        const message = ready.size ? 'Нажмите ноту на MIDI-клавиатуре.'
          : !bound.size ? 'Подключите MIDI-клавиатуру. Ожидаю ноту…'
            : failed.size === bound.size ? 'MIDI-вход недоступен. Проверьте подключение.' : 'Подключение MIDI…';
        onStatus({ phase: ready.size || !bound.size ? 'waiting' : failed.size === bound.size ? 'error' : 'connecting', message });
      };
      const receive = event => {
        if (token !== generation) return;
        const note = noteFromMessage(event.data);
        if (note === null) return;
        cancel(); // One-shot, including repeated/chord notes and pending opens.
        onNote(note);
        onStatus({ phase: 'learned', message: `Определено: ${noteLabel(note)}` });
      };
      const unbind = input => {
        input.removeEventListener('midimessage', receive);
        bound.delete(input); ready.delete(input); failed.delete(input);
        Promise.resolve(input.close()).catch(() => {});
      };
      const connect = () => {
        if (token !== generation) return;
        const inputs = new Set([...access.inputs.values()].filter(input => input.state === 'connected'));
        for (const input of bound.keys()) if (!inputs.has(input)) unbind(input);
        for (const input of inputs) {
          if (bound.has(input)) continue;
          bound.set(input, true);
          input.addEventListener('midimessage', receive);
          Promise.resolve().then(() => token === generation ? input.open() : null).then(() => {
            if (token !== generation || !bound.has(input)) return;
            ready.add(input); publish();
          }).catch(() => {
            if (token !== generation || !bound.has(input)) return;
            failed.add(input); input.removeEventListener('midimessage', receive); publish();
          });
        }
        publish();
      };
      release = () => {
        access.removeEventListener('statechange', connect);
        for (const input of [...bound.keys()]) unbind(input);
      };
      access.addEventListener('statechange', connect);
      connect();
    } catch (error) {
      if (token !== generation) return;
      cancel();
      const message = error.name === 'NotSupportedError' ? 'В этом браузере Web MIDI недоступен. Ручной выбор работает.'
        : error.name === 'NotAllowedError' || error.name === 'SecurityError' ? 'Доступ к MIDI не разрешён. Разрешите его в браузере или выберите ноту вручную.'
          : 'Не удалось подключить MIDI. Можно выбрать ноту вручную.';
      onStatus({ phase: 'error', message });
    }
  };
  return { begin, cancel };
}
