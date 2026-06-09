#!/usr/bin/env bash
set -euo pipefail

source_file=${1:-MicroscopicSpaceCharge.C}

required_names=(
  inter_collision_delta_x
  inter_collision_delta_y
  inter_collision_delta_z
  inter_collision_abs_delta_x
  inter_collision_abs_delta_y
  inter_collision_abs_delta_z
  inter_collision_distance
  inter_collision_signed_displacement_png
  inter_collision_distance_png
)

for name in "${required_names[@]}"; do
  if ! grep -q "$name" "$source_file"; then
    echo "missing inter-collision summary object: $name" >&2
    exit 1
  fi
done

if grep -q "TTree.*inter_collision" "$source_file"; then
  echo "inter-collision values must not be stored in a TTree" >&2
  exit 1
fi

if ! grep -A6 "struct PreviousCollisionState" "$source_file" |
    grep -q "double x = 0\., y = 0\., z = 0\.;"; then
  echo "previous collision state is missing x/y/z coordinates" >&2
  exit 1
fi

echo "Inter-collision displacement summaries are defined without a detail TTree."
