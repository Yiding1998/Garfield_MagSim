# Inter-Collision Electron Displacement Statistics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add signed, absolute, and total electron displacement distributions between consecutive real collisions without per-collision TTree storage.

**Architecture:** Extend the existing per-track previous-collision state with position, and fill seven extendable ROOT histograms directly in `userHandleCollision`. Write individual histograms plus two canvases and embedded PNG objects using the existing output helper.

**Tech Stack:** C++17, Garfield++, ROOT TH1D/TCanvas/TImage, Bash regression checks, CMake, Git.

---

### Task 1: Add regression requirements

**Files:**
- Create: `tests/check_inter_collision_displacement.sh`

- [ ] Add a static test requiring all seven histogram names, both PNG object names, previous-position state, and no displacement TTree.
- [ ] Run it against current source and verify it fails because `inter_collision_delta_x` is absent.

### Task 2: Implement online displacement statistics

**Files:**
- Modify: `MicroscopicSpaceCharge.C`

- [ ] Extend `PreviousCollisionState` with `x`, `y`, and `z`.
- [ ] Add and initialize signed, absolute, and total displacement histograms.
- [ ] For an existing `trackId`, calculate `dx`, `dy`, `dz`, absolute components, and Euclidean displacement, then fill all seven histograms.
- [ ] Update previous position and momentum together after each collision.
- [ ] Write all seven histograms and produce signed and absolute/total canvases with embedded PNG objects.
- [ ] Run both static regression scripts and compile.

### Task 3: Document and validate ROOT output

**Files:**
- Modify: `README.md`

- [ ] Document definitions, units, first-collision handling, chord-distance limitation, ROOT names, and analysis examples.
- [ ] Run one small event and inspect the ROOT file for seven histograms, two canvases, two PNG objects, and absence of a displacement TTree.
- [ ] Run `git diff --check`, commit implementation, and push to `origin/master`.
