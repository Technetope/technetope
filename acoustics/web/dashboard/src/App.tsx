import { useEffect } from "react";

import { DiagnosticsPanel } from "./components/DiagnosticsPanel";
import { DeviceGrid } from "./components/DeviceGrid";
import { FirePanel } from "./components/FirePanel";
import { LogsPanel } from "./components/LogsPanel";
import { TimelinePanel } from "./components/TimelinePanel";
import { useDashboardStore } from "./state/dashboardStore";
import { formatIso } from "./utils/time";

export default function App() {
  const init = useDashboardStore((state) => state.init);
  const refresh = useDashboardStore((state) => state.refresh);
  const devices = useDashboardStore((state) => state.devices);
  const diagnostics = useDashboardStore((state) => state.diagnostics);
  const sendlog = useDashboardStore((state) => state.sendlog);
  const receivelog = useDashboardStore((state) => state.receivelog);
  const ntpNow = useDashboardStore((state) => state.ntpNow);
  const wsStatus = useDashboardStore((state) => state.wsStatus);

  useEffect(() => {
    void init();
  }, [init]);

  return (
    <div className="app-shell">
      <header className="app-header">
        <div>
          <h1>Acoustics Device Dashboard</h1>
          <div style={{ opacity: 0.7, fontSize: "0.9rem" }}>Monitor · Timeline · Diagnostics · Fire</div>
        </div>
        <div className="header-meta">
          <div>
            <div className="ntp-clock">{ntpNow ? formatIso(ntpNow) : "Syncing NTP..."}</div>
            <small style={{ opacity: 0.7 }}>NTP reference</small>
          </div>
          <div className="ws-status">
            <span
              className="ws-status-dot"
              style={{ backgroundColor: wsStatusColor(wsStatus) }}
            />
            {wsStatus}
          </div>
          <button className="primary-button" onClick={() => refresh()}>
            Refresh
          </button>
        </div>
      </header>

      <section className="panel">
        <h2>Device Blocks ({devices.length})</h2>
        <DeviceGrid devices={devices} />
      </section>

      <div className="grid-columns">
        <section className="panel">
          <h2>NTP Timeline</h2>
          <TimelinePanel />
        </section>
        <section className="panel">
          <h2>Single Fire</h2>
          <FirePanel />
        </section>
      </div>

      <section className="panel">
        <h2>Send / Receive Monitor</h2>
        <LogsPanel sendlog={sendlog} receivelog={receivelog} />
      </section>

      <section className="panel">
        <h2>Diagnostics / Rejects</h2>
        <DiagnosticsPanel entries={diagnostics} />
      </section>
    </div>
  );
}

function wsStatusColor(status: string): string {
  switch (status) {
    case "connected":
      return "#4cf0b7";
    case "connecting":
      return "#ffd260";
    default:
      return "#ff7a7a";
  }
}
