# Add explicit budget-utilization reward to the optimizer

## Summary
The scheduler now respects fairness by including all registered tasks in the plan, and runtime enforces CPU/memory caps while dispatching work. However, the optimizer still lacks an explicit incentive to build schedules that make good use of available resource budgets.

Add a budget-utilization reward so the scheduler prefers feasible schedules that use CPU and memory effectively without exceeding the configured caps.

## Problem
Right now, schedule quality is driven mostly by task-level benefit/cost heuristics. That can lead to schedules that are:
- overly conservative
- under-packed relative to available CPU/memory
- dominated by a few high-ratio tasks instead of well-balanced portfolios

Even when fairness guarantees inclusion, the prioritization logic would still benefit from understanding the difference between:
- a schedule that leaves most of the machine idle, and
- a schedule that uses available headroom intelligently.

## Goal
Encourage plans that:
- stay within hard CPU/memory limits
- make good use of available budget
- avoid pathological under-utilization
- do not reward oversubscription

## Proposed approach
Introduce an additional objective or scoring term based on budget utilization.

Possible implementation directions:
1. Compute normalized utilization:
   - `cpuUtilization = totalCpu / maxCpu`
   - `memoryUtilization = totalMemory / maxMemory`
2. Reward utilization only in the feasible range `[0, 1]`
3. Penalize oversubscription separately as a hard infeasibility cost
4. Combine CPU and memory utilization into a packing score, for example:
   - weighted average
   - min(cpuUtilization, memoryUtilization)
   - convex combination that rewards balanced packing

## Acceptance criteria
- Feasible schedules with better resource utilization are preferred over similarly valuable but underutilized schedules
- Infeasible schedules are still rejected or strongly dominated
- The scoring logic is documented in code/comments
- Existing tests still pass
- At least one new test demonstrates that better-packed feasible schedules win over sparse ones

## Notes
This should improve overall schedule quality without breaking the hard resource limits already enforced at runtime.
