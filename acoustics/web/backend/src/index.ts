import cors from "cors";
import express from "express";
import http from "http";
import { WebSocketServer } from "ws";
import { randomUUID } from "crypto";

import { config } from "./config.js";
import { createDeviceStore } from "./deviceStore.js";
import { MonitorClient } from "./monitorClient.js";
import { LogStore } from "./logStore.js";
import { createTimelineService } from "./timelineService.js";
import { WebsocketHub } from "./websocketHub.js";
import {
  DiagnosticsEntry,
  ReceiveLogEntry,
  SendLogEntry,
  fireSchema
} from "./types.js";

const deviceStore = createDeviceStore();
const diagnosticsStore = new LogStore<DiagnosticsEntry>();
const sendLogStore = new LogStore<SendLogEntry>();
const receiveLogStore = new LogStore<ReceiveLogEntry>();
const timelineService = createTimelineService();

async function main(): Promise<void> {
  await deviceStore.init();

  const app = express();
  app.use(cors());
  app.use(express.json({ limit: "2mb" }));

  const server = http.createServer(app);
  const wss = new WebSocketServer({ server });
  const hub = new WebsocketHub(wss);

  deviceStore.on("update", (devices) => hub.broadcast({ type: "devices", devices }));
  timelineService.on("update", (events) => hub.broadcast({ type: "timeline", events }));

  const monitorClient = new MonitorClient(config.monitorWsUrl, {
    deviceStore,
    diagnosticsStore,
    sendLogStore,
    receiveLogStore,
    hub
  });
  monitorClient.start();

  setInterval(() => {
    hub.broadcast({ type: "ntp", nowUtc: new Date().toISOString() });
  }, 1_000);

  const asyncHandler =
    <T>(handler: express.RequestHandler): express.RequestHandler =>
    (req, res, next) => {
      Promise.resolve(handler(req, res, next)).catch(next);
    };

  app.get(
    "/api/health",
    (_req, res) => {
      res.json({ status: "ok" });
    }
  );

  app.get(
    "/api/devices",
    (_req, res) => {
      res.json({ devices: deviceStore.getAll() });
    }
  );

  app.get(
    "/api/diagnostics",
    (_req, res) => {
      res.json({ diagnostics: diagnosticsStore.all() });
    }
  );

  app.get(
    "/api/sendlog",
    (_req, res) => {
      res.json({ entries: sendLogStore.all() });
    }
  );

  app.get(
    "/api/receivelog",
    (_req, res) => {
      res.json({ entries: receiveLogStore.all() });
    }
  );

  app.get(
    "/api/timeline",
    (_req, res) => {
      res.json({ events: timelineService.getEvents() });
    }
  );

  app.get(
    "/api/ntp",
    (_req, res) => {
      res.json({ nowUtc: new Date().toISOString() });
    }
  );

  app.post(
    "/api/timeline/preview",
    asyncHandler(async (req, res) => {
      const events = await timelineService.preview(req.body);
      res.json({ events });
    })
  );

  app.post(
    "/api/timeline/send",
    asyncHandler(async (req, res) => {
      const result = await timelineService.dispatch(req.body);
      res.json(result);
    })
  );

  app.post(
    "/api/fire",
    asyncHandler(async (req, res) => {
      const payload = fireSchema.parse(req.body);
      const command = {
        presetId: payload.preset_id,
        targets: payload.targets,
        leadTimeMs: Math.round(payload.lead_time_seconds * 1000),
        gainDb: payload.gain_db,
        requestId: payload.request_id ?? randomUUID()
      };
      const result = await timelineService.fire(command);
      res.json(result);
    })
  );

  app.use((err: unknown, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
    const message = err instanceof Error ? err.message : String(err);
    const stack = err instanceof Error ? err.stack : undefined;
    console.error(stack ?? message);
    res.status(400).json({ error: message });
  });

  server.listen(config.port, config.host, () => {
    console.log(`[backend] listening on http://${config.host}:${config.port}`);
  });

  process.on("SIGINT", () => {
    monitorClient.stop();
    server.close(() => process.exit(0));
  });
}

main().catch((error) => {
  console.error("[backend] failed to start:", error);
  process.exit(1);
});
