import WebSocket from "ws";
import { randomUUID } from "crypto";

import { DeviceStore } from "./deviceStore.js";
import { config } from "./config.js";
import { LogStore, appendJsonl } from "./logStore.js";
import {
  DiagnosticsEntry,
  ReceiveLogEntry,
  SendLogEntry,
  WebsocketPush
} from "./types.js";
import { WebsocketHub } from "./websocketHub.js";

interface MonitorClientDeps {
  deviceStore: DeviceStore;
  diagnosticsStore: LogStore<DiagnosticsEntry>;
  sendLogStore: LogStore<SendLogEntry>;
  receiveLogStore: LogStore<ReceiveLogEntry>;
  hub: WebsocketHub;
}

const RECONNECT_BASE_MS = 1_000;
const RECONNECT_MAX_MS = 10_000;

export class MonitorClient {
  private socket?: WebSocket;
  private reconnectTimer?: NodeJS.Timeout;

  constructor(private readonly url: string | undefined, private readonly deps: MonitorClientDeps) {}

  start(): void {
    if (!this.url) {
      console.warn("[MonitorClient] monitorWsUrl not configured; skipping connection");
      return;
    }
    this.connect();
  }

  stop(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
    }
    this.socket?.close();
  }

  private connect(delayMs = 0): void {
    if (!this.url) {
      return;
    }
    if (delayMs > 0) {
      this.reconnectTimer = setTimeout(() => this.connect(0), delayMs);
      return;
    }
    this.socket = new WebSocket(this.url);
    this.socket.on("open", () => console.log(`[MonitorClient] connected -> ${this.url}`));
    this.socket.on("message", (data) => this.handleMessage(data));
    this.socket.on("close", (code) => {
      console.warn(`[MonitorClient] disconnected (${code})`);
      this.scheduleReconnect();
    });
    this.socket.on("error", (error) => {
      console.error("[MonitorClient] error:", error);
      this.socket?.close();
    });
  }

  private scheduleReconnect(): void {
    const delay = Math.min(RECONNECT_MAX_MS, RECONNECT_BASE_MS + Math.random() * RECONNECT_MAX_MS);
    this.connect(delay);
  }

  private handleMessage(data: WebSocket.RawData): void {
    try {
      const payload = JSON.parse(data.toString()) as Record<string, unknown>;
      const type = payload["type"];
      if (typeof type !== "string") {
        return;
      }
      switch (type) {
        case "heartbeat":
          this.handleHeartbeat(payload);
          break;
        case "diagnostics":
          this.handleDiagnostics(payload);
          break;
        case "sendlog":
          this.handleSendlog(payload);
          break;
        case "reject":
          this.handleReject(payload);
          break;
        case "hello":
          break;
        default:
          this.handleGeneric(payload);
      }
    } catch (error) {
      console.warn("[MonitorClient] failed to parse message:", error);
    }
  }

  private handleHeartbeat(payload: Record<string, unknown>): void {
    const deviceId = payload["device_id"];
    if (typeof deviceId !== "string") {
      return;
    }
    const latency = typeof payload["latency_ms"] === "number" ? payload["latency_ms"] : undefined;
    const ts = typeof payload["timestamp"] === "string" ? payload["timestamp"] : undefined;
    const queueDepth = typeof payload["queue_depth"] === "number" ? payload["queue_depth"] : undefined;
    const isPlaying = typeof payload["is_playing"] === "boolean" ? payload["is_playing"] : undefined;
    this.deps.deviceStore.applyHeartbeat({
      deviceId,
      latencyMs: latency,
      timestampIso: ts,
      queueDepth,
      isPlaying
    });

    const entry: ReceiveLogEntry = {
      id: randomUUID(),
      deviceId,
      type: "heartbeat",
      payload,
      timestamp: ts ?? new Date().toISOString()
    };
    this.deps.receiveLogStore.add(entry);
    this.deps.hub.broadcast({ type: "receivelog", entry });
  }

  private handleDiagnostics(payload: Record<string, unknown>): void {
    const deviceId = typeof payload["device_id"] === "string" ? payload["device_id"] : "unknown";
    const entry: DiagnosticsEntry = {
      id: randomUUID(),
      deviceId,
      reason: typeof payload["reason"] === "string" ? payload["reason"] : "Unknown diagnostics",
      severity: this.parseSeverity(payload["severity"]),
      timestamp: typeof payload["timestamp"] === "string" ? payload["timestamp"] : new Date().toISOString()
    };
    this.deps.diagnosticsStore.add(entry);
    this.deps.hub.broadcast({ type: "diagnostics", entry });
  }

  private handleSendlog(payload: Record<string, unknown>): void {
    const entry: SendLogEntry = {
      id: randomUUID(),
      requestId: typeof payload["request_id"] === "string" ? payload["request_id"] : randomUUID(),
      deviceId: typeof payload["device_id"] === "string" ? payload["device_id"] : "unknown",
      preset: typeof payload["preset"] === "string" ? payload["preset"] : "unknown",
      status: this.parseSendStatus(payload["status"]),
      scheduledTimeUtc:
        typeof payload["scheduled_time_utc"] === "string"
          ? payload["scheduled_time_utc"]
          : new Date().toISOString(),
      deliveredAtUtc: typeof payload["delivered_at_utc"] === "string" ? payload["delivered_at_utc"] : undefined,
      latencyMs: typeof payload["latency_ms"] === "number" ? payload["latency_ms"] : undefined,
      error: typeof payload["error"] === "string" ? payload["error"] : undefined
    };
    this.deps.sendLogStore.add(entry);
    this.deps.hub.broadcast({ type: "sendlog", entry });
    appendJsonl(config.auditLogPath, { type: "sendlog", entry }).catch((error) =>
      console.warn("[MonitorClient] audit log append failed:", error)
    );
  }

  private handleReject(payload: Record<string, unknown>): void {
    const entry: ReceiveLogEntry = {
      id: randomUUID(),
      deviceId: typeof payload["device_id"] === "string" ? payload["device_id"] : "unknown",
      type: "reject",
      payload,
      timestamp: typeof payload["timestamp"] === "string" ? payload["timestamp"] : new Date().toISOString()
    };
    this.deps.receiveLogStore.add(entry);
    this.deps.hub.broadcast({ type: "receivelog", entry });
  }

  private handleGeneric(payload: Record<string, unknown>): void {
    const entry: ReceiveLogEntry = {
      id: randomUUID(),
      deviceId: typeof payload["device_id"] === "string" ? payload["device_id"] : "n/a",
      type: "diagnostics",
      payload,
      timestamp: typeof payload["timestamp"] === "string" ? payload["timestamp"] : new Date().toISOString()
    };
    this.deps.receiveLogStore.add(entry);
    this.deps.hub.broadcast({ type: "receivelog", entry });
  }

  private parseSeverity(value: unknown): DiagnosticsEntry["severity"] {
    return value === "critical" || value === "warn" || value === "info" ? value : "info";
  }

  private parseSendStatus(value: unknown): SendLogEntry["status"] {
    return value === "failed" || value === "pending" ? value : "succeeded";
  }
}
