# WhiskerToolbox Benchmarking

Local performance stress binaries and heaptrack regression checks. Not part of `ctest`.

## Regression guard (pass / fail)

Requires `HEAPTRACK_EXECUTABLE` in `CMakeUserPresets.json` and a working OpenGL display.

```bash
cmake --preset my-clang-release
cmake --build --preset my-clang-release

# Once per machine:
cmake --build --preset my-clang-release --target record_benchmark_baselines

# After code changes:
cmake --build --preset my-clang-release --target check_benchmark_regressions
```

- Exit **0** — heaptrack `temporary allocations` (and `allocations`) within baseline tolerance
- Exit **1** — regression printed with numbers

Baselines are stored in `benchmark-baselines-local/` (gitignored). Same machine only.

## ScatterPlot view stress

`benchmark_ScatterPlotView` runs a fixed pan/zoom loop on `ScatterPlotOpenGLWidget`
with synthetic analog data (10k points, 50 pan/zoom iterations by default).

```bash
./out/build/Clang/Release/benchmark/benchmark_ScatterPlotView
./out/build/Clang/Release/benchmark/benchmark_ScatterPlotView 10000 50
```

## DataViewer allocation probes

`benchmark_DataViewerView` runs small horizontal-scroll probes on `OpenGLWidget`
(1–2 channels, 1000-sample window). Three named scenarios are recorded under
`benchmark-baselines-local/benchmark_DataViewerView_*.heaptrack.txt`:

```bash
./out/build/Clang/Release/benchmark/benchmark_DataViewerView 1 10000 1000 0 1   # 1ch-init
./out/build/Clang/Release/benchmark/benchmark_DataViewerView 1 10000 1000 1 1   # 1ch-scroll1
./out/build/Clang/Release/benchmark/benchmark_DataViewerView 2 10000 1000 1 1   # 2ch-scroll1
```

See `docs/developer/benchmark/dataviewer_view_benchmark.qmd` for theoretical expectations.

## Optional Google Benchmark JSON

`run_benchmarks` runs Google Benchmark targets only (stress executables are excluded).

## Adding another regression stress binary

1. Add `MyViewInteraction.benchmark.cpp` with a `main()` stress loop
2. Register with `add_selective_benchmark(... STRESS_ONLY ...)`
3. `record_benchmark_baselines` / `check_benchmark_regressions` pick it up automatically

## CMake options

```bash
cmake --preset my-clang-release -DBENCHMARK_SCATTERPLOTVIEW=OFF
```
