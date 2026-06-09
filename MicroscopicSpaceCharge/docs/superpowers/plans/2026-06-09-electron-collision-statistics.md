# Electron Collision Statistics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Record complete, track-correlated electron collision energy, momentum, attachment, and molecular-origin statistics in the simulation ROOT output.

**Architecture:** Add stable track ancestry and per-free-flight electric impulse to Garfield++ microscopic transport, expose them through the collision callback, then consume that callback in the simulation. Buffer compact collision records during each event and write detailed TTrees plus summary histograms after transport.

**Tech Stack:** C++17, Garfield++ 2025.12, ROOT, CMake

---

### Task 1: Back Up and Extend Garfield++ Track Metadata

**Files:**
- Modify: `/home/yzhao/software/garfieldpp/src-2025.12/Include/Garfield/AvalancheMicroscopicTypes.hh`
- Modify: `/home/yzhao/software/garfieldpp/src-2025.12/Include/Garfield/AvalancheMicroscopic.hh`
- Modify: `/home/yzhao/software/garfieldpp/src-2025.12/Source/AvalancheMicroscopic.cc`

- [x] Copy the three original files to a dated backup directory.
- [x] Add `trackId` and `parentTrackId` to microscopic seeds/electrons.
- [x] Assign unique IDs to initial and secondary electrons while preserving parentage.
- [x] Extend the collision callback signature with IDs and accumulated field impulse.
- [x] Compile Garfield++ and resolve all callback call sites.

### Task 2: Accumulate Electric-Field Impulse

**Files:**
- Modify: `/home/yzhao/software/garfieldpp/src-2025.12/Source/AvalancheMicroscopic.cc`

- [x] Locate every microscopic transport backend that invokes `CallUserHandles`.
- [x] Accumulate electron electric impulse over each accepted free-flight integration step.
- [x] Reset the accumulator after each real collision and preserve it over null collisions.
- [x] Pass impulse components to the callback in eV/c.
- [x] Rebuild Garfield++ and run microscopic avalanche integration tests.

### Task 3: Add Simulation Collision Records

**Files:**
- Modify: `MicroscopicSpaceCharge.C`

- [x] Add collision metadata lookup using `MediumMagboltz::GetLevel` and `Medium::GetComponent`.
- [x] Add relativistic energy-to-momentum conversion using electron rest energy.
- [x] Implement the extended collision callback and per-track previous-collision state.
- [x] Record energies, directions, momenta, impulses, time intervals, path distances, collision source and process.
- [x] Correlate ionisation and attachment callbacks with the collision record without double counting.

### Task 4: Write ROOT Trees and Histograms

**Files:**
- Modify: `MicroscopicSpaceCharge.C`

- [x] Write `electron_collisions` with one row per real collision.
- [x] Write `collision_source_summary` grouped by gas/process/type.
- [x] Add all-collision and attachment pre/post energy histograms.
- [x] Add field-impulse and free-flight mechanical momentum-change histograms.
- [x] Add labelled ionisation and attachment source histograms.
- [x] Save canvases and PNG `TImage` objects only in the ROOT file.

### Task 5: Verification and Documentation

**Files:**
- Modify: `README.md`

- [x] Rebuild Garfield++ and `MicroscopicSpaceCharge` from affected targets.
- [x] Run one small event with space charge disabled, then one with it enabled.
- [x] Scan all new ROOT branches and verify source labels match the configured gas components.
- [x] Compare attachment and direct-ionisation collision counts with existing event totals, documenting Penning handling.
- [x] Document definitions, units, ROOT schemas, performance cost and analysis examples in `README.md`.
