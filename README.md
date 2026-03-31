# MOO Task Scheduler

Multi-objective optimization (MOO) scheduler prototype that selects mission tasks while constraining CPU, memory, and network costs. Tasks are authored in C++ by subclassing `moo::Task` and registering them with the scheduler. Scheduling stays in-memory and focuses on Pareto-optimal planning via NSGA-II.

## Features

- **Task abstraction** – implement `run()` and specify mission benefit + cost tuple.
- **NSGA-II optimizer** – explores the Pareto front across 4 objectives (benefit ↑, CPU ↓, memory ↓, network ↓).
- **Online refinement** – builds a fresh schedule for every planning window and can seed from prior runs.
- **Daemon vs one-shot modes** – scheduler keeps both queues in the resulting plan.

## Building

```bash
cmake -S . -B build
cmake --build build -j
./build/moo_demo
```

## Extending

- Tweak `OptimizationSettings` (population, generations, crossover, mutation) before calling `buildSchedule`.
- Swap in MOEA/D or other algorithms by adding additional classes under `include/moo/optimizer/`.
- Integrate telemetry by updating `Task` benefit/cost values before invoking the scheduler.
