#!/usr/bin/env bash
# Builds the image (first run only) and opens a shell in it with the current
# folder mounted at /work. The SDK is x86-64 only; on an Apple Silicon Mac
# Docker emulates it (turn on Rosetta in Docker Desktop for speed).
# Usage, from the repository root:   ./docker/start.sh
set -e
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
docker build --platform linux/amd64 -t upmem-sim "$HERE"
docker run --platform linux/amd64 --rm -it -v "$PWD":/work upmem-sim
