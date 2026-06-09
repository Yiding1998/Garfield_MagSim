# Inter-Collision Electron Displacement Statistics Design

## Goal

Statistically measure electron displacement between consecutive real collisions without storing per-collision rows in a TTree.

## Physical Definition

For consecutive collisions of the same stable electron `trackId`, at positions `r_prev` and `r_current`:

- Signed components: `dx = x_current - x_prev`, `dy`, `dz`.
- Absolute components: `abs(dx)`, `abs(dy)`, `abs(dz)`.
- Total displacement: `dr = sqrt(dx^2 + dy^2 + dz^2)`.

All values use cm. The total is the chord between collision points, not the curved trajectory arc length. A track first collision only initializes state and is not filled.

## Architecture

Extend `PreviousCollisionState` with the previous collision position. Reuse the map keyed by stable `trackId`; it is already cleared at each event boundary. In `userHandleCollision`, calculate all seven quantities before replacing the previous state.

Add seven extendable `TH1D` objects to `CollisionHistograms`:

- `inter_collision_delta_x/y/z`
- `inter_collision_abs_delta_x/y/z`
- `inter_collision_distance`

No TTree or per-flight vector is introduced. Histograms are filled online, written to ROOT, and shown in two canvases saved as ROOT canvas objects and embedded PNG objects.

## Output

- `inter_collision_signed_displacement` and `_png`: signed X/Y/Z distributions.
- `inter_collision_distance` canvas and `_png`: absolute X/Y/Z and total displacement distributions.
- The seven component histograms are also written individually.

## Verification

A static regression test checks object names and absence of any distance-detail TTree. Build the program, run one small event, then inspect the ROOT file to verify all seven histograms and both PNG objects exist.
