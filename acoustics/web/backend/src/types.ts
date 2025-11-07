import { z } from "zod";

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

export const timelineSchema = z.object({
  timeline_id: z.string(),
  events: z.array(
    z.object({
      time_utc: z.string(),
      targets: z.array(z.string()),
      preset: z.string(),
      lead_time_ms: z.number().nonnegative(),
      gain: z.number().optional()
    })
  )
});

export type TimelinePayload = z.infer<typeof timelineSchema>;

export interface FireCommand {
  presetId: string;
  targets: string[];
  leadTimeMs: number;
  gainDb?: number;
  requestId: string;
}

export const fireSchema = z.object({
  preset_id: z.string(),
  targets: z.array(z.string()).min(1),
  lead_time_seconds: z.number().positive().default(3),
  gain_db: z.number().optional(),
  request_id: z.string().optional()
});

export type FirePayload = z.infer<typeof fireSchema>;

export type WebsocketPush =
  | { type: "devices"; devices: DeviceSnapshot[] }
  | { type: "diagnostics"; entry: DiagnosticsEntry }
  | { type: "sendlog"; entry: SendLogEntry }
  | { type: "receivelog"; entry: ReceiveLogEntry }
  | { type: "timeline"; events: TimelineEvent[] }
  | { type: "ntp"; nowUtc: string };
