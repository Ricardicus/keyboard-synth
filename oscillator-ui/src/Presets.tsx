import React, { useEffect, useState } from "react";
import type { ChangeEvent } from "react";

type Preset = {
  name: string;
};

type PresetsResponse = {
  presets: Preset[];
};

export interface PresetsProps {
  /** Called after a preset is loaded successfully. */
  onLoaded?: (presetName: string) => void;
  /** Called after a preset is saved successfully. */
  onSaved?: (presetName: string) => void;
}

const DEFAULT_OPTION = "None";

/**
 * Presets component — allows the user to load and save configuration presets.
 *
 * This version avoids external UI/animation libraries so it can compile in a
 * fresh React + TypeScript project without extra dependencies. Tailwind CSS
 * utility classes are used for styling, but you can swap those out for your
 * preferred CSS solution.
 */
const Presets: React.FC<PresetsProps> = ({ onLoaded, onSaved }) => {
  const [presets, setPresets] = useState<Preset[]>([]);
  const [selectedPreset, setSelectedPreset] = useState<string>(DEFAULT_OPTION);
  const [isFetching, setIsFetching] = useState<boolean>(false);
  const [isProcessing, setIsProcessing] = useState<boolean>(false);

  // Fetch preset list on mount
  useEffect(() => {
    const fetchPresets = async () => {
      setIsFetching(true);
      try {
        const res = await fetch("/api/presets");
        if (!res.ok) {
          throw new Error(`Failed to fetch presets (status ${res.status})`);
        }
        const data: PresetsResponse = await res.json();
        setPresets(data.presets ?? []);
      } catch (err) {
        console.error(err);
      } finally {
        setIsFetching(false);
      }
    };

    fetchPresets();
  }, []);

  const handleLoad = async () => {
    if (selectedPreset === DEFAULT_OPTION) {
      alert("Please select a preset to load.");
      return;
    }
    setIsProcessing(true);
    try {
      const res = await fetch("/api/presets", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ method: "load", preset: selectedPreset }),
      });
      if (!res.ok) {
        throw new Error(`Load failed (${res.status})`);
      }
      alert(`Loaded preset: ${selectedPreset}`);
      onLoaded?.(selectedPreset);
    } catch (err) {
      console.error(err);
      alert("Failed to load preset.");
    } finally {
      setIsProcessing(false);
    }
  };

  const handleSave = async () => {
    const name = prompt("Enter a name for the new preset:");
    if (!name) return;
    setIsProcessing(true);
    try {
      const res = await fetch("/api/presets", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ method: "save", name }),
      });
      if (!res.ok) {
        throw new Error(`Save failed (${res.status})`);
      }
      alert(`Saved preset: ${name}`);
      onSaved?.(name);
      // Optimistically update local list
      setPresets((prev) => [...prev, { name }]);
    } catch (err) {
      console.error(err);
      alert("Failed to save preset.");
    } finally {
      setIsProcessing(false);
    }
  };

  const onSelectChange = (e: ChangeEvent<HTMLSelectElement>) => {
    setSelectedPreset(e.target.value);
  };

  return (
    <div className="w-full max-w-md mx-auto p-4 bg-white shadow-lg rounded-2xl space-y-4">
      <h2 className="text-xl font-bold">Presets</h2>

      <select
        value={selectedPreset}
        onChange={onSelectChange}
        disabled={isFetching || isProcessing}
        className="w-full p-2 border rounded-md"
      >
        <option value={DEFAULT_OPTION}>{DEFAULT_OPTION}</option>
        {presets.map((preset) => (
          <option key={preset.name} value={preset.name}>
            {preset.name}
          </option>
        ))}
      </select>

      <div className="flex justify-end gap-2">
        <button
          onClick={handleSave}
          disabled={isProcessing}
          className="px-4 py-2 border rounded-md hover:bg-gray-50 disabled:opacity-50"
        >
          Save
        </button>
        <button
          onClick={handleLoad}
          disabled={isProcessing}
          className="px-4 py-2 bg-blue-600 text-white rounded-md hover:bg-blue-700 disabled:opacity-50"
        >
          Load
        </button>
      </div>
    </div>
  );
};

export default Presets;
