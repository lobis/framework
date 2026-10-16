#!/usr/bin/env bash
# Select the validated batch default, then let prepare resolve and hash its prefix.
set -euo pipefail
defaults=/afs/desy.de/user/l/lobis/.config/iaxo-cernbox/rest-default.json
install=${IAXO_REST_INSTALL:-}
if [[ -z "$install" ]]; then
    install=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["install"])' "$defaults")
fi
test -f "$install/validated.json"
exec /afs/desy.de/user/l/lobis/.local/bin/cernbox-batch prepare \
    --credentials /afs/desy.de/user/l/lobis/.config/iaxo-cernbox \
    --rest-install "$install" "$@"
