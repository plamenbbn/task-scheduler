# Architecture Overview

## High-Level Flow

1. **Registration** – System developers subclass `moo::Task` and register instances via `TaskRegistry`.
2. **Snapshotting** – `MooScheduler` builds a lightweight `TaskSnapshot` array (benefit + cost tuple + execution mode).
3. **Optimization** – NSGA-II evolves a population of `DecisionVector`s (binary include/exclude genes) to approximate the Pareto front.
4. **Selection** – The scheduler selects a knee-point-ish candidate using a benefit/(cpu+mem+net) heuristic and emits a `SchedulePlan` containing:
   - Chosen one-shot tasks (execute immediately in sequence)
   - Chosen daemon tasks (launched in detached threads for now)
   - Aggregated objective summary for reporting
5. **Execution** – `execute()` runs the plan. Hooks are left for future telemetry loops or external dispatching.

## Key Types

- `Task` – abstract base with mission benefit and cost tuple.
- `TaskSnapshot` – POD copy used by optimizers; decouples runtime tasks from search.
- `DecisionVector` – genome representation; future work can add daemon cadence genes.
- `CandidateSolution` – decision vector + objective tuple + NSGA-II metadata (rank, crowding distance).
- `SchedulePlan` – the actionable schedule payload.

## Future Enhancements

- Add resource budget constraints + penalty repair operators.
- Persist previous Pareto fronts to warm-start new planning windows.
- Support MOEA/D for large task catalogs.
- Replace detached daemon threads with a cooperative executor / reactor loop.
