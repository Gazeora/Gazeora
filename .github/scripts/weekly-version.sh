#!/usr/bin/env bash
# Reads release tags (one a line) on stdin and prints the highest release and the weekly
# pre-release that comes after it: the next patch, as rc.1. For example v1.0.1-rc.1 gives
# "v1.0.1-rc.1 v1.0.2-rc.1", and a stable v1.0.3 gives "v1.0.3 v1.0.4-rc.1". Tags that
# aren't vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-rc.N are skipped; no release at all fails.
set -euo pipefail

highest=$(awk '/^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[0-9]+)?$/ {
    split(substr($0, 2), part, /[.-]/)  # 1.0.1-rc.2 -> 1, 0, 1, rc, 2
    stable = part[4] == "" ? 1 : 0      # a release sorts above its candidates
    printf "%d %d %d %d %d %s\n", part[1], part[2], part[3], stable, part[5], $0
}' | sort -k1,1n -k2,2n -k3,3n -k4,4n -k5,5n | tail -n 1)

if [ -z "$highest" ]; then
    echo "no release to follow: publish the first one by hand" >&2
    exit 1
fi
read -r major minor patch _ _ tag <<< "$highest"
echo "$tag v$major.$minor.$((patch + 1))-rc.1"
