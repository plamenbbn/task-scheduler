# Model daemon resource pressure more realistically

## Summary
Daemon tasks now have duration and execution frequency, and the optimizer includes a basic execution-aware weighting model. That is a solid start, but it still treats daemon pressure too approximately.

Improve the optimizer so daemon tasks account for overlap risk and sustained resource pressure more realistically.

## Problem
Current daemon scoring uses a simplified approximation based on:
- planning window
- invocation count
- duty cycle

That helps, but it does not fully capture cases where daemon tasks:
- overlap with each other
- repeatedly consume resources in bursts
- create scheduler contention for one-shot work
- look cheap in aggregate but are costly in time-distributed execution

## Goal
Make daemon valuation and feasibility closer to how execution actually behaves over time.

## Proposed approach
Potential implementation directions:
1. Estimate expected concurrent daemon load from:
   - duration
   - frequency
   - resource requirements
2. Compute expected overlap pressure across selected daemons
3. Incorporate burstiness / collision risk into feasibility or penalties
4. Account for reduced capacity available to one-shot tasks when many daemons are active

Possible models:
- average steady-state duty-cycle resource usage
- peak overlap approximation
- discrete simulation over the planning window for candidate evaluation
- hybrid heuristic with a cheap upper bound on concurrent pressure

## Acceptance criteria
- Daemon-heavy schedules are evaluated using a more realistic pressure model
- Schedules that would create excessive runtime contention are penalized or rejected
- The model is documented and cheap enough to use in optimization loops
- Existing tests still pass
- New tests cover overlapping daemon scenarios and one-shot starvation risk

## Notes
This work should improve consistency between what the optimizer predicts and what the runtime scheduler actually experiences.
