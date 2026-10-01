#!/usr/bin/env bash
# Compile le projet à l'intérieur du conteneur. Lancé automatiquement par docker-build.bat/.sh,
# ou manuellement une fois dans le conteneur : ./scripts/docker-build.sh
set -euo pipefail

cd "$(dirname "$0")/.."

cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j

echo ""
echo "Build terminé : build-linux/V2VSimulation"
