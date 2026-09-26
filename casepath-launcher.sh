#!/usr/bin/env bash
set -u
PROJECT="/home/we6jbo/Projects/CasePath"

if [[ -x "$PROJECT/build/CasePath" ]]; then
    exec "$PROJECT/build/CasePath" "$@"
fi

candidate="$(find "$HOME" -maxdepth 4 -type f -name CasePath -perm -u+x 2>/dev/null | grep -E '/build[^/]*/CasePath$' | head -n 1 || true)"
if [[ -n "$candidate" ]]; then
    exec "$candidate" "$@"
fi

echo "CasePath executable was not found."
echo "Build it with:"
echo "  cmake -S \"$PROJECT\" -B \"$PROJECT/build\""
echo "  cmake --build \"$PROJECT/build\" -j\"$(nproc)\""
exit 1
