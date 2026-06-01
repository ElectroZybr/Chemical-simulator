#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
bench.py — запускалка бенчмарков для Chemical Simulator.

Использование:
  ./bench.py                          — интерактивное меню
  ./bench.py --filter SimulationFixture
  ./bench.py --filter Correct --save
  ./bench.py --list                   — показать сохранённые результаты
  ./bench.py --open                   — открыть view.html в браузере
"""

SCENES: list[tuple[str, str]] = [
    ("ideal_crystal3d", "Идеальный кристалл 3D"),
    ("crystal3d", "Кристалл 3D"),
    ("crystal2d", "Кристалл 2D"),
    ("random_gas2d", "Случайный газ 2D"),
]

import argparse
import ctypes
import csv
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
import webbrowser
from datetime import datetime
from pathlib import Path

BINARY_NAME = "benchmarks" if sys.platform != "win32" else "benchmarks.exe"
RESULTS_DIR = Path(__file__).parent / "results"
VIEW_HTML = Path(__file__).parent / "view.html"
BENCHMARKS_ROOT = Path(__file__).parent


def resolve_benchmark_binary() -> Path:
    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "build" / "bench" / BINARY_NAME,
        repo_root / "build" / "release" / BINARY_NAME,
        repo_root / BINARY_NAME,
        Path(__file__).parent / BINARY_NAME,
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return candidates[0]


BINARY = resolve_benchmark_binary()

COLOR_RESET = "\033[0m"
COLOR_TITLE = "\033[1;97m"
COLOR_LABEL_WHITE = "\033[37m"
COLOR_GROUP = "\033[92m"
COLOR_SUBGROUP = "\033[95m"
COLOR_SUBGROUP_BRACKET = "\033[90m"
COLOR_INDEX = "\033[33m"
COLOR_INDEX_LIGHT_BLUE = "\033[96m"
COLOR_HINT = "\033[90m"
COLOR_ERROR = "\033[31m"
COLOR_OK = "\033[32m"
COLOR_WARN = "\033[33m"
COLOR_BAD = "\033[31m"
COLOR_UNIT_NS = "\033[90m"
COLOR_UNIT_US = "\033[36m"
COLOR_UNIT_MS = "\033[35m"
COLOR_PROGRESS_SPINNER = "\033[96m"
COLOR_PROGRESS_BAR = "\033[36m"
COLOR_PROGRESS_COUNT = "\033[33m"
COLOR_PROGRESS_CASE = "\033[95m"
COLOR_PROGRESS_TIME = "\033[92m"

SUPPORTED_PRIORITIES = ("normal", "above_normal", "high")
SUPPORTED_HWC = ("none", "auto", "linux-perf", "windows-pcm", "windows-amd-uprof")
RUN_PROFILES: list[tuple[str, str, int, str]] = [
    ("quick", "Быстрый", 1, "0.1s"),
    ("medium", "Средний", 3, "0.5s"),
    ("accurate", "Точный", 7, "1s"),
]

def paint(text: str, color: str) -> str:
    if not sys.stdout.isatty():
        return text
    return f"{color}{text}{COLOR_RESET}"


def paint_subgroup_label(text: str) -> str:
    if not sys.stdout.isatty():
        return f"[{text}]"
    return (
        f"{COLOR_SUBGROUP_BRACKET}[{COLOR_RESET}"
        f"{COLOR_SUBGROUP}{text}{COLOR_RESET}"
        f"{COLOR_SUBGROUP_BRACKET}]{COLOR_RESET}"
    )


def paint_256(text: str, color_code: int) -> str:
    if not sys.stdout.isatty():
        return text
    return f"\033[38;5;{color_code}m{text}{COLOR_RESET}"


def paint_rgb(text: str, r: int, g: int, b: int) -> str:
    if not sys.stdout.isatty():
        return text
    r = max(0, min(255, r))
    g = max(0, min(255, g))
    b = max(0, min(255, b))
    return f"\033[38;2;{r};{g};{b}m{text}{COLOR_RESET}"


def short_bench_id(full_name: str) -> str:
    parts = full_name.split("/")
    return "/".join(parts[:2]) if len(parts) >= 2 else full_name


def bench_arg(full_name: str) -> str:
    parts = full_name.split("/")
    return parts[2] if len(parts) >= 3 else "-"


def fmt_num(value: float | None, digits: int = 2) -> str:
    if value is None:
        return "-"
    return f"{value:.{digits}f}"

def fmt_cv(value: float | None) -> str:
    if value is None:
        return "-"
    text = f"{value:.2f}"
    if value >= 5.0:
        return f"{text} !"
    return text


def fmt_time_ns(value: float | None) -> str:
    if value is None:
        return "-"
    abs_v = abs(value)
    if abs_v >= 1_000_000.0:
        return f"{value / 1_000_000.0:.2f} ms"
    if abs_v >= 1_000.0:
        return f"{value / 1_000.0:.2f} us"
    return f"{value:.2f} ns"


def fmt_ips(value: float | None) -> str:
    if value is None:
        return "-"
    abs_v = abs(value)
    if abs_v >= 1_000_000_000:
        return f"{value / 1_000_000_000:.2f}G"
    if abs_v >= 1_000_000:
        return f"{value / 1_000_000:.2f}M"
    if abs_v >= 1_000:
        return f"{value / 1_000:.2f}k"
    return f"{value:.2f}"


def fmt_compact_number(value: float | None) -> str:
    if value is None:
        return "-"
    abs_v = abs(value)
    if abs_v >= 1_000_000_000:
        return f"{value / 1_000_000_000:.2f}G"
    if abs_v >= 1_000_000:
        return f"{value / 1_000_000:.2f}M"
    if abs_v >= 1_000:
        return f"{value / 1_000:.2f}k"
    return f"{value:.0f}"


def paint_numeric(cell: str, cv_mode: bool = False) -> str:
    if cell.strip() == "-":
        return cell
    if cv_mode:
        try:
            m = re.search(r"[-+]?\d*\.?\d+", cell.strip())
            if not m:
                return paint(cell, COLOR_INDEX)
            cv = float(m.group(0))
        except ValueError:
            return paint(cell, COLOR_INDEX)
        if cv < 2.0:
            return paint(cell, COLOR_OK)
        if cv < 5.0:
            return paint(cell, COLOR_WARN)
        return paint(cell, COLOR_BAD)
    return paint(cell, COLOR_INDEX)

def paint_delta(cell: str, neutral_threshold: float = 2.5) -> str:
    value = cell.strip()
    if value == "-" or not value:
        return cell
    try:
        delta = float(value.replace("%", ""))
    except ValueError:
        return cell
    if delta <= -neutral_threshold:
        return paint(cell, COLOR_OK)    # быстрее / лучше
    if delta >= neutral_threshold:
        return paint(cell, COLOR_BAD)   # медленнее / хуже
    return paint(cell, COLOR_HINT)      # нейтрально


def paint_n_gradient(cell: str, min_n: int, max_n: int) -> str:
    raw = cell.strip()
    if not raw.isdigit():
        return cell
    n = int(raw)
    if max_n <= min_n:
        return paint_256(cell, 81)
    t = (n - min_n) / float(max_n - min_n)
    # Мягкий монотонный градиент: голубой -> лавандовый.
    r = int(120 + (170 - 120) * t)
    g = int(200 + (165 - 200) * t)
    b = int(255 + (245 - 255) * t)
    return paint_rgb(cell, r, g, b)


def paint_time_cell(cell: str) -> str:
    if not sys.stdout.isatty():
        return cell

    value = cell.rstrip()
    tail_spaces = cell[len(value):]
    if value == "-":
        return cell

    if value.endswith(" ns"):
        return value[:-3] + " " + paint("ns", COLOR_UNIT_NS) + tail_spaces
    if value.endswith(" us"):
        return value[:-3] + " " + paint("us", COLOR_UNIT_US) + tail_spaces
    if value.endswith(" ms"):
        return value[:-3] + " " + paint("ms", COLOR_UNIT_MS) + tail_spaces
    return cell


def paint_complexity_label(label_cell: str) -> str:
    raw = label_cell.strip()
    if raw == "O(1)":
        return paint(label_cell, COLOR_OK)
    if raw == "O(N)":
        return paint(label_cell, "\033[34m")
    if raw == "O(log N)":
        return paint(label_cell, COLOR_INDEX_LIGHT_BLUE)
    if raw == "O(N log N)":
        return paint(label_cell, COLOR_WARN)
    if raw.startswith("O(N^") or raw == "O(N^k)":
        return paint(label_cell, COLOR_BAD)
    return label_cell


def paint_points_count(points_cell: str) -> str:
    raw = points_cell.strip()
    if not raw.isdigit():
        return points_cell
    count = int(raw)
    # 3 точки: минимально допустимо, но ненадежно.
    if count <= 3:
        marked = points_cell.rstrip() + " (!)"
        return paint(marked, COLOR_BAD)
    if count <= 5:
        return paint(points_cell, COLOR_WARN)
    return paint(points_cell, COLOR_OK)


def parse_benchmark_rows(data: dict) -> dict[str, dict[str, float | str]]:
    benchmarks = data.get("benchmarks", [])
    rows: dict[str, dict[str, float | str]] = {}

    for item in benchmarks:
        run_name = item.get("run_name") or item.get("name")
        if not isinstance(run_name, str):
            continue
        row = rows.setdefault(run_name, {})
        run_type = item.get("run_type")

        if run_type == "aggregate":
            agg = item.get("aggregate_name")
            if agg == "median":
                row["real_median"] = float(item.get("real_time", 0.0))
                row["cpu_median"] = float(item.get("cpu_time", 0.0))
                if "items_per_second" in item:
                    row["ips"] = float(item.get("items_per_second", 0.0))
            elif agg == "mean":
                row["real_mean"] = float(item.get("real_time", 0.0))
                if "items_per_second" in item and "ips" not in row:
                    row["ips"] = float(item.get("items_per_second", 0.0))
            elif agg == "cv":
                row["real_cv"] = float(item.get("real_time", 0.0)) * 100.0
        elif run_type == "iteration":
            if "real_median" not in row:
                row["real_median"] = float(item.get("real_time", 0.0))
            if "cpu_median" not in row:
                row["cpu_median"] = float(item.get("cpu_time", 0.0))
            if "ips" not in row and "items_per_second" in item:
                row["ips"] = float(item.get("items_per_second", 0.0))

        # HWC метрики, если присутствуют в JSON.
        if "hwc_cache_misses" in item:
            try:
                row["hwc_cache_misses"] = float(item.get("hwc_cache_misses", 0.0))
            except (TypeError, ValueError):
                pass
        if "hwc_cache_miss_rate_pct" in item:
            try:
                row["hwc_cache_miss_rate_pct"] = float(item.get("hwc_cache_miss_rate_pct", 0.0))
            except (TypeError, ValueError):
                pass

    return rows


def extract_scene_meta(data: dict) -> tuple[str | None, str | None]:
    context = data.get("context")
    if not isinstance(context, dict):
        return None, None
    bench_py = context.get("bench_py")
    if not isinstance(bench_py, dict):
        return None, None
    scene_key = bench_py.get("scene_key")
    scene_name = bench_py.get("scene_name")
    return (
        scene_key if isinstance(scene_key, str) else None,
        scene_name if isinstance(scene_name, str) else None,
    )


def find_baseline_file() -> Path | None:
    baseline = RESULTS_DIR / "baseline.json"
    return baseline if baseline.exists() else None


def load_baseline_rows() -> tuple[dict[str, dict[str, float | str]], Path | None]:
    baseline_file = find_baseline_file()
    if baseline_file is None:
        return {}, None
    try:
        data = json.loads(baseline_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}, None
    return parse_benchmark_rows(data), baseline_file


def compare_status(curr_median: float | None, base_median: float | None) -> tuple[str, str]:
    if curr_median is None or base_median is None or base_median <= 0.0:
        return "-", ""

    delta_pct = (curr_median - base_median) / base_median * 100.0
    delta_text = f"{delta_pct:+.1f}%"
    if delta_pct <= -2.5:
        return delta_text, paint("лучше", COLOR_OK)
    if delta_pct >= 2.5:
        return delta_text, paint("хуже", COLOR_BAD)
    return delta_text, paint("~", COLOR_HINT)

def estimate_complexity_label(alpha: float) -> str:
    if alpha < 0.25:
        return "O(1)"
    if alpha < 0.75:
        return "O(log N)"
    if alpha < 1.35:
        return "O(N)"
    if alpha < 1.75:
        return "O(N log N)"
    if alpha < 2.35:
        return "O(N^2)"
    if alpha < 2.85:
        return "O(N^3)"
    return "O(N^k)"


def linear_fit(xs: list[float], ys: list[float]) -> tuple[float, float, float]:
    """
    Возвращает (slope, intercept, r2) для y = slope*x + intercept.
    """
    n = len(xs)
    if n < 2:
        return 0.0, 0.0, 0.0

    mean_x = sum(xs) / n
    mean_y = sum(ys) / n

    ss_xx = sum((x - mean_x) ** 2 for x in xs)
    if ss_xx <= 0.0:
        return 0.0, mean_y, 0.0

    ss_xy = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
    slope = ss_xy / ss_xx
    intercept = mean_y - slope * mean_x

    ss_tot = sum((y - mean_y) ** 2 for y in ys)
    ss_res = sum((y - (slope * x + intercept)) ** 2 for x, y in zip(xs, ys))
    r2 = 1.0 - (ss_res / ss_tot) if ss_tot > 0.0 else 1.0
    return slope, intercept, r2


def print_complexity_estimate(rows: dict[str, dict[str, float | str]],
                              metadata: dict[str, dict[str, str]]) -> None:
    grouped: dict[str, list[tuple[int, float]]] = {}

    for run_name, metrics in rows.items():
        n_str = bench_arg(run_name)
        if not n_str.isdigit():
            continue
        n = int(n_str)

        time_val = metrics.get("real_median")
        if not isinstance(time_val, float):
            time_val = metrics.get("real_mean")
        if not isinstance(time_val, float):
            continue
        if n <= 0 or time_val <= 0.0:
            continue

        grouped.setdefault(short_bench_id(run_name), []).append((n, time_val))

    if not grouped:
        return

    results: list[tuple[str, str, float, float, int]] = []
    for bench_id, points in grouped.items():
        uniq: dict[int, float] = {}
        for n, t in points:
            uniq[n] = t
        sorted_points = sorted(uniq.items(), key=lambda p: p[0])
        if len(sorted_points) < 3:
            continue

        xs = [math.log(float(n)) for n, _ in sorted_points]
        ys = [math.log(float(t)) for _, t in sorted_points]
        alpha, _, r2 = linear_fit(xs, ys)
        ru_name = metadata.get(bench_id, {}).get("ru", bench_id)
        results.append((bench_id, ru_name, alpha, r2, len(sorted_points)))

    if not results:
        return

    table_rows: list[list[str]] = []
    for bench_id, ru_name, alpha, r2, count in sorted(results, key=lambda r: r[0]):
        table_rows.append([
            ru_name,
            f"{alpha:.2f}",
            estimate_complexity_label(alpha),
            f"{r2:.3f}",
            str(count),
        ])

    headers = ["Тест", "alpha", "Сложность", "R2", "Точек"]
    widths = [len(h) for h in headers]
    for row in table_rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))

    print()
    print(paint("Оценка сложности:", COLOR_TITLE))
    header_line = "  " + " | ".join(headers[i].ljust(widths[i]) for i in range(len(headers)))
    sep_line = "  " + "-+-".join("-" * widths[i] for i in range(len(headers)))
    print(header_line)
    print(sep_line)
    for row in table_rows:
        rendered = [row[i].ljust(widths[i]) for i in range(len(headers))]
        rendered[2] = paint_complexity_label(rendered[2])
        r2_value = float(row[3])
        if r2_value >= 0.95:
            rendered[3] = paint(rendered[3], COLOR_OK)
        elif r2_value >= 0.85:
            rendered[3] = paint(rendered[3], COLOR_WARN)
        else:
            rendered[3] = paint(rendered[3], COLOR_BAD)
        rendered[4] = paint_points_count(rendered[4])
        print("  " + " | ".join(rendered))


def print_results_table(
    data: dict,
    metadata: dict[str, dict[str, str]],
    scene_name: str | None = None,
    hwc_rows: dict[str, dict[str, float]] | None = None,
    hwc_backend: str = "none",
) -> None:
    rows = parse_benchmark_rows(data)
    if not rows:
        return

    baseline_rows, baseline_file = load_baseline_rows()
    has_baseline = baseline_file is not None
    current_scene_key, _ = extract_scene_meta(data)
    baseline_scene_key = None
    baseline_scene_name = None
    if baseline_file is not None:
        try:
            baseline_data = json.loads(baseline_file.read_text(encoding="utf-8"))
            baseline_scene_key, baseline_scene_name = extract_scene_meta(baseline_data)
        except (OSError, json.JSONDecodeError):
            baseline_scene_key = None
            baseline_scene_name = None

    def sort_key(item: tuple[str, dict[str, float | str]]) -> tuple[str, int, str]:
        run_name = item[0]
        base = short_bench_id(run_name)
        arg = bench_arg(run_name)
        if arg.isdigit():
            return base, int(arg), ""
        return base, 10**9, arg

    ordered = sorted(rows.items(), key=sort_key)
    table_data: list[list[str]] = []
    for idx, (run_name, metrics) in enumerate(ordered, 1):
        base_id = short_bench_id(run_name)
        ru_name = metadata.get(base_id, {}).get("ru", base_id)
        curr_time_val = metrics.get("real_median")
        if not isinstance(curr_time_val, float):
            curr_time_val = metrics.get("real_mean")

        row = [
            str(idx),
            ru_name,
            bench_arg(run_name),
            fmt_cv(metrics.get("real_cv")),       # type: ignore[arg-type]
            fmt_ips(metrics.get("ips")),          # type: ignore[arg-type]
        ]
        hwc = (hwc_rows or {}).get(run_name, {})
        curr_cache_misses = hwc.get("cache_misses")
        curr_miss_pct = hwc.get("cache_miss_rate_pct")
        if curr_cache_misses is None:
            v = metrics.get("hwc_cache_misses")
            if isinstance(v, float):
                curr_cache_misses = v
        if curr_miss_pct is None:
            v = metrics.get("hwc_cache_miss_rate_pct")
            if isinstance(v, float):
                curr_miss_pct = v
        if has_baseline:
            baseline_metrics = baseline_rows.get(run_name, {})
            base_time_val = baseline_metrics.get("real_median")
            if not isinstance(base_time_val, float):
                base_time_val = baseline_metrics.get("real_mean")

            curr_median_val = curr_time_val if isinstance(curr_time_val, float) else None
            base_median_val = baseline_metrics.get("real_median") if isinstance(baseline_metrics.get("real_median"), float) else None
            if base_median_val is None and isinstance(base_time_val, float):
                base_median_val = base_time_val
            delta_text, _ = compare_status(curr_median_val, base_median_val)
            row.extend([
                fmt_time_ns(curr_time_val),  # type: ignore[arg-type]
                fmt_time_ns(base_time_val),  # type: ignore[arg-type]
                delta_text,
            ])
        else:
            row.append(fmt_time_ns(curr_time_val))  # type: ignore[arg-type]
        if hwc_backend != "none":
            base_miss_pct: float | None = None
            miss_delta_text = "-"
            if has_baseline:
                base_val = baseline_metrics.get("hwc_cache_miss_rate_pct") if isinstance(baseline_metrics, dict) else None
                if isinstance(base_val, float):
                    base_miss_pct = base_val
                if curr_miss_pct is not None and base_miss_pct is not None and base_miss_pct > 0.0:
                    miss_delta = (curr_miss_pct - base_miss_pct) / base_miss_pct * 100.0
                    miss_delta_text = f"{miss_delta:+.1f}%"
            row.extend([
                fmt_compact_number(curr_cache_misses),
                fmt_num(curr_miss_pct, 2),
            ])
            if has_baseline:
                row.extend([
                    fmt_num(base_miss_pct, 2),
                    miss_delta_text,
                ])
        table_data.append(row)

    headers = ["#", "Тест", "N", "cv(%)", "items/s"]
    if has_baseline:
        headers.extend(["time", "baseline", "d (%)"])
    else:
        headers.append("time")
    if hwc_backend != "none":
        headers.extend(["cache-miss", "miss%"])
        if has_baseline:
            headers.extend(["base miss%", "miss d%"])
    widths = [len(h) for h in headers]
    for row in table_data:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))

    print()
    if has_baseline:
        baseline_text = paint("baseline найден", COLOR_OK)
        if baseline_scene_name:
            baseline_scene_text = paint(f"Сцена: {baseline_scene_name}", COLOR_INDEX_LIGHT_BLUE)
            print(f"{baseline_text} | {baseline_scene_text}")
        else:
            print(baseline_text)
    else:
        print(paint("baseline не найден", COLOR_ERROR))
    if has_baseline and current_scene_key and baseline_scene_key and current_scene_key != baseline_scene_key:
        print(paint("Внимание: baseline снят на другой сцене, сравнение может быть некорректным", COLOR_WARN))
        print()
    print(paint("Итоговая таблица:", COLOR_TITLE))
    header_line = "  " + " | ".join(headers[i].ljust(widths[i]) for i in range(len(headers)))
    sep_line = "  " + "-+-".join("-" * widths[i] for i in range(len(headers)))
    print(header_line)
    print(sep_line)
    col = {name: idx for idx, name in enumerate(headers)}
    n_values = [
        int(row[col["N"]])
        for row in table_data
        if row[col["N"]].isdigit()
    ]
    min_n = min(n_values) if n_values else 0
    max_n = max(n_values) if n_values else 0
    for row in table_data:
        rendered = [row[i].ljust(widths[i]) for i in range(len(headers))]
        rendered[col["#"]] = paint(rendered[col["#"]], COLOR_INDEX_LIGHT_BLUE)
        rendered[col["N"]] = paint_n_gradient(rendered[col["N"]], min_n, max_n)
        rendered[col["cv(%)"]] = paint_numeric(rendered[col["cv(%)"]], cv_mode=True)
        rendered[col["time"]] = paint_time_cell(rendered[col["time"]])
        if has_baseline:
            rendered[col["baseline"]] = paint_time_cell(rendered[col["baseline"]])
            rendered[col["d (%)"]] = paint_delta(rendered[col["d (%)"]])
        if hwc_backend != "none":
            rendered[col["miss%"]] = paint_numeric(rendered[col["miss%"]], cv_mode=False)
            if has_baseline:
                rendered[col["miss d%"]] = paint_delta(rendered[col["miss d%"]], neutral_threshold=2.5)
        print("  " + " | ".join(rendered))

    print_complexity_estimate(rows, metadata)


def load_bench_metadata() -> dict[str, dict[str, str]]:
    """
    Читает метаданные бенчмарков из комментариев формата:
    // @bench_meta {"id":"Fixture/Test","ru":"...","group":"Симуляция/Силы"}
    """
    meta: dict[str, dict[str, str]] = {}
    pattern = re.compile(r"@bench_meta\s+(\{.*\})")

    for cpp in BENCHMARKS_ROOT.rglob("*.cpp"):
        if "build" in cpp.parts:
            continue
        try:
            text = cpp.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = cpp.read_text(encoding="utf-8", errors="ignore")

        for line in text.splitlines():
            m = pattern.search(line)
            if not m:
                continue
            try:
                obj = json.loads(m.group(1))
            except json.JSONDecodeError:
                continue
            bench_id = obj.get("id")
            if not bench_id:
                continue
            meta[bench_id] = {
                "ru": str(obj.get("ru", bench_id)),
                "group": str(obj.get("group", "Прочее")),
            }

    return meta


def die(msg: str) -> None:
    print(f"{COLOR_ERROR}Ошибка: {msg}{COLOR_RESET}", file=sys.stderr)
    sys.exit(1)


def ensure_results_dir() -> None:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)


def saved_results() -> list[Path]:
    if not RESULTS_DIR.exists():
        return []
    return sorted(RESULTS_DIR.glob("*.json"), reverse=True)


def format_timestamp(path: Path) -> str:
    return path.stem.replace("_", " ", 1).replace("-", ":", 2)


def safe_file_part(text: str) -> str:
    safe = re.sub(r'[<>:"/\\|?*()\s]+', "_", text)
    safe = re.sub(r"_+", "_", safe).strip("._")
    return safe or "case"


def list_available_filters() -> list[str]:
    if not BINARY.exists():
        die(f"Бинарник не найден: {BINARY}\nСобери проект перед запуском.")

    result = subprocess.run(
        [str(BINARY), "--benchmark_list_tests=true"], capture_output=True, text=True
    )
    names = [line.strip() for line in result.stdout.splitlines() if line.strip()]

    groups: dict[str, None] = {}
    for name in names:
        parts = name.split("/")
        group = "/".join(parts[:2]) if len(parts) >= 2 else parts[0]
        groups[group] = None
    return list(groups.keys())


def list_benchmark_cases(filter_regex: str | None = None) -> list[str]:
    if not BINARY.exists():
        die(f"Бинарник не найден: {BINARY}\nСобери проект перед запуском.")

    result = subprocess.run(
        [str(BINARY), "--benchmark_list_tests=true"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return []

    names = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if not filter_regex:
        return names

    try:
        rx = re.compile(filter_regex)
    except re.error:
        return names
    return [name for name in names if rx.search(name)]

def normalize_benchmark_case(name: str) -> str:
    for suffix in ("_mean", "_median", "_stddev", "_cv"):
        if name.endswith(suffix):
            return name[: -len(suffix)]
    return name


def is_benchmark_result_line(line: str) -> bool:
    s = line.strip()
    if not s:
        return False
    token = s.split()[0]
    if "/" not in token:
        return False
    banned_prefixes = (
        "Benchmark",
        "Run",
        "CPU",
        "L1",
        "L2",
        "L3",
        "***WARNING***",
        "202",
    )
    return not token.startswith(banned_prefixes)


def format_eta(seconds: float | None) -> str:
    if seconds is None or seconds < 0 or math.isinf(seconds) or math.isnan(seconds):
        return "--:--"
    s = int(round(seconds))
    m, s = divmod(s, 60)
    h, m = divmod(m, 60)
    if h > 0:
        return f"{h:02d}:{m:02d}:{s:02d}"
    return f"{m:02d}:{s:02d}"


def parse_duration_to_seconds(value: str | None) -> float | None:
    if not value:
        return None
    s = value.strip().lower()
    m = re.fullmatch(r"(\d+(?:\.\d+)?)(ms|us|ns|s|m)?", s)
    if not m:
        return None
    num = float(m.group(1))
    unit = m.group(2) or "s"
    if unit == "m":
        return num * 60.0
    if unit == "s":
        return num
    if unit == "ms":
        return num / 1000.0
    if unit == "us":
        return num / 1_000_000.0
    if unit == "ns":
        return num / 1_000_000_000.0
    return None


def compute_case_timeout_seconds(
    active_hwc_backend: str,
    repetitions: int,
    min_time: str | None,
    fallback_plain: bool = False,
) -> int:
    # Для обычного запуска держим короткий таймаут.
    timeout = 60

    # Если задан benchmark_min_time, расширяем запас.
    min_time_sec = parse_duration_to_seconds(min_time)
    if min_time_sec is not None and min_time_sec > 0.0:
        timeout = max(timeout, int(min_time_sec * max(1, repetitions) * 4.0 + 30.0))

    # Для HWC нужны длиннее окна, но без многоминутных "залипаний".
    if active_hwc_backend != "none":
        timeout = max(timeout, 90)
        if active_hwc_backend == "windows-amd-uprof":
            timeout = max(timeout, 120)
        elif active_hwc_backend == "windows-pcm":
            timeout = max(timeout, 90)

    # Повторный plain-запуск после HWC-ошибки не должен висеть слишком долго.
    if fallback_plain:
        timeout = min(timeout, 120)
    elif active_hwc_backend != "none":
        timeout = min(timeout, 180)

    return timeout


def scene_label(scene_key: str | None) -> str:
    if scene_key is None:
        return "-"
    for key, label in SCENES:
        if key == scene_key:
            return label
    return scene_key


def detect_hwc_backend(requested: str) -> str:
    if requested == "none":
        return "none"
    if requested == "linux-perf":
        return "linux-perf" if shutil.which("perf") else "none"
    if requested == "windows-pcm":
        return "windows-pcm" if find_intel_pcm_exe() else "none"
    if requested == "windows-amd-uprof":
        return "windows-amd-uprof" if find_amd_uprof_exe() else "none"

    # auto
    if sys.platform.startswith("linux") and shutil.which("perf"):
        return "linux-perf"
    if sys.platform == "win32" and find_amd_uprof_exe():
        return "windows-amd-uprof"
    if sys.platform == "win32" and find_intel_pcm_exe():
        return "windows-pcm"
    return "none"


def find_amd_uprof_exe() -> str | None:
    candidates = [
        "AMDuProfPcm",
        "AMDuProfPcm.exe",
        "AMDuProfCLI",
        "AMDuProfCLI.exe",
    ]
    for name in candidates:
        found = shutil.which(name)
        if found:
            return found

    common_paths = [
        r"C:\Program Files\AMD\AMDuProf\bin\AMDuProfPcm.exe",
        r"C:\Program Files\AMD\AMDuProf\bin\AMDuProfCLI.exe",
        r"C:\Program Files\AMD\AMDuProf\AMDuProfPcm.exe",
        r"C:\Program Files\AMD\AMDuProf\AMDuProfCLI.exe",
    ]
    for path in common_paths:
        if Path(path).exists():
            return path
    return None


def find_intel_pcm_exe() -> str | None:
    candidates = [
        "pcm",
        "pcm.exe",
        "pcm.x",
    ]
    for name in candidates:
        found = shutil.which(name)
        if found:
            return found

    common_paths = [
        r"C:\Program Files\Intel\pcm\pcm.exe",
        r"C:\Program Files\Intel\PCM\pcm.exe",
        r"C:\Tools\pcm\pcm.exe",
    ]
    for path in common_paths:
        if Path(path).exists():
            return path
    return None


def parse_perf_stat(stderr_text: str) -> dict[str, float]:
    out: dict[str, float] = {}
    event_map = {
        "cache-misses": "cache_misses",
        "cache-references": "cache_references",
        "instructions": "instructions",
        "cycles": "cycles",
    }
    for raw in stderr_text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split(";")]
        if len(parts) < 3:
            continue
        value_raw = parts[0].replace(" ", "")
        event_raw = parts[2]
        if event_raw not in event_map:
            continue
        if value_raw in ("<notcounted>", "<not supported>", "notcounted", "notsupported"):
            continue
        value_raw = value_raw.replace(",", "").replace(".", "").replace("\u202f", "").replace("\xa0", "")
        try:
            out[event_map[event_raw]] = float(value_raw)
        except ValueError:
            continue
    misses = out.get("cache_misses")
    refs = out.get("cache_references")
    instr = out.get("instructions")
    if misses is not None and refs and refs > 0:
        out["cache_miss_rate_pct"] = (misses / refs) * 100.0
    if misses is not None and instr and instr > 0:
        out["mpki"] = (misses / instr) * 1000.0
    return out


def parse_amd_uprof_csv(csv_path: Path) -> dict[str, float]:
    if not csv_path.exists():
        return {}

    def norm_key(k: str) -> str:
        return re.sub(r"[^a-z0-9]+", "", k.lower())

    key_aliases = {
        "cachemisses": "cache_misses",
        "l3cachemisses": "cache_misses",
        "dcachemisses": "cache_misses",
        "cachereferences": "cache_references",
        "cacheaccesses": "cache_references",
        "instructions": "instructions",
        "retiredinstructions": "instructions",
        "cycles": "cycles",
        "cpucycles": "cycles",
    }

    totals: dict[str, float] = {}
    try:
        text = csv_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return {}

    # Формат AMDuProfPcm часто выглядит как key,value отчет (не CSV-таблица).
    # Сначала пробуем вытащить агрегированные CORE METRICS.
    key_value: dict[str, float] = {}
    in_core_metrics = False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.upper() == "CORE METRICS":
            in_core_metrics = True
            continue
        if in_core_metrics and line.startswith("Metric,"):
            continue
        if in_core_metrics and line.endswith("METRICS") and line.upper() != "CORE METRICS":
            in_core_metrics = False
            continue
        if not in_core_metrics:
            continue

        parts = [p.strip() for p in line.split(",", 1)]
        if len(parts) != 2:
            continue
        metric_name, value_raw = parts[0], parts[1]
        value_clean = value_raw.replace("%", "").replace(" ", "").replace(",", "")
        try:
            value = float(value_clean)
        except ValueError:
            continue
        key_value[metric_name] = value

    # Предпочтительный путь: L2 access/miss в pti -> miss%.
    l2_access_pti = key_value.get("L2 Access (pti)")
    l2_miss_pti = key_value.get("L2 Miss (pti)")
    gips = key_value.get("Giga Instructions Per Sec")

    if l2_miss_pti is not None:
        # Оценка абсолютных misses через GIPS и pti (предполагаем d=1s).
        if gips is not None and gips > 0:
            instructions = gips * 1_000_000_000.0
            totals["instructions"] = instructions
            totals["cache_misses"] = (l2_miss_pti / 1000.0) * instructions
            if l2_access_pti is not None and l2_access_pti > 0:
                totals["cache_references"] = (l2_access_pti / 1000.0) * instructions
        else:
            # fallback: оставляем в "условных единицах", чтобы хоть miss% считался.
            totals["cache_misses"] = l2_miss_pti
            if l2_access_pti is not None:
                totals["cache_references"] = l2_access_pti

    # Legacy fallback: попробуем CSV-таблицу (если версия uProf даёт иной формат).
    if not totals:
        try:
            with csv_path.open("r", encoding="utf-8", errors="ignore", newline="") as f:
                reader = csv.DictReader(f)
                if reader.fieldnames is not None:
                    mapped_cols: dict[str, str] = {}
                    for col in reader.fieldnames:
                        key = key_aliases.get(norm_key(col))
                        if key:
                            mapped_cols[col] = key
                    for row in reader:
                        for col, metric in mapped_cols.items():
                            raw = (row.get(col) or "").strip()
                            if not raw:
                                continue
                            raw = raw.replace(",", "").replace(" ", "")
                            try:
                                val = float(raw)
                            except ValueError:
                                continue
                            totals[metric] = totals.get(metric, 0.0) + val
        except OSError:
            return {}

    misses = totals.get("cache_misses")
    refs = totals.get("cache_references")
    instr = totals.get("instructions")
    if misses is not None and refs is not None and refs > 0:
        totals["cache_miss_rate_pct"] = (misses / refs) * 100.0
    if misses is not None and instr is not None and instr > 0:
        totals["mpki"] = (misses / instr) * 1000.0
    return totals


def parse_intel_pcm_csv(csv_path: Path) -> dict[str, float]:
    if not csv_path.exists():
        return {}

    try:
        lines = csv_path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError:
        return {}

    # PCM чаще всего пишет с ';' и иногда с ','.
    delimiter = ";" if any(";" in ln for ln in lines[:5]) else ","
    rows = list(csv.reader(lines, delimiter=delimiter))
    if len(rows) < 2:
        return {}

    header_idx = -1
    for i, row in enumerate(rows):
        lowered = " ".join(col.strip().lower() for col in row)
        if "core" in lowered or "socket" in lowered or "system" in lowered:
            if "inst" in lowered or "ipc" in lowered or "l3" in lowered or "l2" in lowered:
                header_idx = i
                break
    if header_idx < 0 or header_idx + 1 >= len(rows):
        return {}

    headers = [h.strip() for h in rows[header_idx]]
    data_rows = rows[header_idx + 1 :]

    # Берем строку TOTAL/SYSTEM, иначе последнюю не-пустую.
    chosen: list[str] | None = None
    for row in data_rows:
        if not row:
            continue
        first = (row[0].strip().lower() if row else "")
        if first in ("total", "system", "sum"):
            chosen = row
            break
    if chosen is None:
        for row in reversed(data_rows):
            if any(cell.strip() for cell in row):
                chosen = row
                break
    if chosen is None:
        return {}

    def parse_num(raw: str) -> float | None:
        s = raw.strip().replace(" ", "").replace("\u202f", "").replace("\xa0", "")
        s = s.replace(",", "")
        if s in ("", "-", "na", "nan"):
            return None
        try:
            return float(s)
        except ValueError:
            return None

    values: dict[str, float] = {}
    for i, col in enumerate(headers):
        if i >= len(chosen):
            continue
        num = parse_num(chosen[i])
        if num is None:
            continue
        key = re.sub(r"[^a-z0-9]+", "", col.lower())
        values[key] = num

    out: dict[str, float] = {}
    # Популярные варианты колонок PCM
    miss_keys = ("l3miss", "l2miss", "cachemisses", "misses")
    ref_keys = ("l3hit", "l2hit", "cachereferences", "references", "cacheaccesses")
    instr_keys = ("inst", "instructions", "retiredinstructions")
    cycle_keys = ("clocks", "cycles", "cpuclk", "cpucycles")

    misses = next((values[k] for k in miss_keys if k in values), None)
    refs = next((values[k] for k in ref_keys if k in values), None)
    instr = next((values[k] for k in instr_keys if k in values), None)
    cycles = next((values[k] for k in cycle_keys if k in values), None)

    if misses is not None:
        out["cache_misses"] = misses
    if refs is not None:
        out["cache_references"] = refs + misses if misses is not None else refs
    if instr is not None:
        out["instructions"] = instr
    if cycles is not None:
        out["cycles"] = cycles

    if "cache_misses" in out and "cache_references" in out and out["cache_references"] > 0:
        out["cache_miss_rate_pct"] = (out["cache_misses"] / out["cache_references"]) * 100.0
    if "cache_misses" in out and "instructions" in out and out["instructions"] > 0:
        out["mpki"] = (out["cache_misses"] / out["instructions"]) * 1000.0
    return out


def collect_hwc_for_cases(
    cases: list[str],
    backend: str,
    scene_key: str | None,
    min_time: str | None,
    hwc_debug: bool = False,
) -> dict[str, dict[str, float]]:
    if backend == "none":
        return {}

    env = os.environ.copy()
    if scene_key:
        env["CHEM_BENCH_SCENE"] = scene_key

    results: dict[str, dict[str, float]] = {}

    if backend == "linux-perf":
        for case in cases:
            cmd = [
                "perf", "stat", "-x", ";",
                "-e", "cache-misses,cache-references,instructions,cycles",
                str(BINARY),
                "--benchmark_format=console",
                "--benchmark_repetitions=1",
                f"--benchmark_filter=^{re.escape(case)}$",
            ]
            if min_time:
                cmd.append(f"--benchmark_min_time={min_time}")
            run = subprocess.run(cmd, capture_output=True, text=True, env=env)
            if hwc_debug:
                stem = safe_file_part(case)
                (RESULTS_DIR / f"hwc_perf_{stem}.stderr.txt").write_text(run.stderr or "", encoding="utf-8")
                (RESULTS_DIR / f"hwc_perf_{stem}.stdout.txt").write_text(run.stdout or "", encoding="utf-8")
            if run.returncode != 0:
                continue
            parsed = parse_perf_stat(run.stderr)
            if parsed:
                results[case] = parsed
        return results

    if backend == "windows-pcm":
        exe = find_intel_pcm_exe()
        if not exe:
            return {}

        for case in cases:
            bench_cmd = [
                str(BINARY),
                "--benchmark_format=console",
                "--benchmark_repetitions=1",
                f"--benchmark_filter=^{re.escape(case)}$",
            ]
            if min_time:
                bench_cmd.append(f"--benchmark_min_time={min_time}")

            stem = safe_file_part(case)
            if hwc_debug:
                out_csv = RESULTS_DIR / f"hwc_pcm_{stem}.csv"
                out_stderr = RESULTS_DIR / f"hwc_pcm_{stem}.stderr.txt"
                out_stdout = RESULTS_DIR / f"hwc_pcm_{stem}.stdout.txt"
            else:
                out_csv = RESULTS_DIR / "_tmp_hwc_pcm.csv"
                out_stderr = None
                out_stdout = None

            if out_csv.exists():
                try:
                    out_csv.unlink()
                except OSError:
                    pass

            # Варианты для разных сборок Intel PCM.
            command_variants = [
                [exe, "1", f"-csv={out_csv}", "--", *bench_cmd],
                [exe, f"-csv={out_csv}", "--", *bench_cmd],
            ]

            parsed: dict[str, float] = {}
            for cmd in command_variants:
                run = subprocess.run(cmd, capture_output=True, text=True, env=env)
                if out_stderr is not None:
                    out_stderr.write_text(run.stderr or "", encoding="utf-8")
                if out_stdout is not None:
                    out_stdout.write_text(run.stdout or "", encoding="utf-8")
                if run.returncode == 0 and out_csv.exists():
                    parsed = parse_intel_pcm_csv(out_csv)
                    if parsed:
                        break
            if parsed:
                results[case] = parsed

            if (not hwc_debug) and out_csv.exists():
                try:
                    out_csv.unlink()
                except OSError:
                    pass
        return results

    if backend == "windows-amd-uprof":
        exe = find_amd_uprof_exe()
        if not exe:
            return {}

        for case in cases:
            bench_cmd = [
                str(BINARY),
                "--benchmark_format=console",
                "--benchmark_repetitions=1",
                f"--benchmark_filter=^{re.escape(case)}$",
            ]
            if min_time:
                bench_cmd.append(f"--benchmark_min_time={min_time}")

            if hwc_debug:
                stem = safe_file_part(case)
                out_csv = RESULTS_DIR / f"hwc_amd_{stem}.csv"
                out_stderr = RESULTS_DIR / f"hwc_amd_{stem}.stderr.txt"
                out_stdout = RESULTS_DIR / f"hwc_amd_{stem}.stdout.txt"
            else:
                out_csv = RESULTS_DIR / "_tmp_hwc_amd.csv"
                out_stderr = None
                out_stdout = None
            if out_csv.exists():
                try:
                    out_csv.unlink()
                except OSError:
                    pass

            # Для Ryzen используем AMDuProfPcm в совместимом режиме.
            # Важно: некоторые версии требуют запуск с правами администратора
            # и загруженным драйвером AMDuProf.
            command_variants = [
                [exe, "-m", "cache_miss,l1,l2,ipc", "-C", "-a", "-d", "1", "-o", str(out_csv), "--", *bench_cmd],
            ]

            parsed: dict[str, float] = {}
            had_permission_error = False
            for cmd in command_variants:
                run = subprocess.run(cmd, capture_output=True, text=True, env=env)
                if out_stderr is not None:
                    out_stderr.write_text(run.stderr or "", encoding="utf-8")
                if out_stdout is not None:
                    out_stdout.write_text(run.stdout or "", encoding="utf-8")
                stderr_low = (run.stderr or "").lower()
                if "createfile failed : 5" in stderr_low or "access is denied" in stderr_low:
                    had_permission_error = True
                if run.returncode == 0 and out_csv.exists():
                    parsed = parse_amd_uprof_csv(out_csv)
                    if parsed:
                        break
            if parsed:
                results[case] = parsed
            elif had_permission_error:
                # Ранняя остановка: без прав дальше все кейсы тоже провалятся.
                return {}

            if (not hwc_debug) and out_csv.exists():
                try:
                    out_csv.unlink()
                except OSError:
                    pass
        return results

    return {}


def parse_cpu_list(spec: str, cpu_total: int | None) -> list[int]:
    result: set[int] = set()
    for raw_token in spec.split(","):
        token = raw_token.strip()
        if not token:
            continue
        if "-" in token:
            parts = token.split("-", 1)
            if len(parts) != 2 or not parts[0].strip().isdigit() or not parts[1].strip().isdigit():
                raise ValueError(f"Неверный диапазон CPU: {token}")
            a = int(parts[0].strip())
            b = int(parts[1].strip())
            if a > b:
                a, b = b, a
            for c in range(a, b + 1):
                result.add(c)
        else:
            if not token.isdigit():
                raise ValueError(f"Неверный индекс CPU: {token}")
            result.add(int(token))

    if not result:
        raise ValueError("Список CPU пуст")

    if cpu_total is not None:
        bad = [c for c in result if c < 0 or c >= cpu_total]
        if bad:
            raise ValueError(
                f"CPU вне диапазона 0..{cpu_total - 1}: {', '.join(str(x) for x in sorted(bad))}"
            )

    return sorted(result)


def apply_process_tuning(pid: int, pin_cpu: str | None, priority: str) -> list[str]:
    warnings: list[str] = []
    if sys.platform != "win32":
        if pin_cpu:
            warnings.append("pin-cpu сейчас реализован только для Windows")
        return warnings

    PROCESS_SET_INFORMATION = 0x0200
    PROCESS_QUERY_INFORMATION = 0x0400
    process_access = PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    handle = kernel32.OpenProcess(process_access, False, pid)
    if not handle:
        warnings.append("Не удалось открыть процесс для настройки affinity/priority")
        return warnings

    try:
        priority_map = {
            "normal": 0x00000020,        # NORMAL_PRIORITY_CLASS
            "above_normal": 0x00008000,  # ABOVE_NORMAL_PRIORITY_CLASS
            "high": 0x00000080,          # HIGH_PRIORITY_CLASS
        }
        prio_class = priority_map.get(priority, priority_map["normal"])
        if not kernel32.SetPriorityClass(handle, prio_class):
            warnings.append(f"Не удалось выставить priority={priority}")

        if pin_cpu:
            cpu_total = os.cpu_count()
            try:
                cpu_ids = parse_cpu_list(pin_cpu, cpu_total)
            except ValueError as exc:
                warnings.append(str(exc))
                cpu_ids = []

            mask = 0
            dropped: list[int] = []
            for cpu_id in cpu_ids:
                if cpu_id >= 64:
                    dropped.append(cpu_id)
                    continue
                mask |= (1 << cpu_id)

            if dropped:
                warnings.append("CPU >= 64 пропущены (ограничение affinity mask WinAPI)")

            if mask != 0:
                if not kernel32.SetProcessAffinityMask(handle, ctypes.c_size_t(mask)):
                    warnings.append(f"Не удалось выставить affinity={pin_cpu}")
    finally:
        kernel32.CloseHandle(handle)

    return warnings


def is_windows_admin() -> bool:
    if sys.platform != "win32":
        return False
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def backend_requires_admin(backend: str) -> bool:
    return backend in ("windows-amd-uprof", "windows-pcm")


def looks_like_amd_msr_busy(stderr_text: str) -> bool:
    s = (stderr_text or "").lower()
    return (
        "hw pmc msrs are in use" in s
        or "pmc msrs are in use" in s
        or "msrs are in use" in s
    )


def try_amd_reset(exe_path: str) -> bool:
    try:
        res = subprocess.run(
            [exe_path, "-r"],
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
        return res.returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def kill_process_tree(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    if sys.platform == "win32":
        try:
            subprocess.run(
                ["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            try:
                proc.kill()
            except OSError:
                pass
        return
    try:
        proc.kill()
    except OSError:
        pass


def print_progress(done: int, total: int, current_case: str = "", start_ts: float | None = None, tick: int = 0) -> None:
    if not sys.stdout.isatty() or total <= 0:
        return

    spinner_frames = ("⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏")
    spin = spinner_frames[tick % len(spinner_frames)]
    width = 24
    ratio = min(1.0, max(0.0, done / float(total)))
    fill = int(width * ratio)
    bar = "█" * fill + "░" * (width - fill)

    eta_seconds: float | None = None
    elapsed_seconds: float | None = None
    if start_ts is not None and done > 0:
        elapsed = time.monotonic() - start_ts
        elapsed_seconds = elapsed
        per_test = elapsed / float(done)
        eta_seconds = per_test * float(max(0, total - done))
    elif start_ts is not None:
        elapsed_seconds = time.monotonic() - start_ts

    case_text = current_case if current_case else "-"
    if len(case_text) > 64:
        case_text = case_text[:61] + "..."

    spin_col = paint(spin, COLOR_PROGRESS_SPINNER)
    bar_col = paint(bar, COLOR_PROGRESS_BAR)
    count_col = paint(f"[{done}/{total}]", COLOR_PROGRESS_COUNT)
    case_col = paint(case_text, COLOR_PROGRESS_CASE)
    eta_col = paint(format_eta(eta_seconds), COLOR_PROGRESS_TIME)
    elapsed_col = paint(format_eta(elapsed_seconds), COLOR_PROGRESS_TIME)
    text = f"{spin_col} [{bar_col}] {count_col} | {case_col} | Elapsed {elapsed_col} | ETA {eta_col}"
    # Clear the whole current line before redrawing.
    sys.stdout.write("\r\033[2K" + paint(text, COLOR_HINT))
    sys.stdout.flush()

def classify_filter_group(filter_name: str) -> str:
    if filter_name.startswith("RendererFixture"):
        return "Рендер"
    if filter_name.startswith("SimulationFixture"):
        return "Симуляция"
    return "Прочее"

def pretty_filter_name(filter_name: str, metadata: dict[str, dict[str, str]]) -> str:
    if filter_name in metadata:
        return metadata[filter_name].get("ru", filter_name)

    parts = filter_name.split("/")
    if len(parts) < 2:
        return filter_name

    fixture, test_name = parts[0], parts[1]
    if fixture.startswith("RendererFixture<Renderer3D>"):
        return f"3D: {test_name}"
    if fixture.startswith("RendererFixture<Renderer2D>"):
        return f"2D: {test_name}"
    if fixture.startswith("SimulationFixture"):
        return re.sub(r"([a-z])([A-Z])", r"\1 \2", test_name)
    return test_name

def classify_by_metadata(filter_name: str, metadata: dict[str, dict[str, str]]) -> tuple[str, str | None]:
    """
    Возвращает (главная_группа, подгруппа_или_None).
    group в metadata может быть:
      "Симуляция/Силы"
      "Рендер/2D"
      "Симуляция"
    """
    if filter_name in metadata:
        raw_group = metadata[filter_name].get("group", "Прочее")
        parts = [p.strip() for p in raw_group.split("/", 1)]
        main = parts[0] if parts and parts[0] else "Прочее"
        sub = parts[1] if len(parts) > 1 and parts[1] else None
        return main, sub
    return classify_filter_group(filter_name), None


def run_benchmark(
    filter_regex: str | None,
    repetitions: int = 3,
    min_time: str | None = None,
    scene_key: str | None = None,
    pin_cpu: str | None = None,
    priority: str = "normal",
    hwc_mode: str = "auto",
    hwc_backend: str = "none",
    hwc_debug: bool = False,
) -> tuple[dict, dict[str, dict[str, float]]]:
    if not BINARY.exists():
        die(f"Бинарник не найден: {BINARY}")

    print(paint(f"Бенч-бинарник: {BINARY}", COLOR_HINT))

    ensure_results_dir()
    total_cases_list = list_benchmark_cases(filter_regex)
    case_list = sorted({normalize_benchmark_case(name) for name in total_cases_list})
    if not case_list:
        die("Нет бенчмарков для запуска по выбранному фильтру.")

    total_cases = len(case_list)
    start_ts = time.monotonic()
    tick = 0
    current_case = ""

    process_env = os.environ.copy()
    if scene_key:
        process_env["CHEM_BENCH_SCENE"] = scene_key

    merged: dict = {"context": {}, "benchmarks": []}
    hwc_rows: dict[str, dict[str, float]] = {}
    active_hwc_backend = hwc_backend

    # Для AMDuProf на Windows полезно заранее сбросить PMU/MSR,
    # чтобы избежать "MSRs are in use" на середине прогона.
    if active_hwc_backend == "windows-amd-uprof":
        amduprof_exe = find_amd_uprof_exe()
        if amduprof_exe:
            if try_amd_reset(amduprof_exe):
                print(paint("HWC: предварительный сброс PMU выполнен (AMDuProfPcm -r)", COLOR_HINT))
            else:
                print(paint("HWC: reset PMU пропущен (AMDuProfPcm -r)", COLOR_HINT))

    print_progress(0, total_cases, current_case, start_ts, tick)

    for idx, case in enumerate(case_list, 1):
        current_case = case
        tick += 1
        print_progress(idx - 1, total_cases, current_case, start_ts, tick)

        case_json = RESULTS_DIR / "_tmp_case_run.json"
        if case_json.exists():
            try:
                case_json.unlink()
            except OSError:
                pass

        bench_cmd = [
            str(BINARY),
            "--benchmark_format=console",
            f"--benchmark_out={case_json}",
            "--benchmark_out_format=json",
            f"--benchmark_repetitions={repetitions}",
            f"--benchmark_filter=^{re.escape(case)}$",
        ]
        if min_time:
            bench_cmd.append(f"--benchmark_min_time={min_time}")

        run_cmd = list(bench_cmd)
        hwc_csv_path: Path | None = None

        if active_hwc_backend == "linux-perf":
            run_cmd = [
                "perf", "stat", "-x", ";",
                "-e", "cache-misses,cache-references,instructions,cycles",
                *bench_cmd,
            ]
        elif active_hwc_backend == "windows-amd-uprof":
            exe = find_amd_uprof_exe()
            if exe:
                stem = safe_file_part(case)
                hwc_csv_path = (RESULTS_DIR / f"hwc_amd_{stem}.csv") if hwc_debug else (RESULTS_DIR / "_tmp_hwc_amd.csv")
                if hwc_csv_path.exists():
                    try:
                        hwc_csv_path.unlink()
                    except OSError:
                        pass
                run_cmd = [exe, "-m", "cache_miss,l1,l2,ipc", "-C", "-a", "-d", "1", "-o", str(hwc_csv_path), "--", *bench_cmd]
        elif active_hwc_backend == "windows-pcm":
            exe = find_intel_pcm_exe()
            if exe:
                stem = safe_file_part(case)
                hwc_csv_path = (RESULTS_DIR / f"hwc_pcm_{stem}.csv") if hwc_debug else (RESULTS_DIR / "_tmp_hwc_pcm.csv")
                if hwc_csv_path.exists():
                    try:
                        hwc_csv_path.unlink()
                    except OSError:
                        pass
                run_cmd = [exe, "1", f"-csv={hwc_csv_path}", "--", *bench_cmd]

        proc = subprocess.Popen(
            run_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            env=process_env,
        )
        tuning_warnings = apply_process_tuning(proc.pid, pin_cpu, priority)
        for warning in tuning_warnings:
            print(paint(f"Предупреждение: {warning}", COLOR_WARN))

        hwc_case_timeout = compute_case_timeout_seconds(
            active_hwc_backend=active_hwc_backend,
            repetitions=repetitions,
            min_time=min_time,
        )
        timed_out = False
        try:
            stdout_data, stderr_data = proc.communicate(timeout=hwc_case_timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            kill_process_tree(proc)
            try:
                stdout_data, stderr_data = proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                stdout_data, stderr_data = "", ""
        return_code = proc.returncode

        if hwc_debug:
            stem = safe_file_part(case)
            prefix = "hwc_none"
            if active_hwc_backend == "linux-perf":
                prefix = "hwc_perf"
            elif active_hwc_backend == "windows-amd-uprof":
                prefix = "hwc_amd"
            elif active_hwc_backend == "windows-pcm":
                prefix = "hwc_pcm"
            (RESULTS_DIR / f"{prefix}_{stem}.stdout.txt").write_text(stdout_data or "", encoding="utf-8")
            (RESULTS_DIR / f"{prefix}_{stem}.stderr.txt").write_text(stderr_data or "", encoding="utf-8")

        if timed_out:
            if active_hwc_backend != "none":
                print()
                print(paint(
                    f"Предупреждение: HWC timeout на кейсе '{case}', переключаюсь на режим без HWC",
                    COLOR_WARN,
                ))
                active_hwc_backend = "none"

                # Повторяем тот же кейс обычным запуском (без HWC), чтобы не терять результат.
                proc_plain = subprocess.Popen(
                    bench_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    bufsize=1,
                    env=process_env,
                )
                tuning_warnings = apply_process_tuning(proc_plain.pid, pin_cpu, priority)
                for warning in tuning_warnings:
                    print(paint(f"Предупреждение: {warning}", COLOR_WARN))

                plain_timed_out = False
                plain_timeout = compute_case_timeout_seconds(
                    active_hwc_backend="none",
                    repetitions=repetitions,
                    min_time=min_time,
                    fallback_plain=True,
                )
                try:
                    stdout_data, stderr_data = proc_plain.communicate(timeout=plain_timeout)
                except subprocess.TimeoutExpired:
                    plain_timed_out = True
                    kill_process_tree(proc_plain)
                    try:
                        stdout_data, stderr_data = proc_plain.communicate(timeout=5)
                    except subprocess.TimeoutExpired:
                        stdout_data, stderr_data = "", ""
                return_code = proc_plain.returncode

                if plain_timed_out:
                    print()
                    print(paint(f"Предупреждение: кейс '{case}' пропущен (timeout {plain_timeout}s)", COLOR_WARN))
                    if case_json.exists():
                        try:
                            case_json.unlink()
                        except OSError:
                            pass
                    print_progress(idx, total_cases, current_case, start_ts, tick)
                    continue
            else:
                print()
                print(paint(f"Предупреждение: кейс '{case}' пропущен (timeout {hwc_case_timeout}s)", COLOR_WARN))
                if hwc_csv_path is not None and (not hwc_debug) and hwc_csv_path.exists():
                    try:
                        hwc_csv_path.unlink()
                    except OSError:
                        pass
                if case_json.exists():
                    try:
                        case_json.unlink()
                    except OSError:
                        pass
                print_progress(idx, total_cases, current_case, start_ts, tick)
                continue

        if return_code != 0:
            hwc_wrapper_failed = (
                active_hwc_backend != "none" and run_cmd != bench_cmd
            )
            if hwc_wrapper_failed and active_hwc_backend == "windows-amd-uprof":
                if looks_like_amd_msr_busy(stderr_data):
                    amduprof_exe = run_cmd[0] if run_cmd else None
                    if isinstance(amduprof_exe, str) and amduprof_exe:
                        print()
                        print(paint("HWC: обнаружен занятый PMU/MSR, пробую автоматический сброс (AMDuProfPcm -r)...", COLOR_WARN))
                        if try_amd_reset(amduprof_exe):
                            # Один повтор того же кейса под HWC после reset.
                            proc_retry = subprocess.Popen(
                                run_cmd,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                text=True,
                                bufsize=1,
                                env=process_env,
                            )
                            tuning_warnings = apply_process_tuning(proc_retry.pid, pin_cpu, priority)
                            for warning in tuning_warnings:
                                print(paint(f"Предупреждение: {warning}", COLOR_WARN))
                            retry_timed_out = False
                            try:
                                stdout_data, stderr_data = proc_retry.communicate(timeout=hwc_case_timeout)
                            except subprocess.TimeoutExpired:
                                retry_timed_out = True
                                kill_process_tree(proc_retry)
                                try:
                                    stdout_data, stderr_data = proc_retry.communicate(timeout=5)
                                except subprocess.TimeoutExpired:
                                    stdout_data, stderr_data = "", ""
                            return_code = proc_retry.returncode if not retry_timed_out else return_code
                            if retry_timed_out:
                                print(paint("HWC: повтор после reset превысил timeout.", COLOR_WARN))
                        else:
                            print(paint("HWC: автоматический сброс PMU не удался.", COLOR_WARN))

            if hwc_wrapper_failed:
                print()
                print(paint(f"Предупреждение: HWC backend '{active_hwc_backend}' недоступен, перехожу на обычный режим без HWC", COLOR_WARN))
                active_hwc_backend = "none"
                hwc_csv_path = None

                proc_plain = subprocess.Popen(
                    bench_cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    bufsize=1,
                    env=process_env,
                )
                tuning_warnings = apply_process_tuning(proc_plain.pid, pin_cpu, priority)
                for warning in tuning_warnings:
                    print(paint(f"Предупреждение: {warning}", COLOR_WARN))

                plain_timed_out = False
                plain_timeout = compute_case_timeout_seconds(
                    active_hwc_backend="none",
                    repetitions=repetitions,
                    min_time=min_time,
                    fallback_plain=True,
                )
                try:
                    stdout_data, stderr_data = proc_plain.communicate(timeout=plain_timeout)
                except subprocess.TimeoutExpired:
                    plain_timed_out = True
                    kill_process_tree(proc_plain)
                    try:
                        stdout_data, stderr_data = proc_plain.communicate(timeout=5)
                    except subprocess.TimeoutExpired:
                        stdout_data, stderr_data = "", ""
                return_code = proc_plain.returncode

                if plain_timed_out:
                    print()
                    print(paint(f"Предупреждение: кейс '{case}' пропущен (timeout {plain_timeout}s)", COLOR_WARN))
                    if case_json.exists():
                        try:
                            case_json.unlink()
                        except OSError:
                            pass
                    print_progress(idx, total_cases, current_case, start_ts, tick)
                    continue

            tail = []
            for line in (stdout_data + "\n" + stderr_data).splitlines():
                if line.strip():
                    tail.append(line)
            if return_code != 0:
                if tail:
                    print("\n".join(tail[-20:]), file=sys.stderr)
                die(f"Бенчмарк '{case}' завершился с кодом {return_code}")

        if not case_json.exists():
            die(f"JSON-файл результата кейса не создан: {case_json}")
        try:
            case_data = json.loads(case_json.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            preview = case_json.read_text(encoding="utf-8", errors="replace")[:300]
            die(f"Не удалось распарсить JSON кейса '{case}': {e}\n{preview}")

        if not merged["context"]:
            merged["context"] = case_data.get("context", {})
        merged["benchmarks"].extend(case_data.get("benchmarks", []))

        parsed_hwc: dict[str, float] = {}
        if active_hwc_backend == "linux-perf":
            parsed_hwc = parse_perf_stat(stderr_data or "")
        elif active_hwc_backend == "windows-amd-uprof" and hwc_csv_path is not None:
            parsed_hwc = parse_amd_uprof_csv(hwc_csv_path)
            if (not hwc_debug) and hwc_csv_path.exists():
                try:
                    hwc_csv_path.unlink()
                except OSError:
                    pass
        elif active_hwc_backend == "windows-pcm" and hwc_csv_path is not None:
            parsed_hwc = parse_intel_pcm_csv(hwc_csv_path)
            if (not hwc_debug) and hwc_csv_path.exists():
                try:
                    hwc_csv_path.unlink()
                except OSError:
                    pass
        if parsed_hwc:
            hwc_rows[case] = parsed_hwc

        try:
            case_json.unlink()
        except OSError:
            pass

        print_progress(idx, total_cases, current_case, start_ts, tick)

    print_progress(total_cases, total_cases, current_case, start_ts, tick + 1)
    if sys.stdout.isatty():
        sys.stdout.write("\n\n")
        sys.stdout.flush()

    context = merged.get("context")
    if not isinstance(context, dict):
        context = {}
        merged["context"] = context

    # Сохраняем HWC в JSON на уровне каждой benchmark-записи,
    # чтобы baseline мог сравнивать не только time, но и miss%.
    if hwc_rows:
        for item in merged.get("benchmarks", []):
            if not isinstance(item, dict):
                continue
            run_name = item.get("run_name") or item.get("name")
            if not isinstance(run_name, str):
                continue
            case_name = normalize_benchmark_case(run_name)
            hwc = hwc_rows.get(case_name)
            if not hwc:
                continue
            if "cache_misses" in hwc:
                item["hwc_cache_misses"] = float(hwc["cache_misses"])
            if "cache_miss_rate_pct" in hwc:
                item["hwc_cache_miss_rate_pct"] = float(hwc["cache_miss_rate_pct"])

    context["bench_py"] = {
        "scene_key": scene_key or "crystal3d",
        "scene_name": scene_label(scene_key or "crystal3d"),
        "pin_cpu": pin_cpu or "",
        "priority": priority,
        "hwc_mode": hwc_mode,
        "hwc_backend": hwc_backend,
    }

    last_run = RESULTS_DIR / "last_run.json"
    last_run.write_text(json.dumps(merged, indent=2), encoding="utf-8")
    return merged, hwc_rows


def save_result(data: dict, filter_used: str | None) -> Path:
    ensure_results_dir()
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    if filter_used:
        # Windows-safe filename suffix.
        safe_filter = re.sub(r'[<>:"/\\|?*()\s]+', "_", filter_used)
        safe_filter = re.sub(r"_+", "_", safe_filter).strip("._")
        suffix = f"_{safe_filter}" if safe_filter else "_filtered"
    else:
        suffix = "_all"
    path = RESULTS_DIR / f"{timestamp}{suffix}.json"
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")
    print(paint(f"Сохранено: {path}", COLOR_OK))
    return path


def merge_current_tests_into_baseline(baseline_data: dict, current_data: dict) -> tuple[dict, int]:
    baseline_benchmarks = baseline_data.get("benchmarks")
    current_benchmarks = current_data.get("benchmarks")
    if not isinstance(baseline_benchmarks, list):
        baseline_benchmarks = []
    if not isinstance(current_benchmarks, list):
        current_benchmarks = []

    current_cases: set[str] = set()
    for item in current_benchmarks:
        if not isinstance(item, dict):
            continue
        run_name = item.get("run_name") or item.get("name")
        if isinstance(run_name, str) and run_name:
            current_cases.add(normalize_benchmark_case(run_name))

    kept: list[dict] = []
    replaced = 0
    for item in baseline_benchmarks:
        if not isinstance(item, dict):
            continue
        run_name = item.get("run_name") or item.get("name")
        if isinstance(run_name, str) and normalize_benchmark_case(run_name) in current_cases:
            replaced += 1
            continue
        kept.append(item)

    merged = dict(baseline_data)
    merged["benchmarks"] = [*kept, *current_benchmarks]
    return merged, replaced


def maybe_save_baseline(data: dict) -> bool:
    if not sys.stdin.isatty():
        return False

    answer = input("\nОбновить данные текущих тестов в baseline? [y/N]: ").strip().lower()
    if answer != "y":
        return False

    ensure_results_dir()
    baseline_path = RESULTS_DIR / "baseline.json"
    if baseline_path.exists():
        try:
            old = json.loads(baseline_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            old = {}
        merged, replaced = merge_current_tests_into_baseline(old, data)
        baseline_path.write_text(json.dumps(merged, indent=2), encoding="utf-8")
        print(paint(f"Baseline обновлен: {baseline_path} (заменено записей: {replaced})", COLOR_OK))
    else:
        baseline_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
        print(paint(f"Baseline создан: {baseline_path}", COLOR_OK))
    return True


def interactive_menu() -> tuple[str | None, int, bool, str | None, str, str | None, str, str, bool]:
    print(f"{paint('=== Chemical Simulator Benchmarks ===', COLOR_TITLE)}\n")

    filters = list_available_filters()
    metadata = load_bench_metadata()
    grouped: dict[str, list[tuple[str, str]]] = {
        "Рендер": [],
        "Симуляция": [],
        "Прочее": [],
    }
    subgrouped: dict[str, dict[str, list[tuple[str, str]]]] = {}
    for f in filters:
        main_group, sub_group = classify_by_metadata(f, metadata)
        if main_group == "Рендер":
            # Для UX держим рендер в одной группе без подкатегорий 2D/3D.
            sub_group = None
        if main_group not in grouped:
            grouped[main_group] = []
        if sub_group:
            if main_group not in subgrouped:
                subgrouped[main_group] = {}
            subgrouped[main_group].setdefault(sub_group, []).append((f, pretty_filter_name(f, metadata)))
        else:
            grouped[main_group].append((f, pretty_filter_name(f, metadata)))

    menu_filters: list[str] = []
    print(f"{paint('Доступные бенчмарки:', COLOR_TITLE)}\n")

    menu_index = 1
    ordered_main_groups = []
    for default_group in ("Рендер", "Симуляция", "Прочее"):
        if default_group in grouped:
            ordered_main_groups.append(default_group)
    for extra_group in grouped.keys():
        if extra_group not in ordered_main_groups:
            ordered_main_groups.append(extra_group)

    for group_name in ordered_main_groups:
        entries = grouped.get(group_name, [])
        sub_entries_map = subgrouped.get(group_name, {})
        if not entries:
            has_sub = any(sub_entries_map.values())
            if not has_sub:
                continue
        print(f"\n  {paint(f'--- {group_name} ---', COLOR_GROUP)}")

        # Сначала подгруппы из metadata, затем элементы без подгруппы.
        for sub_name in sub_entries_map.keys():
            print(f"  {paint_subgroup_label(sub_name)}")
            for raw, pretty in sub_entries_map[sub_name]:
                print(f"  {paint(f'{menu_index})', COLOR_INDEX)} {pretty}")
                menu_filters.append(raw)
                menu_index += 1

        for raw, pretty in entries:
            print(f"  {paint(f'{menu_index})', COLOR_INDEX)} {pretty}")
            menu_filters.append(raw)
            menu_index += 1

    print(f"\n  {paint('0)', COLOR_INDEX)} Все")

    print()
    choice = input("Выбери номер(а) через пробел (Enter = все): ").strip()

    selected: str | None = None
    if choice == "" or choice == "0":
        selected = None
    else:
        tokens = choice.split()
        if len(tokens) == 1:
            try:
                idx = int(tokens[0]) - 1
                if 0 <= idx < len(menu_filters):
                    selected = menu_filters[idx]
                else:
                    die("Неверный номер")
            except ValueError:
                selected = choice
        else:
            if not all(token.isdigit() for token in tokens):
                die("Для множественного выбора укажи только номера через пробел")

            selected_filters: list[str] = []
            for token in tokens:
                idx = int(token) - 1
                if 0 <= idx < len(menu_filters):
                    selected_filters.append(menu_filters[idx])
                else:
                    die("Неверный номер")

            selected_filters = list(dict.fromkeys(selected_filters))
            selected = "(" + "|".join(re.escape(item) for item in selected_filters) + ")"

    print()
    print(paint("Выбор сцены (для SimulationFixture):", COLOR_TITLE))
    default_scene_id = "crystal3d"
    default_scene_name = scene_label(default_scene_id)
    print(f"  {paint('0)', COLOR_INDEX)} {default_scene_name} (по умолчанию)")

    scene_options = [(sid, name) for sid, name in SCENES if sid != default_scene_id]
    for i, (_, ru_name) in enumerate(scene_options, 1):
        print(f"  {paint(f'{i})', COLOR_INDEX)} {ru_name}")

    scene_choice = input("Сцена [0]: ").strip()
    selected_scene = default_scene_id
    if scene_choice and scene_choice != "0":
        if not scene_choice.isdigit():
            die("Неверный номер сцены")
        scene_index = int(scene_choice) - 1
        if not (0 <= scene_index < len(scene_options)):
            die("Неверный номер сцены")
        selected_scene = scene_options[scene_index][0]

    print()
    print(paint("Режим прогона:", COLOR_TITLE))
    default_profile_idx = 2  # Средний
    for i, (_, ru_name, reps, mt) in enumerate(RUN_PROFILES, 1):
        print(f"  {paint(f'{i})', COLOR_INDEX)} {ru_name} ({reps} прогон(а), min_time={mt})")
    profile_choice = input(f"Выбери режим [{default_profile_idx}]: ").strip()
    if profile_choice == "":
        profile_idx = default_profile_idx - 1
    elif profile_choice.isdigit():
        profile_idx = int(profile_choice) - 1
        if not (0 <= profile_idx < len(RUN_PROFILES)):
            die("Неверный номер режима")
    else:
        die("Неверный номер режима")

    _, profile_name, repetitions, min_time = RUN_PROFILES[profile_idx]
    print(paint(f"Выбран режим: {profile_name} (repetitions={repetitions}, min_time={min_time})", COLOR_HINT))

    # В интерактивном режиме держим простой дефолтный профиль CPU.
    pin_cpu = None
    priority = "normal"

    # В интерактивном режиме используем дефолтные HWC-настройки,
    # чтобы не перегружать меню лишними вопросами.
    hwc_mode = "auto"
    hwc_debug = False

    return selected, repetitions, False, min_time, selected_scene, pin_cpu, priority, hwc_mode, hwc_debug


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Запускалка бенчмарков для LATTICE engine",
    )
    parser.add_argument("--filter", metavar="REGEX", help="Фильтр бенчмарков (regex)")
    parser.add_argument("--save", action="store_true", help="Сохранить результат в results/")
    parser.add_argument("--list", action="store_true", help="Показать сохранённые результаты")
    parser.add_argument(
        "--repetitions",
        metavar="N",
        type=int,
        default=3,
        help="Количество прогонов (default: 3)",
    )
    parser.add_argument(
        "--min-time",
        metavar="TIME",
        default=None,
        help="Минимальное время прогона benchmark (пример: 5s, 500ms)",
    )
    parser.add_argument(
        "--scene",
        metavar="SCENE",
        choices=[scene_id for scene_id, _ in SCENES],
        default="crystal3d",
        help="Сцена для SimulationFixture: crystal3d | crystal2d | random_gas2d",
    )
    parser.add_argument(
        "--pin-cpu",
        metavar="LIST",
        default=None,
        help="Фиксация CPU для процесса benchmarks (пример: 2,3 или 2-5)",
    )
    parser.add_argument(
        "--priority",
        metavar="LEVEL",
        choices=list(SUPPORTED_PRIORITIES),
        default="normal",
        help="Приоритет процесса benchmarks (normal|above_normal|high)",
    )
    parser.add_argument(
        "--hwc",
        metavar="MODE",
        choices=list(SUPPORTED_HWC),
        default="auto",
        help="Сбор cache-miss счётчиков: none|auto|linux-perf|windows-pcm|windows-amd-uprof",
    )
    parser.add_argument(
        "--hwc-debug",
        action="store_true",
        help="Сохранять сырые HWC файлы (CSV/stdout/stderr) в Benchmarks/results",
    )
    parser.add_argument("--open", action="store_true", help="Открыть view.html в браузере")
    args = parser.parse_args()

    if args.list:
        results = saved_results()
        if not results:
            print("Нет сохранённых результатов.")
        else:
            print("\nСохранённые результаты:\n")
            for p in results:
                print(f"  {format_timestamp(p)}  —  {p}")
        return

    if args.open:
        if not VIEW_HTML.exists():
            die(f"view.html не найден: {VIEW_HTML}")
        webbrowser.open(VIEW_HTML.as_uri())
        return

    repetitions = args.repetitions
    min_time = args.min_time
    selected_scene = args.scene
    pin_cpu = args.pin_cpu
    priority = args.priority
    hwc_mode = args.hwc
    hwc_debug = args.hwc_debug
    if args.filter or args.save:
        filter_regex = args.filter
        save_flag = args.save
    else:
        filter_regex, repetitions, save_flag, min_time, selected_scene, pin_cpu, priority, hwc_mode, hwc_debug = interactive_menu()

    print()
    print(paint(f"Выбрана сцена: {scene_label(selected_scene)}", COLOR_INDEX_LIGHT_BLUE))
    cpu_profile = pin_cpu if pin_cpu else "без pin"
    print(paint(f"CPU профиль: {cpu_profile} | priority: {priority}", COLOR_LABEL_WHITE))
    effective_hwc = detect_hwc_backend(hwc_mode)
    if sys.platform == "win32" and backend_requires_admin(effective_hwc) and not is_windows_admin():
        print(paint("Запуск без прав администратора. HWC метрики отключены", COLOR_WARN))
        effective_hwc = "none"
    hwc_line = (
        f"{paint('HWC режим:', COLOR_LABEL_WHITE)} {paint(hwc_mode, COLOR_INDEX_LIGHT_BLUE)} "
        f"{paint('->', COLOR_LABEL_WHITE)} {paint(effective_hwc, COLOR_INDEX_LIGHT_BLUE)}"
    )
    print(hwc_line)
    print()
    data, hwc_rows = run_benchmark(
        filter_regex,
        repetitions,
        min_time,
        selected_scene,
        pin_cpu,
        priority,
        hwc_mode,
        effective_hwc,
        hwc_debug,
    )
    metadata = load_bench_metadata()
    if effective_hwc != "none" and not hwc_rows:
        if effective_hwc == "windows-amd-uprof":
            print(paint("Предупреждение: HWC метрики не собраны. Для AMDuProf на Windows обычно нужен запуск от администратора и доступ к драйверу PMU.", COLOR_WARN))
        else:
            print(paint("Предупреждение: HWC метрики не собраны (проверь права/инструмент)", COLOR_WARN))
    elif effective_hwc != "none" and hwc_debug:
        print(paint(f"HWC debug: сырые файлы сохранены в {RESULTS_DIR}", COLOR_HINT))
    print_results_table(data, metadata, scene_label(selected_scene), hwc_rows, effective_hwc)

    maybe_save_baseline(data)

    if save_flag:
        save_result(data, filter_regex)


if __name__ == "__main__":
    main()
