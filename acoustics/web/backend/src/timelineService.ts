import EventEmitter from "events";
import fs from "fs";
import os from "os";
import path from "path";
import { spawn } from "child_process";
import { randomUUID } from "crypto";

import { config } from "./config.js";
import { appendJsonl } from "./logStore.js";
import { FireCommand, TimelineEvent, TimelinePayload, timelineSchema } from "./types.js";

export interface TimelineDispatchResult {
  requestId: string;
  events: TimelineEvent[];
}

export class TimelineService extends EventEmitter {
  private events: TimelineEvent[] = [];

  getEvents(): TimelineEvent[] {
    return [...this.events];
  }

  async preview(payload: unknown): Promise<TimelineEvent[]> {
    const parsed = timelineSchema.parse(payload);
    return this.flattenTimeline(parsed, randomUUID());
  }

  async dispatch(payload: unknown, requestId = randomUUID()): Promise<TimelineDispatchResult> {
    const parsed = timelineSchema.parse(payload);
    const events = this.flattenTimeline(parsed, requestId);
    this.appendEvents(events);
    this.emitUpdate();
    await appendJsonl(config.auditLogPath, { type: "timeline_dispatch", requestId, timeline_id: parsed.timeline_id });
    await this.invokeScheduler(parsed, requestId, events);
    return { requestId, events };
  }

  async fire(command: FireCommand): Promise<TimelineDispatchResult> {
    const timeUtc = new Date(Date.now() + command.leadTimeMs).toISOString();
    const payload: TimelinePayload = {
      timeline_id: `single-fire-${command.requestId}`,
      events: [
        {
          time_utc: timeUtc,
          targets: command.targets,
          preset: command.presetId,
          lead_time_ms: command.leadTimeMs,
          gain: command.gainDb
        }
      ]
    };
    return this.dispatch(payload, command.requestId);
  }

  private appendEvents(events: TimelineEvent[]): void {
    this.events.push(...events);
    const MAX = 500;
    if (this.events.length > MAX) {
      this.events.splice(0, this.events.length - MAX);
    }
  }

  private flattenTimeline(payload: TimelinePayload, requestId: string): TimelineEvent[] {
    return payload.events.map((event, index) => ({
      id: `${requestId}-${index}`,
      requestId,
      preset: event.preset,
      targets: event.targets,
      scheduledTimeUtc: event.time_utc,
      status: "scheduled" as TimelineEvent["status"]
    }));
  }

  private async invokeScheduler(timeline: TimelinePayload, requestId: string): Promise<void> {
    if (!fs.existsSync(config.schedulerBinary)) {
      console.warn(`[TimelineService] Scheduler binary not found: ${config.schedulerBinary}`);
      this.markEvents(requestId, "failed");
      this.emitUpdate();
      return;
    }
    const tempDir = await fs.promises.mkdtemp(path.join(os.tmpdir(), "timeline-"));
    const timelinePath = path.join(tempDir, `${timeline.timeline_id || requestId}.json`);
    await fs.promises.writeFile(timelinePath, JSON.stringify(timeline, null, 2), "utf-8");
    const args = [
      timelinePath,
      "--host",
      config.defaultSend.host,
      "--port",
      config.defaultSend.port.toString(),
      "--lead-time",
      config.defaultSend.leadTimeSeconds.toString(),
      "--osc-config",
      config.oscConfigPath
    ];
    if (config.dryRun) {
      args.push("--dry-run");
    }
    await appendJsonl(config.auditLogPath, { type: "scheduler_exec", requestId, args });
    await this.runProcess(config.schedulerBinary, args)
      .then(() => {
        this.markEvents(requestId, "sent");
      })
      .catch((error) => {
        console.error("[TimelineService] scheduler failed:", error);
        this.markEvents(requestId, "failed");
      })
      .finally(() => this.emitUpdate());
  }

  private markEvents(requestId: string, status: TimelineEvent["status"]): void {
    this.events = this.events.map((event) => {
      if (event.requestId === requestId) {
        return { ...event, status: status === "sent" ? "sent" : "failed" };
      }
      return event;
    });
  }

  private emitUpdate(): void {
    this.emit("update", this.getEvents());
  }

  private runProcess(cmd: string, args: string[]): Promise<void> {
    return new Promise((resolve, reject) => {
      const child = spawn(cmd, args, { stdio: ["ignore", "pipe", "pipe"] });
      child.stdout?.on("data", (chunk) => process.stdout.write(`[scheduler] ${chunk}`));
      child.stderr?.on("data", (chunk) => process.stderr.write(`[scheduler] ${chunk}`));
      child.on("error", reject);
      child.on("close", (code) => {
        if (code === 0) {
          resolve();
        } else {
          reject(new Error(`scheduler exited with code ${code}`));
        }
      });
    });
  }
}

export function createTimelineService(): TimelineService {
  return new TimelineService();
}
