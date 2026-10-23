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

#include <optional>
#include <vector>

#include "cbf_math.h"
#include "config.h"
#include "dialogs.h"
#include "ocpn_plugin.h"

class piDC;

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
  void OnContextMenuItemCallbackExt(int id, std::string obj_ident, std::string obj_type,
                                    double lat, double lon) override;
  void SetPluginMessage(wxString& message_id, wxString& message_body) override;

  using opencpn_plugin_118::RenderGLOverlayMultiCanvas;
  using opencpn_plugin_118::RenderOverlayMultiCanvas;
  bool RenderOverlayMultiCanvas(wxDC& dc, PlugIn_ViewPort* vp, int canvas_ix,
                                int priority) override;
  bool RenderGLOverlayMultiCanvas(wxGLContext* pcontext, PlugIn_ViewPort* vp,
                                  int canvas_ix, int priority) override;

private:
  struct Bearing {
    wxString name;
    cbf::LatLon object;
    double bearing = 0;  // magnetic
    int minutes = 0;     // time of day taken
  };

  struct DrawnLine {
    cbf::LatLon start;
    cbf::LatLon end;
    bool dashed = false;  // advanced (running fix) line
  };

  void StartBearing(const cbf::LatLon& pos, const wxString& name);
  void PlotFix();
  void ComputeAndPlot(const std::optional<RunParams>& run);
  wxString ResultText(const cbf::FixResult& r, const std::optional<RunParams>& run,
                      double course_true) const;
  void ClearBearings();
  void UpdateMenus();
  void DrawLines(piDC& dc, PlugIn_ViewPort* vp);

  wxBitmap m_panel_bitmap;
  wxWindow* m_parent_window = nullptr;

  // Position under the cursor, updated continuously by OpenCPN; when the
  // context menu opens it is the right-clicked position.
  double m_cursor_lat = 0;
  double m_cursor_lon = 0;

  int m_bearing_menu_id = -1;
  int m_waypoint_menu_id = -1;
  int m_plot_menu_id = -1;
  int m_clear_menu_id = -1;

  std::vector<Bearing> m_bearings;
  std::optional<double> m_variation;  // last used, east positive
  bool m_variation_from_wmm = false;
  BearingDialog* m_open_dialog = nullptr;  // to fill in a late WMM reply
  RunEntry m_run_entry;

  std::vector<DrawnLine> m_lines;       // lines of the current fix
  std::vector<DrawnLine> m_kept_lines;  // lines kept from earlier fixes
};

#endif  // CROSSBEARING_PI_H_
