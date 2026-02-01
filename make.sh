set -euo pipefail

if [ -d build ]; then
  rm -r build
fi
cmake -S . -B build
cmake --build build
ctest --test-dir build
