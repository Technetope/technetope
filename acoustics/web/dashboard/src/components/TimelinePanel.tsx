import { useState } from "react";

import { useDashboardStore } from "../state/dashboardStore";
import { TimelineEvent } from "../types";
import { formatIso } from "../utils/time";

export function TimelinePanel() {
  const timeline = useDashboardStore((state) => state.timeline);
  const preview = useDashboardStore((state) => state.timelinePreview);
  const previewTimeline = useDashboardStore((state) => state.previewTimeline);
  const sendTimeline = useDashboardStore((state) => state.sendTimeline);
  const [uploadPayload, setUploadPayload] = useState<unknown>();
  const [fileName, setFileName] = useState<string>("");
  const [error, setError] = useState<string>();
  const [sending, setSending] = useState(false);

  async function handleFileChange(event: React.ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    if (!file) {
      return;
    }
    try {
      const text = await file.text();
      const json = JSON.parse(text);
      setUploadPayload(json);
      setFileName(file.name);
      setError(undefined);
      await previewTimeline(json);
    } catch (err) {
      console.error(err);
      setError("Failed to parse JSON");
    }
  }

  async function handleSend() {
    if (!uploadPayload) {
      return;
    }
    setSending(true);
    try {
      await sendTimeline(uploadPayload);
      setUploadPayload(undefined);
      setFileName("");
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setSending(false);
    }
  }

  return (
    <div className="stack">
      <div className="timeline-upload">
        <p style={{ margin: 0, fontWeight: 600 }}>Upload timeline JSON</p>
        <input type="file" accept="application/json" onChange={handleFileChange} />
        {fileName && <p style={{ margin: "8px 0 0", fontSize: "0.85rem" }}>Selected: {fileName}</p>}
        {error && (
          <p style={{ color: "#ff7a7a", margin: "8px 0 0", fontSize: "0.85rem" }}>
            {error}
          </p>
        )}
        <button
          className="primary-button"
          style={{ marginTop: 12 }}
          onClick={handleSend}
          disabled={!uploadPayload || sending}
        >
          {sending ? "Sending..." : "Arm & Send"}
        </button>
      </div>

      {preview && preview.length > 0 && (
        <div>
          <h3>Preview ({preview.length} events)</h3>
          <TimelineTable events={preview} />
        </div>
      )}

      <div>
        <h3>Scheduled / Sent</h3>
        <TimelineTable events={timeline} />
      </div>
    </div>
  );
}

function TimelineTable({ events }: { events: TimelineEvent[] }) {
  if (!events.length) {
    return <p style={{ fontSize: "0.9rem", opacity: 0.7 }}>No events.</p>;
  }
  return (
    <div className="table-scroll">
      <table className="timeline-table">
        <thead>
          <tr>
            <th>When (UTC)</th>
            <th>Preset</th>
            <th>Targets</th>
            <th>Status</th>
          </tr>
        </thead>
        <tbody>
          {events.map((event) => (
            <tr key={event.id}>
              <td>{formatIso(event.scheduledTimeUtc)}</td>
              <td>{event.preset}</td>
              <td>{event.targets.join(", ")}</td>
              <td>{event.status}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
