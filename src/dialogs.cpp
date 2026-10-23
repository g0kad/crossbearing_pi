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

#include "dialogs.h"

#include <wx/statline.h>

namespace {

const wxString kDeg = wxString::FromUTF8("\xC2\xB0");

// Add "Label [field] suffix" as a row of a two-column grid.
wxTextCtrl* AddField(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label,
                     const wxString& value, int width, const wxString& suffix,
                     wxStaticText** suffix_ctrl = nullptr) {
  grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
  auto* row = new wxBoxSizer(wxHORIZONTAL);
  auto* text = new wxTextCtrl(parent, wxID_ANY, value, wxDefaultPosition,
                              wxSize(parent->FromDIP(width), -1));
  row->Add(text, 0, wxALIGN_CENTER_VERTICAL);
  auto* suf = new wxStaticText(parent, wxID_ANY, suffix);
  row->Add(suf, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, parent->FromDIP(6));
  if (suffix_ctrl) *suffix_ctrl = suf;
  grid->Add(row, 0, wxEXPAND);
  return text;
}

wxStaticText* MakeError(wxWindow* parent) {
  auto* error = new wxStaticText(parent, wxID_ANY, "");
  error->SetForegroundColour(*wxRED);
  error->Hide();
  return error;
}

void SetError(wxDialog* dialog, wxStaticText* error, const wxString& message) {
  error->SetLabel(message);
  error->Show(!message.empty());
  dialog->GetSizer()->Fit(dialog);
  dialog->Layout();
}

}  // namespace

// --- BearingDialog --------------------------------------------------------------

BearingDialog::BearingDialog(wxWindow* parent, int number, const BearingEntry& initial,
                             std::optional<double> variation, bool variation_from_wmm,
                             const wxString& earlier_bearings, bool can_plot)
    : wxDialog(parent, wxID_ANY, wxString::Format(_("Cross bearing - bearing %d"), number)),
      m_entry(initial) {
  int gap = FromDIP(8);
  auto* top = new wxBoxSizer(wxVERTICAL);

  m_error = MakeError(this);
  top->Add(m_error, 0, wxALL, gap);

  auto* grid = new wxFlexGridSizer(2, gap / 2, gap);
  m_name = AddField(this, grid, _("Object name"), initial.name, 180, "");
  m_bearing = AddField(this, grid, _("Bearing"), "", 70, kDeg + "M");
  m_time = AddField(this, grid, _("Time"), W(cbf::FormatTime(initial.minutes)), 70, _("hhmm"));
  m_variation = AddField(this, grid, _("Variation"),
                         variation ? W(cbf::FormatVarDev(*variation)) : wxString(), 70,
                         variation_from_wmm ? _("(WMM)") : _("e.g. 1.5W"), &m_variation_hint);
  top->Add(grid, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);

  m_position = new wxStaticText(this, wxID_ANY, "");
  top->Add(m_position, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);
  UpdatePositionText();

  if (!earlier_bearings.empty()) {
    top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, gap);
    top->Add(new wxStaticText(this, wxID_ANY, earlier_bearings), 0, wxALL, gap);
  }

  auto* buttons = new wxBoxSizer(wxHORIZONTAL);
  buttons->Add(new wxButton(this, wxID_CANCEL, _("Cancel")), 0, wxRIGHT, gap);
  buttons->Add(new wxButton(this, ID_POSITION, _("Position...")), 0, wxRIGHT, gap);
  auto* add = new wxButton(this, ID_ADD_MORE, _("Add more"));
  buttons->Add(add, 0, wxRIGHT, gap);
  if (can_plot) buttons->Add(new wxButton(this, ID_PLOT_FIX, _("Plot fix")), 0);
  top->Add(buttons, 0, wxALIGN_RIGHT | wxALL, gap);
  add->SetDefault();

  Bind(wxEVT_BUTTON, &BearingDialog::OnPosition, this, ID_POSITION);
  Bind(wxEVT_BUTTON, &BearingDialog::OnAdd, this, ID_ADD_MORE);
  Bind(wxEVT_BUTTON, &BearingDialog::OnAdd, this, ID_PLOT_FIX);

  SetSizerAndFit(top);
  m_bearing->SetFocus();
  CentreOnParent();
}

void BearingDialog::UpdatePositionText() {
  m_position->SetLabel((m_entry.typed_position ? _("Position (typed)") : _("Position (from chart)")) +
                       "\n" + W(cbf::FormatLat(m_entry.position.lat)) + "  " +
                       W(cbf::FormatLon(m_entry.position.lon)));
}

void BearingDialog::OfferVariation(double variation) {
  if (!m_variation->GetValue().Trim().empty()) return;
  m_variation->SetValue(W(cbf::FormatVarDev(variation)));
  m_variation_hint->SetLabel(_("(WMM)"));
  Layout();
}

void BearingDialog::OnPosition(wxCommandEvent&) {
  PositionDialog dlg(this, m_entry.position);
  if (dlg.ShowModal() == wxID_OK && dlg.Changed()) {
    m_entry.position = dlg.GetPosition();
    m_entry.typed_position = true;
    UpdatePositionText();
    GetSizer()->Fit(this);
  }
}

void BearingDialog::OnAdd(wxCommandEvent& event) {
  auto bearing = cbf::ParseBearing(U(m_bearing->GetValue()));
  if (!bearing) {
    SetError(this, m_error, _("Bearing must be a number from 0 to 360."));
    m_bearing->SetFocus();
    return;
  }
  auto minutes = cbf::ParseTime(U(m_time->GetValue()));
  if (!minutes) {
    SetError(this, m_error, _("Time must be hhmm, e.g. 1432."));
    m_time->SetFocus();
    return;
  }
  auto variation = cbf::ParseVarDev(U(m_variation->GetValue()));
  if (!variation || m_variation->GetValue().Trim().empty()) {
    SetError(this, m_error, _("Enter the variation, e.g. 1.5W, 2E or 0."));
    m_variation->SetFocus();
    return;
  }
  m_entry.name = m_name->GetValue().Trim().Trim(false);
  m_entry.bearing = *bearing;
  m_entry.minutes = *minutes;
  m_entry.variation = *variation;
  EndModal(event.GetId());
}

// --- PositionDialog ---------------------------------------------------------------

PositionDialog::PositionDialog(wxWindow* parent, const cbf::LatLon& position)
    : wxDialog(parent, wxID_ANY, _("Object position")), m_position(position) {
  int gap = FromDIP(8);
  auto* top = new wxBoxSizer(wxVERTICAL);
  top->Add(new wxStaticText(this, wxID_ANY,
                            _("Only change this to type a position,\n"
                              "e.g. from a list of lights.")),
           0, wxALL, gap);
  m_error = MakeError(this);
  top->Add(m_error, 0, wxLEFT | wxRIGHT, gap);

  m_lat_text = W(cbf::FormatLat(position.lat));
  m_lon_text = W(cbf::FormatLon(position.lon));
  auto* grid = new wxFlexGridSizer(2, gap / 2, gap);
  m_lat = AddField(this, grid, _("Latitude"), m_lat_text, 140, "");
  m_lon = AddField(this, grid, _("Longitude"), m_lon_text, 140, "");
  top->Add(grid, 0, wxALL, gap);
  top->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALIGN_RIGHT | wxALL, gap);
  Bind(wxEVT_BUTTON, &PositionDialog::OnOK, this, wxID_OK);
  SetSizerAndFit(top);
  CentreOnParent();
}

void PositionDialog::OnOK(wxCommandEvent&) {
  wxString lat_text = m_lat->GetValue().Trim().Trim(false);
  wxString lon_text = m_lon->GetValue().Trim().Trim(false);
  // Keep the exact chart position unless the user actually changed it.
  if (lat_text == m_lat_text && lon_text == m_lon_text) {
    EndModal(wxID_OK);
    return;
  }
  auto lat = cbf::ParseLatLon(U(lat_text), false);
  auto lon = cbf::ParseLatLon(U(lon_text), true);
  if (!lat || !lon) {
    SetError(this, m_error, _("Cannot read that. Use e.g. 50 47.13N  001 20.40W"));
    return;
  }
  m_position = {*lat, *lon};
  m_changed = true;
  EndModal(wxID_OK);
}

// --- RunDialog ----------------------------------------------------------------------

RunDialog::RunDialog(wxWindow* parent, const wxString& explanation, const RunEntry& initial)
    : wxDialog(parent, wxID_ANY, _("Running fix")), m_entry(initial) {
  int gap = FromDIP(8);
  auto* top = new wxBoxSizer(wxVERTICAL);
  top->Add(new wxStaticText(this, wxID_ANY, explanation), 0, wxALL, gap);
  m_error = MakeError(this);
  top->Add(m_error, 0, wxLEFT | wxRIGHT, gap);

  auto* grid = new wxFlexGridSizer(2, gap / 2, gap);
  m_course = AddField(this, grid, _("Course steered"), initial.course, 70, kDeg + "M");
  m_deviation = AddField(this, grid, _("Deviation"), initial.deviation, 70, _("e.g. 2W"));
  m_speed = AddField(this, grid, _("Speed through water"), initial.speed, 70, _("kn"));
  m_set = AddField(this, grid, _("Tidal set"), initial.set, 70, kDeg + "T");
  m_rate = AddField(this, grid, _("Tidal rate"), initial.rate, 70, _("kn"));
  top->Add(grid, 0, wxALL, gap);

  auto* buttons = new wxBoxSizer(wxHORIZONTAL);
  buttons->Add(new wxButton(this, wxID_CANCEL, _("Cancel")), 0, wxRIGHT, gap);
  auto* ok = new wxButton(this, wxID_OK, _("Plot running fix"));
  buttons->Add(ok, 0);
  ok->SetDefault();
  top->Add(buttons, 0, wxALIGN_RIGHT | wxALL, gap);
  Bind(wxEVT_BUTTON, &RunDialog::OnOK, this, wxID_OK);
  SetSizerAndFit(top);
  m_course->SetFocus();
  CentreOnParent();
}

void RunDialog::OnOK(wxCommandEvent&) {
  m_entry = {m_course->GetValue().Trim().Trim(false), m_deviation->GetValue().Trim().Trim(false),
             m_speed->GetValue().Trim().Trim(false), m_set->GetValue().Trim().Trim(false),
             m_rate->GetValue().Trim().Trim(false)};
  auto course = cbf::ParseBearing(U(m_entry.course));
  if (!course) {
    SetError(this, m_error, _("Course must be a number from 0 to 360."));
    return;
  }
  auto deviation = cbf::ParseVarDev(U(m_entry.deviation));
  if (!deviation) {
    SetError(this, m_error, _("Cannot read the deviation, e.g. 2W, 1E or 0."));
    return;
  }
  auto speed = cbf::ParseNonNegative(U(m_entry.speed));
  if (!speed) {
    SetError(this, m_error, _("Speed must be a number of knots."));
    return;
  }
  std::optional<double> set = 0.0, rate = 0.0;
  if (!m_entry.set.empty() || !m_entry.rate.empty()) {
    set = cbf::ParseBearing(U(m_entry.set));
    rate = cbf::ParseNonNegative(U(m_entry.rate));
  }
  if (!set || !rate) {
    SetError(this, m_error,
             _("Tidal set must be 0-360 and rate in knots\n(leave both empty for no tide)."));
    return;
  }
  m_params = {*course, *deviation, *speed, *set, *rate};
  EndModal(wxID_OK);
}

// --- ResultDialog -------------------------------------------------------------------

ResultDialog::ResultDialog(wxWindow* parent, const wxString& title, const wxString& text)
    : wxDialog(parent, wxID_ANY, title) {
  int gap = FromDIP(8);
  auto* top = new wxBoxSizer(wxVERTICAL);
  top->Add(new wxStaticText(this, wxID_ANY, text), 0, wxALL, gap);
  auto* buttons = new wxBoxSizer(wxHORIZONTAL);
  buttons->Add(new wxButton(this, ID_KEEP_LINES, _("Keep lines")), 0, wxRIGHT, gap);
  auto* fresh = new wxButton(this, ID_NEW_FIX, _("New fix"));
  buttons->Add(fresh, 0);
  fresh->SetDefault();
  top->Add(buttons, 0, wxALIGN_RIGHT | wxALL, gap);
  for (int id : {static_cast<int>(ID_KEEP_LINES), static_cast<int>(ID_NEW_FIX)})
    Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) { EndModal(e.GetId()); }, id);
  SetEscapeId(ID_KEEP_LINES);
  SetSizerAndFit(top);
  CentreOnParent();
}
