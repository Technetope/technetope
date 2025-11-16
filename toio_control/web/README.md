# Web Dashboard Stack

This directory hosts the new web-first dashboard that mirrors the requirements in
`acoustics/docs/gui_dashboard_requirements.md`.

## Structure

- `backend/` – TypeScript/Express service that:
  - streams `state/devices.json` updates,
  - terminates WebSocket connections from the monitor,
  - exposes REST endpoints for devices, diagnostics, logs, timeline preview/send, and the single-fire command,
  - shells out to `build/scheduler/agent_a_scheduler` (dry-run by default) to deliver timelines.
- `dashboard/` – React/Vite SPA that renders the modern UI (device cards, NTP header, timeline uploader, send/receive monitor, diagnostics, and single-fire form). It consumes the REST/WebSocket APIs from the backend.

## Install

```bash
# from repo root
npm install
```

This installs dependencies for both workspaces (`@acoustics/gui-backend`, `@acoustics/gui-dashboard`).

## Backend

```bash
npm run dev --workspace @acoustics/gui-backend
```

Key env/config knobs (override by copying `backend/config.example.json` to `backend/config.json` or by setting `GUI_BACKEND_CONFIG`):

- `host` / `port`: bind address for REST+WS.
- `monitorWsUrl`: upstream WebSocket emitting `/heartbeat`, `/diagnostics`, `/sendlog`, `/reject`.
- `devicesPath`, `diagnosticsPath`: file watchers for baseline snapshots.
- `schedulerBinary`: path to `agent_a_scheduler` (dry-run until you flip `dryRun` to `false`).

## Frontend

```bash
npm run dev --workspace @acoustics/gui-dashboard
```

The Vite dev server proxies `/api` and `/ws` to the backend port (`48090` by default) so the SPA can talk to the same API surface during development.

`npm run build --workspace @acoustics/gui-dashboard` emits static assets under `acoustics/web/dashboard/dist/`. Serve them behind any static host and point them at the backend URL.

## Feature Coverage

- **Device Grid** – Latency-colored cards sourced from `state/devices.json` + live heartbeats.
- **NTP Header** – Top bar displays current backend UTC in sync with `/api/ntp`.
- **Timeline Flow** – Upload JSON → preview (Zod schema) → arm/send (CLI bridge).
- **Send/Receive Monitor** – WebSocket feed for `/ws` powering log tables plus audit JSONL.
- **Instant Fire** – Safety checkbox + form for preset/targets/lead time (3 s default).
- **Diagnostics** – `/diagnostics/reject` entries listed with severity chips and jump context.

See `acoustics/docs/gui_dashboard_requirements.md` for the authoritative UX specification; this stack implements those modules in a web-native form.
