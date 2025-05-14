import React, { useState, useEffect, useRef, useCallback } from 'react';
import Knob from './Knob';

interface Echo {
  rate: number;
  feedback: number;
  mix: number;
}

const minGain = 0.000001;
const maxGain = 0.001;

function toLogScale(value: number): number {
  const ratio = Math.log(value / minGain) / Math.log(maxGain / minGain);
  return Math.round(ratio * 100);
}

function fromLogScale(knobValue: number): number {
  return minGain * Math.pow(maxGain / minGain, knobValue / 100);
}

const ConfigPanel: React.FC = () => {
  const [gainKnob, setGainKnob] = useState<number>(50);
  const [echo, setEcho] = useState<Echo>({ rate: 0.3, feedback: 50, mix: 50 });
  const isFirstUpdate = useRef(true);
  // Initialize debounce timer ref with undefined
  const debounceTimer = useRef<number | undefined>(undefined);

  // Fetch initial config
  useEffect(() => {
    fetch('/api/config')
      .then(res => res.json())
      .then(data => {
        setGainKnob(toLogScale(data.gain));
        setEcho({
          rate: data.echo.rate,
          feedback: Math.round(data.echo.feedback * 100),
          mix: Math.round(data.echo.mix * 100),
        });
        isFirstUpdate.current = false;
      });
  }, []);

  // Post config with debounce when knobs change
  const postConfig = useCallback(() => {
    fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        gain: fromLogScale(gainKnob),
        echo: {
          rate: echo.rate,
          feedback: echo.feedback / 100,
          mix: echo.mix / 100,
        },
      }),
    });
  }, [gainKnob, echo]);

  useEffect(() => {
    if (isFirstUpdate.current) return;
    // clear existing timer
    if (debounceTimer.current !== undefined) {
      window.clearTimeout(debounceTimer.current);
    }
    // set new debounce
    debounceTimer.current = window.setTimeout(() => {
      postConfig();
    }, 500);
    return () => {
      if (debounceTimer.current !== undefined) {
        window.clearTimeout(debounceTimer.current);
      }
    };
  }, [gainKnob, echo, postConfig]);

  const updateEcho = (key: keyof Echo, value: number) => {
    setEcho(prev => ({ ...prev, [key]: value }));
  };

  return (
    <div className="config-panel p-6 bg-gray-50 rounded-lg shadow-md">
      <div className="flex flex-col lg:flex-row lg:space-x-12">
        {/* Global Config */}
        <div className="flex-1 mb-8 lg:mb-0">
          <h2 className="text-xl font-bold mb-4">Global Config</h2>
          <div className="flex items-center space-x-6">
            <Knob
              size={100}
              min={0}
              max={100}
              step={1}
              value={gainKnob}
              onChange={value => setGainKnob(value)}
              label="Gain"
            />
            <span className="text-lg font-medium">
              {fromLogScale(gainKnob).toPrecision(4)}
            </span>
          </div>
        </div>

        {/* Echo Settings */}
        <div className="flex-1">
          <h2 className="text-xl font-bold mb-4">Echo Settings</h2>
          <div className="flex space-x-8">
            <div className="flex flex-col items-center space-y-2">
              <Knob
                size={80}
                min={0}
                max={300}
                step={1}
                value={Math.round(echo.rate * 100)}
                onChange={v => updateEcho('rate', v / 100)}
                label="Rate (s)"
              />
              <span>{echo.rate.toFixed(2)}</span>
            </div>
            <div className="flex flex-col items-center space-y-2">
              <Knob
                size={80}
                min={0}
                max={100}
                step={1}
                value={echo.feedback}
                onChange={v => updateEcho('feedback', v)}
                label="Feedback"
              />
              <span>{(echo.feedback / 100).toFixed(2)}</span>
            </div>
            <div className="flex flex-col items-center space-y-2">
              <Knob
                size={80}
                min={0}
                max={100}
                step={1}
                value={echo.mix}
                onChange={v => updateEcho('mix', v)}
                label="Mix"
              />
              <span>{(echo.mix / 100).toFixed(2)}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default ConfigPanel;

