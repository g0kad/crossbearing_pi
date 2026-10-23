/***************************************************************************
 * CrossBearing plugin dialogs.
 *
 * Copyright (C) 2026 Mike G0KAD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ***************************************************************************/

#ifndef CROSSBEARING_DIALOGS_H_
#define CROSSBEARING_DIALOGS_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <optional>

#include "cbf_math.h"

/** UTF-8 std::string to wxString. */
inline wxString W(const std::string& s) { return wxString::FromUTF8(s.c_str()); }

/** wxString to UTF-8 std::string. */
inline std::string U(const wxString& s) { return std::string(s.ToUTF8()); }

/** What the user entered for one bearing. */
struct BearingEntry {
  wxString name;
  cbf::LatLon position;
  bool typed_position = false;  ///< typed rather than clicked on the chart
  double bearing = 0;           ///< magnetic
  int minutes = 0;              ///< time of day the bearing was taken
  double variation = 0;         ///< east positive
};

/**
 * Enter one bearing. The object's position is shown but not editable here,
 * so it can't be overwritten by accident; Position... opens a separate
 * dialog to type one. ShowModal() returns wxID_CANCEL, ID_ADD_MORE or
 * ID_PLOT_FIX; the entry is valid for the last two.
 */
class BearingDialog : public wxDialog {
public:
  enum { ID_ADD_MORE = wxID_HIGHEST + 1, ID_PLOT_FIX, ID_POSITION };

  BearingDialog(wxWindow* parent, int number, const BearingEntry& initial,
                std::optional<double> variation, bool variation_from_wmm,
                const wxString& earlier_bearings, bool can_plot);

  const BearingEntry& GetEntry() const { return m_entry; }

  /** WMM replied while the dialog is open: fill variation if still empty. */
  void OfferVariation(double variation);

private:
  void OnPosition(wxCommandEvent& event);
  void OnAdd(wxCommandEvent& event);
  void UpdatePositionText();

  BearingEntry m_entry;
  wxTextCtrl* m_name;
  wxTextCtrl* m_bearing;
  wxTextCtrl* m_time;
  wxTextCtrl* m_variation;
  wxStaticText* m_variation_hint;
  wxStaticText* m_position;
  wxStaticText* m_error;
};

/** Type an object's position, e.g. from a list of lights. */
class PositionDialog : public wxDialog {
public:
  PositionDialog(wxWindow* parent, const cbf::LatLon& position);

  /** True if the user changed the position (and it is valid). */
  bool Changed() const { return m_changed; }
  const cbf::LatLon& GetPosition() const { return m_position; }

private:
  void OnOK(wxCommandEvent& event);

  cbf::LatLon m_position;
  bool m_changed = false;
  wxString m_lat_text;
  wxString m_lon_text;
  wxTextCtrl* m_lat;
  wxTextCtrl* m_lon;
  wxStaticText* m_error;
};

/** Show the fix. ShowModal() returns ID_NEW_FIX or ID_KEEP_LINES. */
class ResultDialog : public wxDialog {
public:
  enum { ID_NEW_FIX = wxID_HIGHEST + 10, ID_KEEP_LINES };

  ResultDialog(wxWindow* parent, const wxString& title, const wxString& text);
};

#endif  // CROSSBEARING_DIALOGS_H_
