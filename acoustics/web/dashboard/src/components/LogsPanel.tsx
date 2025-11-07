import { ReceiveLogEntry, SendLogEntry } from "../types";
import { formatIso } from "../utils/time";

interface Props {
  sendlog: SendLogEntry[];
  receivelog: ReceiveLogEntry[];
}

export function LogsPanel({ sendlog, receivelog }: Props) {
  return (
    <div className="logs-container">
      <div>
        <h3>Send Log</h3>
        <div className="table-scroll">
          <table className="log-table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Device</th>
                <th>Preset</th>
                <th>Status</th>
              </tr>
            </thead>
            <tbody>
              {sendlog.slice().reverse().map((entry) => (
                <tr key={entry.id}>
                  <td>{formatIso(entry.deliveredAtUtc ?? entry.scheduledTimeUtc)}</td>
                  <td>{entry.deviceId}</td>
                  <td>{entry.preset}</td>
                  <td>{entry.status}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
      <div>
        <h3>Receive Log</h3>
        <div className="table-scroll">
          <table className="log-table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Device</th>
                <th>Type</th>
              </tr>
            </thead>
            <tbody>
              {receivelog.slice().reverse().map((entry) => (
                <tr key={entry.id}>
                  <td>{formatIso(entry.timestamp)}</td>
                  <td>{entry.deviceId}</td>
                  <td>{entry.type}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
