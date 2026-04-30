#!/usr/bin/env bash
# Build (and optionally run) the kernel inside a container.
#
# Usage:
#   ./docker-build.sh              # equivalent to `make`
#   ./docker-build.sh make clean   # any make target
#   ./docker-build.sh run          # `make run` with KVM passthrough (Linux host only)
#   ./docker-build.sh shell        # interactive shell inside the build env
set -euo pipefail

IMAGE=temple-os-build
REPO_DIR=$(cd "$(dirname "$0")" && pwd)

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "[docker-build] Building image $IMAGE (one-time, ~2 min)..."
    docker build -t "$IMAGE" "$REPO_DIR"
fi

DOCKER_FLAGS=(--rm -v "$REPO_DIR":/src -w /src)

# Forward KVM + tty when the user wants to actually run the OS.
if [[ "${1:-}" == "run" || "${1:-}" == "run-uefi" ]]; then
    if [[ ! -e /dev/kvm ]]; then
        echo "[docker-build] /dev/kvm missing on host; QEMU will fall back to TCG (slow)." >&2
        echo "                On macOS/Windows hosts, build inside the container and run QEMU on the host instead." >&2
    else
        DOCKER_FLAGS+=(--device /dev/kvm)
    fi
    DOCKER_FLAGS+=(-it)
    exec docker run "${DOCKER_FLAGS[@]}" "$IMAGE" make "$@"
fi

if [[ "${1:-}" == "shell" ]]; then
    exec docker run "${DOCKER_FLAGS[@]}" -it "$IMAGE" bash
fi

exec docker run "${DOCKER_FLAGS[@]}" "$IMAGE" "${@:-make}"
