#pragma once

#include <charconv>
#include <cmath>
#include <string>

namespace minera {

// The text SQLite 3.45 gives a REAL (sqlite3_column_text, "%!.15g"): 15
// significant digits rounded half up from the value's exact decimal
// expansion, %g's choice between fixed and exponent form (exponent below -4
// or 15 and above), trailing zeros dropped but one digit kept after the
// point ("2.0", "1.0e+20", "1.5e-05"), "Inf"/"-Inf", and 0.0 without a
// sign. Newer libraries (3.47 on, Windows' winsqlite3) spell a value that
// does not round-trip in 15 digits with 17, through sqlite3_snprintf too,
// so the reader formats REAL cells itself (round ED, the Windows CI runs of
// rounds EB and EC).
inline std::string sqlite_legacy_real_text(double x) {
  if (std::isnan(x)) return "NaN";
  if (std::isinf(x)) return x < 0 ? "-Inf" : "Inf";
  if (x == 0) return "0.0";
  // Enough digits that the expansion is exact for any REAL a time column
  // holds; the 16th digit then decides the rounding, as in SQLite.
  char buffer[128];
  const auto converted =
      std::to_chars(buffer, buffer + sizeof buffer, std::fabs(x), std::chars_format::scientific, 60);
  const std::string scientific(buffer, converted.ptr);
  const auto e = scientific.find('e');
  std::string digits = scientific.substr(0, 1) + scientific.substr(2, e - 2);
  int exponent = std::stoi(scientific.substr(e + 1));
  const bool up = digits[15] >= '5';
  digits.resize(15);
  if (up) {
    int i = 14;
    while (i >= 0 && digits[static_cast<std::size_t>(i)] == '9') digits[static_cast<std::size_t>(i--)] = '0';
    if (i < 0) {
      digits.insert(digits.begin(), '1');
      digits.pop_back();
      ++exponent;
    } else {
      ++digits[static_cast<std::size_t>(i)];
    }
  }
  while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
  std::string out = x < 0 ? "-" : "";
  if (exponent < -4 || exponent >= 15) {
    out += digits[0];
    out += '.';
    out += digits.size() > 1 ? digits.substr(1) : "0";
    out += exponent < 0 ? "e-" : "e+";
    const int magnitude = std::abs(exponent);
    if (magnitude < 10) out += '0';
    out += std::to_string(magnitude);
    return out;
  }
  if (exponent < 0) {
    out += "0.";
    out.append(static_cast<std::size_t>(-exponent - 1), '0');
    out += digits;
    return out;
  }
  const auto whole = static_cast<std::size_t>(exponent) + 1;
  if (digits.size() <= whole) {
    out += digits;
    out.append(whole - digits.size(), '0');
    out += ".0";
  } else {
    out += digits.substr(0, whole);
    out += '.';
    out += digits.substr(whole);
  }
  return out;
}

}  // namespace minera
