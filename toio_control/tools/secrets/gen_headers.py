#!/usr/bin/env python3
"""Generate firmware Secrets.h from osc_config.json.

- Reads acoustics/secrets/osc_config.json (or --config).
- Emits acoustics/firmware/include/Secrets.h ready for PlatformIO builds.
- Registers itself as a PlatformIO extra script when imported via platformio.ini.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Iterable, List


def _detect_acoustics_root() -> Path:
    """Return the acoustics/ directory regardless of how the script is invoked."""
    if "__file__" in globals():
        return Path(__file__).resolve().parents[2]

    project_dir_env = (
        os.environ.get("PROJECT_DIR")
        or os.environ.get("PIOPROJECT_DIR")
        or os.environ.get("PIO_PROJECT_DIR")
    )
    if project_dir_env:
        project_dir = Path(project_dir_env)
        if (project_dir / "platformio.ini").exists():
            return project_dir.parent

    cwd = Path.cwd()
    if (cwd / "platformio.ini").exists():
        return cwd.parent

    raise RuntimeError(
        "Cannot determine acoustics root path. Run this script from the repo "
        "or set PROJECT_DIR/PIOPROJECT_DIR to the PlatformIO project folder."
    )


ACOUSTICS_ROOT = _detect_acoustics_root()
DEFAULT_CONFIG = ACOUSTICS_ROOT / "secrets" / "osc_config.json"
DEFAULT_OUTPUT = ACOUSTICS_ROOT / "firmware" / "include" / "Secrets.h"
ENV_VAR = "OSC_CONFIG_PATH"


class ConfigError(RuntimeError):
    """Raised when the secrets configuration is missing or malformed."""


def _escape_cpp(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def _collect_hex(value: str, expected_bytes: int) -> List[int]:
    digits = [ch for ch in value.strip() if ch.lower() in "0123456789abcdef"]
    if len(digits) != expected_bytes * 2:
        raise ConfigError(
            f"Expected {expected_bytes * 2} hex characters but found {len(digits)} in key/iv"
        )
    bytes_out: List[int] = []
    for i in range(0, len(digits), 2):
        byte = int("".join(digits[i : i + 2]), 16)
        bytes_out.append(byte)
    return bytes_out


def _chunk(values: Iterable[int], size: int) -> Iterable[List[int]]:
    chunk: List[int] = []
    for value in values:
        chunk.append(value)
        if len(chunk) == size:
            yield chunk
            chunk = []
    if chunk:
        yield chunk


def _format_array(name: str, values: List[int], row_width: int = 8) -> str:
    lines = [f"constexpr std::array<uint8_t, {len(values)}> {name} = {{"]
    for chunk in _chunk(values, row_width):
        formatted = ", ".join(f"0x{byte:02X}" for byte in chunk)
        lines.append(f"    {formatted},")
    lines.append("};")
    return "\n".join(lines)


def _load_config(path: Path) -> dict:
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError as exc:
        raise ConfigError(
            f"Secrets config not found: {path}. Copy osc_config.example.json and fill in real values."
        ) from exc
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ConfigError(f"Failed to parse {path}: {exc}") from exc
    return data


def _require(mapping: dict, dotted_path: str) -> object:
    node: object = mapping
    for key in dotted_path.split('.'):
        if not isinstance(node, dict) or key not in node:
            raise ConfigError(f"Missing '{dotted_path}' in osc_config.json")
        node = node[key]
    return node


def _optional(mapping: dict, dotted_path: str, default: object) -> object:
    node: object = mapping
    parts = dotted_path.split('.')
    for key in parts[:-1]:
        if not isinstance(node, dict) or key not in node:
            return default
        node = node[key]
    last = parts[-1]
    if not isinstance(node, dict) or last not in node:
        return default
    return node[last]


def generate(config_path: Path, output_path: Path) -> Path:
    data = _load_config(config_path)

    wifi_primary_ssid = str(_require(data, "wifi.primary.ssid"))
    wifi_primary_pass = str(_require(data, "wifi.primary.pass"))
    wifi_secondary_ssid = str(_optional(data, "wifi.secondary.ssid", ""))
    wifi_secondary_pass = str(_optional(data, "wifi.secondary.pass", ""))

    listen_port = int(_require(data, "osc.listen_port"))
    osc_key_hex = str(_require(data, "osc.key_hex"))
    osc_iv_hex = str(_require(data, "osc.iv_hex"))

    heartbeat_host = str(_require(data, "heartbeat.host"))
    heartbeat_port = int(_require(data, "heartbeat.port"))

    ntp_server = str(_require(data, "ntp.server"))
    ntp_offset = int(_optional(data, "ntp.offset", 0))
    ntp_interval = int(_optional(data, "ntp.interval_ms", 30000))

    osc_key = _collect_hex(osc_key_hex, 32)
    osc_iv = _collect_hex(osc_iv_hex, 16)

    contents = []
    contents.append("#pragma once\n\n")
    contents.append("#include <array>\n\n")
    contents.append("namespace secrets {\n\n")
    contents.append(f"constexpr const char WIFI_PRIMARY_SSID[] = \"{_escape_cpp(wifi_primary_ssid)}\";\n")
    contents.append(f"constexpr const char WIFI_PRIMARY_PASS[] = \"{_escape_cpp(wifi_primary_pass)}\";\n\n")
    contents.append(f"constexpr const char WIFI_SECONDARY_SSID[] = \"{_escape_cpp(wifi_secondary_ssid)}\";\n")
    contents.append(f"constexpr const char WIFI_SECONDARY_PASS[] = \"{_escape_cpp(wifi_secondary_pass)}\";\n\n")
    contents.append(f"constexpr uint16_t OSC_LISTEN_PORT = {listen_port};\n")
    contents.append(f"constexpr uint16_t HEARTBEAT_REMOTE_PORT = {heartbeat_port};\n")
    contents.append(f"constexpr const char HEARTBEAT_REMOTE_HOST[] = \"{_escape_cpp(heartbeat_host)}\";\n\n")
    contents.append(_format_array("OSC_AES_KEY", osc_key))
    contents.append("\n")
    contents.append(_format_array("OSC_AES_IV", osc_iv))
    contents.append("\n\n")
    contents.append(f"constexpr const char NTP_SERVER[] = \"{_escape_cpp(ntp_server)}\";\n")
    contents.append(f"constexpr long NTP_TIME_OFFSET_SEC = {ntp_offset};\n")
    contents.append(f"constexpr unsigned long NTP_UPDATE_INTERVAL_MS = {ntp_interval};\n\n")
    contents.append("}  // namespace secrets\n")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("".join(contents), encoding="utf-8")
    print(f"[secrets] Generated {output_path}")
    return output_path


def _auto_paths(config_override: str | None, output_override: str | None) -> tuple[Path, Path]:
    config = Path(config_override or os.environ.get(ENV_VAR, str(DEFAULT_CONFIG)))
    output = Path(output_override or DEFAULT_OUTPUT)
    return config, output


def _register_platformio_hook() -> None:
    try:
        env = Import("env")  # type: ignore[name-defined]
    except Exception:  # noqa: BLE001 - Import raises non-ImportError sometimes inside PIO
        return
    if env is None:
        return

    def _action(*_, **__):
        config_path, output_path = _auto_paths(None, None)
        try:
            generate(config_path, output_path)
        except ConfigError as exc:  # pragma: no cover - PlatformIO surface
            print(f"[secrets] {exc}", file=sys.stderr)
            env.Exit(1)

    env.AddPreAction("buildprog", _action)


_register_platformio_hook()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=str, help=f"Path to osc_config.json (default: {DEFAULT_CONFIG})")
    parser.add_argument("--output", type=str, help=f"Path to Secrets.h (default: {DEFAULT_OUTPUT})")
    args = parser.parse_args(argv)
    config_path, output_path = _auto_paths(args.config, args.output)
    try:
        generate(config_path, output_path)
    except ConfigError as exc:
        print(f"[secrets] {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
