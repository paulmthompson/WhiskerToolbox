"""Unit tests for spike2_sonpy helper (no SonPy required)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "resources" / "python"))

from whiskertoolbox_io import spike2_sonpy  # noqa: E402


class Spike2SonPyProcessingTests(unittest.TestCase):
    def test_apply_analog_processing_invert_and_subtract_mean(self) -> None:
        values = np.array([1.0, 2.0, 3.0], dtype=np.float32)
        processed = spike2_sonpy._apply_analog_processing(
            values,
            {"invert": True, "subtract_mean": True},
            np,
        )
        self.assertTrue(np.allclose(processed, np.array([1.0, 0.0, -1.0], dtype=np.float32)))

    def test_threshold_intervals_finds_two_regions(self) -> None:
        times = np.arange(10, dtype=np.int64)
        values = np.array([0.1, 0.1, 3.2, 3.2, 3.2, 0.1, 0.1, 3.2, 3.2, 0.1], dtype=np.float32)
        starts, ends = spike2_sonpy._threshold_intervals(np, times, values, 1.0)
        self.assertEqual(starts.tolist(), [2, 7])
        self.assertEqual(ends.tolist(), [4, 8])

    def test_normalize_config_reads_channel_and_threshold(self) -> None:
        norm = spike2_sonpy._normalize_config(
            {
                "data_type": "digital_event",
                "channel": 0,
                "processing": {"invert": True, "subtract_mean": True},
                "threshold": 2.0,
            }
        )
        self.assertEqual(norm["channel"], 0)
        self.assertEqual(norm["threshold"], 2.0)
        self.assertTrue(norm["processing"]["invert"])
        self.assertTrue(norm["read_adc"])

    def test_normalize_config_requires_channel(self) -> None:
        with self.assertRaises(ValueError):
            spike2_sonpy._normalize_config({"data_type": "analog"})

    def test_load_spike2_fake_payload_with_threshold_keys(self) -> None:
        payload = spike2_sonpy.load_spike2(
            "fake.smrx",
            {
                "data_type": "digital_event",
                "channel": 0,
                "fake_payload": {
                    "analog": [
                        {
                            "name": "Camera_raw",
                            "title": "Camera",
                            "channel": 0,
                            "times": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
                            "values": [0.1, 0.1, 3.2, 3.2, 3.2, 0.1, 0.1, 3.2, 3.2, 0.1],
                        }
                    ]
                },
                "threshold": 1.0,
            },
        )
        self.assertEqual(len(payload["events"]), 1)
        self.assertEqual(payload["events"][0]["times"], [2, 7])
        self.assertEqual(payload["analog"], [])


if __name__ == "__main__":
    unittest.main()
