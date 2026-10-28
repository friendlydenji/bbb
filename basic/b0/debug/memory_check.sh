#!/usr/bin/env bash
# Verify that the vmalloc area containing an LKM's __init / __exit code is freed.
# Intended for Linux 6.18 on ARM32 (BeagleBone Black).
set -euo pipefail

if (( EUID != 0 )); then
    echo "Run as root: sudo $0 ./simple_module_release.ko" >&2
    exit 1
fi

T=/sys/kernel/tracing
KO=${1:-./simple_module_release.ko}
[[ -f "$KO" ]] || { echo "Missing module: $KO" >&2; exit 1; }

for cmd in insmod rmmod dmesg; do
    command -v "$cmd" >/dev/null || {
        echo "Missing '$cmd' (install with: sudo apt install kmod)" >&2
        exit 1
    }
done

if command -v modinfo >/dev/null 2>&1; then
    MOD=$(modinfo -F name "$KO")
else
    MOD=${KO##*/}
    MOD=${MOD%.ko}
    MOD=${MOD//-/_}
fi

if [[ -d /sys/module/$MOD ]]; then
    echo "Module '$MOD' is already loaded. Unload it first with: sudo rmmod $MOD" >&2
    exit 1
fi

if [[ ! -d "$T/events/vmalloc/alloc_vmap_area" || \
      ! -d "$T/events/vmalloc/free_vmap_area_noflush" ]]; then
    echo "Kernel does not expose vmalloc allocation/free tracepoints." >&2
    echo "Check: sudo ls $T/events/vmalloc/" >&2
    exit 2
fi

# Private trace instance: avoid modifying an existing ftrace session.
I="$T/instances/memcheck_$$"
mkdir "$I"
cleanup() {
    # Disable our private trace events, then remove the instance.
    printf '0\n' > "$I/tracing_on" 2>/dev/null || true
    printf '0\n' > "$I/events/vmalloc/alloc_vmap_area/enable" 2>/dev/null || true
    printf '0\n' > "$I/events/vmalloc/free_vmap_area_noflush/enable" 2>/dev/null || true
    rmdir "$I" 2>/dev/null || true
}
trap cleanup EXIT

printf '1\n' > "$I/events/vmalloc/alloc_vmap_area/enable"
printf '1\n' > "$I/events/vmalloc/free_vmap_area_noflush/enable"
printf '\n' > "$I/trace"
printf '1\n' > "$I/tracing_on"

TAG="MEMCHECK-START-$$"
# Use a kernel log marker to ignore log lines from previous insmod/rmmod runs.
printf '<6>%s\n' "$TAG" > /dev/kmsg

# Returns the last address logged after our marker, or an empty string.
logged_addr() {
    local keyword=$1
    dmesg | awk -v tag="$TAG" -v key="$keyword" '
        index($0, tag) { seen=1; next }
        seen && index($0, key) { last=$0 }
        END { print last }
    '
}

# Verify: code address falls inside an allocation that has a matching free event.
check_address() {
    local label=$1 raw=$2 tracefile=$3
    local normalized free_base free_hex alloc_base alloc_len line
    local matched=0 exact=0
    local -A sizes=()

    if [[ -z "$raw" ]]; then
        echo "[$label] INCONCLUSIVE: no function address was logged."
        return 1
    fi
    normalized=$(( (16#$raw) & ~1 )) # ARM/Thumb function pointer: bit 0 indicates Thumb state
    printf '\n[%s] function pointer=0x%s; code address=0x%08x\n' \
        "$label" "$raw" "$normalized"

    while IFS= read -r line; do
        if [[ "$line" == *'alloc_vmap_area:'* && \
              "$line" =~ va_start:[[:space:]]([0-9]+)[[:space:]]size=([0-9]+) ]]; then
            alloc_base=${BASH_REMATCH[1]}
            alloc_len=${BASH_REMATCH[2]}
            sizes["$alloc_base"]=$alloc_len
        elif [[ "$line" == *'free_vmap_area_noflush:'* && \
                "$line" =~ va_start=0x([[:xdigit:]]+) ]]; then
            free_hex=${BASH_REMATCH[1]}
            free_base=$((16#$free_hex))
            if (( free_base == normalized )); then
                exact=1
                matched=1
                printf '  free event: %s\n' "$line"
            elif [[ -n "${sizes[$free_base]:-}" ]]; then
                alloc_len=${sizes[$free_base]}
                if (( normalized >= free_base && normalized < free_base + alloc_len )); then
                    matched=1
                    printf '  freed allocation: start=0x%08x, size=%s bytes\n' \
                        "$free_base" "$alloc_len"
                    printf '  free event: %s\n' "$line"
                fi
            fi
        fi
    done < "$tracefile"

    if (( matched )); then
        echo "[$label] CONFIRMED: the vmalloc area containing the function address was freed."
        return 0
    fi
    echo "[$label] INCONCLUSIVE: no matching vmalloc free event recorded."
    echo '  This does not prove the area remained allocated (tracing may not cover the allocator path).'
    return 1
}

TRACE1=$(mktemp)
TRACE2=$(mktemp)
trap 'rm -f "$TRACE1" "$TRACE2"; cleanup' EXIT

echo "=== LOAD $MOD ==="
insmod "$KO"
sleep 3
cat "$I/trace" > "$TRACE1"
INIT_LOG=$(logged_addr 'hello addr:')
INIT_RAW=''
if [[ "$INIT_LOG" =~ hello[[:space:]]addr:[[:space:]]*(0x)?([[:xdigit:]]+) ]]; then
    INIT_RAW=${BASH_REMATCH[2]}
fi
init_rc=0
check_address '__init' "$INIT_RAW" "$TRACE1" || init_rc=1

echo
printf '=== UNLOAD %s ===\n' "$MOD"
rmmod "$MOD"
sleep 1
cat "$I/trace" > "$TRACE2"
EXIT_LOG=$(logged_addr 'byeaddr:')
EXIT_RAW=''
if [[ "$EXIT_LOG" =~ byeaddr:[[:space:]]*(0x)?([[:xdigit:]]+) ]]; then
    EXIT_RAW=${BASH_REMATCH[2]}
fi
exit_rc=0
check_address '__exit' "$EXIT_RAW" "$TRACE2" || exit_rc=1

if (( init_rc || exit_rc )); then
    echo
    echo 'One or more checks were inconclusive. See raw vmalloc events below:'
    grep -E 'alloc_vmap_area:|free_vmap_area_noflush:' "$TRACE2" | tail -n 50 || true
    exit 2
fi

echo
echo 'Both memory-area releases were confirmed via vmalloc tracepoints.'

