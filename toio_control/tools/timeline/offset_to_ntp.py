#!/usr/bin/env python3
"""
Convert a scheduler offset timeline (version 1.2) into an absolute NTP timeline JSON.

This is intended to run immediately before uploading a timeline via the GUI:

    python offset_to_ntp.py acoustics/pc_tools/scheduler/examples/sample_loop_x3.json \
        --start-offset-seconds 8 \
        --lead-time-ms 3000

The output (default: *.rest.json next to the source file) can be uploaded directly
to the GUI's NTP Timeline panel.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import math
import pathlib
import sys
from typing import Any, Dict, List


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "timeline",
        type=pathlib.Path,
        help="Offset-based scheduler JSON (version 1.2)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default=None,
        help="Path for the absolute timeline JSON (default: <timeline>.rest.json). Use '-' for stdout.",
    )
    parser.add_argument(
        "--timeline-id",
        type=str,
        help="Override timeline_id in the output (default: derived from file name).",
    )
    parser.add_argument(
        "--base-time",
        type=str,
        help="ISO-8601 UTC timestamp that represents offset=0. "
        "If omitted, now()+start-offset is used.",
    )
    parser.add_argument(
        "--start-offset-seconds",
        type=float,
        default=5.0,
        help="Seconds to add to current UTC time when base-time is omitted (default: 5).",
    )
    parser.add_argument(
        "--lead-time-ms",
        type=int,
        help="Explicit lead_time_ms for every event. Overrides other lead-time flags.",
    )
    parser.add_argument(
        "--lead-time-seconds",
        type=float,
        help="Lead time in seconds (converted to ms) if --lead-time-ms is not provided.",
    )
    return parser.parse_args()


def load_json(path: pathlib.Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def parse_iso8601(value: str) -> dt.datetime:
    try:
        normalized = value.replace("Z", "+00:00")
        parsed = dt.datetime.fromisoformat(normalized)
        if parsed.tzinfo is None:
            raise ValueError("Timestamp must include timezone information")
        return parsed.astimezone(dt.timezone.utc)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"Invalid ISO-8601 timestamp: {value}") from exc


def isoformat_utc(value: dt.datetime) -> str:
    return value.astimezone(dt.timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def resolve_timeline_id(path: pathlib.Path, override: str | None) -> str:
    if override:
        return override
    stem = path.stem
    return stem.replace("_", "-")


def resolve_lead_time_ms(
    raw: Dict[str, Any],
    lead_time_ms: int | None,
    lead_time_seconds: float | None,
) -> int:
    if lead_time_ms is not None:
        return lead_time_ms
    if lead_time_seconds is not None:
        return int(round(lead_time_seconds * 1000))
    default_lead = raw.get("default_lead_time")
    if isinstance(default_lead, (int, float)):
        return int(round(float(default_lead) * 1000))
    # Fallback to 4 seconds if the timeline omits lead_time
    return 4000


def determine_base_time(args: argparse.Namespace) -> dt.datetime:
    if args.base_time:
        return parse_iso8601(args.base_time)
    now = dt.datetime.now(dt.timezone.utc)
    return now + dt.timedelta(seconds=args.start_offset_seconds)


def extract_events(
    raw_events: List[Dict[str, Any]],
    base_time: dt.datetime,
    lead_time_ms: int,
) -> List[Dict[str, Any]]:
    events = []
    for index, event in enumerate(raw_events):
        offset = event.get("offset")
        targets = event.get("targets")
        args = event.get("args", [])
        if not isinstance(offset, (int, float)):
            raise ValueError(f"Event #{index} is missing a numeric 'offset'")
        if not isinstance(targets, list) or not all(isinstance(t, str) for t in targets):
            raise ValueError(f"Event #{index} must contain a list of string targets")
        if not args or not isinstance(args[0], str):
            raise ValueError(f"Event #{index} args must start with preset string")

        preset = args[0]
        gain = None
        if len(args) > 2 and isinstance(args[2], (int, float)):
            gain = float(args[2])
            if math.isclose(gain, 1.0, rel_tol=1e-6, abs_tol=1e-6):
                gain = None

        scheduled_time = base_time + dt.timedelta(seconds=float(offset))
        result: Dict[str, Any] = {
            "time_utc": isoformat_utc(scheduled_time),
            "preset": preset,
            "targets": targets,
            "lead_time_ms": lead_time_ms,
        }
        if gain is not None:
            result["gain"] = gain
        events.append(result)
    return events


def main() -> int:
    args = parse_args()
    timeline_path: pathlib.Path = args.timeline
    if not timeline_path.exists():
        print(f"[error] Timeline file not found: {timeline_path}", file=sys.stderr)
        return 1

    raw = load_json(timeline_path)
    raw_events = raw.get("events")
    if not isinstance(raw_events, list) or not raw_events:
        print("[error] Timeline must contain at least one event", file=sys.stderr)
        return 1

    timeline_id = resolve_timeline_id(timeline_path, args.timeline_id)
    lead_time_ms = resolve_lead_time_ms(raw, args.lead_time_ms, args.lead_time_seconds)
    base_time = determine_base_time(args)
    events = extract_events(raw_events, base_time, lead_time_ms)

    payload = {"timeline_id": timeline_id, "events": events}

    output_path = args.output
    if output_path == "-" or (output_path is None and timeline_path == pathlib.Path("-")):
        json.dump(payload, sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 0

    if output_path:
        final_path = pathlib.Path(output_path)
    else:
        if timeline_path.suffix:
            final_path = timeline_path.with_suffix(".rest.json")
        else:
            final_path = timeline_path.with_name(timeline_path.name + ".rest.json")
    final_path.parent.mkdir(parents=True, exist_ok=True)
    with final_path.open("w", encoding="utf-8") as fh:
        json.dump(payload, fh, indent=2)
        fh.write("\n")

    print(
        f"[ok] Wrote {len(events)} events to {final_path} "
        f"(timeline_id={timeline_id}, base={isoformat_utc(base_time)}, lead={lead_time_ms}ms)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
