#!/bin/sh
# Verify that the vmalloc area containing an LKM's __init / __exit code is freed.
# POSIX-sh version for Linux 6.18 on ARM32 (BeagleBone Black) and BusyBox initramfs.
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root: sudo $0 ./simple_module_release.ko" >&2
    exit 1
fi

T=/sys/kernel/tracing
KO=${1:-./simple_module_release.ko}
if [ ! -f "$KO" ]; then
    echo "Missing module: $KO" >&2
    exit 1
fi

for cmd in insmod rmmod dmesg awk sed grep tail mktemp tr; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Missing command: $cmd" >&2
        exit 1
    fi
done

# modinfo may exist but fail in a minimal initramfs when /lib/modules/$(uname -r)/modules.dep
# is absent. For a simple out-of-tree .ko, its module name normally comes from the filename.
MOD=''
if command -v modinfo >/dev/null 2>&1; then
    MOD=$(modinfo -F name "$KO" 2>/dev/null || true)
fi
if [ -z "$MOD" ]; then
    MOD=${KO##*/}
    MOD=${MOD%.ko}
    MOD=$(printf '%s\n' "$MOD" | tr '-' '_')
fi

if [ -d "/sys/module/$MOD" ]; then
    echo "Module '$MOD' is already loaded. Unload it first with: rmmod $MOD" >&2
    exit 1
fi

if [ ! -d "$T/events/vmalloc/alloc_vmap_area" ] ||
   [ ! -d "$T/events/vmalloc/free_vmap_area_noflush" ]; then
    echo "Kernel does not expose vmalloc allocation/free tracepoints." >&2
    echo "Check: ls $T/events/vmalloc/" >&2
    exit 2
fi

I="$T/instances/memcheck_$$"
TRACE1=$(mktemp)
TRACE2=$(mktemp)
cleanup() {
    if [ -d "${I:-}" ]; then
        printf '0\n' > "$I/tracing_on" 2>/dev/null || true
        printf '0\n' > "$I/events/vmalloc/alloc_vmap_area/enable" 2>/dev/null || true
        printf '0\n' > "$I/events/vmalloc/free_vmap_area_noflush/enable" 2>/dev/null || true
        rmdir "$I" 2>/dev/null || true
    fi
    [ -z "${TRACE1:-}" ] || rm -f "$TRACE1"
    [ -z "${TRACE2:-}" ] || rm -f "$TRACE2"
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

mkdir "$I"
printf '1\n' > "$I/events/vmalloc/alloc_vmap_area/enable"
printf '1\n' > "$I/events/vmalloc/free_vmap_area_noflush/enable"
printf '\n' > "$I/trace"
printf '1\n' > "$I/tracing_on"

TAG="MEMCHECK-START-$$"
printf '<6>%s\n' "$TAG" > /dev/kmsg

logged_addr() {
    log_key=$1
    dmesg | awk -v tag="$TAG" -v key="$log_key" '
        index($0, tag) { seen=1; next }
        seen && index($0, key) { last=$0 }
        END { print last }
    '
}

extract_addr() {
    addr_line=$1
    addr_key=$2
    printf '%s\n' "$addr_line" |
        sed -n "s/.*${addr_key}[[:space:]]*//p" |
        sed -n '1{s/^0[xX]//;s/[^[:xdigit:]].*//;/[[:xdigit:]]/p;}'
}

check_address() {
    label=$1
    raw=$2
    tracefile=$3

    if [ -z "$raw" ]; then
        echo "[$label] INCONCLUSIVE: no function address was logged."
        return 1
    fi

    awk -v label="$label" -v raw="$raw" '
        function hexval(s, n, i, c, p) {
            sub(/^0[xX]/, "", s)
            n = 0
            for (i = 1; i <= length(s); i++) {
                c = tolower(substr(s, i, 1))
                p = index("0123456789abcdef", c) - 1
                if (p < 0) return -1
                n = n * 16 + p
            }
            return n
        }
        BEGIN {
            addr = hexval(raw)
            if (addr < 0) {
                printf "[%s] INCONCLUSIVE: invalid function address: %s\n", label, raw
                bad = 1
                exit
            }
            # ARM/Thumb function pointer: bit 0 marks Thumb state.
            addr = int(addr / 2) * 2
            ptr = tolower(raw)
            sub(/^0x/, "", ptr)
            last = substr(ptr, length(ptr), 1)
            digit = index("0123456789abcdef", last) - 1
            code_hex = substr(ptr, 1, length(ptr) - 1) substr("02468ace", int(digit / 2) + 1, 1)
            printf "\n[%s] function pointer=0x%s; code address=0x%s\n", label, raw, code_hex
        }
        {
            if (index($0, "alloc_vmap_area:") &&
                match($0, /va_start:[[:space:]]*[0-9]+[[:space:]]+size=[0-9]+/)) {
                rest = substr($0, RSTART, RLENGTH)
                sub(/.*va_start:[[:space:]]*/, "", rest)
                split(rest, fields, /[[:space:]]+/)
                alloc_base = fields[1] + 0
                sub(/.*size=/, "", rest)
                alloc_size = rest + 0
                sizes[alloc_base] = alloc_size
            } else if (index($0, "free_vmap_area_noflush:") &&
                       match($0, /va_start=0[xX][[:xdigit:]]+/)) {
                rest = substr($0, RSTART, RLENGTH)
                sub(/.*=/, "", rest)
                free_base = hexval(rest)
                if (free_base == addr) {
                    matched = 1
                    printf "  free event: %s\n", $0
                } else if ((free_base in sizes) &&
                           addr >= free_base && addr < free_base + sizes[free_base]) {
                    matched = 1
                    printf "  freed allocation: start=%s, size=%s bytes\n", free_base, sizes[free_base]
                    printf "  free event: %s\n", $0
                }
            }
        }
        END {
            if (bad) exit 1
            if (matched) {
                printf "[%s] CONFIRMED: the vmalloc area containing the function address was freed.\n", label
                exit 0
            }
            printf "[%s] INCONCLUSIVE: no matching vmalloc free event recorded.\n", label
            print "  This does not prove the area remained allocated (tracing may not cover the allocator path)."
            exit 1
        }
    ' "$tracefile"
}

echo "=== LOAD $MOD ==="
insmod "$KO"
sleep 3
cat "$I/trace" > "$TRACE1"
INIT_LOG=$(logged_addr 'hello addr:')
INIT_RAW=$(extract_addr "$INIT_LOG" 'hello addr:')
init_rc=0
check_address '__init' "$INIT_RAW" "$TRACE1" || init_rc=1

echo
printf '=== UNLOAD %s ===\n' "$MOD"
rmmod "$MOD"
sleep 1
cat "$I/trace" > "$TRACE2"
EXIT_LOG=$(logged_addr 'byeaddr:')
EXIT_RAW=$(extract_addr "$EXIT_LOG" 'byeaddr:')
exit_rc=0
check_address '__exit' "$EXIT_RAW" "$TRACE2" || exit_rc=1

if [ "$init_rc" -ne 0 ] || [ "$exit_rc" -ne 0 ]; then
    echo
    echo 'One or more checks were inconclusive. See raw vmalloc events below:'
    grep -E 'alloc_vmap_area:|free_vmap_area_noflush:' "$TRACE2" | tail -n 50 || true
    exit 2
fi

echo
echo 'Both memory-area releases were confirmed via vmalloc tracepoints.'
