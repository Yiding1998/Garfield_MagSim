#!/usr/bin/env bash
set -euo pipefail

source_file=${1:-MicroscopicSpaceCharge.C}

if grep -q 'TTree collisionTree("electron_collisions"' "$source_file"; then
  echo "electron_collisions detail tree is still created" >&2
  exit 1
fi

for branch in energyBefore energyAfter process gasName; do
  if grep -q "collisionTree.Branch(\"${branch}\"" "$source_file"; then
    echo "per-collision branch ${branch} is still written" >&2
    exit 1
  fi
done

for required_object in collision_energy_before collision_energy_after collision_sources_all; do
  if ! grep -q "${required_object}" "$source_file"; then
    echo "missing summary object ${required_object}" >&2
    exit 1
  fi
done

echo "No per-collision detail tree is written, and summary objects are present."
