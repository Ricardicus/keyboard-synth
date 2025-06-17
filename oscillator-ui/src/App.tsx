import { useEffect, useState } from "react";
import OscillatorControl from "./OscillatorControl";
import ConfigPanel from "./ConfigPanel";
import Keyboard from "./Keyboard";
import Presets from "./Presets";
import type { Oscillator } from "./types";
import "./App.css";

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
