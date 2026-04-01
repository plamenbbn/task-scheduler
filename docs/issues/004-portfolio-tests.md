# Create tests that assert portfolio behavior under resource caps

## Summary
The project has useful scheduler tests now, including incremental registration, execution timing, daemon frequency, and fairness checks. What is still missing is a stronger test suite that explicitly validates portfolio selection behavior under constrained CPU/memory budgets.

Add deterministic tests that assert the scheduler chooses sensible multi-task portfolios under hard resource limits.

## Problem
Current tests prove mechanics, but they do not fully lock down strategic scheduling behavior. In particular, they do not reliably assert that:
- mixed portfolios are preferred when appropriate
- resource constraints influence portfolio composition correctly
- daemon and one-shot combinations behave sensibly together
- regressions in optimizer behavior are caught early

## Goal
Build deterministic, readable tests that validate portfolio-level behavior rather than just individual task execution.

## Proposed approach
Add scenario-based tests with hand-crafted fixtures.

Suggested scenarios:
1. **Packing test**
   - several feasible combinations exist
   - best schedule should use available CPU/memory effectively
2. **Complementarity test**
   - two medium tasks together should beat one flashy but narrow task
3. **Daemon contention test**
   - overlapping daemon tasks should reduce attractiveness of overloaded schedules
4. **Fairness test**
   - all registered tasks must appear in the plan and one-shots must execute at least once
5. **Incremental evolution test**
   - adding tasks should produce predictable changes in ordering and execution behavior

## Acceptance criteria
- Tests are deterministic and do not rely on luck or unstable random seeds
- Tests clearly document the scheduling behavior being protected
- Tests cover both planning and execution phases
- Tests verify behavior under the 100 CPU / 100 memory caps
- CI/test output remains understandable when failures occur

## Notes
This issue is about building a regression suite that protects the scheduler from slowly drifting back into dumb behavior.
