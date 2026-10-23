/***************************************************************************
 * CrossBearing plugin: cross bearing fixes without GPS.
 *
 * Copyright (C) 2026 Mike G0KAD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ***************************************************************************/

// Workflow: right-click each charted object (or an existing waypoint),
// enter the magnetic bearing and time. Each position line is drawn straight
// away. "Plot fix" on the last bearing trims the lines and drops a fix mark
// (circle round a dot) named with the time.
//
// Running fixes were tried and removed as too fiddly in practice; cbf_math
// still supports advancing a line (cbf::Observation::run).
//
// Position lines are drawn as a chart overlay, not routes, so nothing is
// added to the Route Manager. The fix itself is a normal waypoint.

#include "crossbearing_pi.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <wx/datetime.h>

#include "pidc.h"
#include "plug_utils.h"
#include "wx/jsonreader.h"

namespace {

const double kProvisionalLineNm = 4;  // length of a line before there is a fix
const double kOverrunNm = 0.5;        // how far lines run past the fix
const char* const kFixIcon = "Symbol-Circle";
const int kSpreadWarnMinutes = 5;     // warn if bearings were taken further apart
const wxColour kLineColour(192, 0, 122);  // magenta

const wxString kDeg = wxString::FromUTF8("\xC2\xB0");

int NowMinutes() {
  wxDateTime now = wxDateTime::Now();
  return now.GetHour() * 60 + now.GetMinute();
}

// Numbers in JSON need a decimal point whatever the locale.
wxString JsonNumber(double v) { return W(cbf::FormatFixed(v, 6)); }

}  // namespace

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

  m_bearing_menu_id = AddCanvasContextMenuItem(
      new wxMenuItem(nullptr, wxID_ANY, _("Cross bearing: bearing to object here")), this);
  m_plot_menu_id = AddCanvasContextMenuItem(
      new wxMenuItem(nullptr, wxID_ANY, _("Cross bearing: plot fix")), this);
  m_clear_menu_id = AddCanvasContextMenuItem(
      new wxMenuItem(nullptr, wxID_ANY, _("Cross bearing: clear bearings and lines")), this);
  m_waypoint_menu_id = AddCanvasContextMenuItemExt(
      new wxMenuItem(nullptr, wxID_ANY, _("Cross bearing: bearing to this waypoint")), this,
      "Waypoint");
  UpdateMenus();

  return WANTS_CURSOR_LATLON | WANTS_PLUGIN_MESSAGING | WANTS_CONFIG |
         WANTS_OVERLAY_CALLBACK | WANTS_OPENGL_OVERLAY_CALLBACK;
}

bool CrossBearingPi::DeInit() {
  for (int* id : {&m_bearing_menu_id, &m_plot_menu_id, &m_clear_menu_id, &m_waypoint_menu_id}) {
    if (*id >= 0) RemoveCanvasContextMenuItem(*id);
    *id = -1;
  }
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

void CrossBearingPi::UpdateMenus() {
  SetCanvasContextMenuItemGrey(m_plot_menu_id, m_bearings.size() < 2);
  SetCanvasContextMenuItemGrey(m_clear_menu_id, m_bearings.empty() && m_kept_lines.empty());
}

// --- choosing the object --------------------------------------------------------

void CrossBearingPi::OnContextMenuItemCallback(int id) {
  if (id == m_bearing_menu_id)
    StartBearing({m_cursor_lat, m_cursor_lon}, "");
  else if (id == m_plot_menu_id)
    PlotFix();
  else if (id == m_clear_menu_id) {
    m_kept_lines.clear();
    ClearBearings();
  }
}

void CrossBearingPi::OnContextMenuItemCallbackExt(int id, std::string obj_ident,
                                                  std::string obj_type, double lat,
                                                  double lon) {
  if (id != m_waypoint_menu_id) return;
  cbf::LatLon pos{lat, lon};
  wxString name;
  PlugIn_Waypoint_ExV2 wp;
  if (GetSingleWaypointExV2(W(obj_ident), &wp)) {
    pos = {wp.m_lat, wp.m_lon};
    name = wp.m_MarkName;
  }
  StartBearing(pos, name);
}

// Variation from the WMM plugin. The reply may arrive during the
// SendPluginMessage call or later; a late reply fills the open dialog.
void CrossBearingPi::SetPluginMessage(wxString& message_id, wxString& message_body) {
  if (message_id != "WMM_VARIATION") return;
  wxJSONValue root;
  wxJSONReader reader;
  if (reader.Parse(message_body, &root) > 0 || !root.HasMember("Decl")) return;
  double decl = root["Decl"].AsDouble();
  if (m_open_dialog) m_open_dialog->OfferVariation(decl);
  if (!m_variation) {
    m_variation = decl;
    m_variation_from_wmm = true;
  }
}

// --- adding a bearing ---------------------------------------------------------------

void CrossBearingPi::StartBearing(const cbf::LatLon& pos, const wxString& name) {
  if (!m_variation) {
    wxDateTime now = wxDateTime::Now();
    wxString request = "{\"Lat\":" + JsonNumber(pos.lat) + ",\"Lon\":" + JsonNumber(pos.lon) +
                       wxString::Format(",\"Year\":%d,\"Month\":%d,\"Day\":%d}", now.GetYear(),
                                        now.GetMonth() + 1, now.GetDay());
    SendPluginMessage("WMM_VARIATION_REQUEST", request);
  }

  int n = static_cast<int>(m_bearings.size()) + 1;
  BearingEntry initial;
  initial.name = name.empty() ? wxString::Format(_("Object %d"), n) : name;
  initial.position = pos;
  initial.minutes = m_bearings.empty() ? NowMinutes() : m_bearings.back().minutes;

  wxString earlier;
  for (size_t i = 0; i < m_bearings.size(); i++) {
    const auto& b = m_bearings[i];
    earlier += wxString::Format("%d. ", static_cast<int>(i + 1)) + W(cbf::FormatTime(b.minutes)) +
               "  " + b.name + "  " + W(cbf::FormatBearing(b.bearing)) + kDeg + "M\n";
  }
  earlier.Trim();

  BearingDialog dlg(m_parent_window, n, initial, m_variation, m_variation_from_wmm, earlier,
                    !m_bearings.empty());
  m_open_dialog = &dlg;
  int result = dlg.ShowModal();
  m_open_dialog = nullptr;
  if (result == wxID_CANCEL) return;

  const BearingEntry& e = dlg.GetEntry();
  if (!m_variation || std::fabs(e.variation - *m_variation) > 1e-9) m_variation_from_wmm = false;
  m_variation = e.variation;

  Bearing b;
  b.name = e.name.empty() ? wxString::Format(_("Object %d"), n) : e.name;
  b.object = e.position;
  b.bearing = e.bearing;
  b.minutes = e.minutes;
  m_bearings.push_back(b);

  auto ends = cbf::PositionLineEnds(b.object, cbf::CompassToTrue(b.bearing, *m_variation),
                                    kProvisionalLineNm);
  m_lines.push_back({ends[0], ends[1]});
  UpdateMenus();
  RequestRefresh(m_parent_window);

  if (result == BearingDialog::ID_PLOT_FIX) PlotFix();
}

// --- plotting the fix -------------------------------------------------------------------

void CrossBearingPi::PlotFix() {
  if (m_bearings.size() < 2) {
    OCPNMessageBox_PlugIn(m_parent_window, _("Need at least two bearings for a fix."),
                          _("Cross bearing"), wxOK);
    return;
  }
  // The fix is for the time of the last bearing; note if they were spread out.
  int ref = m_bearings.back().minutes;
  int span = 0;
  for (const auto& b : m_bearings) span = std::max(span, std::abs(cbf::MinutesBefore(b.minutes, ref)));

  std::vector<cbf::Observation> obs;
  for (const auto& b : m_bearings) {
    cbf::Observation o;
    o.name = U(b.name);
    o.object = b.object;
    o.bearing = b.bearing;
    obs.push_back(o);
  }

  cbf::FixResult result;
  try {
    cbf::FixOptions options;
    options.variation = m_variation.value_or(0);
    result = cbf::ComputeFix(obs, options);
  } catch (const std::exception& e) {
    OCPNMessageBox_PlugIn(m_parent_window, _("No fix: ") + W(e.what()), _("Cross bearing"), wxOK);
    return;
  }

  // Redraw each line from its object to just past the fix.
  m_lines.clear();
  for (const auto& l : result.lines) {
    auto ends = cbf::PositionLineEnds(l.object, l.true_bearing, std::max(l.range_nm, 0.0) + kOverrunNm);
    m_lines.push_back({ends[0], ends[1]});
  }

  wxString label = _("Fix ") + W(cbf::FormatTime(ref));
  wxString summary = ResultText(result, span);

  PlugIn_Waypoint_ExV2 wp(result.fix.lat, result.fix.lon, kFixIcon, label, GetNewGUID());
  wp.IsNameVisible = true;
  wp.IsVisible = true;
  wp.m_MarkDescription = summary;
  AddSingleWaypointExV2(&wp, true);
  RequestRefresh(m_parent_window);

  ResultDialog dlg(m_parent_window, label, summary);
  if (dlg.ShowModal() == ResultDialog::ID_KEEP_LINES)
    m_kept_lines.insert(m_kept_lines.end(), m_lines.begin(), m_lines.end());
  // The fix mark stays either way; the next fix starts with no bearings.
  ClearBearings();
}

wxString CrossBearingPi::ResultText(const cbf::FixResult& r, int span_minutes) const {
  wxString t = W(cbf::FormatLat(r.fix.lat)) + "  " + W(cbf::FormatLon(r.fix.lon)) + "\n";
  t += _("Variation ") + W(cbf::FormatVarDev(m_variation.value_or(0))) +
       (m_variation_from_wmm ? _(" (WMM)") : _(" (entered)")) + "\n";
  for (size_t i = 0; i < r.lines.size(); i++) {
    const auto& l = r.lines[i];
    t += W(cbf::FormatTime(m_bearings[i].minutes)) + " " + W(l.name) + ": " +
         W(cbf::FormatBearing(l.observed_bearing)) + kDeg + "M = " +
         W(cbf::FormatBearing(l.true_bearing)) + kDeg + "T, " +
         W(cbf::FormatFixed(l.range_nm, 1)) + " nm\n";
  }
  if (r.cocked_hat) {
    t += _("Cocked hat: longest side ") + W(cbf::FormatFixed(r.cocked_hat->max_side_nm, 2)) +
         " nm (" + wxString::Format("%d", static_cast<int>(std::lround(r.cocked_hat->max_side_nm * 1852))) +
         " m)\n";
  } else if (r.lines.size() > 3) {
    t += _("Spread of crossings: ") + W(cbf::FormatFixed(r.spread_nm, 2)) + " nm\n";
  }
  for (const auto& w : r.warnings) t += _("Warning: ") + W(w) + "\n";
  if (span_minutes > kSpreadWarnMinutes)
    t += wxString::Format(_("Warning: bearings were taken %d minutes apart; take them as close "
                            "together as possible."),
                          span_minutes) +
         "\n";
  return t.Trim();
}

void CrossBearingPi::ClearBearings() {
  m_bearings.clear();
  m_lines.clear();
  UpdateMenus();
  RequestRefresh(m_parent_window);
}

// --- drawing ------------------------------------------------------------------------------

bool CrossBearingPi::RenderOverlayMultiCanvas(wxDC& dc, PlugIn_ViewPort* vp, int canvas_ix,
                                              int priority) {
  if (priority != OVERLAY_LEGACY || (m_lines.empty() && m_kept_lines.empty())) return false;
  piDC pidc(dc);
  DrawLines(pidc, vp);
  return true;
}

bool CrossBearingPi::RenderGLOverlayMultiCanvas(wxGLContext* pcontext, PlugIn_ViewPort* vp,
                                                int canvas_ix, int priority) {
  if (priority != OVERLAY_LEGACY || (m_lines.empty() && m_kept_lines.empty())) return false;
  piDC pidc(pcontext);
  pidc.SetVP(vp);
  DrawLines(pidc, vp);
  return true;
}

void CrossBearingPi::DrawLines(piDC& dc, PlugIn_ViewPort* vp) {
  int width = std::max(2, static_cast<int>(std::lround(GetOCPNGUIToolScaleFactor_PlugIn() * 2)));
  dc.SetPen(wxPen(kLineColour, width, wxPENSTYLE_SOLID));
  for (const auto* set : {&m_kept_lines, &m_lines}) {
    for (const auto& l : *set) {
      wxPoint a, b;
      GetCanvasPixLL(vp, &a, l.start.lat, l.start.lon);
      GetCanvasPixLL(vp, &b, l.end.lat, l.end.lon);
      dc.DrawLine(a.x, a.y, b.x, b.y);
    }
  }
}
