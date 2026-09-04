# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]
*Active development changes that have not yet been packaged into an official release.*

### Added
- 

### Changed
- 

### Deprecated
- 

### Removed
- 

### Fixed
- 

### Security
- 

---

## [1.0.0] - 2026-08-17
*Initial release of the system after executing the PSB setup phase for NexVR Engine.*

### Added
- Complete **PSB (Plan, Setup, Build) system** configuration templates for NexVR Engine.
- Core workspace memory file (`claude.md`) with VR injection regression prevention rules.
- High-level engineering roadmap (`project-status.md`) for MVP and milestone tracking.
- System design specs and C++/Electron architecture rules (`architecture.md`).
- Unified team onboarding playbook (`team-onboarding-playbook.md`).

---

## Team & Claude Update Protocol

To keep this ledger accurate and useful for both human developers and AI sessions:

1. **Keep it High-Level:** Unlike raw Git commit logs, the changelog should focus on user-facing value and architectural milestones.
2. **Draft Before Merge:** Ensure the `[Unreleased]` section is updated on your feature branch *before* submitting a Pull Request to `main`.
3. **Tag Releases:** When merging to `main` and deploying, move current `[Unreleased]` bullet points under a new version header with the current date.
