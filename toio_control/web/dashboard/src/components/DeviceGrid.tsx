import clsx from "clsx";

import { DeviceSnapshot } from "../types";
import { formatIso, timeAgo } from "../utils/time";

interface Props {
  devices: DeviceSnapshot[];
}

export function DeviceGrid({ devices }: Props) {
  if (!devices.length) {
    return <p>No devices detected.</p>;
  }
  return (
    <div className="device-grid">
      {devices.map((device) => (
        <article key={device.deviceId} className="device-card" data-status={device.status}>
          <div className="device-card-header">
            <div>
              <div className="device-alias">{device.alias || device.deviceId}</div>
              <div className="device-id">{device.deviceId}</div>
            </div>
            <span className={clsx("status-pill", device.status)}>{device.status}</span>
          </div>
          <div className="device-meta">
            <span>
              Latency:{" "}
              <strong style={{ color: latencyColor(device.latencyMeanMs ?? 0) }}>
                {device.latencyMeanMs?.toFixed(1) ?? "-"} ms
              </strong>
            </span>
            <span>Heartbeat: {timeAgo(device.lastHeartbeatIso)}</span>
            <span>IP: {device.ip ? `${device.ip}:${device.port ?? 0}` : "?"}</span>
            <span>RSSI/Batt: {formatOptional(device.rssi, "dBm")} / {formatOptional(device.batteryPct, "%")}</span>
            <span>Queue: {device.queueDepth ?? 0} | Playing: {device.isPlaying ? "Yes" : "No"}</span>
            <span>NTP offset: {device.ntpOffsetMs !== undefined ? `${device.ntpOffsetMs.toFixed(1)} ms` : "-"}</span>
          </div>
          {device.tags && device.tags.length > 0 && (
            <div style={{ fontSize: "0.78rem", opacity: 0.8 }}>Tags: {device.tags.join(", ")}</div>
          )}
          <div style={{ fontSize: "0.75rem", opacity: 0.7 }}>Last seen: {formatIso(device.lastHeartbeatIso)}</div>
        </article>
      ))}
    </div>
  );
}

function latencyColor(latency: number): string {
  if (latency < 50) {
    return "#4cf0b7";
  }
  if (latency < 150) {
    return "#ffd260";
  }
  return "#ff7a7a";
}

function formatOptional(value: number | undefined, suffix: string): string {
  return value !== undefined ? `${value}${suffix}` : "-";
}
