// Unit tests for src/cbf_math.cpp. No test framework needed: build with
// tests/CMakeLists.txt and run, or `ctest`. Exit code is the failure count.

#include <clocale>
#include <cmath>
#include <cstdio>
#include <locale>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "cbf_math.h"

using namespace cbf;

namespace {

int g_failures = 0;
std::string g_test;

void Check(bool ok, const char* expr, int line) {
  if (!ok) {
    std::printf("FAIL [%s] line %d: %s\n", g_test.c_str(), line, expr);
    g_failures++;
  }
}

#define CHECK(x) Check((x), #x, __LINE__)
#define CHECK_NEAR(a, b, tol) Check(std::fabs((a) - (b)) < (tol), #a " ~ " #b, __LINE__)
#define CHECK_THROWS(stmt)                       \
  do {                                           \
    bool thrown = false;                         \
    try {                                        \
      stmt;                                      \
    } catch (const std::exception&) {            \
      thrown = true;                             \
    }                                            \
    Check(thrown, "throws: " #stmt, __LINE__);   \
  } while (0)

const double kMPerNm = 1852;
const LatLon kBoat{50.7700, -1.3000};
const std::vector<Observation> kObjects = {
    {"Tower", {50.7950, -1.3400}, 0, {}, {}},
    {"Church", {50.7900, -1.2500}, 0, {}, {}},
    {"Light", {50.7350, -1.2900}, 0, {}, {}},
};

// Bearings a perfect observer at kBoat would take, as compass bearings.
std::vector<Observation> PerfectObs(double variation, double deviation = 0) {
  auto obs = kObjects;
  for (auto& o : obs) o.bearing = TrueToCompass(BearingTo(kBoat, o.object), variation, deviation);
  return obs;
}

bool Inside(const std::vector<LatLon>& v, const LatLon& f) {
  auto side = [](const LatLon& a, const LatLon& b, const LatLon& p) {
    return (b.lon - a.lon) * (p.lat - a.lat) - (b.lat - a.lat) * (p.lon - a.lon);
  };
  double s1 = side(v[0], v[1], f), s2 = side(v[1], v[2], f), s3 = side(v[2], v[0], f);
  return (s1 > 0 && s2 > 0 && s3 > 0) || (s1 < 0 && s2 < 0 && s3 < 0);
}

void VariationAndDeviation() {
  CHECK_NEAR(*ParseVarDev("2.5W"), -2.5, 1e-12);
  CHECK_NEAR(*ParseVarDev("3E"), 3, 1e-12);
  CHECK_NEAR(*ParseVarDev("1 30W"), -1.5, 1e-12);
  CHECK_NEAR(*ParseVarDev("1\xC2\xB0" "30'W"), -1.5, 1e-12);
  CHECK_NEAR(*ParseVarDev("-2"), -2, 1e-12);
  CHECK_NEAR(*ParseVarDev(" 0.9e "), 0.9, 1e-12);
  CHECK_NEAR(*ParseVarDev(""), 0, 1e-12);
  CHECK(!ParseVarDev("lots"));
  CHECK(!ParseVarDev("1 2 3W"));
  // West is best: compass 100, variation 3W -> true 097. East is least.
  CHECK_NEAR(CompassToTrue(100, -3), 97, 1e-12);
  CHECK_NEAR(CompassToTrue(100, 3), 103, 1e-12);
  CHECK_NEAR(CompassToTrue(359, 2), 1, 1e-12);
  CHECK_NEAR(CompassToTrue(100, -2, 1), 99, 1e-12);
}

void AngleOfCutTest() {
  CHECK_NEAR(AngleOfCut(0, 90), 90, 1e-12);
  CHECK_NEAR(AngleOfCut(10, 350), 20, 1e-12);
  CHECK_NEAR(AngleOfCut(45, 225), 0, 1e-12);
  CHECK_NEAR(AngleOfCut(30, 190), 20, 1e-12);
}

void ThreePerfectBearings() {
  FixOptions opt;
  opt.variation = -1.5;
  auto r = ComputeFix(PerfectObs(-1.5), opt);
  CHECK(DistanceNm(r.fix, kBoat) * kMPerNm < 10);
  CHECK(r.cocked_hat.has_value());
  CHECK(r.cocked_hat->max_side_nm * kMPerNm < 10);
  CHECK(r.warnings.empty());
  for (size_t i = 0; i < r.lines.size(); i++) {
    CHECK_NEAR(r.lines[i].range_nm, DistanceNm(kBoat, kObjects[i].object), 0.01);
    CHECK_NEAR(Norm360(r.lines[i].reciprocal - r.lines[i].true_bearing), 180, 1e-9);
  }
}

void TwoBearings() {
  auto obs = PerfectObs(0);
  obs.pop_back();
  auto r = ComputeFix(obs);
  CHECK(DistanceNm(r.fix, kBoat) * kMPerNm < 10);
  CHECK(!r.cocked_hat);
  CHECK(r.crossings.size() == 1);
}

void CockedHatContainsFix() {
  auto obs = PerfectObs(0);
  obs[0].bearing += 2;
  obs[1].bearing -= 1.5;
  obs[2].bearing += 1;
  auto r = ComputeFix(obs);
  CHECK(r.cocked_hat->max_side_nm > 0.05);
  CHECK(r.cocked_hat->area_sq_nm > 0);
  CHECK(Inside(r.cocked_hat->vertices, r.fix));
  CHECK(DistanceNm(r.fix, kBoat) < 0.1);
}

void NearestDanger() {
  auto obs = PerfectObs(0);
  obs[0].bearing += 3;
  obs[1].bearing -= 3;
  FixOptions opt;
  opt.danger = LatLon{50.7600, -1.3200};
  auto r = ComputeFix(obs, opt);
  const auto& v = r.cocked_hat->vertices;
  LatLon best = v[0];
  for (const auto& p : v)
    if (DistanceNm(p, *opt.danger) < DistanceNm(best, *opt.danger)) best = p;
  CHECK(r.cocked_hat->nearest_danger->lat == best.lat);
  CHECK(r.cocked_hat->nearest_danger->lon == best.lon);
}

void MoreThanThree() {
  auto obs = PerfectObs(0);
  Observation mast{"Mast", {50.7600, -1.3500}, 0, {}, {}};
  mast.bearing = BearingTo(kBoat, mast.object);
  obs.push_back(mast);
  auto r = ComputeFix(obs);
  CHECK(r.crossings.size() == 6);
  CHECK(!r.cocked_hat);
  CHECK(DistanceNm(r.fix, kBoat) * kMPerNm < 10);
}

void WrongVariationSign() {
  auto obs = PerfectObs(-10);
  FixOptions good, bad;
  good.variation = -10;
  bad.variation = 10;
  CHECK(ComputeFix(obs, bad).cocked_hat->max_side_nm >
        20 * ComputeFix(obs, good).cocked_hat->max_side_nm + 0.1);
}

void WarningsAndErrors() {
  Observation a{"A", {50.80, -1.30}, 0, {}, {}};
  Observation b{"B", {50.80, -1.29}, 10, {}, {}};
  FixOptions t;
  t.true_bearings = true;
  auto r = ComputeFix({a, b}, t);
  bool poor = false;
  for (const auto& w : r.warnings) poor = poor || w.find("poor angle of cut") != std::string::npos;
  CHECK(poor);
  Observation c{"C", {50.80, -1.29}, 0, {}, {}};
  CHECK_THROWS(ComputeFix({a, c}, t));
  CHECK_THROWS(ComputeFix({a}));
}

void LineEnds() {
  auto r = ComputeFix(PerfectObs(0));
  for (const auto& l : r.lines) {
    auto ends = PositionLineEnds(l.anchor, l.true_bearing, l.range_nm + 0.5);
    CHECK(ends[0].lat == l.object.lat && ends[0].lon == l.object.lon);
    CHECK_NEAR(DistanceNm(ends[0], ends[1]), l.range_nm + 0.5, 0.01);
    CHECK_NEAR(BearingTo(ends[1], ends[0]), l.true_bearing, 0.1);
  }
}

void AdvanceTest() {
  LatLon p{50.8, -1.3};
  LatLon q = Advance(p, 90, 3, 180, 1);
  CHECK_NEAR(DistanceNm(p, q), std::sqrt(10.0), 0.01);
  CHECK_NEAR(BearingTo(p, q), 90 + std::atan2(1.0, 3.0) * 180 / 3.14159265358979, 0.1);
}

void RunningFix() {
  // 090T at 5 kn for 30 min (2.5 nm), tide 180T at 1 kn (0.5 nm).
  LatLon obj{50.8000, -1.3000}, start{50.7700, -1.3600};
  LatLon end = Advance(start, 90, 2.5, 180, 0.5);
  Observation first{"Tower 1000", obj, BearingTo(start, obj), {}, Run{90, 2.5, 180, 0.5}};
  Observation second{"Tower 1030", obj, BearingTo(end, obj), {}, {}};
  FixOptions t;
  t.true_bearings = true;
  auto r = ComputeFix({first, second}, t);
  CHECK(DistanceNm(r.fix, end) * kMPerNm < 20);
  CHECK(r.lines[0].advanced);
  CHECK(r.lines[0].object.lat == obj.lat && r.lines[0].object.lon == obj.lon);
  CHECK_NEAR(DistanceNm(r.lines[0].anchor, obj), std::sqrt(2.5 * 2.5 + 0.25), 0.01);
  CHECK(!r.lines[1].advanced);

  // Ignoring the tide puts the fix in the wrong place.
  first.run = Run{90, 2.5, 0, 0};
  CHECK(DistanceNm(ComputeFix({first, second}, t).fix, end) > 0.2);
}

void ParseLatLonTest() {
  auto near = [](std::optional<double> a, double b) { return a && std::fabs(*a - b) < 1e-9; };
  CHECK(near(ParseLatLon("50.7855", false), 50.7855));
  CHECK(near(ParseLatLon("-1.34", true), -1.34));
  CHECK(near(ParseLatLon("50 47.13N", false), 50 + 47.13 / 60));
  CHECK(near(ParseLatLon("50\xC2\xB0" "47.130'N", false), 50 + 47.13 / 60));
  CHECK(near(ParseLatLon("N50 47.13", false), 50 + 47.13 / 60));
  CHECK(near(ParseLatLon("001\xC2\xB0" "20.400'W", true), -(1 + 20.4 / 60)));
  CHECK(near(ParseLatLon("001 20 24W", true), -(1 + 20 / 60.0 + 24 / 3600.0)));
  CHECK(near(ParseLatLon(" 1 20.4 e ", true), 1 + 20.4 / 60));
  CHECK(!ParseLatLon("50 47N", true));
  CHECK(!ParseLatLon("1 20W", false));
  CHECK(!ParseLatLon("91", false));
  CHECK(!ParseLatLon("50 61N", false));
  CHECK(!ParseLatLon("fifty", false));
  CHECK(!ParseLatLon("", false));
  CHECK(near(ParseLatLon(FormatLon(-1.2345), true), -1.2345) ||
        std::fabs(*ParseLatLon(FormatLon(-1.2345), true) + 1.2345) < 1e-5);
}

void Formatting() {
  CHECK(FormatLat(50.5) == "50\xC2\xB0" "30.000'N");
  CHECK(FormatLon(-1.0833333) == "001\xC2\xB0" "05.000'W");
  CHECK(FormatLat(-0.1) == "00\xC2\xB0" "06.000'S");
  CHECK(FormatLat(49.99999999) == "50\xC2\xB0" "00.000'N");
}

void InputHelpers() {
  CHECK_NEAR(*ParseBearing("045"), 45, 1e-12);
  CHECK_NEAR(*ParseBearing("91.5"), 91.5, 1e-12);
  CHECK_NEAR(*ParseBearing("45M"), 45, 1e-12);
  CHECK_NEAR(*ParseBearing("45\xC2\xB0" "M"), 45, 1e-12);
  CHECK_NEAR(*ParseBearing("360"), 0, 1e-12);
  CHECK(!ParseBearing("361"));
  CHECK(!ParseBearing("abc"));
  CHECK(!ParseBearing(""));
  CHECK(!ParseBearing("-5"));
  CHECK_NEAR(*ParseNonNegative("5.5"), 5.5, 1e-12);
  CHECK(!ParseNonNegative("x"));
  CHECK(!ParseNonNegative(""));
  CHECK(*ParseTime("1432") == 14 * 60 + 32);
  CHECK(*ParseTime("14:32") == 14 * 60 + 32);
  CHECK(*ParseTime("932") == 9 * 60 + 32);
  CHECK(!ParseTime("2500"));
  CHECK(!ParseTime("1260"));
  CHECK(!ParseTime("12"));
  CHECK(!ParseTime("ab12"));
  CHECK(FormatTime(9 * 60 + 5) == "0905");
  CHECK(FormatTime(1440 + 30) == "0030");
  CHECK(MinutesBefore(600, 630) == 30);
  CHECK(MinutesBefore(23 * 60 + 50, 10) == 20);  // across midnight
  CHECK(MinutesBefore(640, 630) == -10);
  CHECK(FormatBearing(45) == "045");
  CHECK(FormatBearing(91.5) == "091.5");
  CHECK(FormatBearing(359.96) == "360");
  CHECK(FormatVarDev(-1.5) == "1.5W");
  CHECK(FormatVarDev(0.9) == "0.9E");
  CHECK(FormatVarDev(0.01) == "0");
}

// OpenCPN may run in a decimal comma locale; output and input must not change.
void DecimalCommaLocale() {
  const char* names[] = {"de_DE.UTF-8", "de_DE.utf8", "German_Germany.1252", "de-DE"};
  const char* set = nullptr;
  for (const char* n : names)
    if ((set = std::setlocale(LC_ALL, n)) != nullptr) break;
  if (!set) {
    std::printf("     (no German locale available; locale test skipped)\n");
    return;
  }
  try {
    std::locale::global(std::locale(set));
  } catch (const std::exception&) {
  }
  CHECK(FormatLat(50.5) == "50\xC2\xB0" "30.000'N");
  CHECK(FormatVarDev(-1.5) == "1.5W");
  CHECK(FormatBearing(91.5) == "091.5");
  CHECK(ParseLatLon("50 47.13N", false) && std::fabs(*ParseLatLon("50 47.13N", false) - (50 + 47.13 / 60)) < 1e-9);
  CHECK(ParseVarDev("2.5W") && std::fabs(*ParseVarDev("2.5W") + 2.5) < 1e-12);
  CHECK(ParseBearing("91.5") && std::fabs(*ParseBearing("91.5") - 91.5) < 1e-12);
  std::setlocale(LC_ALL, "C");
  std::locale::global(std::locale::classic());
}

}  // namespace

int main() {
  const std::vector<std::pair<const char*, std::function<void()>>> tests = {
      {"variation and deviation", VariationAndDeviation},
      {"angle of cut", AngleOfCutTest},
      {"three perfect bearings", ThreePerfectBearings},
      {"two bearings", TwoBearings},
      {"cocked hat contains fix", CockedHatContainsFix},
      {"nearest danger", NearestDanger},
      {"more than three", MoreThanThree},
      {"wrong variation sign", WrongVariationSign},
      {"warnings and errors", WarningsAndErrors},
      {"line ends", LineEnds},
      {"advance", AdvanceTest},
      {"running fix", RunningFix},
      {"parse lat/lon", ParseLatLonTest},
      {"formatting", Formatting},
      {"input helpers", InputHelpers},
      {"decimal comma locale", DecimalCommaLocale},
  };
  for (const auto& t : tests) {
    g_test = t.first;
    int before = g_failures;
    try {
      t.second();
    } catch (const std::exception& e) {
      std::printf("FAIL [%s] exception: %s\n", t.first, e.what());
      g_failures++;
    }
    std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", t.first);
  }
  std::printf("%zu tests, %d failures\n", tests.size(), g_failures);
  return g_failures;
}
