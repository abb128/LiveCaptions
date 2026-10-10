#!/bin/sh
# Runs Live Captions from builddir/ without installing it. Build with ./setup.sh.
# Extra arguments are passed to the app.
set -eu
cd "$(dirname "$0")"

if [ ! -x builddir/src/livecaptions ]; then
    echo "Live Captions is not built yet. Run ./setup.sh first." >&2
    exit 1
fi

export GSETTINGS_SCHEMA_DIR="$PWD/builddir/data"
export APRIL_MODEL_PATH="${APRIL_MODEL_PATH:-$PWD/april-english-dev-01110_en.april}"
export ORT_DISABLE_TELEMETRY=1
if [ ! -f "$APRIL_MODEL_PATH" ]; then
    echo "Model not found at $APRIL_MODEL_PATH. Run ./setup.sh to download it." >&2
    exit 1
fi

exec ./builddir/src/livecaptions "$@"
