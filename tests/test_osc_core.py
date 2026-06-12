import json
import math
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "tools" / "osc_core_probe.py"


def run_probe(samples, sample_rate_hz=10000):
    completed = subprocess.run(
        [sys.executable, str(PROBE), "--sample-rate", str(sample_rate_hz)],
        input=" ".join(str(v) for v in samples),
        text=True,
        capture_output=True,
        check=True,
    )
    return json.loads(completed.stdout)


def run_probe_mode(samples, mode, sample_rate_hz=10000):
    completed = subprocess.run(
        [sys.executable, str(PROBE), "--sample-rate", str(sample_rate_hz), "--mode", mode],
        input=" ".join(str(v) for v in samples),
        text=True,
        capture_output=True,
        check=True,
    )
    return json.loads(completed.stdout)


def test_pwm_metrics_for_25_percent_duty_signal():
    samples = []
    for _ in range(10):
        samples.extend([3000] * 25)
        samples.extend([200] * 75)

    metrics = run_probe(samples, sample_rate_hz=10000)

    assert metrics["min_mv"] == 161
    assert metrics["max_mv"] == 2417
    assert metrics["vpp_mv"] == 2256
    assert math.isclose(metrics["frequency_hz"], 100.0, abs_tol=0.5)
    assert math.isclose(metrics["duty_permille"], 250, abs_tol=5)


def test_display_points_are_clamped_to_waveform_area():
    samples = [0, 4095, 2048, 1024, 3072]

    metrics = run_probe(samples, sample_rate_hz=1000)

    assert metrics["points"][0] == 207
    assert metrics["points"][1] == 24
    assert all(24 <= y <= 207 for y in metrics["points"])


def test_auto_scaled_points_center_small_signal():
    samples = [160, 180, 220, 360, 420, 380, 260, 200]

    metrics = run_probe_mode(samples, "auto")

    assert min(metrics["points"]) < 80
    assert max(metrics["points"]) > 150
    assert all(24 <= y <= 207 for y in metrics["points"])


def test_trigger_align_starts_near_rising_edge():
    samples = [100] * 20 + [800] * 20 + [100] * 20 + [800] * 20

    metrics = run_probe_mode(samples, "trigger")

    assert metrics["trigger_index"] == 20
    assert metrics["triggered_samples"][0] == 800


def test_smoothed_points_reduce_pwm_pulse_jitter():
    samples = []
    for _ in range(40):
        samples.extend([3200] * 5)
        samples.extend([120] * 15)

    metrics = run_probe_mode(samples, "smooth")

    assert max(metrics["smoothed_raw"]) - min(metrics["smoothed_raw"]) <= 350
    assert max(metrics["points"]) - min(metrics["points"]) <= 2


def test_smoothed_points_reject_single_sample_spikes():
    samples = [1000] * 320
    samples[50] = 3900
    samples[160] = 0
    samples[250] = 3600

    metrics = run_probe_mode(samples, "smooth")

    assert max(metrics["smoothed_raw"]) - min(metrics["smoothed_raw"]) <= 120
    assert max(metrics["points"]) - min(metrics["points"]) <= 4


def test_trimmed_mean_ignores_pwm_edges_and_spikes():
    samples = []
    for _ in range(80):
        samples.extend([3200] * 1)
        samples.extend([120] * 3)
    samples[13] = 4095
    samples[217] = 0

    metrics = run_probe_mode(samples, "frame")

    assert 850 <= metrics["frame_raw"] <= 930


def test_history_scroll_can_advance_multiple_pixels():
    samples = list(range(10))

    metrics = run_probe_mode(samples, "history")

    assert metrics["history"] == [4, 5, 6, 7, 8, 9, 28, 52, 76, 99]


if __name__ == "__main__":
    test_pwm_metrics_for_25_percent_duty_signal()
    test_display_points_are_clamped_to_waveform_area()
    test_auto_scaled_points_center_small_signal()
    test_trigger_align_starts_near_rising_edge()
    test_smoothed_points_reduce_pwm_pulse_jitter()
    test_smoothed_points_reject_single_sample_spikes()
    test_trimmed_mean_ignores_pwm_edges_and_spikes()
    test_history_scroll_can_advance_multiple_pixels()
