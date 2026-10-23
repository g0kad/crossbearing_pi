#!/bin/sh
# Copy the source tree to the OpenCPN Pi, build it natively and install the
# plugin into the user's OpenCPN plugin directory. Restart OpenCPN to load it.
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
  make -j4 > make.log 2>&1 || { grep -E "error|Error" make.log | head -30; exit 1; }
  grep -E "warning:" make.log | grep -v opencpn-libs | head -20 || true
  # Install like the plugin manager does: library and data under ~/.local
  install -D -m 644 libCrossBearing_pi.so ~/.local/lib/opencpn/libCrossBearing_pi.so
  mkdir -p ~/.local/share/opencpn/plugins/CrossBearing_pi/data
  cp ../data/*.svg ~/.local/share/opencpn/plugins/CrossBearing_pi/data/
  echo "Built and installed: $(ls -l ~/.local/lib/opencpn/libCrossBearing_pi.so)"
'
