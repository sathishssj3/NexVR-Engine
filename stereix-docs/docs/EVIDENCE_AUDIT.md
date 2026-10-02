# NexVR — Internal Evidence Audit

Purpose: establish what the existing injector has actually achieved on real games, before the public experiment. This is the highest-quality evidence in existence about the black-box generalization hypothesis, because nobody else has run this code.

Fill from **logs and commit history**, not memory. Recall bias runs in one direction — you will remember the games that worked and forget how much hand-tuning they took.

## Per-game record

One row per game, in `docs/evidence/games.csv` or equivalent.

| Field | Values | Notes |
|---|---|---|
| `game` | string | |
| `first_encounter_date` | date | From logs/commits, not memory |
| `first_encounter_build` | commit SHA / version | Which build first saw it |
| `engine` | Unreal / Unity / RE / id Tech / custom / unknown | |
| `engine_version` | string / unknown | |
| `renderer` | DX11 / DX12 / Vulkan | |
| `injection` | pass / fail | Process attach succeeded |
| `hooks_installed` | pass / fail | MinHook install on the target API |
| `swapchain_detected` | pass / fail | |
| `camera_detected` | pass / fail | A candidate was accepted |
| `camera_correct` | **yes / no / partial** | **See below — this is the important one** |
| `depth_available` | yes / no / partial | |
| `stereo_started` | pass / fail | |
| `openxr_session` | pass / fail | |
| `frames_submitted` | pass / fail | |
| `failure_stage` | one of the §6 failure states | First stage that failed |
| `hand_tuning` | none / config / code | See classification below |
| `tuning_hours` | number | Rough is fine; zero is a real value |
| `outcome` | working / degraded / failed | |
| `notes` | free text | Symptom description if degraded |

### Why `camera_correct` is separate from `camera_detected`

A wrong-but-accepted matrix candidate produces a session that *starts* — and then looks broken: wrong scale, inverted pitch, world locked to head, stereo that doesn't converge.

If you collapse these into one column, your detection success rate will look far better than your working-session rate and you won't be able to tell why.

This distinction is likely the single most informative thing in the whole audit. A high `camera_detected` / low `camera_correct` ratio means the discovery machinery finds candidates fine and **ranking/validation is the real unsolved problem** — which is precisely the capability worth building next.

### `hand_tuning` classification

- **none** — worked from a shipped build with default settings
- **config** — worked after adjusting exposed settings or writing a profile, no code change
- **code** — required a source change, a new heuristic, or a game-specific branch

`code` is disqualifying for generalization evidence. `config` is ambiguous — record which settings, because "the user could have done this" and "only you knew to do this" are different results.

## Derived metrics

Compute these from the table. Report all of them; no single headline number.

1. `n_games` — total distinct games encountered
2. `n_engine_families`, `n_renderer_families` — **breadth, not depth**. Twenty Unreal DX11 games is one data point about generalization, not twenty.
3. Stereo-session rate: `stereo_started` / `n_games`
4. Working-outcome rate: `outcome == working` / `n_games`
5. **Zero-tuning working rate**: `outcome == working AND hand_tuning == none` / `n_games` — this is the generalization number.
6. Failure-stage histogram — count per state; find the modal failure.
7. Camera-detection rate vs. camera-correct rate (the ratio above).
8. Failure-stage histogram **split by engine and renderer** — does one family dominate?
9. Recurrence: how many games share an identical failure signature.

## Interpretation guardrails

**Small n**. If you've touched 10–20 games, every rate above has error bars wide enough to swallow the result. "3 of 12 generalized" and "6 of 12" are not reliably distinguishable at that size. Treat the audit as *direction-finding*, not as a verdict — it tells you which failure stage to attack, not whether the hypothesis holds.

**Selection bias**. You chose these games, probably partly because you expected them to work. Strangers won't. Expect the public numbers to be worse than the audit, and don't treat that gap as regression.

**Familiarity contamination**. Any game you debugged is permanently developer-tested (per CLAUDE.md §2). If most of your library is contaminated, say so — the honest output may be "I have almost no clean generalization data yet," and that itself is the finding that makes the 30-day experiment worth running.

**What would falsify**. Decide before you compute: what zero-tuning working rate, across how many distinct engine/renderer families, would make you doubt the hypothesis? Write it down first.

## Output

A short report answering, in order:

1. How many games, across how many engine/renderer families
2. How many reached a working stereo session with zero tuning
3. Where failures cluster (modal failure stage)
4. Camera detected vs. camera correct
5. How many failures share a signature — i.e. is there one general fix or many specific ones
6. How much of the library is contaminated by your own debugging
7. What this changes about what you build next
