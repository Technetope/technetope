import fs from "fs";
import path from "path";

export interface BackendConfig {
  host: string;
  port: number;
  devicesPath: string;
  diagnosticsPath: string;
  auditLogPath: string;
  eventCsvPath: string;
  metricsPath: string;
  schedulerBinary: string;
  oscConfigPath: string;
  monitorWsUrl?: string;
  defaultSend: {
    host: string;
    port: number;
    leadTimeSeconds: number;
  };
  dryRun: boolean;
}

const repoRoot = path.resolve(process.cwd(), "../../..");

function resolveRepoPath(...segments: string[]): string {
  return path.join(repoRoot, ...segments);
}

function readConfigFile(): Partial<BackendConfig> | undefined {
  const explicit = process.env.GUI_BACKEND_CONFIG;
  const candidate = explicit ?? resolveRepoPath("acoustics", "web", "backend", "config.json");
  if (!fs.existsSync(candidate)) {
    return undefined;
  }
  try {
    const raw = fs.readFileSync(candidate, "utf-8");
    return JSON.parse(raw);
  } catch (error) {
    console.warn(`[config] Failed to parse ${candidate}: ${(error as Error).message}`);
    return undefined;
  }
}

const defaults: BackendConfig = {
  host: "127.0.0.1",
  port: 48090,
  devicesPath: resolveRepoPath("state", "devices.json"),
  diagnosticsPath: resolveRepoPath("state", "diagnostics.json"),
  auditLogPath: resolveRepoPath("logs", "gui_audit.jsonl"),
  eventCsvPath: resolveRepoPath("logs", "gui_event_log.csv"),
  metricsPath: resolveRepoPath("logs", "gui_dashboard_metrics.jsonl"),
  schedulerBinary: resolveRepoPath("build", "scheduler", "agent_a_scheduler"),
  oscConfigPath: resolveRepoPath("acoustics", "secrets", "osc_config.json"),
  monitorWsUrl: "ws://127.0.0.1:48080/ws/events",
  defaultSend: {
    host: "255.255.255.255",
    port: 9000,
    leadTimeSeconds: 4
  },
  dryRun: true
};

const merged = Object.assign({}, defaults, readConfigFile());

export const config: BackendConfig = {
  ...defaults,
  ...merged
};

export const paths = {
  repoRoot,
  resolveRepoPath
};
