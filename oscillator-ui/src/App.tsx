import { useEffect, useState } from "react";
import OscillatorControl from "./OscillatorControl";
import ConfigPanel from "./ConfigPanel";
import Keyboard from "./Keyboard";
import Presets from "./Presets";
import Recorder from "./Recorder";
import type { Oscillator } from "./types";
import "./App.css";
import "./Recorder.css";

const API_URL = "/api/oscillators";

function App() {
  const [oscillators, setOscillators] = useState<Oscillator[]>([]);

  useEffect(() => {
    fetch(API_URL)
      .then((res) => res.json())
      .then((data: Oscillator[]) => setOscillators(data))
      .catch((err) => console.error("Failed to load oscillator data:", err));

    // On start, release all notes
    const allNotes = [
      "C2",
      "D2",
      "E2",
      "F2",
      "G2",
      "A2",
      "B2",
      "C3",
      "D3",
      "E3",
      "F3",
      "G3",
      "A3",
      "B3",
      "C4",
      "D4",
      "E4",
      "F4",
      "G4",
      "A4",
      "B4",
      "C5",
      "D5",
      "E5",
      "F5",
      "G5",
      "A5",
      "B5",
      "C6",
      "D6",
      "E6",
    ];

    allNotes.forEach((note) => {
      fetch("/api/input/release", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ key: note }),
      });
    });
  }, []);

  const updateOscillator = (id: number, updatedData: Partial<Oscillator>) => {
    fetch(API_URL, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id, ...updatedData }),
    }).then(() => {
      const newOscillators = [...oscillators];
      newOscillators[id] = { ...newOscillators[id], ...updatedData };
      setOscillators(newOscillators);
    });
  };

  const updateRecorder = (payload: {
    action: "record" | "stop" | "set" | "clear";
    track?: number;
    bpm?: number;
    metronome?: "on" | "off";
  }) => {
    return fetch("/api/recorder", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    }).then((res) => {
      if (!res.ok) throw new Error("Recorder API error");
    });
  };

  return (
    <div className="app">
      <h1>Keyboard Settings</h1>
      <center>
        <table>
          <tbody>
            <tr>
              <td>
                <ConfigPanel />
                <Presets />
              </td>
              <td>
                <h1>Oscillators</h1>
                <div className="oscillator-grid">
                  {oscillators.map((osc, i) => (
                    <OscillatorControl
                      key={i}
                      id={i}
                      data={osc}
                      onUpdate={updateOscillator}
                    />
                  ))}
                </div>
                <h1>Recorder</h1>
                <div className="recorder-grid">
                  <Recorder onUpdate={updateRecorder} />
                </div>
              </td>
            </tr>
          </tbody>
        </table>
      </center>
      <Keyboard />
    </div>
  );
}

export default App;
