#!/bin/sh
# Copy the source tree to the OpenCPN Pi, build it natively and make a plugin
# tarball in ~/crossbearing-test. Install it from OpenCPN with
# Options > Plugins > Import plugin..., which registers it properly (a library
# copied in by hand loads but is not listed in the plugin manager).
# Uses Windows OpenSSH so the Windows ssh-agent key is available.
HOST=${1:-pi@openplotter.local}
SSH=/c/Windows/System32/OpenSSH/ssh.exe
cd "$(dirname "$0")/.." || exit 1
tar --exclude=.git --exclude='build*' -czf - . | $SSH -o BatchMode=yes "$HOST" '
  set -e
  mkdir -p ~/build/crossbearing_pi && cd ~/build/crossbearing_pi
  find . -mindepth 1 -maxdepth 1 ! -name build -exec rm -rf {} +
  tar xzf -
  mkdir -p build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo > cmake.log 2>&1 || { tail -30 cmake.log; exit 1; }
  rm -f CrossBearing-*.tar.gz CrossBearing-*.xml
  make -j4 tarball > make.log 2>&1 || { grep -E "error|Error" make.log | head -30; exit 1; }
  grep -E "warning:" make.log | grep -v opencpn-libs | head -20 || true
  mkdir -p ~/crossbearing-test && rm -f ~/crossbearing-test/*
  cp CrossBearing-*.tar.gz CrossBearing-*.xml ~/crossbearing-test/
  echo "Tarball ready:"; ls -1 ~/crossbearing-test
'
