# Long-Term Codex Handoff Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give future Codex sessions a stable, quickly discoverable record of project architecture, physics, decisions, Git history, current workspace state, and continuation procedures.

**Architecture:** Add a short repository-root `AGENTS.md` that future agents automatically discover and a detailed `docs/CODEX_HANDOFF.md` as the canonical continuity record. Keep user-facing physics documentation in `README.md` and link existing specs, plans, and analyses instead of duplicating them blindly.

**Tech Stack:** Markdown, Git, C++/Garfield++/ROOT project metadata.

---

### Task 1: Automatic entry point

**Files:**
- Create: `AGENTS.md`

- [ ] Direct future Codex sessions to read `docs/CODEX_HANDOFF.md`, `README.md`, and `git status` before editing.
- [ ] State that dirty-worktree changes belong to the user unless proven otherwise.
- [ ] Record required validation and Git hygiene.

### Task 2: Canonical handoff

**Files:**
- Create: `docs/CODEX_HANDOFF.md`

- [ ] Summarize purpose, dependencies, local Garfield++ patch, architecture, algorithms, command line, ROOT outputs, storage decisions, physics caveats, tests, backups, and key documents.
- [ ] Record the dated Git timeline and recovery tag.
- [ ] Mark the current uncommitted limits and 10/80/10 gas composition as experimental workspace state, not committed baseline.
- [ ] Add a future-session startup checklist and maintenance rules.

### Task 3: Verification and version control

- [ ] Verify all referenced local files and key commits exist.
- [ ] Verify the handoff explicitly distinguishes HEAD from uncommitted source state.
- [ ] Stage only `AGENTS.md`, `docs/CODEX_HANDOFF.md`, and this plan; leave `MicroscopicSpaceCharge.C` unstaged.
- [ ] Commit and push documentation to `origin/master`.
