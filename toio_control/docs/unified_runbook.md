# Unified Control Runbook

This document explains how to reproduce the current unified build that drives
both **swarm_control** (toio drive) and the **scheduler** (acoustics) in a
single executable. Share this with anyone who needs to rebuild or operate the
integration locally.

---

## 1. Repository Layout

| Path | Purpose |
|------|---------|
| `pc_tools/swarm_control` | Real-time toio drive controller (high-frequency OSC). |
| `pc_tools/scheduler` | Acoustic scheduler (timeline → OSC bundle). |
| `pc_tools/unified` | Entry point that instantiates both controllers. |
| `docs/architecture_refactoring.md` | Background on the directory split & OSC policy. |

The unified executable links directly against `toio_control_scheduler` and the
swarm sources, so any change in those modules is automatically reflected.

---

## 2. Prerequisites

| Requirement | Notes |
|-------------|-------|
| Linux with CMake ≥ 3.16 | Tested on Arch Linux (kernel 6.17). |
| GCC / Clang with C++17 | The project enforces `-std=c++17`. |
| `git submodule` not required | Third-party deps fetched via `FetchContent`. |
| OpenSSL dev package | Used for OSC AES-CTR encryption. |

Optional: `ninja` if you prefer `-G Ninja`.

---

## 3. Clean Build Steps

```bash
# 1. Navigate to repo root
cd /home/ksk432/biotope

# 2. (Recommended) remove any stale cache from other projects
rm -rf build/toio_control

# 3. Configure
cmake -S toio_control/pc_tools \
      -B build/toio_control \
      -DCMAKE_BUILD_TYPE=Release

# 4. Build unified target (pulls fmt/spdlog/asio/... automatically)
cmake --build build/toio_control --target toio_control
```

Artifacts:

- Executable: `build/toio_control/unified/toio_control`
- Static libs:
  - `libtoio_control_osc.a`
  - `libtoio_control_scheduler.a`

---

## 4. Configuration

Sample file: `pc_tools/unified/config/toio_control_config.example.json`

```jsonc
{
  "osc": {
    "listen_port": 5006,
    "send_port": 5005,
    "send_address": "255.255.255.255",
    "broadcast": true,
    "encryption": {
      "enabled": true,
      "key_file": "toio_control/secrets/osc_config.json"
    }
  },
  "swarm": {
    "enabled": true,
    "assignment_storage": "state/swarm_assignments.json",
    "device_registry": "state/devices.json"
  },
  "scheduler": {
    "enabled": true,
    "timeline_path": "toio_control/pc_tools/scheduler/examples/basic_timeline.json",
    "lead_time": 3.0,
    "bundle_spacing": 0.01,
    "target_map": "",
    "default_targets": []
  }
}
```

Key toggles:

- `swarm.enabled` – start/stop real-time drive control.
- `scheduler.enabled` – start/stop acoustic scheduler thread.
- `scheduler.timeline_path` – JSON timeline file (see scheduler README).
- `osc.encryption.enabled` + `key_file` – AES-256-CTR config shared by both controllers.

---

## 5. Running the Unified App

```bash
./build/toio_control/unified/toio_control \
    -c config/toio_control_config.json
```

Behaviour:

- Swarm loop runs in the main thread and uses `swarm_control::OscSender`
  (constant connection, high-frequency).
- Scheduler executes on a detached worker thread and uses
  `scheduler::osc::OscBundleSender` (batch send, disconnect after send).
- Both components reuse the same DeviceRegistry / assignment files.

Logs:

- Swarm control prints assignment / target info every 10 seconds.
- Scheduler reports bundle counts when a timeline finishes.

---

## 6. Verification Checklist

1. **Build succeeds** without linker errors (see Section 3).
2. **Config file resolves** (app warns if paths missing).
3. **Swarm-only mode** – set `"scheduler.enabled": false` and verify positions
   are received and targets sent.
4. **Scheduler-only mode** – set `"swarm.enabled": false` and verify bundles are
   sent (watch logs for `Scheduler completed: X bundles sent`).
5. **Fully unified** – enable both and observe concurrent logs.

Optional automated tests (from repo root):

```bash
cmake --build build/toio_control --target toio_control_scheduler_tests
ctest --test-dir build/toio_control
```

---

## 7. Publishing / Push Checklist

1. `git status` – ensure only intended files modified.
2. `git add` key files (CMake, doc, configs, etc.).
3. `git commit -m "Add unified runbook and build integration"` (adjust msg).
4. `git push <remote> <branch>`.

If sharing binaries, archive `build/toio_control/unified/toio_control` plus the
config/timeline assets referenced above.

---

## 8. References

- `pc_tools/unified/README.md`
- `pc_tools/README.md` (module overview)
- `docs/architecture_refactoring.md` (motivation & design notes)

