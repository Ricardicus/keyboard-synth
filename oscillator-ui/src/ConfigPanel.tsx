// ConfigPanel.tsx
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

interface Modulator {
  depth: number;
  frequency: number;
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

function formatFrequency(value: number): string {
  return value >= 1000
    ? `${(value / 1000).toFixed(2)} kHz`
    : `${value.toFixed(2)} Hz`;
}

// Re-usable wrapper around a knob + readout
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
  const [highpass, setHighpass] = useState<number>(0);
  const [lowpass, setLowpass] = useState<number>(21000);

  // New: Vibrato & Tremolo
  const [vibrato, setVibrato] = useState<Modulator>({
    depth: 0,
    frequency: 5,
  });
  const [tremolo, setTremolo] = useState<Modulator>({
    depth: 0,
    frequency: 5,
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
        setHighpass(data.highpass || 0);
        setLowpass(data.lowpass || 21000);
        setVibrato({
          depth: data.vibrato?.depth ?? 0,
          frequency: data.vibrato?.frequency ?? 5,
        });
        setTremolo({
          depth: data.tremolo?.depth ?? 0,
          frequency: data.tremolo?.frequency ?? 5,
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
        highpass,
        lowpass,
        vibrato,
        tremolo,
      }),
    });
  }, [gainKnob, echo, adsr, highpass, lowpass, vibrato, tremolo]);

  useEffect(() => {
    if (isFirstUpdate.current) return;

    if (debounceTimer.current !== undefined)
      window.clearTimeout(debounceTimer.current);

    debounceTimer.current = window.setTimeout(postConfig, 500);

    return () => {
      if (debounceTimer.current !== undefined)
        window.clearTimeout(debounceTimer.current);
    };
  }, [gainKnob, echo, adsr, highpass, lowpass, vibrato, tremolo, postConfig]);

  /* ----------------------- Helpers ----------------------- */
  const updateEcho = (key: keyof Echo, value: number) =>
    setEcho((prev) => ({ ...prev, [key]: value }));
  const updateAdsr = (key: keyof ADSR, value: number) =>
    setAdsr((prev) => ({ ...prev, [key]: value }));
  const updateVibrato = (key: keyof Modulator, value: number) =>
    setVibrato((prev) => ({ ...prev, [key]: value }));
  const updateTremolo = (key: keyof Modulator, value: number) =>
    setTremolo((prev) => ({ ...prev, [key]: value }));

  /* ----------------------- Render ----------------------- */
  return (
    <div className="config-panel p-6 bg-gray-50 rounded-lg shadow-md flex justify-center">
      <div className="flex flex-col space-y-12 lg:flex-row lg:space-y-0 lg:space-x-12 justify-center">


        {/* ----------------------- Filter Cutoffs ----------------------- */}
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
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={0}
                          max={21000}
                          step={10}
                          value={highpass}
                          onChange={setHighpass}
                          label="Highpass"
                        />
                        <span>{formatFrequency(highpass)}</span>
                      </div>
                    </td>
                    <td className="px-4">
                      <div className={knobWrapper}>
                        <Knob
                          size={80}
                          min={0}
                          max={21000}
                          step={10}
                          value={lowpass}
                          onChange={setLowpass}
                          label="Lowpass"
                        />
                        <span>{formatFrequency(lowpass)}</span>
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

        {/* ----------------------- Vibrato ----------------------- */}
        <section className="flex-1 flex flex-col items-center">
          <table>
            <tbody>
              <tr>
                <td>
                  <h2 className="text-xl font-bold mb-4 text-center">
                    Vibrato
                  </h2>
                </td>
                {/* ----------------------- Tremolo ----------------------- */}
                <td>
                  <h2 className="text-xl font-bold mb-4 text-center">
                    Tremolo
                  </h2>
                </td>
              </tr>
              <tr>
                <td>
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
                                  max={10}
                                  step={0.1}
                                  value={vibrato.depth}
                                  onChange={(v) =>
                                    updateVibrato(
                                      "depth",
                                      parseFloat(v.toFixed(1)),
                                    )
                                  }
                                  label="Depth"
                                />
                                <span>{vibrato.depth.toFixed(1)}</span>
                              </div>
                            </td>
                            <td className="px-4">
                              <div className={knobWrapper}>
                                <Knob
                                  size={80}
                                  min={0}
                                  max={30}
                                  step={0.1}
                                  value={vibrato.frequency}
                                  onChange={(v) =>
                                    updateVibrato("frequency", v)
                                  }
                                  label="Freq (Hz)"
                                />
                                <span>
                                  {formatFrequency(vibrato.frequency)}
                                </span>
                              </div>
                            </td>
                          </tr>
                        </tbody>
                      </table>
                    </center>
                  </div>
                </td>
                <td>
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
                                  max={1.0}
                                  step={0.01}
                                  value={tremolo.depth}
                                  onChange={(v) =>
                                    updateTremolo(
                                      "depth",
                                      parseFloat(v.toFixed(2)),
                                    )
                                  }
                                  label="Depth"
                                />
                                <span>{tremolo.depth.toFixed(2)}</span>
                              </div>
                            </td>
                            <td className="px-4">
                              <div className={knobWrapper}>
                                <Knob
                                  size={80}
                                  min={0}
                                  max={30}
                                  step={0.1}
                                  value={tremolo.frequency}
                                  onChange={(v) =>
                                    updateTremolo("frequency", v)
                                  }
                                  label="Freq (Hz)"
                                />
                                <span>
                                  {formatFrequency(tremolo.frequency)}
                                </span>
                              </div>
                            </td>
                          </tr>
                        </tbody>
                      </table>
                    </center>
                  </div>
                </td>
              </tr>
            </tbody>
          </table>
        </section>
      </div>
    </div>
  );
};

export default ConfigPanel;
