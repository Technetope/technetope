import EventEmitter from "events";
import fs from "fs";
import path from "path";

import chokidar from "chokidar";

import { config } from "./config.js";
import { DeviceSnapshot, DeviceSeverity } from "./types.js";

const HEARTBEAT_WARNING_MS = 3_000;
const HEARTBEAT_CRITICAL_MS = 10_000;
const LATENCY_WARN_MS = 50;
const LATENCY_CRITICAL_MS = 150;

interface RawDevice {
  id?: string;
  device_id?: string;
  alias?: string;
  ip?: string;
  port?: number;
  last_seen?: string;
  last_heartbeat?: string;
  heartbeat?: {
    mean_ms?: number;
    latency_mean_ms?: number;
    latency_max_ms?: number;
    m2?: number;
  };
  latency_mean_ms?: number;
  latency_p99_ms?: number;
  ntp_offset_ms?: number;
  tags?: string[];
  rssi?: number;
  battery_pct?: number;
}

export class DeviceStore extends EventEmitter {
  private devices = new Map<string, DeviceSnapshot>();
  private watcher?: chokidar.FSWatcher;

  constructor(private readonly devicesPath: string) {
    super();
  }

  async init(): Promise<void> {
    await this.loadFromDisk();
    this.watch();
  }

  getAll(): DeviceSnapshot[] {
    return Array.from(this.devices.values()).sort((a, b) => a.deviceId.localeCompare(b.deviceId));
  }

  applyHeartbeat(event: {
    deviceId: string;
    latencyMs?: number;
    timestampIso?: string;
    queueDepth?: number;
    isPlaying?: boolean;
  }): void {
    const existing = this.devices.get(event.deviceId);
    const now = Date.now();
    const lastHeartbeatIso = event.timestampIso ?? new Date(now).toISOString();
    const latencyMean = event.latencyMs ?? existing?.latencyMeanMs ?? 0;
    const updated: DeviceSnapshot = {
      deviceId: event.deviceId,
      alias: existing?.alias,
      ip: existing?.ip,
      port: existing?.port,
      lastHeartbeatIso,
      ntpOffsetMs: existing?.ntpOffsetMs,
      latencyMeanMs: latencyMean,
      latencyMaxMs: Math.max(event.latencyMs ?? 0, existing?.latencyMaxMs ?? 0),
      rssi: existing?.rssi,
      batteryPct: existing?.batteryPct,
      tags: existing?.tags,
      queueDepth: event.queueDepth ?? existing?.queueDepth,
      isPlaying: event.isPlaying ?? existing?.isPlaying,
      status: classifySeverity(lastHeartbeatIso, latencyMean),
      lastUpdated: now
    };
    this.devices.set(event.deviceId, updated);
    this.emitUpdate();
  }

  updateDiagnosticsBaseline(deviceId: string, overrides: Partial<DeviceSnapshot>): void {
    const existing = this.devices.get(deviceId);
    if (!existing) {
      const now = Date.now();
      this.devices.set(deviceId, {
        deviceId,
        status: "offline",
        lastUpdated: now,
        ...overrides
      });
    } else {
      this.devices.set(deviceId, {
        ...existing,
        ...overrides
      });
    }
    this.emitUpdate();
  }

  private watch(): void {
    if (!fs.existsSync(path.dirname(this.devicesPath))) {
      fs.mkdirSync(path.dirname(this.devicesPath), { recursive: true });
    }
    this.watcher = chokidar.watch(this.devicesPath, { ignoreInitial: true, awaitWriteFinish: true });
    this.watcher.on("add", () => this.loadFromDisk());
    this.watcher.on("change", () => this.loadFromDisk());
  }

  private async loadFromDisk(): Promise<void> {
    if (!fs.existsSync(this.devicesPath)) {
      return;
    }
    try {
      const raw = await fs.promises.readFile(this.devicesPath, "utf-8");
      if (!raw.trim()) {
        return;
      }
      const parsed = JSON.parse(raw) as unknown;
      const devices = this.extractDevices(parsed);
      const now = Date.now();
      devices.forEach((device) => {
        const previous = this.devices.get(device.deviceId);
        this.devices.set(device.deviceId, {
          ...previous,
          ...device,
          status: device.status ?? previous?.status ?? "offline",
          lastUpdated: now
        });
      });
      this.emitUpdate();
    } catch (error) {
      console.warn(`[DeviceStore] Failed to load devices: ${(error as Error).message}`);
    }
  }

  private extractDevices(data: unknown): DeviceSnapshot[] {
    if (Array.isArray(data)) {
      return data.map((entry) => this.normalizeEntry(entry)).filter((entry): entry is DeviceSnapshot => Boolean(entry));
    }
    if (data && typeof data === "object" && "devices" in data) {
      const list = (data as { devices?: RawDevice[] }).devices ?? [];
      return list.map((entry) => this.normalizeEntry(entry)).filter((entry): entry is DeviceSnapshot => Boolean(entry));
    }
    return [];
  }

  private normalizeEntry(entry: unknown): DeviceSnapshot | undefined {
    if (!entry || typeof entry !== "object") {
      return undefined;
    }
    const raw = entry as RawDevice;
    const deviceId = raw.device_id ?? raw.id;
    if (!deviceId) {
      return undefined;
    }
    const lastHeartbeatIso = raw.last_heartbeat ?? raw.last_seen;
    const latencyMean = raw.latency_mean_ms ?? raw.heartbeat?.mean_ms ?? raw.heartbeat?.latency_mean_ms;
    const latencyMax = raw.latency_p99_ms ?? raw.heartbeat?.latency_max_ms;
    const status = classifySeverity(lastHeartbeatIso, latencyMean);
    return {
      deviceId,
      alias: raw.alias,
      ip: raw.ip,
      port: raw.port,
      lastHeartbeatIso,
      latencyMeanMs: latencyMean,
      latencyMaxMs: latencyMax,
      ntpOffsetMs: raw.ntp_offset_ms,
      tags: raw.tags,
      rssi: raw.rssi,
      batteryPct: raw.battery_pct,
      status,
      lastUpdated: Date.now()
    };
  }

  private emitUpdate(): void {
    this.emit("update", this.getAll());
  }
}

function classifySeverity(lastHeartbeatIso?: string, latencyMeanMs?: number): DeviceSeverity {
  if (!lastHeartbeatIso) {
    return "offline";
  }
  const lastHeartbeat = Date.parse(lastHeartbeatIso);
  if (Number.isNaN(lastHeartbeat)) {
    return "offline";
  }
  const age = Date.now() - lastHeartbeat;
  if (age > HEARTBEAT_CRITICAL_MS) {
    return "critical";
  }
  if (age > HEARTBEAT_WARNING_MS) {
    return "warning";
  }
  if (latencyMeanMs === undefined) {
    return "ok";
  }
  if (latencyMeanMs > LATENCY_CRITICAL_MS) {
    return "critical";
  }
  if (latencyMeanMs > LATENCY_WARN_MS) {
    return "warning";
  }
  return "ok";
}

export function createDeviceStore(): DeviceStore {
  return new DeviceStore(config.devicesPath);
}

export type DeviceStoreEventNames = keyof DeviceStoreEvents;
