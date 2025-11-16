export type DeviceSeverity = "ok" | "warning" | "critical" | "offline";

export interface DeviceSnapshot {
  deviceId: string;
  alias?: string;
  ip?: string;
  port?: number;
  lastHeartbeatIso?: string;
  ntpOffsetMs?: number;
  latencyMeanMs?: number;
  latencyMaxMs?: number;
  rssi?: number;
  batteryPct?: number;
  tags?: string[];
  queueDepth?: number;
  isPlaying?: boolean;
  status: DeviceSeverity;
  lastUpdated: number;
}

export interface DiagnosticsEntry {
  id: string;
  deviceId: string;
  reason: string;
  severity: "info" | "warn" | "critical";
  timestamp: string;
}

export interface SendLogEntry {
  id: string;
  requestId: string;
  deviceId: string;
  preset: string;
  status: "pending" | "succeeded" | "failed";
  scheduledTimeUtc: string;
  deliveredAtUtc?: string;
  latencyMs?: number;
  error?: string;
}

export interface ReceiveLogEntry {
  id: string;
  deviceId: string;
  type: "heartbeat" | "diagnostics" | "reject";
  payload: Record<string, unknown>;
  timestamp: string;
}

export interface TimelineEvent {
  id: string;
  requestId: string;
  preset: string;
  targets: string[];
  scheduledTimeUtc: string;
  status: "scheduled" | "sent" | "failed";
}

export interface TimelineDispatchResult {
  requestId: string;
  events: TimelineEvent[];
}

export interface FireFormState {
  presetId: string;
  targets: string;
  leadTimeSeconds: number;
  gainDb?: number;
  armed: boolean;
}

export type WebsocketPush =
  | { type: "devices"; devices: DeviceSnapshot[] }
  | { type: "diagnostics"; entry: DiagnosticsEntry }
  | { type: "sendlog"; entry: SendLogEntry }
  | { type: "receivelog"; entry: ReceiveLogEntry }
  | { type: "timeline"; events: TimelineEvent[] }
  | { type: "ntp"; nowUtc: string };
