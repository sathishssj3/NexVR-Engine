# NexVR Engine — Validation Criteria

This file holds the exact GO / ITERATE / KILL thresholds for the 30-day locked v0.1.0 experiment. 
These metrics are the premise of the experiment and must not be altered mid-flight.

## The Hypothesis
> Black-box runtime analysis and adaptation can generalize beyond games that were manually prepared for NexVR.

## Metrics & Thresholds

| Metric | Target | Outcome if Missed |
|---|---|---|
| **Zero-Tuning Working Rate** | > 15% | ITERATE on Candidate Validation Engine |
| **Camera Detection Rate** | > 60% | ITERATE on Universal Scanner coverage |
| **Camera Correctness Ratio** (Correct / Detected) | > 80% | ITERATE on Validation Heuristics |
| **Crash/Deadlock Rate** | 0% | STOP and fix failure state machine |
| **Telemetry False-Positive** (User opt-out fails) | 0% | STOP and fix immediately |

*(Note: A 15% zero-tuning success rate on previously untested, unseen games using pure black-box runtime semantic inference is a monumental technical achievement. If we hit this, the hypothesis is proven.)*

## Falsification Condition
The hypothesis is falsified (KILL) if the Zero-Tuning Working Rate across 50 randomly selected, previously untested DX11/DX12 games is exactly **0%**, despite a Camera Detection Rate of > 80%. 
This would indicate that finding *something* that looks like a camera is trivial, but generically validating it without per-game profiles is computationally unachievable.
