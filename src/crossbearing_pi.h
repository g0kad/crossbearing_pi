/***************************************************************************
 * CrossBearing plugin: cross bearing and running fixes without GPS.
 *
 * Copyright (C) 2026 Mike G0KAD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ***************************************************************************/

#ifndef CROSSBEARING_PI_H_
#define CROSSBEARING_PI_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "config.h"
#include "ocpn_plugin.h"

class CrossBearingPi : public opencpn_plugin_120 {
public:
  explicit CrossBearingPi(void* ppimgr);
  ~CrossBearingPi() override = default;

  int Init() override;
  bool DeInit() override;

  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetPlugInVersionPatch() override;
  int GetPlugInVersionPost() override;
  const char* GetPlugInVersionPre() override;
  const char* GetPlugInVersionBuild() override;

  wxBitmap* GetPlugInBitmap() override;
  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;

  void SetCursorLatLon(double lat, double lon) override;
  void OnContextMenuItemCallback(int id) override;

private:
  wxBitmap m_panel_bitmap;
  wxWindow* m_parent_window = nullptr;

  // Position under the cursor, updated continuously by OpenCPN; when the
  // context menu opens it is the right-clicked position.
  double m_cursor_lat = 0;
  double m_cursor_lon = 0;

  int m_bearing_menu_id = -1;
};

#endif  // CROSSBEARING_PI_H_
