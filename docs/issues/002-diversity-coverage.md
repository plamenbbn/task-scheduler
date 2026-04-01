# Add task diversity / coverage reward

## Summary
The scheduler currently ranks tasks largely by benefit/cost characteristics and fairness ensures they all appear in the plan. What is still missing is a stronger notion of schedule diversity or mission coverage.

Add a diversity / coverage reward so schedules with complementary tasks are favored over schedules that repeatedly prioritize only one style of work.

## Problem
Without a diversity-aware objective, schedules can become lopsided:
- too many similar tasks
- too much preference for one task archetype
- poor overall mission coverage even when resource usage is acceptable

This is especially visible when one family of tasks has slightly better raw ratios than everything else.

## Goal
Encourage schedules that improve mission breadth, not just narrow local efficiency.

Examples of what this could mean:
- mix one-shot and daemon work appropriately
- reward selection across multiple task classes or categories
- reduce repeated domination by one task profile when alternatives add complementary value

## Proposed approach
Possible designs:
1. Add metadata to tasks describing category / capability / mission domain
2. Track per-schedule coverage across those categories
3. Reward schedules that cover more categories
4. Optionally add diminishing returns for selecting multiple near-identical tasks

Potential scoring patterns:
- category count reward
- submodular coverage gain
- diminishing marginal reward for repeated task types
- explicit balancing across one-shot and daemon selections

## Acceptance criteria
- Scheduler can represent task category/capability metadata
- Scoring reflects diversity or mission coverage in a documented way
- Schedules with broader useful coverage can beat narrowly concentrated schedules when otherwise comparable
- Existing tests still pass
- New tests demonstrate that complementary portfolios are preferred to repetitive ones

## Notes
This issue is about improving strategic quality of schedules, not just fairness of inclusion.
