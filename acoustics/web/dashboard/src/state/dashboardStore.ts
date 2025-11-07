import { create } from "zustand";

import {
  DeviceSnapshot,
  DiagnosticsEntry,
  ReceiveLogEntry,
  SendLogEntry,
  TimelineDispatchResult,
  TimelineEvent,
  WebsocketPush
} from "../types";

type WsStatus = "connecting" | "connected" | "disconnected";

interface DashboardState {
  devices: DeviceSnapshot[];
  diagnostics: DiagnosticsEntry[];
  sendlog: SendLogEntry[];
  receivelog: ReceiveLogEntry[];
  timeline: TimelineEvent[];
  timelinePreview?: TimelineEvent[];
  ntpNow?: string;
  wsStatus: WsStatus;
  init: () => Promise<void>;
  refresh: () => Promise<void>;
  previewTimeline: (payload: unknown) => Promise<void>;
  sendTimeline: (payload: unknown) => Promise<TimelineDispatchResult>;
  fireOnce: (payload: {
    presetId: string;
    targets: string[];
    leadTimeSeconds: number;
    gainDb?: number;
  }) => Promise<TimelineDispatchResult>;
  connectSocket: () => void;
}

let socket: WebSocket | undefined;

export const useDashboardStore = create<DashboardState>((set, get) => ({
  devices: [],
  diagnostics: [],
  sendlog: [],
  receivelog: [],
  timeline: [],
  wsStatus: "connecting",

  init: async () => {
    await get().refresh();
    get().connectSocket();
  },

  refresh: async () => {
    const [devices, diagnostics, sendlog, receivelog, timeline, ntp] = await Promise.all([
      getJson<{ devices: DeviceSnapshot[] }>("/api/devices"),
      getJson<{ diagnostics: DiagnosticsEntry[] }>("/api/diagnostics"),
      getJson<{ entries: SendLogEntry[] }>("/api/sendlog"),
      getJson<{ entries: ReceiveLogEntry[] }>("/api/receivelog"),
      getJson<{ events: TimelineEvent[] }>("/api/timeline"),
      getJson<{ nowUtc: string }>("/api/ntp")
    ]);
    set({
      devices: devices.devices,
      diagnostics: diagnostics.diagnostics,
      sendlog: sendlog.entries,
      receivelog: receivelog.entries,
      timeline: timeline.events,
      ntpNow: ntp.nowUtc
    });
  },

  previewTimeline: async (payload: unknown) => {
    const result = await postJson<{ events: TimelineEvent[] }>("/api/timeline/preview", payload);
    set({ timelinePreview: result.events });
  },

  sendTimeline: async (payload: unknown) => {
    const result = await postJson<TimelineDispatchResult>("/api/timeline/send", payload);
    set({ timelinePreview: undefined });
    return result;
  },

  fireOnce: async ({ presetId, targets, leadTimeSeconds, gainDb }) => {
    const result = await postJson<TimelineDispatchResult>("/api/fire", {
      preset_id: presetId,
      targets,
      lead_time_seconds: leadTimeSeconds,
      gain_db: gainDb
    });
    return result;
  },

  connectSocket: () => {
    socket?.close();
    set({ wsStatus: "connecting" });
    const url = buildWsUrl();
    socket = new WebSocket(url);
    socket.onopen = () => set({ wsStatus: "connected" });
    socket.onclose = () => set({ wsStatus: "disconnected" });
    socket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data) as WebsocketPush;
        handleSocketPayload(data, set);
      } catch (error) {
        console.warn("WS parse error", error);
      }
    };
  }
}));

async function getJson<T>(path: string): Promise<T> {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`GET ${path} failed (${response.status})`);
  }
  return response.json() as Promise<T>;
}

async function postJson<T>(path: string, body: unknown): Promise<T> {
  const response = await fetch(path, {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify(body)
  });
  if (!response.ok) {
    const text = await response.text();
    throw new Error(text || `POST ${path} failed`);
  }
  return response.json() as Promise<T>;
}

function buildWsUrl(): string {
  const isHttps = window.location.protocol === "https:";
  const protocol = isHttps ? "wss" : "ws";
  return `${protocol}://${window.location.host}/ws`;
}

type StoreSetter = (
  partial: Partial<DashboardState> | ((state: DashboardState) => Partial<DashboardState>),
  replace?: boolean
) => void;

function handleSocketPayload(data: WebsocketPush, set: StoreSetter): void {
  switch (data.type) {
    case "devices":
      set(() => ({ devices: data.devices }));
      break;
    case "diagnostics":
      set((state) => ({ diagnostics: [...state.diagnostics, data.entry].slice(-500) }));
      break;
    case "sendlog":
      set((state) => ({ sendlog: [...state.sendlog, data.entry].slice(-500) }));
      break;
    case "receivelog":
      set((state) => ({ receivelog: [...state.receivelog, data.entry].slice(-500) }));
      break;
    case "timeline":
      set(() => ({ timeline: data.events }));
      break;
    case "ntp":
      set(() => ({ ntpNow: data.nowUtc }));
      break;
    default:
      break;
  }
}
