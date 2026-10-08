#!/usr/bin/env bash
# ------------------------------------------------------------------
#  scripts/demo.sh — scripted end-to-end walkthrough
#
#  Feeds a deterministic input script to the app so anyone can see
#  the full workflow (reserve -> deliver -> late return -> penalty
#  -> income report) without typing. Run from the repo root:
#
#      ./scripts/demo.sh          # uses the demo fleet & accounts
# ------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -x ./car_rental ]; then
    echo "Building first..."
    make -s car_rental
fi

# Strip ANSI colors for a clean transcript.
./car_rental | sed 's/\x1b\[[0-9;]*m//g' <<'INPUT'
2
ali
customer123
1
4
Benz C200
5
9
7
2
sara
staff123
3
Benz C200
4
Benz C200
11
8
2
admin
manager123
4
3
INPUT
