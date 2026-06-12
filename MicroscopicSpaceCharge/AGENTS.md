# Codex Project Instructions

This repository has a long-running simulation history and a locally patched Garfield++ dependency.

Before editing code:

1. Read `docs/CODEX_HANDOFF.md` completely.
2. Read the relevant sections of `README.md`.
3. Run `git status --short` and `git log --oneline --decorate -10`.
4. Treat every pre-existing uncommitted change as user-owned. Do not revert, reformat, stage, or commit it unless explicitly requested.
5. Check whether the task depends on the custom Garfield++ files under `.garfield-patch/`.

Project rules:

- Main source: `MicroscopicSpaceCharge.C`.
- Preserve online histogram aggregation; do not reintroduce a per-collision detail TTree without explicit approval.
- Generated plots must remain batch-only, be written to ROOT, and have embedded PNG objects. Do not display GUI windows.
- Per-event diffusion graphs belong under `diffusion_event_graphs/`, not the ROOT top-level directory.
- Keep electron and ion space charge enabled as sources when the space-charge option is on.
- Y-space-charge bins use fixed physical width and a gap-dependent bin count.
- Use Git to checkpoint the baseline before substantial changes and commit the completed change separately.
- Stage only files related to the current task. The parent-directory `RaedMe.txt` has historically been untracked and must be ignored unless the user asks otherwise.

Minimum validation after source changes:

```bash
bash tests/check_no_collision_detail_tree.sh
bash tests/check_inter_collision_displacement.sh
cmake --build build_RPCgas_v2 -j2
```

For behavioral/output changes, also run a small simulation and inspect the produced ROOT keys and entries with ROOT in batch mode.

After completing a meaningful change, update `docs/CODEX_HANDOFF.md` sections "Current Workspace State" and "Git Timeline" if they became stale.
