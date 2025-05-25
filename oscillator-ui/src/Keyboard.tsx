import { useEffect } from "react";

const keyToNoteMap: Record<string, string> = {
  "1": "C5",
  "2": "D5",
  "3": "E5",
  "4": "F5",
  "5": "G5",
  "6": "A5",
  "7": "B5",
  "8": "C6",
  "9": "D6",
  "0": "E6",
  q: "C4",
  w: "D4",
  e: "E4",
  r: "F4",
  t: "G4",
  y: "A4",
  u: "B4",
  i: "C5",
  a: "C3",
  s: "D3",
  d: "E3",
  f: "F3",
  g: "G3",
  h: "A3",
  j: "B3",
  k: "C4",
  l: "D4",
  z: "C2",
  x: "D2",
  c: "E2",
  v: "F2",
  b: "G2",
  n: "A2",
  m: "B2",
};

const pushNote = async (note: string) => {
  await fetch("/api/input/push", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ key: note }),
  });
};

const releaseNote = async (note: string) => {
  await fetch("/api/input/release", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ key: note }),
  });
};

const Keyboard = () => {
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      const note = keyToNoteMap[e.key];
      if (note) pushNote(note);
    };

    const handleKeyUp = (e: KeyboardEvent) => {
      const note = keyToNoteMap[e.key];
      if (note) releaseNote(note);
    };

    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("keyup", handleKeyUp);
    };
  }, []);

  return null; // No visible keyboard rendering
};

export default Keyboard;
