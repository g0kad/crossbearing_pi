# ~~~
# Summary:      Local, non-generic plugin setup
# Copyright (c) 2020-2021 Mike Rossiter (ShipDriver template)
# Copyright (c) 2026 Mike G0KAD
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.


# -------- Options ----------

set(OCPN_TEST_REPO
    "opencpn/crossbearing-alpha"
    CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
    "opencpn/crossbearing-beta"
    CACHE STRING
    "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
    "opencpn/crossbearing-prod"
    CACHE STRING
    "Default repository for tagged builds not matching 'beta'"
)

#
#
# -------  Plugin setup --------
#
set(PKG_NAME CrossBearing_pi)
set(PKG_VERSION 0.1.0) # Major.Minor.Patch
set(PKG_PRERELEASE "")  # Empty, or a tag like 'beta'

set(DISPLAY_NAME CrossBearing)    # Dialogs, installer artifacts, ...
set(PLUGIN_API_NAME CrossBearing) # As of GetCommonName() in plugin API
set(PKG_SUMMARY "Cross bearing fixes without GPS")
set(PKG_DESCRIPTION [=[
Plot a position fix from hand bearing compass bearings when GPS is not
available. Right-click charted objects, enter the magnetic bearings, and the
plugin draws the position lines and drops a fix mark in the cocked hat.
Variation comes from the WMM plugin.
]=])

set(PKG_AUTHOR "Mike G0KAD")
set(PKG_IS_OPEN_SOURCE "yes")
set(PKG_HOMEPAGE https://github.com/g0kad/crossbearing_pi)
set(PKG_INFO_URL https://github.com/g0kad/crossbearing_pi)

set(SRC
    src/crossbearing_pi.h
    src/crossbearing_pi.cpp
    src/cbf_math.h
    src/cbf_math.cpp
    src/dialogs.h
    src/dialogs.cpp
    src/plug_utils.cpp
    src/plug_utils.h
)

set(PKG_API_LIB api-20)  #  A dir in opencpn-libs/ e. g., api-17 or api-16

# Minimum version which should be loaded. Ignored if api < 22.
# Allows plugins using functions from 1.22+ API to be loaded by
# hosts only supporting 1.21; plugins then needs to check if host
# supports 1.22+ before using such functions.
## set(MIN_API_VERSION 1.21)

macro(late_init)
  # Perform initialization after the PACKAGE_NAME library, compilers
  # and ocpn::api is available.
  if (APPLE)
    target_compile_definitions(${PACKAGE_NAME} PUBLIC OCPN_GHC_FILESYSTEM)
  endif ()
endmacro ()

macro(add_plugin_libraries)
  # Add libraries required by this plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/libs/std_filesystem")
  target_link_libraries(${PACKAGE_NAME} ocpn::filesystem)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxJSON")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxjson)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/plugin_dc")
  target_link_libraries(${PACKAGE_NAME} ocpn::plugin-dc)

  # The wxsvg library enables SVG overall in the plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxsvg")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxsvg)
endmacro ()
