#!/bin/sh
set -eu
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$root/zig-out/bin"
export SHAIYA_LOGIN_HOST=127.0.0.1
exec wine ./game.exe start game
