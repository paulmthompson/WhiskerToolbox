"""Spike2/SonPy extraction helper for WhiskerToolbox."""

from __future__ import annotations

from typing import Any


DEFAULT_CHUNK_SAMPLES = 1_000_000


def load_spike2(path: str, config: dict[str, Any]) -> dict[str, Any]:
    """Load a Spike2 file and return plain arrays plus metadata.

    Parameters
    ----------
    path:
        Path to the `.smr` or `.smrx` file.
    config:
        Loader configuration. If ``fake_payload`` is present, it is returned
        directly. This supports C++ conversion tests before SonPy extraction is
        implemented.
    """

    fake_payload = config.get("fake_payload")
    if fake_payload is not None:
        return _normalize_payload(fake_payload)

    sonpy = _import_sonpy()
    np = _import_numpy()

    son_file = sonpy.SonFile(path, True)
    open_error = int(son_file.GetOpenError())
    if open_error != int(sonpy.Son_OK):
        raise RuntimeError(f"Failed to open Spike2 file {path!r}: {sonpy.GetErrorString(open_error)}")

    payload = {"analog": [], "events": [], "intervals": [], "metadata": _file_metadata(son_file)}
    max_channels = int(son_file.MaxChannels())

    requested_titles = set(config.get("channel_titles", []))
    include_raw_analog = bool(config.get("include_raw_analog", True))
    include_event_channels = bool(config.get("include_event_channels", True))
    include_level_channels = bool(config.get("include_level_channels", True))

    for channel in range(1, max_channels + 1):
        channel_type = son_file.ChannelType(channel)
        if channel_type == sonpy.Off:
            continue

        info = _channel_info(sonpy, son_file, channel, channel_type)
        if requested_titles and info["title"] not in requested_titles:
            continue

        if include_raw_analog and channel_type in (sonpy.Adc, sonpy.RealWave):
            payload["analog"].append(_read_wave_channel(np, son_file, info, config))
        elif include_event_channels and channel_type in (sonpy.EventFall, sonpy.EventRise):
            payload["events"].append(_read_event_channel(son_file, info, config))
        elif include_level_channels and channel_type == sonpy.EventBoth:
            payload["intervals"].append(_read_level_channel(son_file, info, config))

    if config.get("preset") == "colleague_task_events":
        _apply_colleague_task_events(np, payload, config)

    return payload


def _import_sonpy():
    try:
        import sonpy
    except ModuleNotFoundError as exc:
        raise ModuleNotFoundError(
            "Spike2 import requires the optional Python package 'sonpy'. "
            "Install it with 'python -m pip install sonpy numpy'."
        ) from exc
    return sonpy


def _import_numpy():
    try:
        import numpy as np
    except ModuleNotFoundError as exc:
        raise ModuleNotFoundError(
            "Spike2 import through SonPy requires NumPy because SonPy waveform reads return NumPy arrays. "
            "Install it with 'python -m pip install numpy'."
        ) from exc
    return np


def _file_metadata(son_file) -> dict[str, Any]:
    return {
        "name": son_file.GetName(),
        "version": int(son_file.GetVersion()),
        "time_base_seconds": float(son_file.GetTimeBase()),
        "max_time_ticks": int(son_file.MaxTime()),
        "max_channels": int(son_file.MaxChannels()),
        "is_32_file": bool(son_file.is32file()),
        "is_64_file": bool(son_file.is64file()),
    }


def _channel_info(sonpy, son_file, channel: int, channel_type) -> dict[str, Any]:
    max_time = int(son_file.ChannelMaxTime(channel))
    first_time = None
    if max_time >= 0 and channel_type in (sonpy.Adc, sonpy.RealWave):
        first_time = int(son_file.FirstTime(channel, 0, max_time + 1))

    return {
        "channel": int(channel),
        "channel_type": int(channel_type),
        "channel_type_name": str(channel_type).split(".")[-1],
        "title": str(son_file.GetChannelTitle(channel)).strip(),
        "units": str(son_file.GetChannelUnits(channel)).strip(),
        "rate_hz": float(son_file.GetIdealRate(channel)),
        "divide_ticks": int(son_file.ChannelDivide(channel)),
        "max_time_ticks": max_time,
        "first_time_ticks": first_time,
    }


def _read_wave_channel(np, son_file, info: dict[str, Any], config: dict[str, Any]) -> dict[str, Any]:
    channel = info["channel"]
    divide = int(info["divide_ticks"])
    max_time = int(info["max_time_ticks"])
    first_time = info.get("first_time_ticks")
    if first_time is None or max_time < first_time:
        values = np.array([], dtype=np.float32)
        times = np.array([], dtype=np.int64)
    else:
        chunk_samples = int(config.get("chunk_samples", DEFAULT_CHUNK_SAMPLES))
        values_parts = []
        times_parts = []
        cursor = int(first_time)
        stop = max_time + divide
        while cursor <= max_time:
            values = son_file.ReadFloats(channel, chunk_samples, cursor, stop)
            if len(values) == 0:
                break
            values = values.astype(np.float32, copy=False)
            values_parts.append(values)
            times_parts.append(cursor + np.arange(len(values), dtype=np.int64) * divide)
            cursor = int(times_parts[-1][-1]) + divide

        if values_parts:
            values = np.concatenate(values_parts).astype(np.float32, copy=False)
            times = np.concatenate(times_parts).astype(np.int64, copy=False)
        else:
            values = np.array([], dtype=np.float32)
            times = np.array([], dtype=np.int64)

    return {
        "name": f"{_safe_name(info['title'])}_raw",
        **info,
        "times": times,
        "values": values,
    }


def _read_event_channel(son_file, info: dict[str, Any], config: dict[str, Any]) -> dict[str, Any]:
    channel = info["channel"]
    max_events = int(config.get("max_events", 50_000_000))
    max_time = int(info["max_time_ticks"])
    times = son_file.ReadEvents(channel, max_events, 0, max_time + 1 if max_time >= 0 else max_time)
    return {
        "name": _safe_name(info["title"]),
        **info,
        "times": [int(t) for t in times],
    }


def _read_level_channel(son_file, info: dict[str, Any], config: dict[str, Any]) -> dict[str, Any]:
    # SonPy exposes EventBoth data as events/markers. Phase 4 returns transition
    # intervals by pairing adjacent transition times. If initial level semantics
    # are needed, this can be refined with marker-code aware reading.
    channel = info["channel"]
    max_events = int(config.get("max_events", 50_000_000))
    max_time = int(info["max_time_ticks"])
    transitions = [int(t) for t in son_file.ReadEvents(channel, max_events, 0, max_time + 1 if max_time >= 0 else max_time)]
    starts = transitions[0::2]
    ends = transitions[1::2]
    if len(starts) > len(ends):
        ends.append(max_time)
    return {
        "name": _safe_name(info["title"]),
        **info,
        "starts": starts,
        "ends": ends,
    }


def _apply_colleague_task_events(np, payload: dict[str, Any], config: dict[str, Any]) -> None:
    analog_by_title = {item["title"]: item for item in payload["analog"]}
    defaults = {
        "Sound": {"threshold": 1.0, "invert": False, "subtract_mean": False, "event_name": "Trial_start", "interval_name": "Sound_interval"},
        "Camera": {"threshold": 2.0, "invert": True, "subtract_mean": True, "event_name": "Frame_start", "interval_name": "Camera_interval"},
        "imaging": {"threshold": 2.0, "invert": False, "subtract_mean": False, "event_name": "ImgFrame_start", "interval_name": "Imaging_interval"},
        "Laser": {"threshold": 4.0, "invert": False, "subtract_mean": False, "event_name": None, "interval_name": "US_start_stop"},
    }
    overrides = config.get("threshold_channels", {})
    for title, options in defaults.items():
        if title not in analog_by_title:
            continue
        merged = {**options, **overrides.get(title, {})}
        item = analog_by_title[title]
        values = np.asarray(item["values"], dtype=np.float32).copy()
        if merged["subtract_mean"]:
            values = values - np.mean(values)
        if merged["invert"]:
            values = -values
        starts, ends = _threshold_intervals(np, np.asarray(item["times"], dtype=np.int64), values, float(merged["threshold"]))
        payload["intervals"].append({"name": merged["interval_name"], "channel": item["channel"], "source_title": title, "starts": starts, "ends": ends})
        if merged["event_name"]:
            payload["events"].append({"name": merged["event_name"], "channel": item["channel"], "source_title": title, "times": starts})


def _threshold_intervals(np, times, values, threshold: float):
    above = values > threshold
    if above.size == 0:
        return np.array([], dtype=np.int64), np.array([], dtype=np.int64)
    changes = np.diff(above.astype(np.int8))
    starts = np.flatnonzero(changes == 1) + 1
    ends = np.flatnonzero(changes == -1)
    if above[0]:
        starts = np.r_[0, starts]
    if above[-1]:
        ends = np.r_[ends, above.size - 1]
    return times[starts].astype(np.int64), times[ends].astype(np.int64)


def _safe_name(name: str) -> str:
    cleaned = "_".join(str(name).strip().split())
    return cleaned or "channel"


def _normalize_payload(payload: dict[str, Any]) -> dict[str, Any]:
    """Ensure the payload has the expected top-level lists."""

    return {
        "analog": list(payload.get("analog", [])),
        "events": list(payload.get("events", [])),
        "intervals": list(payload.get("intervals", [])),
        "metadata": dict(payload.get("metadata", {})),
    }
