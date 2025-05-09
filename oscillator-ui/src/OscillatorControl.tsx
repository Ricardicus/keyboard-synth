import './OscillatorControl.css';
import type { Oscillator } from './types';

const SOUNDS = [
  "SuperSaw", "FatTriangle", "PulseSquare", "SineSawDrone",
  "SuperSawWithSub", "GlitchMix", "LushPad", "RetroLead",
  "BassGrowl", "AmbientDrone", "SynthStab", "GlassBells",
  "OrganTone", "Sine", "Triangular", "Saw", "Square", "None"
];

interface Props {
  id: number;
  data: Oscillator;
  onUpdate: (id: number, updatedData: Partial<Oscillator>) => void;
}

function OscillatorControl({ id, data, onUpdate }: Props) {
  const handleChange = (key: keyof Oscillator, value: any) => {
    onUpdate(id, { [key]: value });
  };

  return (
    <div className="oscillator-card">
      <h3>Oscillator {id + 1}</h3>
      <label>
        Sound:
        <select value={data.sound} onChange={(e) => handleChange('sound', e.target.value)}>
          {SOUNDS.map(s => <option key={s} value={s}>{s}</option>)}
        </select>
      </label>
      <label>
        Volume:
        <input
          type="range"
          min="0"
          max="1"
          step="0.01"
          value={data.volume}
          onChange={(e) => handleChange('volume', parseFloat(e.target.value))}
        />
      </label>
      <label>
        Detune:
        <input
          type="number"
          value={data.detune}
          onChange={(e) => handleChange('detune', parseInt(e.target.value))}
        />
      </label>
      <label>
        Octave:
        <input
          type="number"
          value={data.octave}
          onChange={(e) => handleChange('octave', parseInt(e.target.value))}
        />
      </label>
    </div>
  );
}

export default OscillatorControl;

