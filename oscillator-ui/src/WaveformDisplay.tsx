import { useEffect, useRef, useState } from "react";
import "./WaveformDisplay.css";

interface WaveformDisplayProps {
  oscillatorId: number;
  width?: number;
  height?: number;
}

function WaveformDisplay({ 
  oscillatorId, 
  width = 300, 
  height = 150 
}: WaveformDisplayProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [waveformData, setWaveformData] = useState<number[]>([]);
  const [frequency, setFrequency] = useState<number>(440);
  const [octave, setOctave] = useState<number>(0);
  const [detune, setDetune] = useState<number>(0);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchWaveform = async () => {
      try {
        const response = await fetch(
          `/api/waveform?id=${oscillatorId}&samples=512`
        );
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data = await response.json();
        setWaveformData(data.waveform || []);
        setFrequency(data.frequency || 440);
        setOctave(data.octave || 0);
        setDetune(data.detune || 0);
        setError(null);
      } catch (err) {
        console.error("Failed to fetch waveform:", err);
        setError("Failed to load waveform");
      }
    };

    fetchWaveform();
    
    // Refresh waveform periodically (every 2 seconds)
    const interval = setInterval(fetchWaveform, 2000);
    
    return () => clearInterval(interval);
  }, [oscillatorId]);

  useEffect(() => {
    if (!waveformData.length || !canvasRef.current) return;

    const canvas = canvasRef.current;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    // Clear canvas
    ctx.fillStyle = "#1a1a2e";
    ctx.fillRect(0, 0, width, height);

    // Draw grid
    ctx.strokeStyle = "#16213e";
    ctx.lineWidth = 1;
    
    // Horizontal lines
    for (let i = 0; i <= 4; i++) {
      const y = (height / 4) * i;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width, y);
      ctx.stroke();
    }
    
    // Vertical lines
    for (let i = 0; i <= 8; i++) {
      const x = (width / 8) * i;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, height);
      ctx.stroke();
    }

    // Draw center line
    ctx.strokeStyle = "#0f3460";
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(0, height / 2);
    ctx.lineTo(width, height / 2);
    ctx.stroke();

    // Normalize and draw waveform
    const max = Math.max(...waveformData.map(Math.abs));
    const scale = max > 0 ? (height / 2) * 0.9 / max : 1;

    ctx.strokeStyle = "#00d9ff";
    ctx.lineWidth = 2;
    ctx.beginPath();

    waveformData.forEach((sample, index) => {
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
    ctx.shadowBlur = 10;
    ctx.shadowColor = "#00d9ff";
    ctx.strokeStyle = "#00d9ff";
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.shadowBlur = 0;

  }, [waveformData, width, height]);

  if (error) {
    return (
      <div className="waveform-error" style={{ width, height }}>
        {error}
      </div>
    );
  }

  return (
    <div className="waveform-container">
      <canvas
        ref={canvasRef}
        width={width}
        height={height}
        className="waveform-canvas"
      />
      <div className="waveform-label">
        {frequency.toFixed(1)} Hz
        {octave !== 0 && ` (Oct${octave > 0 ? '+' : ''}${octave})`}
        {detune !== 0 && ` ${detune > 0 ? '+' : ''}${detune}¢`}
      </div>
    </div>
  );
}

export default WaveformDisplay;
