import { FormEvent, useState } from "react";

import { useDashboardStore } from "../state/dashboardStore";

export function FirePanel() {
  const fireOnce = useDashboardStore((state) => state.fireOnce);
  const [presetId, setPresetId] = useState("");
  const [targets, setTargets] = useState("");
  const [leadTime, setLeadTime] = useState(3);
  const [gainDb, setGainDb] = useState<number | undefined>();
  const [armed, setArmed] = useState(false);
  const [status, setStatus] = useState<string>();
  const [busy, setBusy] = useState(false);

  async function handleSubmit(event: FormEvent) {
    event.preventDefault();
    if (!armed || !presetId || !targets.trim()) {
      return;
    }
    setBusy(true);
    setStatus(undefined);
    try {
      const result = await fireOnce({
        presetId,
        targets: targets.split(",").map((id) => id.trim()).filter(Boolean),
        leadTimeSeconds: leadTime,
        gainDb
      });
      setStatus(`Request ${result.requestId} queued (${result.events.length} events).`);
      setArmed(false);
    } catch (err) {
      setStatus((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <form onSubmit={handleSubmit} className="stack">
      <div className="control-group">
        <label htmlFor="preset">Preset ID</label>
        <input id="preset" value={presetId} onChange={(e) => setPresetId(e.target.value)} placeholder="e.g. A4_mf" />
      </div>
      <div className="control-group">
        <label htmlFor="targets">Targets (comma-separated device IDs)</label>
        <input
          id="targets"
          value={targets}
          onChange={(e) => setTargets(e.target.value)}
          placeholder="m5-001,m5-014"
        />
      </div>
      <div className="grid-columns">
        <div className="control-group">
          <label htmlFor="lead">Lead Time (seconds)</label>
          <input
            id="lead"
            type="number"
            min={1}
            value={leadTime}
            onChange={(e) => setLeadTime(Number(e.target.value))}
          />
        </div>
        <div className="control-group">
          <label htmlFor="gain">Gain (dB, optional)</label>
          <input
            id="gain"
            type="number"
            value={gainDb ?? ""}
            onChange={(e) => setGainDb(e.target.value ? Number(e.target.value) : undefined)}
          />
        </div>
      </div>
      <label style={{ display: "flex", alignItems: "center", gap: 8, fontSize: "0.9rem" }}>
        <input type="checkbox" checked={armed} onChange={(e) => setArmed(e.target.checked)} /> Enable fire
      </label>
      <button className="primary-button" type="submit" disabled={!armed || busy || !presetId || !targets.trim()}>
        {busy ? "Firing..." : "Send Single Fire"}
      </button>
      {status && <p style={{ fontSize: "0.85rem" }}>{status}</p>}
    </form>
  );
}
