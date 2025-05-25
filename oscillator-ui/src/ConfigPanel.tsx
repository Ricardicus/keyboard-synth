import React, { useState, useEffect, useRef, useCallback } from "react";
import Knob from "./Knob";

interface Echo {
  rate: number;
  feedback: number;
  mix: number;
}

interface ADSR {
  attack: number;
  decay: number;
  sustain: number;
  release: number;
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

// Re‑usable wrapper around a knob + readout
const knobWrapper = "flex flex-col items-center space-y-2";

const ConfigPanel: React.FC = () => {
  const [gainKnob, setGainKnob] = useState<number>(50);
  const [echo, setEcho] = useState<Echo>({ rate: 0.3, feedback: 50, mix: 50 });
  const [adsr, setAdsr] = useState<ADSR>({
    attack: 5,
    decay: 5,
    sustain: 10,
    release: 5,
  });

  const isFirstUpdate = useRef(true);
  const debounceTimer = useRef<number | undefined>(undefined);

  /* ----------------------- Fetch initial config ----------------------- */
  useEffect(() => {
    fetch("/api/config")
      .then((res) => res.json())
      .then((data) => {
        setGainKnob(toLogScale(data.gain));
        setEcho({
          rate: data.echo.rate,
          feedback: Math.round(data.echo.feedback * 100),
          mix: Math.round(data.echo.mix * 100),
        });
        setAdsr({
          attack: data.adsr.attack,
          decay: data.adsr.decay,
          sustain: data.adsr.sustain,
          release: data.adsr.release,
        });
        isFirstUpdate.current = false;
      });
  }, []);

  /* ----------------------- Persist config (debounced) ----------------------- */
  const postConfig = useCallback(() => {
    fetch("/api/config", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        gain: fromLogScale(gainKnob),
        echo: {
          rate: echo.rate,
          feedback: echo.feedback / 100,
          mix: echo.mix / 100,
        },
        adsr: {
          attack: adsr.attack,
          decay: adsr.decay,
          sustain: adsr.sustain,
          release: adsr.release,
        },
      }),
    });
  }, [gainKnob, echo, adsr]);

  useEffect(() => {
    if (isFirstUpdate.current) return;

    if (debounceTimer.current !== undefined)
      window.clearTimeout(debounceTimer.current);

    debounceTimer.current = window.setTimeout(postConfig, 500);

    return () => {
      if (debounceTimer.current !== undefined)
        window.clearTimeout(debounceTimer.current);
    };
  }, [gainKnob, echo, adsr, postConfig]);

  /* ----------------------- Helpers ----------------------- */
  const updateEcho = (key: keyof Echo, value: number) =>
    setEcho((prev) => ({ ...prev, [key]: value }));
  const updateAdsr = (key: keyof ADSR, value: number) =>
    setAdsr((prev) => ({ ...prev, [key]: value }));

  /* ----------------------- Render ----------------------- */
  return (
    <div className="config-panel p-6 bg-gray-50 rounded-lg shadow-md flex justify-center">
      <div className="flex flex-col space-y-12 lg:flex-row lg:space-y-0 lg:space-x-12 justify-center">
        {/* ----------------------- Global Config ----------------------- */}
        <section className="flex-1 flex flex-col items-center">
          <h2 className="text-xl font-bold mb-4 text-center">Global Config</h2>
          <div className="overflow-x-auto">
            <center>
              <table className="mx-auto w-max">
                <tbody>
                  <tr className="align-top">
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={100}
                          min={0}
                          max={100}
                          step={1}
                          value={gainKnob}
                          onChange={setGainKnob}
                          label="Gain"
                        />
                        <span className="text-lg font-medium">
                          {fromLogScale(gainKnob).toPrecision(4)}
                        </span>
                      </div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </center>
          </div>
        </section>

        {/* ----------------------- Echo Settings ----------------------- */}
        <section className="flex-1 flex flex-col items-center">
          <h2 className="text-xl font-bold mb-4 text-center">Echo Settings</h2>
          <div className="overflow-x-auto">
            <center>
              <table className="mx-auto w-max">
                <tbody>
                  <tr className="align-top">
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={0}
                          max={300}
                          step={1}
                          value={Math.round(echo.rate * 100)}
                          onChange={(v) => updateEcho("rate", v / 100)}
                          label="Rate (s)"
                        />
                        <span>{echo.rate.toFixed(2)}</span>
                      </div>
                    </td>
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={0}
                          max={100}
                          step={1}
                          value={echo.feedback}
                          onChange={(v) => updateEcho("feedback", v)}
                          label="Feedback"
                        />
                        <span>{(echo.feedback / 100).toFixed(2)}</span>
                      </div>
                    </td>
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={0}
                          max={100}
                          step={1}
                          value={echo.mix}
                          onChange={(v) => updateEcho("mix", v)}
                          label="Mix"
                        />
                        <span>{(echo.mix / 100).toFixed(2)}</span>
                      </div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </center>
          </div>
        </section>

        {/* ----------------------- ADSR Envelope ----------------------- */}
        <section className="flex-1 flex flex-col items-center">
          <h2 className="text-xl font-bold mb-4 text-center">ADSR Envelope</h2>
          <div className="overflow-x-auto">
            <center>
              <table className="mx-auto w-max">
                <tbody>
                  <tr className="align-top">
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={1}
                          max={20}
                          step={1}
                          value={adsr.attack}
                          onChange={(v) => updateAdsr("attack", v)}
                          label="Attack"
                        />
                        <span>{adsr.attack}</span>
                      </div>
                    </td>
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={1}
                          max={20}
                          step={1}
                          value={adsr.decay}
                          onChange={(v) => updateAdsr("decay", v)}
                          label="Decay"
                        />
                        <span>{adsr.decay}</span>
                      </div>
                    </td>
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={1}
                          max={20}
                          step={1}
                          value={adsr.sustain}
                          onChange={(v) => updateAdsr("sustain", v)}
                          label="Sustain"
                        />
                        <span>{adsr.sustain}</span>
                      </div>
                    </td>
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={1}
                          max={20}
                          step={1}
                          value={adsr.release}
                          onChange={(v) => updateAdsr("release", v)}
                          label="Release"
                        />
                        <span>{adsr.release}</span>
                      </div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </center>
          </div>
        </section>
      </div>
    </div>
  );
};

export default ConfigPanel;
