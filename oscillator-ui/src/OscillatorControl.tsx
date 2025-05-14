import './OscillatorControl.css';
import type { Oscillator } from './types';
import Knob from './Knob';
import { useEffect, useRef, useState } from 'react';

const SOUNDS = [
  'SuperSaw', 'FatTriangle', 'PulseSquare', 'SineSawDrone',
  'SuperSawWithSub', 'GlitchMix', 'LushPad', 'RetroLead',
  'BassGrowl', 'AmbientDrone', 'SynthStab', 'GlassBells',
  'OrganTone', 'Sine', 'Triangular', 'Saw', 'Square', 'None'
];

interface Props {
  id: number;
  data: Oscillator;
  onUpdate: (id: number, updatedData: Partial<Oscillator>) => void;
}

function OscillatorControl({ id, data, onUpdate }: Props) {
  const [localData, setLocalData] = useState<Oscillator>(data);
  const debounceTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  // Update local state if props.data changes externally
  useEffect(() => {
    setLocalData(data);
  }, [data]);

  // Debounced update effect
  useEffect(() => {
    if (JSON.stringify(localData) === JSON.stringify(data)) return;

    if (debounceTimer.current) {
      clearTimeout(debounceTimer.current);
    }

    debounceTimer.current = setTimeout(() => {
      const changes: Partial<Oscillator> = {};
      for (const key in localData) {
        if (localData[key as keyof Oscillator] !== data[key as keyof Oscillator]) {
          changes[key as keyof Oscillator] = localData[key as keyof Oscillator] as any;
        }
      }
      if (Object.keys(changes).length > 0) {
        onUpdate(id, changes);
      }
    }, 1000);

    return () => {
      if (debounceTimer.current) {
        clearTimeout(debounceTimer.current);
      }
    };
  }, [localData, data, id, onUpdate]);

  const handleChange = (key: keyof Oscillator, value: any) => {
    setLocalData(prev => ({ ...prev, [key]: value }));
  };

  return (
    <div className="oscillator-card">
      <h3>Oscillator {id + 1}</h3>

      <label>
        Sound:
        <select
          value={localData.sound}
          onChange={e => handleChange('sound', e.target.value)}
        >
          {SOUNDS.map(s => (
            <option key={s} value={s}>{s}</option>
          ))}
        </select>
      </label>

      <label>
        Volume:
        <input
          type="range"
          min="0"
          max="1"
          step="0.01"
          value={localData.volume}
          onChange={e => handleChange('volume', parseFloat(e.target.value))}
        />
      </label>

      <div className="knob-wrapper">
        <Knob
          size={80}
          min={-120}
          max={120}
          step={1}
          value={localData.detune}
          displayValue={true}
          onChange={v => handleChange('detune', v)}
          label="Detune (¢)"
        />
      </div>

      <div className="knob-wrapper">
        <Knob
          size={80}
          min={-4}
          max={4}
          step={1}
          value={localData.octave}
          onChange={v => handleChange('octave', v)}
          label="Octave"
          displayValue={true}
        />
      </div>
    </div>
  );
}

export default OscillatorControl;
