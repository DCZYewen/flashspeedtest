#!/bin/bash
#
# flashspeedtest — integration test suite
#
# Runs filesystem-mode tests in a tmpdir (no root required),
# then raw block-device tests on a loop device (needs sudo).
# Cleanup is guaranteed via trap on EXIT regardless of pass/fail.
#
# Usage:  bash test.sh        (or: make test)
#

set -e

# ---- config ----
BINARY="./flashspeedtest"       # path to the binary under test
IMG="/tmp/flashspeedtest_test.img"   # temporary image for loop device
MNT="/tmp/flashspeedtest_mnt"       # temporary mount point for FS tests
LOOP=""                         # will hold /dev/loopN after losetup
FAILED=0
SKIPPED=0

# ---- cleanup (runs on EXIT no matter what) ----
cleanup() {
    set +e
    if [ -n "$LOOP" ]; then
        umount "$MNT" 2>/dev/null
        losetup -d "$LOOP" 2>/dev/null
    fi
    rmdir "$MNT" 2>/dev/null
    rm -f "$IMG"
}
trap cleanup EXIT

# ---- helpers ----
pass() { echo "  PASS: $1"; }
fail() { echo "  FAIL: $1"; FAILED=1; }
skip() { echo "  SKIP: $1 (no root)"; SKIPPED=$((SKIPPED + 1)); }

# check if we are currently running as root (EUID == 0)
check_root() {
    [ "$(id -u)" -eq 0 ]
}

echo "=== flashspeedtest test suite ==="
echo

# ============================================================
# [1] FS mode — write/read on a real filesystem (safe, no root)
# ============================================================
echo "[1] FS mode (tmpdir)"
mkdir -p "$MNT"

# default block size, small total — basic write+read cycle
if $BINARY -bs 4K -sz 1M -y "$MNT" >/dev/null 2>&1; then
    pass "fs write+read"
else
    fail "fs write+read"
fi

# larger block size with -verify — checks data integrity
if $BINARY -bs 64K -sz 4M -verify -y "$MNT" >/dev/null 2>&1; then
    pass "fs verify"
else
    fail "fs verify"
fi

# write-only mode — no read-back
if $BINARY -w -bs 1M -sz 1M -y "$MNT" >/dev/null 2>&1; then
    pass "fs write-only"
else
    fail "fs write-only"
fi

# read-only mode — pre-allocates with fallocate, fills, then reads
if $BINARY -r -bs 1M -sz 1M -y "$MNT" >/dev/null 2>&1; then
    pass "fs read-only"
else
    fail "fs read-only"
fi
echo

# ============================================================
# [2] Raw block device mode — direct I/O on /dev/loopN (needs root)
# ============================================================
echo "[2] Raw block device mode (loop device)"

if check_root; then
    # create a 16 MB image and attach it as a loop device
    dd if=/dev/zero of="$IMG" bs=1M count=16 status=none 2>/dev/null
    LOOP=$(losetup --find --show "$IMG")
    if [ -z "$LOOP" ]; then
        fail "losetup setup"
    else
        pass "losetup setup ($LOOP)"

        # raw write+read — destructive to loop device, but that's fine
        if $BINARY -raw -bs 4K -sz 1M -y "$LOOP" >/dev/null 2>&1; then
            pass "raw write+read"
        else
            fail "raw write+read"
        fi

        # raw with verify
        if $BINARY -raw -bs 64K -sz 4M -verify -y "$LOOP" >/dev/null 2>&1; then
            pass "raw verify"
        else
            fail "raw verify"
        fi
    fi
else
    skip "raw write+read"
    skip "raw verify"
fi
echo

# ============================================================
# [3] Error handling — invalid inputs should fail cleanly
# ============================================================
echo "[3] Error handling"

# no arguments at all
if $BINARY 2>/dev/null; then
    fail "no args should fail"
else
    pass "no args -> exit 1"
fi

# invalid block size string
if $BINARY -bs bad /tmp 2>/dev/null; then
    fail "bad -bs should fail"
else
    pass "bad -bs -> exit 1"
fi

# -raw on a non-block device (/dev/null is a char device)
if $BINARY -raw /dev/null 2>/dev/null; then
    fail "non-block device with -raw should fail"
else
    pass "-raw on non-block -> exit 1"
fi
echo

# ---- summary ----
PASSED=$((7 - FAILED - SKIPPED))
echo "Results: $PASSED passed, $FAILED failed, $SKIPPED skipped"
if [ "$FAILED" -eq 0 ]; then
    echo "=== ALL TESTS PASSED ==="
else
    echo "=== SOME TESTS FAILED ==="
fi
exit $FAILED
