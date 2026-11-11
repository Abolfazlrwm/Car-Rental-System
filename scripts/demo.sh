#!/usr/bin/env bash
# ------------------------------------------------------------------
#  scripts/demo.sh — scripted end-to-end walkthrough
#
#  Feeds a deterministic input script to the app so anyone can see
#  the full workflow (reserve -> deliver -> late return -> penalty
#  -> income report) without typing. The walkthrough dates are
#  derived from today's date, so the demo always shows a fresh
#  booking with a live late-return penalty. Run from the repo root:
#
#      ./scripts/demo.sh          # uses the demo fleet & accounts
# ------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -x ./car_rental ]; then
    echo "Building first..."
    make -s car_rental
fi

# Walkthrough dates relative to today: book the Benz for
# today..today+4 (5 billed days) and return it two days past the
# due date, so the 1.5x penalty branch is always part of the story.
plus_days() {
    if date -d "+$1 days" +%F >/dev/null 2>&1; then
        date -d "+$1 days" +%F          # GNU coreutils
    elif date -v+"$1"d +%F >/dev/null 2>&1; then
        date -v+"$1"d +%F               # BSD / macOS
    else
        echo "demo.sh: no supported date arithmetic on this host" >&2
        return 1
    fi
}
START=$(date +%F)
END=$(plus_days 4)
LATE=$(plus_days 6)

# Strip ANSI colors for a clean transcript.
# NOTE: the heredoc must feed car_rental (the left side of the pipe);
# attaching it to sed instead would bypass the app entirely and make
# the pipeline die with SIGPIPE once sed finishes with the input.
# The heredoc tag is unquoted so $START/$END/$LATE expand, and the
# pressed numbers match the CURRENT menus (customer logout = 12,
# manager income report = 9, exit = 16). The 0 after $END answers the
# v3.0.0 recurrence prompt (one-off booking, no series).
./car_rental <<INPUT | sed 's/\x1b\[[0-9;]*m//g'
2
ali
customer123
4
Benz C200
$START
$END
0
3
12
2
sara
staff123
3
Benz C200
4
Benz C200
$LATE
9
2
admin
manager123
9
16
INPUT
