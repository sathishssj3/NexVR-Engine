# NexVR — Session Start Prompt

Paste this at the beginning of a Claude Code session in the `vr-inject` repo.

It is deliberately short: `CLAUDE.md` already carries the standing rules, invariants, and scope. This prompt only re-establishes *where we are* and *what happens next*.

## The prompt

```text
Resuming NexVR Engine MVP work.

Read CLAUDE.md and docs/VALIDATION_CRITERIA.md first. Everything in them applies — especially the preservation invariants, the evidence rules, and the decision hierarchy. Do not edit either file.

Before proposing anything, establish current state from commands you actually run. Show the command and its output for each:

1. git log --oneline -20 — what landed most recently
2. git status — anything uncommitted or in flight
3. Clean CMake configure + Release build — does it build right now?
4. Full test suite — what passes, what fails, what is skipped

Anything you cannot verify by execution, mark UNVERIFIED. Do not describe the build as working because it compiles, or a subsystem as working because its code exists.

Then produce a short status report:
- Build: pass/fail, with the failing output if it fails
- Tests: passed / failed / skipped, and which failures are new
- In-flight work: what the last few commits were doing and whether it is finished
- MVP gap list: for each of the 12 items in CLAUDE.md §13, mark DONE / PARTIAL / NOT STARTED, with the evidence for each judgement

Known in-flight engineering context (verify against the repo rather than assuming):
DX12 depth discovery — hooking CreateDepthStencilView plus CopyDescriptors / CopyDescriptorsSimple to map CPU descriptor handles to ID3D12Resource*.

Finally, propose exactly ONE next vertical slice. It must be the smallest change that moves a §13 launch-blocking item or a §7 failure state closer to done. Give me:
- the problem in one paragraph
- the proposed change and why it is the smallest one that works
- files affected
- which invariants (§5) it touches
- how you will verify it
- what you are explicitly not doing

Then STOP and wait for my approval. Do not modify production code in this first response.
```

## Notes on using it

**Don't paste this every message**. It is the resume prompt for a fresh session. Mid-session, just describe the next task — `CLAUDE.md` is already loaded and the working agreement in §6 governs each change.

**Update the in-flight context line** as the work moves on. It exists so the agent doesn't spend its first pass rediscovering what you already know; a stale line is worse than none.

**If the build is broken, that is the slice**. Do not let a proposal move past a red build or a failing suite to something more interesting.

**Priority order for choosing the slice**, when several look reasonable:
1. Broken build or newly failing tests
2. A §7 failure state that currently crashes or hangs instead of failing safely
3. Panic hotkey and clean uninstall (§13 items 11–12, launch-blocking)
4. Telemetry and diagnostics — without them the 30-day run teaches you nothing
5. Camera correctness signal (§3) — the highest-value diagnostic you don't yet have
6. Detection reliability improvements
7. Everything else
