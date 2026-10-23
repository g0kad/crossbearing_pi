/***************************************************************************
 * Cross bearing fix maths. See cbf_math.h for conventions.
 *
 * Copyright (C) 2026 Mike G0KAD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 ***************************************************************************/

#include "cbf_math.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace cbf {

namespace {

const double kNmPerDeg = 60.0;
const double kPi = 3.14159265358979323846;
const double kDeg = kPi / 180.0;
const char* const kDegSign = "\xC2\xB0";  // UTF-8 degree sign

struct XY {
  double x = 0;  // east, nm
  double y = 0;  // north, nm
};

// Local equirectangular projection about an origin.
class Projection {
public:
  explicit Projection(const LatLon& origin)
      : m_origin(origin), m_k(std::cos(origin.lat * kDeg)) {}

  XY ToXY(const LatLon& p) const {
    return {(p.lon - m_origin.lon) * m_k * kNmPerDeg,
            (p.lat - m_origin.lat) * kNmPerDeg};
  }

  LatLon ToLL(const XY& q) const {
    return {m_origin.lat + q.y / kNmPerDeg,
            m_origin.lon + q.x / (m_k * kNmPerDeg)};
  }

private:
  LatLon m_origin;
  double m_k;
};

XY DirVec(double brg) { return {std::sin(brg * kDeg), std::cos(brg * kDeg)}; }

double Dist(const XY& a, const XY& b) { return std::hypot(a.x - b.x, a.y - b.y); }

// A position line through point p (the object or advanced point) with unit
// direction u of the OBSERVED bearing. The observer is at p - t*u, t > 0.
struct Line {
  XY p;
  XY u;
  std::string name;
  LatLon object;
  LatLon anchor;
  bool advanced = false;
  double bearing = 0;
  double true_bearing = 0;
};

struct Intersection {
  XY xy;
  double t1 = 0;
  double t2 = 0;
};

std::optional<Intersection> Intersect(const Line& l1, const Line& l2) {
  // p1 - t1*u1 = p2 - t2*u2  =>  t1*u1 - t2*u2 = p1 - p2
  double det = -l1.u.x * l2.u.y + l1.u.y * l2.u.x;
  if (std::fabs(det) < 1e-9) return std::nullopt;
  double dx = l1.p.x - l2.p.x, dy = l1.p.y - l2.p.y;
  double t1 = (-dx * l2.u.y + dy * l2.u.x) / det;
  double t2 = (l1.u.x * dy - l1.u.y * dx) / det;
  return Intersection{{l1.p.x - t1 * l1.u.x, l1.p.y - t1 * l1.u.y}, t1, t2};
}

// Point minimising the sum of squared perpendicular distances to the lines.
std::optional<XY> LeastSquaresPoint(const std::vector<Line>& lines) {
  double a = 0, b = 0, c = 0, d = 0, e = 0;
  for (const auto& l : lines) {
    XY n{l.u.y, -l.u.x};  // unit normal
    double k = n.x * l.p.x + n.y * l.p.y;
    a += n.x * n.x;
    b += n.x * n.y;
    c += n.y * n.y;
    d += n.x * k;
    e += n.y * k;
  }
  double det = a * c - b * b;
  if (std::fabs(det) < 1e-12) return std::nullopt;
  return XY{(c * d - b * e) / det, (a * e - b * d) / det};
}

double PerpDistance(const Line& l, const XY& q) {
  return std::fabs(l.u.x * (q.y - l.p.y) - l.u.y * (q.x - l.p.x));
}

std::string Trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string Upper(std::string s) {
  for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return s;
}

// Remove degree, minute and second marks (ASCII and UTF-8) so only digits,
// signs, dots, spaces and hemisphere letters remain.
std::string StripMarks(const std::string& s) {
  static const char* const kMarks[] = {"\xC2\xB0", "\xE2\x80\xB2",
                                       "\xE2\x80\xB3", "'", "\""};
  std::string out = s;
  for (const char* m : kMarks) {
    std::string mark(m);
    size_t pos;
    while ((pos = out.find(mark)) != std::string::npos) out.replace(pos, mark.size(), " ");
  }
  return out;
}

// Split into unsigned decimal numbers; false if anything else is present
// apart from spaces and the given allowed letters.
bool SplitNumbers(const std::string& s, const std::string& allowed,
                  std::vector<double>* nums) {
  size_t i = 0;
  while (i < s.size()) {
    char ch = s[i];
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      size_t j = i;
      while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) j++;
      if (j < s.size() && s[j] == '.') {
        size_t k = j + 1;
        while (k < s.size() && std::isdigit(static_cast<unsigned char>(s[k]))) k++;
        if (k == j + 1) return false;  // "12." with no decimals
        j = k;
      }
      nums->push_back(std::stod(s.substr(i, j - i)));
      i = j;
    } else if (ch == ' ' || allowed.find(ch) != std::string::npos) {
      i++;
    } else {
      return false;
    }
  }
  return true;
}

std::string FormatDDM(double v, int deg_width, char pos, char neg) {
  char h = v < 0 ? neg : pos;
  double a = std::fabs(v);
  int d = static_cast<int>(std::floor(a));
  double m = (a - d) * 60;
  if (m >= 59.9995) {
    d += 1;
    m = 0;
  }
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%0*d%s%06.3f'%c", deg_width, d, kDegSign, m, h);
  return buf;
}

}  // namespace

double Norm360(double deg) {
  double d = std::fmod(deg, 360.0);
  return d < 0 ? d + 360 : d;
}

std::optional<double> ParseVarDev(const std::string& text) {
  std::string s = Upper(Trim(StripMarks(text)));
  if (s.empty()) return 0.0;
  bool neg = false;
  if (s[0] == '+' || s[0] == '-') {
    neg = s[0] == '-';
    s = Trim(s.substr(1));
  }
  char hemi = 0;
  if (!s.empty() && (s.back() == 'E' || s.back() == 'W')) {
    hemi = s.back();
    s = Trim(s.substr(0, s.size() - 1));
  }
  std::vector<double> nums;
  if (!SplitNumbers(s, "", &nums) || nums.empty() || nums.size() > 2) return std::nullopt;
  double deg = nums[0] + (nums.size() > 1 ? nums[1] / 60 : 0);
  if (neg) deg = -deg;
  if (hemi == 'W') deg = -std::fabs(deg);
  if (hemi == 'E') deg = std::fabs(deg);
  return deg;
}

double CompassToTrue(double compass, double variation, double deviation) {
  return Norm360(compass + deviation + variation);
}

double TrueToCompass(double true_bearing, double variation, double deviation) {
  return Norm360(true_bearing - deviation - variation);
}

double AngleOfCut(double b1, double b2) {
  double a = std::fmod(std::fabs(Norm360(b1 - b2)), 180.0);
  return a > 90 ? 180 - a : a;
}

FixResult ComputeFix(const std::vector<Observation>& observations,
                     const FixOptions& options) {
  if (observations.size() < 2)
    throw std::invalid_argument("Need at least two bearings for a fix");

  // A line's anchor is its object, moved by the run for a running fix.
  std::vector<LatLon> anchors;
  LatLon mean;
  for (const auto& o : observations) {
    LatLon a = o.run ? Advance(o.object, o.run->course_true, o.run->distance_nm,
                               o.run->set_true, o.run->drift_nm)
                     : o.object;
    anchors.push_back(a);
    mean.lat += a.lat / observations.size();
    mean.lon += a.lon / observations.size();
  }
  Projection proj(mean);
  FixResult result;

  std::vector<Line> lines;
  for (size_t i = 0; i < observations.size(); i++) {
    const auto& o = observations[i];
    Line l;
    l.name = o.name.empty() ? "Object " + std::to_string(i + 1) : o.name;
    l.object = o.object;
    l.anchor = anchors[i];
    l.advanced = o.run.has_value();
    l.bearing = o.bearing;
    l.true_bearing = options.true_bearings
                         ? Norm360(o.bearing)
                         : CompassToTrue(o.bearing, options.variation,
                                         o.deviation.value_or(options.deviation));
    l.p = proj.ToXY(anchors[i]);
    l.u = DirVec(l.true_bearing);
    lines.push_back(l);
  }

  std::vector<XY> crossing_xy;
  for (size_t i = 0; i < lines.size(); i++) {
    for (size_t j = i + 1; j < lines.size(); j++) {
      std::string pair = lines[i].name + " / " + lines[j].name;
      double cut = AngleOfCut(lines[i].true_bearing, lines[j].true_bearing);
      auto x = Intersect(lines[i], lines[j]);
      if (!x) {
        result.warnings.push_back(lines[i].name + " and " + lines[j].name +
                                  " lines are parallel");
        continue;
      }
      if (cut < 30) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f", cut);
        result.warnings.push_back(pair + ": poor angle of cut (" + buf + kDegSign + ")");
      }
      if (x->t1 < 0 || x->t2 < 0) {
        result.warnings.push_back(pair +
                                  ": lines cross on the far side of an object - "
                                  "check bearings and variation sign");
      }
      result.crossings.push_back({static_cast<int>(i), static_cast<int>(j),
                                  proj.ToLL(x->xy), cut});
      crossing_xy.push_back(x->xy);
    }
  }

  auto fix_xy = LeastSquaresPoint(lines);
  if (!fix_xy) throw std::runtime_error("Position lines are all parallel - no fix");
  result.fix = proj.ToLL(*fix_xy);

  for (const auto& c : crossing_xy)
    result.spread_nm = std::max(result.spread_nm, Dist(c, *fix_xy));

  if (lines.size() == 3 && crossing_xy.size() == 3) {
    const auto& v = crossing_xy;
    CockedHat hat;
    for (const auto& c : result.crossings) hat.vertices.push_back(c.pos);
    hat.area_sq_nm = std::fabs((v[1].x - v[0].x) * (v[2].y - v[0].y) -
                               (v[2].x - v[0].x) * (v[1].y - v[0].y)) / 2;
    hat.max_side_nm = std::max({Dist(v[0], v[1]), Dist(v[1], v[2]), Dist(v[2], v[0])});
    hat.centroid = proj.ToLL({(v[0].x + v[1].x + v[2].x) / 3, (v[0].y + v[1].y + v[2].y) / 3});
    if (options.danger) {
      XY d = proj.ToXY(*options.danger);
      int best = 0;
      for (int k = 1; k < 3; k++)
        if (Dist(v[k], d) < Dist(v[best], d)) best = k;
      hat.nearest_danger = hat.vertices[best];
    }
    result.cocked_hat = hat;
  }

  for (const auto& l : lines) {
    LineResult r;
    r.name = l.name;
    r.object = l.object;
    r.anchor = l.anchor;
    r.advanced = l.advanced;
    r.observed_bearing = l.bearing;
    r.true_bearing = l.true_bearing;
    r.reciprocal = Norm360(l.true_bearing + 180);
    r.range_nm = (l.p.x - fix_xy->x) * l.u.x + (l.p.y - fix_xy->y) * l.u.y;
    r.residual_nm = PerpDistance(l, *fix_xy);
    result.lines.push_back(r);
  }
  return result;
}

std::vector<LatLon> PositionLineEnds(const LatLon& start, double true_bearing,
                                     double length_nm) {
  Projection proj(start);
  XY u = DirVec(true_bearing);
  double len = std::max(length_nm, 0.0);
  return {start, proj.ToLL({-len * u.x, -len * u.y})};
}

LatLon Advance(const LatLon& pos, double course_true, double distance_nm,
               double set_true, double drift_nm) {
  Projection proj(pos);
  XY c = DirVec(course_true), s = DirVec(set_true);
  return proj.ToLL({c.x * distance_nm + s.x * drift_nm,
                    c.y * distance_nm + s.y * drift_nm});
}

double BearingTo(const LatLon& a, const LatLon& b) {
  double p1 = a.lat * kDeg, p2 = b.lat * kDeg, dl = (b.lon - a.lon) * kDeg;
  double y = std::sin(dl) * std::cos(p2);
  double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
  return Norm360(std::atan2(y, x) / kDeg);
}

double DistanceNm(const LatLon& a, const LatLon& b) {
  double p1 = a.lat * kDeg, p2 = b.lat * kDeg;
  double dp = p2 - p1, dl = (b.lon - a.lon) * kDeg;
  double h = std::sin(dp / 2) * std::sin(dp / 2) +
             std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  return 2 * std::asin(std::sqrt(h)) / kDeg * kNmPerDeg;
}

std::optional<double> ParseLatLon(const std::string& text, bool is_lon) {
  std::string s = Upper(Trim(StripMarks(text)));
  bool neg = false;
  if (!s.empty() && s[0] == '-') {
    neg = true;
    s = s.substr(1);
  }
  char hemi = 0;
  for (char ch : s) {
    if (ch == 'N' || ch == 'S' || ch == 'E' || ch == 'W') {
      if (hemi) return std::nullopt;  // two hemisphere letters
      hemi = ch;
    }
  }
  if (hemi && (is_lon ? (hemi == 'N' || hemi == 'S') : (hemi == 'E' || hemi == 'W')))
    return std::nullopt;
  if (hemi == 'S' || hemi == 'W') neg = true;

  std::vector<double> nums;
  if (!SplitNumbers(s, "NSEW", &nums) || nums.empty() || nums.size() > 3) return std::nullopt;
  double v = nums[0];
  if (nums.size() > 1) {
    if (nums[1] >= 60) return std::nullopt;
    v += nums[1] / 60;
  }
  if (nums.size() > 2) {
    if (nums[2] >= 60) return std::nullopt;
    v += nums[2] / 3600;
  }
  if (v > (is_lon ? 180 : 90)) return std::nullopt;
  return neg ? -v : v;
}

std::string FormatLat(double lat) { return FormatDDM(lat, 2, 'N', 'S'); }
std::string FormatLon(double lon) { return FormatDDM(lon, 3, 'E', 'W'); }

}  // namespace cbf
