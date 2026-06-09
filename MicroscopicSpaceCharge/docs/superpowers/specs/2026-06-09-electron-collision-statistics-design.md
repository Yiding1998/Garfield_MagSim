# Electron Collision Statistics Design

## Goal

Extend Garfield++ and `MicroscopicSpaceCharge` so every real electron collision can be correlated to a stable electron track and written to ROOT with collision energy, gas/process origin, electric-field impulse, and adjacent-collision mechanical momentum change.

## Garfield++ Interface

Each avalanche electron seed receives a monotonically increasing `trackId` and a `parentTrackId`. The initial electron has no parent. Secondary electrons produced by direct ionisation and Penning transfer receive the current electron track as parent.

The collision callback is extended with the two IDs and the accumulated electric-field impulse since the previous real collision. The impulse is accumulated inside microscopic transport over the same free-flight integration used by Garfield++, so nonuniform external and space-charge fields are included.

## Physics Definitions

Relativistic electron momentum magnitude, in eV/c, is

```text
p(E) = sqrt(E * (E + 2 m_e c^2))
```

with `m_e c^2` expressed in eV. Momentum vectors use the callback direction vectors.

For each free flight:

```text
fieldImpulse = q integral(E dt)
mechanicalDeltaP = p(next collision, before) - p(previous collision, after)
```

Both vector components and magnitudes are stored. The second quantity is measured from collision states and is not labelled as a pure electric-field contribution.

## ROOT Output

`electron_collisions` stores one row per real collision with event and track ancestry, collision index, position/time, collision type/level, gas and process labels, pre/post energies and directions, pre/post momenta, field impulse, free-flight momentum change, elapsed time and distance, plus ionisation/attachment/Penning flags.

`collision_source_summary` stores per gas/process/type counts for all collisions, ionisation-producing collisions and attachment collisions.

Histograms and ROOT-internal PNG images summarize all-collision energies, attachment energies, field impulse, free-flight momentum change, and ionisation/attachment source categories.

Attachment spectra use collision `energyBefore` by default while retaining `energyAfter` in the detailed tree.

Penning-created electrons are identified separately from direct ionisation and attributed to the excitation level reported by Magboltz.

## Validation

Build and test the modified Garfield++ library, rebuild the simulation, run a one-event low-cost case, and verify ROOT branches, row counts, source labels, finite momentum values, and consistency with existing ionisation/attachment totals.
