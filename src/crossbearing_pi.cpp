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

#include "crossbearing_pi.h"

#include <string>

#include "cbf_math.h"
#include "plug_utils.h"

// The class factories, used to create and destroy instances of the PlugIn
extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new CrossBearingPi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) { delete p; }

CrossBearingPi::CrossBearingPi(void* ppimgr) : opencpn_plugin_120(ppimgr) {
  auto icon_path = GetPluginIcon("crossbearing_panel_icon", PKG_NAME);
  if (icon_path.type == IconPath::Type::Svg)
    m_panel_bitmap = LoadSvgIcon(icon_path.path.c_str());
  else if (icon_path.type == IconPath::Type::Png)
    m_panel_bitmap = LoadPngIcon(icon_path.path.c_str());
  else
    wxLogWarning("CrossBearing: cannot find icon crossbearing_panel_icon");
}

int CrossBearingPi::Init() {
  AddLocaleCatalog("opencpn-CrossBearing_pi");
  m_parent_window = GetOCPNCanvasWindow();

  auto* item = new wxMenuItem(nullptr, wxID_ANY, _("Cross bearing: bearing to object here"));
  m_bearing_menu_id = AddCanvasContextMenuItem(item, this);

  return WANTS_CURSOR_LATLON | WANTS_PLUGIN_MESSAGING | WANTS_CONFIG;
}

bool CrossBearingPi::DeInit() {
  if (m_bearing_menu_id >= 0) RemoveCanvasContextMenuItem(m_bearing_menu_id);
  m_bearing_menu_id = -1;
  return true;
}

int CrossBearingPi::GetAPIVersionMajor() { return atoi(API_VERSION); }

int CrossBearingPi::GetAPIVersionMinor() {
  std::string v(API_VERSION);
  size_t dotpos = v.find('.');
  return atoi(v.substr(dotpos + 1).c_str());
}

int CrossBearingPi::GetPlugInVersionMajor() { return PLUGIN_VERSION_MAJOR; }
int CrossBearingPi::GetPlugInVersionMinor() { return PLUGIN_VERSION_MINOR; }
int CrossBearingPi::GetPlugInVersionPatch() { return PLUGIN_VERSION_PATCH; }
int CrossBearingPi::GetPlugInVersionPost() { return PLUGIN_VERSION_TWEAK; }
const char* CrossBearingPi::GetPlugInVersionPre() { return PKG_PRERELEASE; }
const char* CrossBearingPi::GetPlugInVersionBuild() { return PKG_BUILD_INFO; }

wxBitmap* CrossBearingPi::GetPlugInBitmap() { return &m_panel_bitmap; }
wxString CrossBearingPi::GetCommonName() { return PLUGIN_API_NAME; }
wxString CrossBearingPi::GetShortDescription() { return PKG_SUMMARY; }
wxString CrossBearingPi::GetLongDescription() { return PKG_DESCRIPTION; }

void CrossBearingPi::SetCursorLatLon(double lat, double lon) {
  m_cursor_lat = lat;
  m_cursor_lon = lon;
}

void CrossBearingPi::OnContextMenuItemCallback(int id) {
  if (id != m_bearing_menu_id) return;
  wxString pos = wxString::FromUTF8(cbf::FormatLat(m_cursor_lat).c_str()) + "  " +
                 wxString::FromUTF8(cbf::FormatLon(m_cursor_lon).c_str());
  OCPNMessageBox_PlugIn(m_parent_window, _("Object at ") + pos, _("Cross bearing"),
                        wxOK);
}
