import argparse
import json
import sys


ADC_MAX = 4095
WAVE_TOP = 24
WAVE_BOTTOM = 207


def raw_to_mv(raw):
    raw = max(0, min(ADC_MAX, raw))
    return (raw * 3300) // ADC_MAX


def raw_to_y(raw):
    raw = max(0, min(ADC_MAX, raw))
    height = WAVE_BOTTOM - WAVE_TOP
    return WAVE_BOTTOM - ((raw * height) // ADC_MAX)


def auto_points(samples):
    minimum = min(samples)
    maximum = max(samples)
    span = max(maximum - minimum, 80)
    center = round((minimum + maximum) / 2)
    low = center - span // 2
    high = center + span // 2
    margin = span // 8
    low -= margin
    high += margin
    if high <= low:
        high = low + 1
    height = WAVE_BOTTOM - WAVE_TOP
    result = []
    for sample in samples:
        raw = max(low, min(high, sample))
        result.append(WAVE_BOTTOM - (((raw - low) * height) // (high - low)))
    return result


def find_rising_trigger(samples):
    minimum = min(samples)
    maximum = max(samples)
    if maximum - minimum < 32:
        return 0
    threshold = minimum + ((maximum - minimum) // 2)
    previous_high = samples[0] >= threshold
    for index, sample in enumerate(samples[1:], start=1):
        high = sample >= threshold
        if high and not previous_high:
            return index
        previous_high = high
    return 0


def measure_pwm(samples, sample_rate_hz, threshold):
    first_rise = None
    second_rise = None
    previous_high = samples[0] >= threshold

    for index, sample in enumerate(samples[1:], start=1):
        high = sample >= threshold
        if high and not previous_high:
            if first_rise is None:
                first_rise = index
            else:
                second_rise = index
                break
        previous_high = high

    if first_rise is None or second_rise is None or second_rise <= first_rise:
        return 0, 0

    period = second_rise - first_rise
    high_count = sum(1 for sample in samples[first_rise:second_rise] if sample >= threshold)
    return round(sample_rate_hz / period), round(high_count * 1000 / period)


def smoothed_raw(samples, point_count=None):
    if point_count is None:
        point_count = len(samples)
    window = 40
    result = []
    for index in range(point_count):
        start = (index * len(samples)) // point_count
        end = ((index + 1) * len(samples)) // point_count
        if end <= start:
            end = start + 1
        if end - start < window:
            if len(samples) <= window:
                start = 0
                end = len(samples)
            elif start + window <= len(samples):
                end = start + window
            else:
                start = len(samples) - window
                end = len(samples)
        window_samples = samples[start:end]
        if len(window_samples) >= 5:
            total = sum(window_samples) - min(window_samples) - max(window_samples)
            used = len(window_samples) - 2
        else:
            total = sum(window_samples)
            used = len(window_samples)
        result.append(round(total / max(1, used)))
    return result


def trimmed_mean(samples):
    low_limit = ADC_MAX // 20
    high_limit = ADC_MAX - low_limit
    low_values = [sample for sample in samples if sample <= low_limit]
    high_values = [sample for sample in samples if sample >= high_limit]
    if len(samples) > 16 and low_values and high_values and len(low_values) + len(high_values) < len(samples) // 4:
        trimmed = [sample for sample in samples if low_limit < sample < high_limit]
    else:
        trimmed = samples
    return round(sum(trimmed) / len(trimmed))


def scroll_history(samples, value=99, advance=4):
    if not samples or advance <= 0:
        return samples
    advance = min(advance, len(samples))
    previous = samples[len(samples) - advance - 1]
    inserted = []
    for index in range(advance):
        numerator = previous * (advance - index - 1) + value * (index + 1)
        inserted.append(round(numerator / advance))
    return samples[advance:] + inserted


def analyze(samples, sample_rate_hz, mode):
    samples = [max(0, min(ADC_MAX, int(sample))) for sample in samples]
    trigger_index = find_rising_trigger(samples)
    triggered_samples = samples[trigger_index:]
    minimum = min(samples)
    maximum = max(samples)
    average = round(sum(samples) / len(samples))
    frequency_hz, duty_permille = measure_pwm(samples, sample_rate_hz, average)
    smoothed = smoothed_raw(samples) if mode == "smooth" else []
    frame_raw = trimmed_mean(samples)
    history = scroll_history(samples) if mode == "history" else []
    if mode == "history":
        points = auto_points(history)
    elif mode == "frame":
        points = auto_points([frame_raw] * len(samples))
    elif mode == "smooth":
        points = auto_points(smoothed)
    elif mode == "auto":
        points = auto_points(samples)
    else:
        points = [raw_to_y(sample) for sample in samples]

    return {
        "min_mv": raw_to_mv(minimum),
        "max_mv": raw_to_mv(maximum),
        "avg_mv": raw_to_mv(average),
        "vpp_mv": raw_to_mv(maximum) - raw_to_mv(minimum),
        "frequency_hz": frequency_hz,
        "duty_permille": duty_permille,
        "points": points,
        "smoothed_raw": smoothed,
        "frame_raw": frame_raw,
        "history": history,
        "trigger_index": trigger_index,
        "triggered_samples": triggered_samples,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sample-rate", type=int, required=True)
    parser.add_argument("--mode", choices=["raw", "auto", "trigger", "smooth", "frame", "history"], default="raw")
    args = parser.parse_args()

    text = sys.stdin.read().strip()
    if not text:
        raise SystemExit("no samples on stdin")

    samples = [int(part) for part in text.split()]
    print(json.dumps(analyze(samples, args.sample_rate, args.mode)))


if __name__ == "__main__":
    main()
