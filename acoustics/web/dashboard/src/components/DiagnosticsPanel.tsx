import clsx from "clsx";

import { DiagnosticsEntry } from "../types";
import { formatIso } from "../utils/time";

interface Props {
  entries: DiagnosticsEntry[];
}

export function DiagnosticsPanel({ entries }: Props) {
  return (
    <div className="stack">
      {entries.slice().reverse().slice(0, 30).map((entry) => (
        <div key={entry.id} className={clsx("device-card", `diag-${entry.severity}`)}>
          <div className="device-card-header">
            <div>
              <div className="device-alias">{entry.deviceId}</div>
              <div style={{ fontSize: "0.8rem", opacity: 0.7 }}>{formatIso(entry.timestamp)}</div>
            </div>
            <span className={clsx("status-pill", severityClass(entry.severity))}>{entry.severity}</span>
          </div>
          <p style={{ margin: 0 }}>{entry.reason}</p>
        </div>
      ))}
      {!entries.length && <p>No diagnostics yet.</p>}
    </div>
  );
}

function severityClass(severity: DiagnosticsEntry["severity"]) {
  if (severity === "critical") {
    return "critical";
  }
  if (severity === "warn") {
    return "warning";
  }
  return "ok";
}
