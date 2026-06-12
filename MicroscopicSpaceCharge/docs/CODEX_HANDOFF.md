# Codex Long-Term Project Handoff

Last reconstructed from conversation and repository state: **2026-06-12**.

This is the canonical continuity document for future Codex sessions. It records project purpose, architecture, physics decisions, modifications, Git history, current workspace state, and the procedure for resuming work without old chat context.

## 1. Resume Here

At the start of a future session, run:

```bash
pwd
git status --short
git log --oneline --decorate -10
git diff -- MicroscopicSpaceCharge.C
```

Then read this file, the relevant `README.md` sections, applicable files under `docs/`, and the named functions in `MicroscopicSpaceCharge.C`.

Never assume a dirty worktree is disposable. Existing changes may be an active user experiment.

## 2. Repository Identity

- Project: `/ustcfs/STCFUser/yzhao/Simulation/Garfield/MySim/Magnetic/MicroscopicSpaceCharge`
- Remote: `https://github.com/Yiding1998/Garfield_MagSim.git`
- Main branch: `master`
- Main source: `MicroscopicSpaceCharge.C`
- Build definition: `CMakeLists.txt`
- User documentation: `README.md`
- Build directories and ROOT outputs are ignored by Git.

At handoff creation, the committed source baseline was `37c2f0d`. The later handoff commit changes documentation only.

## 3. Project Purpose

The program simulates microscopic electron avalanches in a parallel-plate gas gap with Garfield++ and ROOT. It compares behavior versus event count, gap, pressure, voltage, Y magnetic field, space charge, and source-edited gas composition.

It follows electrons microscopically, creates and drifts positive and negative ions, updates a charged-ring space-charge approximation between time frames, and records ionisation, attachment, collection, gain, diffusion, endpoints, collision physics, and molecular source channels.

## 4. Environment

Known working dependencies from `build_RPCgas_v2/CMakeCache.txt`:

- Garfield++ 2025.12 install: `/home/yzhao/software/garfieldpp/install-2025.12`
- Garfield++ source: `/home/yzhao/software/garfieldpp/src-2025.12`
- Garfield CMake config: `/home/yzhao/software/garfieldpp/install-2025.12/lib64/cmake/Garfield`
- ROOT 6.26.16: `/software/STCF/OSCAR/2.6.0/ExternalLibs/ROOT/6.26.16`

Typical build:

```bash
cmake -S . -B build_RPCgas_v2 -DGarfield_DIR=/home/yzhao/software/garfieldpp/install-2025.12/lib64/cmake/Garfield
cmake --build build_RPCgas_v2 -j2
```

Build directory names are generated study artifacts, not source versions.

## 5. Required Garfield++ Patch

The program depends on a locally modified Garfield++ collision callback. Stock Garfield++ 2025.12 is insufficient.

Repository patch copies:

- `.garfield-patch/Include/Garfield/AvalancheMicroscopic.hh`
- `.garfield-patch/Include/Garfield/AvalancheMicroscopicTypes.hh`
- `.garfield-patch/Source/AvalancheMicroscopic.cc`

Original backups: `/home/yzhao/software/garfieldpp/backups/2026-06-09-collision-statistics`.

The patch provides stable `trackId`, `parentTrackId`, identity across `ResumeAvalanche()` windows, pre/post collision directions, and accumulated electric-field impulse across real collisions and time windows.

When upgrading Garfield++: diff against upstream, deliberately reapply these concepts, rebuild/install Garfield++, reconfigure this project, and compile before changing simulation logic. Never blindly overwrite a newer version with old patch files.

## 6. Command Line and Output Name

```bash
./build_RPCgas_v2/MicroscopicSpaceCharge [nEvents] [gapUm] [pressureAtm] [voltageAbs] [magFieldY] [enableSpaceCharge]
```

Example:

```bash
./build_RPCgas_v2/MicroscopicSpaceCharge 3 215 1 600 0.5 1
```

Committed defaults: 3 events, 215 micrometres, 1 atm, 750 V magnitude, 0 T, space charge enabled. The top plane voltage is made negative internally.

Output format:

```text
<gap>um_<pressure>atm_<voltage>V_<B>T_<events>e_SC<0|1>.root
```

## 7. Geometry and Coordinates

- Bottom plane: `y=0`, 0 V.
- Top plane: `y=gap`, negative voltage.
- Garfield++ length unit: cm; 1 micrometre is `1e-4 cm`.
- Initial electron: `x=0`, `z=0`, `y=top-0.1 micrometre`, energy 0.1 eV.
- Electrons drift toward the bottom plane.
- Magnetic field is `(0, magFieldY, 0)`.

## 8. Source Architecture

Important functions:

- `writeCanvasWithPngImage`: writes a `TCanvas` plus embedded PNG `TASImage`; batch mode suppresses GUI display.
- `get_electron_spread`: active-electron means and RMS widths.
- `initialiseCollisionHistograms`: online extendable collision histograms.
- `getCollisionSource`: maps Magboltz levels to gas, process, and threshold.
- `userHandleCollision`: collision energy/source, impulse, momentum, position, and consecutive-collision displacement statistics.
- `userHandleIonisation`: counts produced electrons, records molecular source, creates positive ions.
- `userHandleAttachment`: counts attachments and creates negative ions.
- `buildBinnedSpaceCharge`: represents electrons, positive ions, and negative ions using Y-binned charged rings.
- `writeCollisionStatistics`: writes compact summaries, canvases, and PNG objects.
- `main`: arguments, gas, fields, adaptive time stepping, events, endpoints, fits, diffusion, and ROOT output.

Global state includes avalanche/drift objects, charged rings, current event and counts, online histograms, source maps, electron birth sources, and previous collision state keyed by stable track ID.

## 9. Time Evolution and Space Charge

Each event advances through time windows using electron and ion `ResumeAvalanche()` calls. Every frame inspects active particles, rebuilds space charge, adapts the next time step, resumes transport, and records diffusion.

Confirmed user decisions:

- electrons, positive ions, and negative ions remain space-charge sources;
- Y bins use fixed physical width and gap-dependent count;
- current bin width is `2e-4 cm`, or 2 micrometres;
- per-frame evolution remains available;
- plots are never displayed interactively.

## 10. Gain and Limits

Two different limits exist:

- Garfield++ `avalancheSizeLimit`, controlling concurrent avalanche size;
- program `maxTotalIonisations`, stopping on cumulative callbacks.

The program defines `gain = 1 + totalIonisationElectrons`. This is gross cumulative ionisation, not collected-electron gain. A concurrent size limit does not necessarily cap cumulative callbacks. For detector effective gain, prefer collected electrons or induced charge.

An earlier discussion of a size limit of 300 and gain above 300 was resolved by this distinction. Later studies intentionally raised limits.


## 11. Collision Storage Decision

The original `electron_collisions` TTree stored every real collision. A 33,432,061-row test file was about 786 MiB; the tree was 99.84 percent of the file. Largest branches were `energyBefore` 30.92 percent, `energyAfter` 30.91 percent, `process` 16.72 percent, and `gasName` 10.10 percent.

Decision: never reintroduce per-collision detail storage without explicit approval. Commit `faf682b` removed the complete tree and collision-record vector.

Current online summaries include all collision energies, attachment energies, energy by collision type, field impulse, free-flight momentum change, collision positions/times, consecutive-collision displacement, and process source counts.

Remaining per-electron trees that may grow are `electron_birth_sources` and `electron_endpoints`.

## 12. Consecutive-Collision Definitions

For consecutive real collisions of the same stable track:

```text
dx = x_current - x_previous
dy = y_current - y_previous
dz = z_current - z_previous
dr = sqrt(dx^2 + dy^2 + dz^2)
```

Signed and absolute X/Y/Z components and total distance are filled online. The first collision initializes state. The map is cleared per event. `dr` is a straight chord, not the exact curved arc length.

Relativistic momentum:

```text
p(E) = sqrt(E * (E + 2 m_e c^2))
m_e c^2 = 510998.95 eV
```

At zero magnetic field, mechanical momentum change and field impulse should be close; this was observed in the 1900 V analysis.

## 13. Diffusion and Endpoints

`electron_diffusion` stores event, frame, time, active count, means, sigma X/Y/Z, and `sigmaT = sqrt((sigmaX^2 + sigmaZ^2)/2)`.

Per-event graphs are under `diffusion_event_graphs/`, not ROOT top level.

All terminated endpoints are stored in `electron_endpoints`. Collected electrons near the bottom plane populate X/Y/Z/time histograms. Fits use plus/minus three histogram RMS values. Fit parameters and status are in `endpoint_fit_summary`; annotations were moved to upper-left to avoid legends.

Caveats:

- endpoint Y is pinned to the electrode and is not a physical Gaussian diffusion distribution;
- failed fit status overrides any residual TF1 parameters;
- late diffusion decrease with few survivors is boundary/survivor bias, not reverse diffusion;
- one-event transverse centroid shifts are fluctuations, not systematic drift.

## 14. ROOT Output Map

Run/event:

- `run_summary`, `avalanche_events`

Collision and sources:

- `collision_energy_before/after`
- `attachment_energy_before/after`
- six `collision_energy_<type>` histograms
- `field_impulse_magnitude`, `free_flight_delta_p_magnitude`
- `collision_position_x/y/z`, `collision_time`
- `collision_source_summary`, `collision_sources_all`
- `ionisation_electron_sources`, `attachment_sources`
- `electron_birth_sources`

Displacement:

- `inter_collision_delta_x/y/z`
- `inter_collision_abs_delta_x/y/z`
- `inter_collision_distance`

Endpoints:

- `electron_endpoints`
- `endpoint_x/y/z`, `endpoint_time`
- `fit_endpoint_x/y/z/time`
- `endpoint_fit_summary`

Diffusion:

- `electron_diffusion`
- `diffusion_event_graphs/diffusion_sigma_t_event_<event>`
- `diffusion_graph`

The program also stores canvases and matching `*_png` `TASImage` objects for collision, source, displacement, endpoint, diffusion, and field views. `gROOT->SetBatch(kTRUE)` must remain enabled.

## 15. Gas and Pressure Physics

`MediumMagboltz` uses 293.15 K, pressure `760 * pressureAtm` Torr, maximum electron energy 5000 eV, and Penning transfer when supported.

At fixed voltage and gap, reducing pressure increases reduced field and the high-energy tail:

```text
E/N proportional to V / (gap * pressure)
```

To keep electron energies similar, scale voltage approximately with pressure. Molecular coolants such as C2H2F4, CO2, or iC4H10 can drain energy through rotational/vibrational channels. More SF6 mainly removes selected electrons and is not equivalent to cooling the survivor distribution.

SF6 attachment interpretation:

- the low-energy structure comes from SF6 channels;
- the approximately 5.2 eV peak is mainly `e + SF6 -> SF5 + F-` dissociative attachment;
- attachment spectrum is electron distribution times velocity times cross section, not the cross section alone.

## 16. Known 1900 V Analysis

Full report: `docs/analysis/2026-06-10-215um-1atm-1900V-SC1-root-physics-analysis.md`.

Analyzed file: `build_RPCgas_90_5_5/215um_1.00atm_1900V_0.00T_1e_SC1.root`.

Key numbers:

- 661,180 ionisations;
- 46,103 attachments;
- 234,257 collected electrons;
- about 423 million real collisions;
- SF6 caused about 98.5 percent of attachments;
- transverse endpoint Gaussian core width about 15.3 micrometres;
- endpoint Y Gaussian fit failed correctly because of boundary pinning;
- late diffusion decrease came from a shrinking survivor population.

## 17. Tests

```bash
bash tests/check_no_collision_detail_tree.sh
bash tests/check_inter_collision_displacement.sh
cmake --build build_RPCgas_v2 -j2
```

For behavior/output changes, run a small event, then inspect ROOT keys and entries in batch mode. A previous quick run was:

```bash
./build_RPCgas_v2/MicroscopicSpaceCharge 1 50 1 100 0 0
```

Verify required keys, nonzero entries where expected, and absence of forbidden detail trees.

## 18. Backups and Recovery

Comparison-only backups:

- `backups/MicroscopicSpaceCharge.C.before_optimization`
- `backups/CMakeLists.txt.before_optimization`

Recovery tag:

```text
before-remove-collision-tree-20260609 -> 059d166
```

Useful Git commands:

```bash
git show <commit>
git diff before-remove-collision-tree-20260609..HEAD
git log -p -- MicroscopicSpaceCharge.C
```

Never overwrite current source with backups.


## 19. Git Timeline

| Date | Commit | Meaning |
|---|---|---|
| 2026-06-09 | `3cb26d7` | Initial simulation, docs, backups, and Garfield++ patch |
| 2026-06-09 | `7a42c40` | CMake permission normalization |
| 2026-06-09 | `ed30bbc` | Per-event diffusion graphs moved into ROOT subdirectory |
| 2026-06-09 | `059d166` | First per-collision storage reduction |
| 2026-06-09 | `faf682b` | Complete `electron_collisions` removal and online summaries |
| 2026-06-09 | `57cdee5` | Committed avalanche-size limit increased to 50,000 |
| 2026-06-09 | `b660408` | Inter-collision displacement design |
| 2026-06-09 | `39c7d72` | Inter-collision displacement plan |
| 2026-06-09 | `7e7bf67` | Signed, absolute, total displacement implementation |
| 2026-06-10 | `a41e7b9` | Full 1900 V ROOT physics analysis |
| 2026-06-10 | `37c2f0d` | 5.2 eV SF6 attachment peak explanation |

The tag `before-remove-collision-tree-20260609` preserves the state before full detail-tree removal.

## 20. Current Workspace State: Uncommitted Experimental Configuration

**Critical: as of 2026-06-12, `MicroscopicSpaceCharge.C` has user-owned, uncommitted experimental changes. They are not part of the committed source baseline. Do not revert them and do not silently include them in unrelated commits.**

Dirty changes:

```text
avalancheSizeLimit: committed 50000 -> experiment 100000, written 1e5
maxTotalIonisations: committed 500000 -> experiment 1000000, written 1e6
active gas composition:
  committed C2H2F4/iC4H10/SF6 = 60/30/10
  experimental C2H2F4/iC4H10/SF6 = 10/80/10
```

Uncommitted comments also add:

- example `./MicroscopicSpaceCharge 1 1000 0.2 1600 0`;
- candidate 90/5/5 mixture;
- candidate 70/5/5/20 mixtures with CO2, N2, or H2O.

Always inspect:

```bash
git diff -- MicroscopicSpaceCharge.C
```

If a task intersects gas composition or limits, ask whether to commit, alter, or preserve this experiment. For unrelated work, stage only task files.

Historical untracked parent file: `../RaedMe.txt`. Ignore unless requested.

## 21. Known Limitations and Risk Areas

1. Gas composition is hard-coded and absent from `run_summary` and filenames, making later ROOT interpretation ambiguous.
2. `gain` is gross ionisation, not effective collected gain.
3. `electron_birth_sources` and `electron_endpoints` can dominate large files.
4. Endpoint Y Gaussian fitting is not physically meaningful at a collection plane.
5. Charged-ring space charge is a Y-binned approximation.
6. Electric field is stored as a canvas/image, not reusable numeric grid data.
7. One event cannot establish stable centroid or gain distributions.
8. Ion mobility currently loads `IonMobility_Ar+_Ar.txt` even for fluorocarbon mixtures; this is an approximation.
9. Penning transfer can be unsupported for a mixture; inspect runtime output.
10. Large limits can make callbacks and endpoint storage expensive even without a collision tree.

## 22. Recommended Future Improvements

1. Make gas composition configurable and store component names/fractions in ROOT.
2. Store nominal field, reduced field, temperature, and mixture in `run_summary`.
3. Add a flag/counter for Garfield++ avalanche-size limiting.
4. Define effective gain separately from gross ionisation gain.
5. Save numeric electric-field profiles for space-charge studies.
6. Add optional histogram-only or sampled endpoint/birth storage.
7. Add automated ROOT schema tests.
8. Validate ion mobility data for each mixture.

## 23. Documentation Map

- `README.md`: detailed user-facing program and physics reference.
- `docs/CODEX_HANDOFF.md`: canonical long-term developer continuity record.
- `docs/analysis/2026-06-10-215um-1atm-1900V-SC1-root-physics-analysis.md`: object-by-object ROOT analysis.
- `docs/superpowers/specs/2026-06-09-electron-collision-statistics-design.md`: collision design.
- `docs/superpowers/plans/2026-06-09-electron-collision-statistics.md`: collision implementation record.
- `docs/superpowers/specs/2026-06-09-inter-collision-displacement-design.md`: displacement design.
- `docs/superpowers/plans/2026-06-09-inter-collision-displacement.md`: displacement plan.
- `backups/`: comparison-only pre-optimization copies.
- `.garfield-patch/`: required custom Garfield++ implementation.

## 24. Future Change Procedure

For substantial changes:

1. Read this file and inspect Git state.
2. Identify whether dirty changes intersect the task.
3. Create a baseline commit/tag only for changes the user wants recorded.
4. Confirm physics definition: quantity, units, population, event aggregation, signed/absolute/cumulative meaning.
5. Prefer online histograms and compact summaries over per-interaction trees.
6. Add a regression check before changing behavior where practical.
7. Compile and run a small event.
8. Inspect generated ROOT output, not only console text.
9. Update `README.md` for user-facing behavior and this file for architecture/history.
10. Commit focused files and push only after fresh verification.

When maintaining this handoff, keep three states separate:

- committed behavior at `HEAD`;
- current uncommitted experiments;
- proposed future work.
