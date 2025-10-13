import { useCallback, useEffect, useState } from "react";

export type RecorderAction = "record" | "stop" | "set" | "clear";

export interface RecorderUpdatePayload {
  action: RecorderAction;
  track?: number;
  bpm?: number;
  metronome?: "on" | "off";
}

interface RecorderStateResponse {
  track: number;
  bpm: number;
  metronome: "on" | "off";
  recording: boolean;
}

interface RecorderProps {
  onUpdate: (payload: RecorderUpdatePayload) => Promise<any> | void;
}

export default function Recorder({ onUpdate }: RecorderProps) {
  const [track, setTrack] = useState<number>(1);
  const [bpm, setBpm] = useState<number>(120);
  const [metronome, setMetronome] = useState<"on" | "off">("off");
  const [isRecording, setIsRecording] = useState(false);
  const [message, _setMessage] = useState<string>("");
  const [error, setError] = useState<string>("");
  const [loading, setLoading] = useState(true);

  const submit = useCallback(
    async (payload: RecorderUpdatePayload) => {
      try {
        const maybePromise = onUpdate?.(payload);
        if (maybePromise && typeof (maybePromise as any).then === "function") {
          await (maybePromise as Promise<any>);
        }
        if (payload.action === "record") setIsRecording(true);
        if (payload.action === "stop") setIsRecording(false);
      } catch (e: any) {
        setError(e?.message || "Something went wrong");
      }
    },
    [onUpdate],
  );

  // ✅ Load initial state from backend
  useEffect(() => {
    async function fetchState() {
      try {
        const res = await fetch("/api/recorder");
        if (!res.ok) throw new Error("Failed to fetch recorder state");
        const data: RecorderStateResponse = await res.json();
        console.log(data);
        setTrack(data.track ?? 1);
        setBpm(data.bpm ?? 120);
        setMetronome(data.metronome ?? "off");
        setIsRecording(data.recording ?? false);
      } catch (err: any) {
        console.error("Failed to load recorder state:", err);
        setError("Failed to load recorder state");
      } finally {
        setLoading(false);
      }
    }
    fetchState();
  }, []);

  // Send updates to backend when user changes controls (after loading)
  useEffect(() => {
    if (!loading) {
      submit({ action: "set", track, bpm, metronome });
    }
  }, [track, bpm, metronome]);

  const toggleRecord = () => {
    const action = isRecording ? "stop" : "record";
    submit({ action, track });
  };

  const toggleMetronome = () =>
    setMetronome((prev) => (prev === "on" ? "off" : "on"));

  const handleClear = () => submit({ action: "clear", track });

  return (
    <div className="recorder">
      {loading ? (
        <div className="rec-status">Loading recorder state...</div>
      ) : (
        <>
          {/* Top: BPM + Metronome */}
          <div className="recorder-row top-controls">
            <label className="rec-label" htmlFor="rec-bpm">
              BPM
            </label>
            <input
              id="rec-bpm"
              className="rec-input"
              type="number"
              min={30}
              max={300}
              step={1}
              value={bpm}
              onChange={(e) => setBpm(Number(e.target.value))}
            />

            <button
              className={`rec-btn rec-btn-icon rec-btn-metronome ${metronome === "on" ? "active" : ""}`}
              onClick={toggleMetronome}
              title={`Metronome ${metronome === "on" ? "On" : "Off"}`}
            >
              <img
                src={metronome === "on" ? "sound.png" : "no_sound.png"}
                alt={metronome === "on" ? "Metronome On" : "Metronome Off"}
              />
            </button>
          </div>

          {/* Track selector */}
          <div className="recorder-row track-row">
            {[1, 2, 3, 4].map((t) => (
              <button
                key={t}
                className={`rec-btn rec-btn-track ${track === t ? "active" : ""}`}
                onClick={() => setTrack(t)}
              >
                {t}
              </button>
            ))}
          </div>

          {/* Record + Clear controls */}
          <div className="recorder-row control-row">
            <button
              className={`rec-btn rec-btn-icon rec-btn-record ${isRecording ? "active" : ""}`}
              onClick={toggleRecord}
              title={isRecording ? "Stop Recording" : "Start Recording"}
            >
              <img
                src={isRecording ? "no_record.png" : "record.png"}
                alt={isRecording ? "Stop Recording" : "Start Recording"}
              />
            </button>
            <button
              className="rec-btn rec-btn-danger"
              onClick={handleClear}
              title="Clear selected track"
            >
              ✖ Clear
            </button>
          </div>
        </>
      )}

      {(message || error) && (
        <div
          className={`rec-status ${error ? "rec-status-error" : "rec-status-ok"}`}
        >
          {error || message}
        </div>
      )}
    </div>
  );
}
