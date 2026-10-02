# NexVR Engine — MVP Scope (v0.1.0)

**Status: FROZEN at first public release.** Changes before release require deleting something else of equal size. Changes after release wait for v0.2.0.

This document defines what ships. `CLAUDE.md` defines how work is done. `docs/VALIDATION_CRITERIA.md` defines how the release is judged.

## 1. What v0.1.0 is

> A signed Windows installer that lets a stranger point NexVR at a single-player PC game they own and either get a working stereoscopic OpenXR session, or a clear reason why not.

That is the entire product. Every feature below exists to serve that sentence or to record what happened.

## 2. In scope — with definition of done

Each item is done only when its verification column passes. "Implemented" is not done.

| # | Item | Done when |
|---|---|---|
| 1 | Process injection | Attaches to a running game and reports a terminal state from §7 of CLAUDE.md, every time, across at least 5 distinct titles |
| 2 | Renderer detection | Correctly identifies the backend, or returns `RENDERER_NOT_DETECTED` — never guesses |
| 3 | Swapchain + frame interception | Frames are intercepted for the full session without leaking or deadlocking on exit |
| 4 | Camera / matrix discovery | Produces a ranked candidate set and a decision, with the decision and its confidence written to the log |
| 5 | Camera correctness signal | The session records whether the accepted candidate produced a usable view — see CLAUDE.md §3 |
| 6 | Stereo rendering | Both eyes render with non-divergent, bounded relative convergence (depth ordering correct, no inverted depth) |
| 7 | OpenXR session + submission | Session starts, frames submit, session ends cleanly on game exit and on panic |
| 8 | Panic hotkey | Unhooks and returns the game to flat rendering without killing the process, from any state |
| 9 | Clean uninstall | Removes binaries, config, and logs; leaves no injected state behind |
| 10 | Failure state machine | Every terminal state in CLAUDE.md §7 is reachable and tested, each with a human-readable reason |
| 11 | Diagnostic log | User can find it, read it, and attach it to a bug report without instruction |
| 12 | Opt-in telemetry | Schema per CLAUDE.md §8; disabled means zero network calls, verified by packet capture |
| 13 | Launcher | Select game, launch, show status, show failure reason, toggle telemetry. Nothing more |
| 14 | Signed binary | Consistent publisher identity, timestamped, submitted to Microsoft's Security Intelligence portal |
| 15 | Documentation | Install steps, supported-title list, known limits, anti-cheat warning, SmartScreen explanation, bug reporting path |
| 16 | Demo video | 30–60s, real headset, real game, real head movement, no mock UI |

## 3. Explicitly out of scope

Named here so they cannot creep back in. Each is a legitimate future feature and none of them belongs in v0.1.0.

- Every model in the deferred ML roadmap (CLAUDE.md §10)
- HUD extraction and world-space UI
- Motion controllers, haptics, hand presence, physical interaction
- Locomotion systems and comfort options beyond what already exists
- Vehicle, cutscene, and scripted-camera handling
- Depth reconstruction and disocclusion repair
- Performance optimization work not required to reach a stable session
- Multiplayer, anti-cheat titles, DRM-protected processes
- Cloud services, accounts, compatibility website, SEO database
- Launcher polish beyond the five functions in item 13
- Any additional graphics backend not decided in §4

If a task does not map to a numbered item in §2, it is out of scope. Write it in `docs/BACKLOG.md` and move on.

## 4. The one decision to freeze now: backend coverage

The 90-day plan says DX12-only, deferring Vulkan. That leaves DX11 unresolved, and it is a real choice with real consequences:

- **Ship DX11 as well** if the existing DX11 path passes the same §2 bar with no extra work. Most older and back-catalog titles are DX11, which is where the *diversity* of previously untested games lives — and diversity is what the hypothesis is actually being tested on.
- **Ship DX12 only** if DX11 has drifted and would need its own hardening pass. A narrower release that works beats a wider one that produces confusing failures.

Vulkan is out either way — smallest install base, largest remaining risk.

Decide this before writing more code, record it in one line here, and do not revisit it during the experiment:

> Backends shipping in v0.1.0: DX11 + DX12

## 5. Release sequence

Dependency order, not a schedule. Do not skip ahead.

1. **Green build, green suite**. Nothing else starts until both are true.
2. **Failure states complete**. Every state in CLAUDE.md §7 reachable, tested, and safe. A crash is not a failure state.
3. **Panic hotkey and clean uninstall**. Launch-blocking, and easier to build before the surrounding code hardens.
4. **Camera correctness signal**. Without it the 30-day run cannot distinguish "found the wrong camera" from "found no camera" — the single most valuable thing it could tell you.
5. **Telemetry + diagnostic logs**. Without these the experiment produces anecdotes.
6. **Internal evidence audit** (`docs/EVIDENCE_AUDIT.md`) against the current build.
7. **Signing, submission, documentation**. Start signing early — hash reputation takes time to accrue and every rebuild resets it.
8. **Demo video**.
9. **Closed test** — 3–5 people who are not you, on machines that are not yours.
10. **Public release**.

## 6. Ready-to-ship checklist

Do not post until every line is true:

- [ ] Clean Release build from a fresh clone
- [ ] Full suite green, including failure-path and mutation tests
- [ ] All 16 items in §2 meet their definition of done
- [ ] Backend decision in §4 recorded
- [ ] Installed and run successfully on a machine that has never had the dev environment
- [ ] At least one person who is not you completed install → session without help
- [ ] Telemetry-off verified by packet capture
- [ ] Binary signed, timestamped, submitted to Microsoft
- [ ] SmartScreen warning documented before users hit it
- [ ] `docs/VALIDATION_CRITERIA.md` unchanged since it was locked
- [ ] Demo video recorded

## 7. Freeze rules

- Scope is frozen at first public release. Bugs get fixed; features wait.
- During the 30 days, ship fixes for failure-path and crash bugs only. New detection heuristics change the thing being measured mid-measurement.
- Every release during the experiment gets a version tag, and telemetry records it — so a mid-experiment fix can be separated out during analysis instead of silently contaminating the result.
