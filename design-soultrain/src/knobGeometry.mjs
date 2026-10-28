import authoredDots from './authorKnobDots.json' with { type: 'json' };

export const KNOB_DOTS = Object.freeze(authoredDots.map(dot => Object.freeze(dot)));

export function knobGeometry(spec, rawValue) {
  const value = Math.max(spec.min, Math.min(spec.max, rawValue));
  const shown = value + spec.offset;
  const min = spec.min + spec.offset, max = spec.max + spec.offset;
  const bipolar = min < 0 && max > 0;
  // Map each signed half separately: MIDI's -64…+63 has its true zero at raw 64.
  const position = bipolar
    ? shown < 0 ? shown / -min : shown / max
    : (value - spec.min) / (spec.max - spec.min || 1);
  const angle = bipolar ? position * 135 : -135 + position * 270;
  const middle = (KNOB_DOTS.length - 1) / 2;
  const progress = Math.abs(position) * (bipolar ? middle : KNOB_DOTS.length - 1);
  const levels = KNOB_DOTS.map(({ index }) => {
    const distance = bipolar ? Math.abs(index - middle) : index;
    if (bipolar && (shown === 0 || (position < 0 && index > middle) || (position > 0 && index < middle))) return 0;
    if (!bipolar && position === 0) return 0;
    // The next stationary dot fades in between detents; full-scale reaches both endpoints.
    return Math.max(0, Math.min(1, progress - distance + 1));
  });
  return { shown, min, max, bipolar, position, angle, levels };
}
