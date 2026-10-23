/***************************************************************************
 * Cross bearing fix maths: position lines, fixes, cocked hats, running fixes.
 *
 * Copyright (C) 2026 Mike G0KAD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ***************************************************************************/

// Plain C++17 with no wxWidgets dependency, so it can be unit tested on its
// own (see tests/).
//
// Conventions
//   Positions:   decimal degrees, north and east positive.
//   Bearings:    degrees 0-360, clockwise from north.
//   Variation and deviation: degrees, EAST POSITIVE, WEST NEGATIVE.
//     True = Compass + Deviation + Variation   ("East is least, West is
//     best": going True -> Compass you subtract East and add West, so going
//     Compass -> True you add East and subtract West.)
//   Distances:   nautical miles.
//
// Geometry uses a local flat-earth (equirectangular) projection centred on
// the observed objects. For objects a few miles away this is accurate to a
// few metres, far better than a hand bearing compass.

#ifndef CBF_MATH_H_
#define CBF_MATH_H_

#include <optional>
#include <string>
#include <vector>

namespace cbf {

struct LatLon {
  double lat = 0;
  double lon = 0;
};

/** The boat's run since a bearing was taken, for a running fix. */
struct Run {
  double course_true = 0;
  double distance_nm = 0;
  double set_true = 0;
  double drift_nm = 0;
};

struct Observation {
  std::string name;
  LatLon object;              ///< charted position of the object
  double bearing = 0;         ///< observed bearing TO the object
  std::optional<double> deviation;  ///< overrides FixOptions::deviation
  std::optional<Run> run;     ///< running fix: advance this line by the run
};

struct FixOptions {
  double variation = 0;       ///< east positive
  double deviation = 0;       ///< east positive, applied to every bearing
  bool true_bearings = false; ///< bearings are already true
  std::optional<LatLon> danger;  ///< report the cocked hat corner nearest it
};

struct LineResult {
  std::string name;
  LatLon object;              ///< the charted object
  LatLon anchor;              ///< where the line starts: object or advanced point
  bool advanced = false;
  double observed_bearing = 0;
  double true_bearing = 0;
  double reciprocal = 0;
  double range_nm = 0;        ///< from the fix to the anchor, along the line
  double residual_nm = 0;     ///< perpendicular distance of the fix from the line
};

struct Crossing {
  int a = 0;
  int b = 0;
  LatLon pos;
  double angle_of_cut = 0;
};

struct CockedHat {
  std::vector<LatLon> vertices;
  double area_sq_nm = 0;
  double max_side_nm = 0;
  LatLon centroid;
  std::optional<LatLon> nearest_danger;
};

struct FixResult {
  LatLon fix;                 ///< least squares point; inside the cocked hat
  std::vector<LineResult> lines;
  std::vector<Crossing> crossings;
  std::optional<CockedHat> cocked_hat;  ///< exactly three lines only
  double spread_nm = 0;       ///< furthest crossing from the fix
  std::vector<std::string> warnings;
};

double Norm360(double deg);

/**
 * Read a variation or deviation: "2.5W", "3E", "1 30W", "1°30'W", "-2" or
 * "" (zero). Returns degrees east positive, or nullopt if unreadable.
 */
std::optional<double> ParseVarDev(const std::string& text);

double CompassToTrue(double compass, double variation, double deviation = 0);
double TrueToCompass(double true_bearing, double variation,
                     double deviation = 0);

/** Angle between two position lines, 0-90 degrees. */
double AngleOfCut(double b1, double b2);

/**
 * Compute a fix from two or more bearings. Throws std::invalid_argument for
 * fewer than two observations and std::runtime_error when all lines are
 * parallel.
 */
FixResult ComputeFix(const std::vector<Observation>& observations,
                     const FixOptions& options = FixOptions());

/** End points of a line drawn from start along the reciprocal bearing. */
std::vector<LatLon> PositionLineEnds(const LatLon& start, double true_bearing,
                                     double length_nm);

/** Move a position by a run (course and distance) plus tidal set and drift. */
LatLon Advance(const LatLon& pos, double course_true, double distance_nm,
               double set_true = 0, double drift_nm = 0);

/** Initial great circle bearing from a to b (true). */
double BearingTo(const LatLon& a, const LatLon& b);

/** Great circle distance in nautical miles. */
double DistanceNm(const LatLon& a, const LatLon& b);

/**
 * Read a typed latitude or longitude: decimal ("50.7855", "-1.34"), degrees
 * and minutes ("50 47.13N", "50°47.130'N", "N50 47.13") or degrees, minutes
 * and seconds ("001 20 24W"). Returns nullopt if unreadable or out of range.
 */
std::optional<double> ParseLatLon(const std::string& text, bool is_lon);

/** 50.123456 -> "50°07.407'N" (UTF-8). */
std::string FormatLat(double lat);
std::string FormatLon(double lon);

}  // namespace cbf

#endif  // CBF_MATH_H_
