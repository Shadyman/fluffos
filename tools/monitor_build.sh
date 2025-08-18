#!/bin/bash

# FluffOS Build Monitor Shell Wrapper
# Usage: ./monitor_build.sh <bash_id> [options]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MONITOR_SCRIPT="$SCRIPT_DIR/fluffos_build_monitor.py"

# Check if monitor script exists
if [ ! -f "$MONITOR_SCRIPT" ]; then
    echo "❌ Build monitor script not found: $MONITOR_SCRIPT"
    exit 1
fi

# Check if Python 3 is available
if ! command -v python3 >/dev/null 2>&1; then
    echo "❌ Python 3 is required but not installed"
    exit 1
fi

# Default packages to monitor
DEFAULT_PACKAGES="http rest openapi"

# Parse arguments
BASH_ID="$1"
shift

if [ -z "$BASH_ID" ]; then
    echo "FluffOS Build Monitor"
    echo ""
    echo "Usage: $0 <bash_id> [options]"
    echo ""
    echo "Options:"
    echo "  --packages <pkg1> <pkg2>     Packages to focus on (default: $DEFAULT_PACKAGES)"
    echo "  --max-time <seconds>         Maximum monitoring time (default: 3600)"
    echo "  --single-check              Single status check instead of continuous"
    echo "  --output-format <fmt>        Output format: text or json (default: text)"
    echo "  --silent                    Minimal output mode"
    echo ""
    echo "Examples:"
    echo "  $0 bash_123                                    # Monitor with defaults"
    echo "  $0 bash_123 --packages http rest              # Monitor specific packages"
    echo "  $0 bash_123 --single-check --output-format json  # Single check, JSON output"
    echo "  $0 bash_123 --max-time 1800                   # 30-minute timeout"
    echo ""
    echo "The monitor uses intelligent wait intervals and filters out noise to minimize"
    echo "token usage while providing focused updates on build progress and package completions."
    exit 1
fi

# Add default packages if none specified
if [[ "$*" != *"--packages"* ]]; then
    set -- --packages $DEFAULT_PACKAGES "$@"
fi

# Execute the Python monitor with all arguments
exec python3 "$MONITOR_SCRIPT" "$BASH_ID" "$@"