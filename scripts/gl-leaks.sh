#!/bin/sh

set -ue

tmpfile=$(mktemp --suffix _ngl.trace)
trap 'rm -f "$tmpfile"' EXIT
apitrace trace -a egl -o "$tmpfile" "$@"

# executable doesn't exit in error so we look for "errors" in stderr
leaks=$(apitrace leaks "$tmpfile" 2>&1 > /dev/null)
if echo "$leaks" | grep -q "error:"; then
    echo "GPU leaks detected: YES"
    echo "$leaks"
    exit 1
else
    echo "GPU leaks detected: NO"
fi
