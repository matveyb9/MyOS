#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image_path="${1:-${project_root}/myos.img}"
qemu_binary="${QEMU_SYSTEM_X86_64:-qemu-system-x86_64}"
ovmf_code="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}"
ovmf_vars_source="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS_4M.fd}"
timeout_seconds="${MYOS_SMOKE_TIMEOUT_SECONDS:-12}"
bios_log="$(mktemp)"
uefi_log="$(mktemp)"
uefi_vars="$(mktemp)"

cleanup() {
    rm -f "${bios_log}" "${uefi_log}" "${bios_log}.qemu" "${uefi_log}.qemu" "${uefi_vars}"
}
trap cleanup EXIT

fail() {
    printf 'boot smoke: %s\n' "$1" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || fail "required file is missing: $1"
}

run_guest() {
    local name="$1"
    local log_path="$2"
    shift 2
    local status

    set +e
    timeout "${timeout_seconds}s" "$@" >"${log_path}.qemu" 2>&1
    status=$?
    set -e
    if [[ ${status} -ne 0 && ${status} -ne 124 ]]; then
        cat "${log_path}" >&2
        fail "${name} QEMU exited with status ${status}"
    fi
}

require_file "${image_path}"
command -v "${qemu_binary}" >/dev/null 2>&1 || fail "QEMU was not found: ${qemu_binary}"

run_guest "BIOS" "${bios_log}" \
    "${qemu_binary}" -machine q35 -m 256M \
    -drive "if=ide,format=raw,file=${image_path}" \
    -boot c -serial "file:${bios_log}" -display none -no-reboot -no-shutdown

grep -Fq '[ok] Firmware: BIOS' "${bios_log}" || fail 'BIOS firmware marker missing'
grep -Fq '[ok] Persistent storage mount: ready' "${bios_log}" || fail 'BIOS persistent-storage marker missing'
grep -Fq '[myos]$' "${bios_log}" || fail 'BIOS user-shell marker missing'
printf 'boot smoke: BIOS passed\n'

require_file "${ovmf_code}"
require_file "${ovmf_vars_source}"
cp "${ovmf_vars_source}" "${uefi_vars}"

run_guest "UEFI" "${uefi_log}" \
    "${qemu_binary}" -machine q35 -m 256M \
    -drive "if=pflash,format=raw,readonly=on,file=${ovmf_code}" \
    -drive "if=pflash,format=raw,file=${uefi_vars}" \
    -drive "if=ide,format=raw,file=${image_path}" \
    -boot c -serial "file:${uefi_log}" -display none -no-reboot -no-shutdown

grep -Fq '[ok] Firmware: UEFI x86_64' "${uefi_log}" || fail 'UEFI firmware marker missing'
grep -Fq '[ok] Persistent storage mount: ready' "${uefi_log}" || fail 'UEFI persistent-storage marker missing'
grep -Fq '[myos]$' "${uefi_log}" || fail 'UEFI user-shell marker missing'
printf 'boot smoke: UEFI passed\n'
