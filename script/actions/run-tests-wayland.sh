#!/bin/bash
set -euo pipefail

export XDG_SESSION_TYPE=wayland
exec wlheadless-run -c weston -- "$@" --ozone-platform=wayland
