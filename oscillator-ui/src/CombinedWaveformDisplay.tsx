import { useEffect, useRef, useState } from "react";
import "./CombinedWaveformDisplay.css";

interface OscillatorInfo {
  id: number;
  volume: number;
  octave?: number;
  detune?: number;
  sound?: string;
  frequency?: number;
  active: boolean;
}

interface CombinedWaveformDisplayProps {
  width?: number;
  height?: number;
}

function CombinedWaveformDisplay({ 
  width = 640, 
  height = 200 
}: CombinedWaveformDisplayProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [waveformData, setWaveformData] = useState<number[]>([]);
  const [oscillators, setOscillators] = useState<OscillatorInfo[]>([]);
  const [referenceFreq, setReferenceFreq] = useState<number>(440);
  const [baseOctave, setBaseOctave] = useState<number>(0);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchWaveform = async () => {
      try {
        const response = await fetch(`/api/waveform/combined?samples=512`);
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data = await response.json();
        setWaveformData(data.waveform || []);
        setOscillators(data.oscillators || []);
        setReferenceFreq(data.reference_frequency || 440);
        setBaseOctave(data.base_octave || 0);
        setError(null);
      } catch (err) {
        console.error("Failed to fetch combined waveform:", err);
        setError("Failed to load combined waveform");
      }
    };

    fetchWaveform();
    
    // Refresh waveform periodically (every 2 seconds)
    const interval = setInterval(fetchWaveform, 2000);
    
    return () => clearInterval(interval);
  }, []);

  useEffect(() => {
    if (!waveformData.length || !canvasRef.current) return;

    const canvas = canvasRef.current;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    // Clear canvas
    ctx.fillStyle = "#0a0a15";
    ctx.fillRect(0, 0, width, height);

    // Draw grid
    ctx.strokeStyle = "#1a1a2e";
    ctx.lineWidth = 1;
    
    // Horizontal lines
    for (let i = 0; i <= 6; i++) {
      const y = (height / 6) * i;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width, y);
      ctx.stroke();
    }
    
    // Vertical lines
    for (let i = 0; i <= 10; i++) {
      const x = (width / 10) * i;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, height);
      ctx.stroke();
    }

    // Draw center line
    ctx.strokeStyle = "#16213e";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(0, height / 2);
    ctx.lineTo(width, height / 2);
    ctx.stroke();

    // Normalize and draw waveform
    const max = Math.max(...waveformData.map(Math.abs), 0.001);
    const scale = (height / 2) * 0.85 / max;

    // Draw main waveform
    ctx.strokeStyle = "#ff6b6b";
    ctx.lineWidth = 2.5;
    ctx.beginPath();

    waveformData.forEach((sample: number, index: number) => {
      const x = (index / waveformData.length) * width;
      const y = height / 2 - sample * scale;

      if (index === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    });

    ctx.stroke();

    // Draw glow effect
    ctx.shadowBlur = 15;
    ctx.shadowColor = "#ff6b6b";
    ctx.strokeStyle = "#ff6b6b";
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.shadowBlur = 0;

  }, [waveformData, width, height]);

  const activeOscillators = oscillators.filter(osc => osc.active);

  if (error) {
    return (
      <div className="combined-waveform-error" style={{ width, height }}>
        {error}
      </div>
    );
  }

  return (
    <div className="combined-waveform-container">
      <div className="combined-waveform-header">
        <h2>Combined Waveform</h2>
        <div className="active-oscillators">
          {activeOscillators.length > 0 ? (
            <span>
              Active: {activeOscillators.map(osc => {
                const octaveStr = osc.octave !== undefined && osc.octave !== 0 
                  ? ` Oct${osc.octave > 0 ? '+' : ''}${osc.octave}` 
                  : '';
                const detuneStr = osc.detune !== undefined && osc.detune !== 0 
                  ? ` ${osc.detune > 0 ? '+' : ''}${osc.detune}¢` 
                  : '';
                return `Osc ${osc.id + 1} (${(osc.volume * 100).toFixed(0)}%${octaveStr}${detuneStr})`;
              }).join(", ")}
            </span>
          ) : (
            <span className="no-active">No active oscillators</span>
          )}
        </div>
      </div>
      <canvas
        ref={canvasRef}
        width={width}
        height={height}
        className="combined-waveform-canvas"
      />
      <div className="combined-waveform-label">
        Full cycle at {referenceFreq.toFixed(1)} Hz (base octave: {baseOctave})
      </div>
    </div>
  );
}

export default CombinedWaveformDisplay;
