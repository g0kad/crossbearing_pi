# CrossBearing plugin for OpenCPN

Plot a position fix from hand bearing compass bearings when GPS is not available.

**Status: early development, not yet released.**

- Right-click a charted object, choose *Cross bearing: bearing to object here*, and enter the magnetic
  bearing and time.
- Variation comes from OpenCPN's WMM plugin and can be overridden.
- Position lines are drawn on the chart; the fix mark (circle round a dot) is dropped in the cocked hat
  and named with the time.
- Three bearings show the cocked hat; the result reports its size and warns about poor angles of cut
  or bearings taken far apart.

A JavaScript prototype of the same tool, for the OpenCPN JavaScript plugin, is at
[g0kad/CBF_OCPN](https://github.com/g0kad/CBF_OCPN).

## Building

The build system is the [ShipDriver plugin template](https://github.com/Rasbats/shipdriver_pi); see
`INSTALL.md`. In short, on Linux:

    git submodule update --init opencpn-libs
    mkdir build && cd build && cmake .. && make tarball

The fix maths (`src/cbf_math.*`) has no wxWidgets dependency and its unit tests build on their own:

    cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests

`scripts/pi-build.sh` copies the tree to a Raspberry Pi running OpenCPN, builds it there and installs it
for testing.

## Licence

GPLv3 or later, as the template. See `COPYING`.
