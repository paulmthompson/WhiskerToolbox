"""Spike2/SonPy extraction helper for WhiskerToolbox."""

from __future__ import annotations

from typing import Any

# Matches the SonPy ReadFloats max-count used for full-channel waveform extraction.
DEFAULT_CHUNK_SAMPLES = 50_000_000

_DATA_TYPE_ANALOG = "analog"
_DATA_TYPE_DIGITAL_EVENT = "digital_event"
_DATA_TYPE_DIGITAL_INTERVAL = "digital_interval"

_DEFAULT_PROCESSING: dict[str, bool] = {
    "invert": False,
    "subtract_mean": False,
}


def load_spike2(path: str, config: dict[str, Any]) -> dict[str, Any]:
    """Load a Spike2 file and return plain arrays plus metadata.

    Parameters
    ----------
    path:
        Path to the ``.smr`` or ``.smrx`` file.
    config:
        Loader configuration. If ``fake_payload`` is present, it is returned
        after optional threshold processing. Each entry requires ``channel``,
        ``data_type`` (``analog``, ``digital_event``, ``digital_interval``),
        and optional ``processing`` (``invert``, ``subtract_mean``) and
        ``threshold`` for ADC-derived digital outputs.
    """
    norm = _normalize_config(config)

    if config.get("fake_payload") is not None:
        payload = _normalize_payload(config["fake_payload"])
        payload = _process_fake_payload(payload, norm)
    else:
        payload = _build_payload_from_file(path, norm)

    return _filter_payload_by_data_type(payload, norm)


def _normalize_config(config: dict[str, Any]) -> dict[str, Any]:
    """Normalize per-entry Spike2 extraction config."""
    data_type = str(config.get("data_type", _DATA_TYPE_ANALOG))

    if "channel" not in config:
        raise ValueError("Spike2 config requires 'channel'.")

    channel = int(config["channel"])
    processing = {**_DEFAULT_PROCESSING, **config.get("processing", {})}
    threshold = config.get("threshold")
    has_threshold = threshold is not None

    return {
        "data_type": data_type,
        "channel": channel,
        "processing": processing,
        "threshold": float(threshold) if threshold is not None else None,
        "read_adc": data_type == _DATA_TYPE_ANALOG or has_threshold,
        "read_native_events": data_type == _DATA_TYPE_DIGITAL_EVENT and not has_threshold,
        "read_native_intervals": data_type == _DATA_TYPE_DIGITAL_INTERVAL and not has_threshold,
        "chunk_samples": int(config.get("chunk_samples", DEFAULT_CHUNK_SAMPLES)),
        "max_events": int(config.get("max_events", 50_000_000)),
    }


def _build_payload_from_file(path: str, norm: dict[str, Any]) -> dict[str, Any]:
    sonpy = _import_sonpy()
    np = _import_numpy()

    son_file = sonpy.SonFile(path, True)
    open_error = int(son_file.GetOpenError())
    if open_error != int(sonpy.Son_OK):
        raise RuntimeError(f"Failed to open Spike2 file {path!r}: {sonpy.GetErrorString(open_error)}")

    return _read_single_channel_payload(sonpy, np, son_file, norm["channel"], norm)


def _read_single_channel_payload(sonpy, np, son_file, channel: int, norm: dict[str, Any]) -> dict[str, Any]:
    channel_type = son_file.ChannelType(channel)
    if channel_type == sonpy.Off:
        raise RuntimeError(f"Spike2 channel {channel} is not active in file {son_file.GetName()!r}")

    info = _channel_info(sonpy, son_file, channel, channel_type)
    payload: dict[str, Any] = {
        "analog": [],
        "events": [],
        "intervals": [],
        "metadata": _file_metadata(son_file),
    }

    data_type = norm["data_type"]
    threshold = norm["threshold"]

    if norm["read_adc"]:
        if channel_type not in (sonpy.Adc, sonpy.RealWave):
            raise RuntimeError(
                f"Channel {channel} ({info['title']!r}) is not an ADC waveform channel; "
                "cannot produce analog or threshold-derived digital output."
            )
        wave = _read_wave_channel(np, son_file, info, norm)
        values = _apply_analog_processing(np.asarray(wave["values"], dtype=np.float32), norm["processing"], np)
        wave["values"] = values
        if data_type == _DATA_TYPE_ANALOG:
            payload["analog"].append(wave)
        else:
            starts, ends = _threshold_intervals(np, np.asarray(wave["times"], dtype=np.int64), values, threshold)
            _append_threshold_results(payload, wave, starts, ends, data_type)
        return payload

    if norm["read_native_events"]:
        if channel_type not in (sonpy.EventFall, sonpy.EventRise):
            raise RuntimeError(f"Channel {channel} ({info['title']!r}) is not a native event channel.")
        payload["events"].append(_read_event_channel(son_file, info, norm))
        return payload

    if norm["read_native_intervals"]:
        if channel_type != sonpy.EventBoth:
            raise RuntimeError(f"Channel {channel} ({info['title']!r}) is not a native level channel.")
        payload["intervals"].append(_read_level_channel(son_file, info, norm))
        return payload

    raise RuntimeError(f"Unsupported data_type {data_type!r} for Spike2 channel import.")


def _process_fake_payload(payload: dict[str, Any], norm: dict[str, Any]) -> dict[str, Any]:
    """Apply threshold extraction to fake payloads for C++ conversion tests."""
    if norm["threshold"] is not None and payload["analog"]:
        np = _import_numpy()
        item = payload["analog"][0]
        values = _apply_analog_processing(
            np.asarray(item["values"], dtype=np.float32),
            norm["processing"],
            np,
        )
        starts, ends = _threshold_intervals(
            np,
            np.asarray(item["times"], dtype=np.int64),
            values,
            norm["threshold"],
        )
        _append_threshold_results(payload, item, starts, ends, norm["data_type"])

    return payload


def _append_threshold_results(
    payload: dict[str, Any],
    item: dict[str, Any],
    starts,
    ends,
    data_type: str,
) -> None:
    title = item["title"]
    safe_title = _safe_name(title)
    starts_list = starts.tolist() if hasattr(starts, "tolist") else list(starts)
    ends_list = ends.tolist() if hasattr(ends, "tolist") else list(ends)
    if data_type == _DATA_TYPE_DIGITAL_INTERVAL:
        payload["intervals"].append(
            {
                "name": f"{safe_title}_interval",
                "channel": item["channel"],
                "source_title": title,
                "starts": starts_list,
                "ends": ends_list,
            }
        )
    elif data_type == _DATA_TYPE_DIGITAL_EVENT:
        payload["events"].append(
            {
                "name": f"{safe_title}_events",
                "channel": item["channel"],
                "source_title": title,
                "times": starts_list,
            }
        )


def _filter_payload_by_data_type(payload: dict[str, Any], norm: dict[str, Any]) -> dict[str, Any]:
    data_type = norm["data_type"]
    filtered = {
        "analog": [],
        "events": [],
        "intervals": [],
        "metadata": dict(payload.get("metadata", {})),
    }
    if data_type == _DATA_TYPE_ANALOG:
        filtered["analog"] = list(payload.get("analog", []))
    elif data_type == _DATA_TYPE_DIGITAL_EVENT:
        filtered["events"] = list(payload.get("events", []))
    elif data_type == _DATA_TYPE_DIGITAL_INTERVAL:
        filtered["intervals"] = list(payload.get("intervals", []))
    else:
        return _normalize_payload(payload)
    return filtered


def _apply_analog_processing(values, processing: dict[str, bool], np):
    """Apply invert and subtract-mean preprocessing to waveform samples."""
    result = np.asarray(values, dtype=np.float32).copy()
    if processing.get("subtract_mean", False) and result.size > 0:
        result = result - np.mean(result)
    if processing.get("invert", False):
        result = -result
    return result


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


def _read_wave_channel(np, son_file, info: dict[str, Any], norm: dict[str, Any]) -> dict[str, Any]:
    """Read an ADC or RealWave channel using SonPy ``ReadFloats``."""
    channel = info["channel"]
    divide = int(info["divide_ticks"])
    max_time = int(info["max_time_ticks"])
    first_time = info.get("first_time_ticks")
    if first_time is None or max_time < first_time:
        values = np.array([], dtype=np.float32)
        times = np.array([], dtype=np.int64)
    else:
        first_time = int(first_time)
        stop = max_time + divide
        max_read = int(norm.get("chunk_samples", DEFAULT_CHUNK_SAMPLES))
        values_parts: list = []
        times_parts: list = []
        cursor = first_time
        while cursor <= max_time:
            chunk = son_file.ReadFloats(channel, max_read, cursor, stop)
            if len(chunk) == 0:
                break
            chunk = chunk.astype(np.float32, copy=False)
            values_parts.append(chunk)
            times_parts.append(cursor + np.arange(len(chunk), dtype=np.int64) * divide)
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


def _read_event_channel(son_file, info: dict[str, Any], norm: dict[str, Any]) -> dict[str, Any]:
    channel = info["channel"]
    max_events = int(norm.get("max_events", 50_000_000))
    max_time = int(info["max_time_ticks"])
    times = son_file.ReadEvents(channel, max_events, 0, max_time + 1 if max_time >= 0 else max_time)
    return {
        "name": _safe_name(info["title"]),
        **info,
        "times": [int(value) for value in times],
    }


def _read_level_channel(son_file, info: dict[str, Any], norm: dict[str, Any]) -> dict[str, Any]:
    channel = info["channel"]
    max_events = int(norm.get("max_events", 50_000_000))
    max_time = int(info["max_time_ticks"])
    transitions = [
        int(value) for value in son_file.ReadEvents(channel, max_events, 0, max_time + 1 if max_time >= 0 else max_time)
    ]
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
