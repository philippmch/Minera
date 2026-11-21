#include "pandas_to_datetime.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

namespace minera::pandas_to_datetime {
namespace {

// Thrown inside the model: the column is outside it.
struct Outside {};
// An integer past 18 digits inside dateutil's reading: outside the model for
// the error pandas raises, but pandas raises (round EC).
struct LongInteger : Outside {};
// datetime.replace overflowed (a field past a C int).
struct Overflow {};
// pandas' error for the column.
struct Error {
  std::string message;
};

constexpr const char* kSuggestions =
    ". You might want to try:\n    - passing `format` if your strings have a consistent format;\n    - "
    "passing `format='ISO8601'` if your strings are all ISO8601 but not necessarily in exactly the same "
    "format;\n    - passing `format='mixed'`, and the format will be inferred for each element "
    "individually. You might want to use `dayfirst` alongside this.";
constexpr const char* kMixed =
    "ValueError: Mixed timezones detected. Pass utc=True in to_datetime or tz='UTC' in DatetimeIndex to "
    "convert to a common timezone.";

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
// The covered texts hold printable ASCII and tabs only.
bool is_space(char c) { return c == ' ' || c == '\t'; }
bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
char to_lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string lower(std::string_view text) {
  std::string out(text);
  for (auto& c : out) c = to_lower(c);
  return out;
}

bool all_digits(std::string_view text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), is_digit);
}

// int() of a token or a slice of one: digits only (anything else is
// Python's ValueError, signalled by nullopt); past 18 digits outside.
std::optional<std::int64_t> py_int(std::string_view text) {
  if (!all_digits(text)) return std::nullopt;
  std::size_t start = 0;
  while (start + 1 < text.size() && text[start] == '0') ++start;
  if (text.size() - start > 18) throw LongInteger{};
  std::int64_t value = 0;
  for (std::size_t i = start; i < text.size(); ++i) value = value * 10 + (text[i] - '0');
  return value;
}

struct ParseFailure {};  // dateutil's ValueError or IndexError inside _parse

std::int64_t need_int(std::string_view text) {
  const auto value = py_int(text);
  if (!value) throw ParseFailure{};
  return *value;
}

bool leap(std::int64_t year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int month_days(std::int64_t year, std::int64_t month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && leap(year) ? 29 : kDays[month - 1];
}

std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

// Monday 0 .. Sunday 6.
int weekday_of(std::int64_t year, int month, int day) {
  const auto days = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  return static_cast<int>(((days % 7) + 7 + 3) % 7);
}

std::string two(std::int64_t value) {
  std::string out = std::to_string(value);
  return out.size() < 2 ? "0" + out : out;
}

// ------------------------------------------------------------ lexers

// dateutil's _timelex.split.
std::vector<std::string> dateutil_lex(std::string_view s) {
  std::vector<std::string> out;
  std::size_t i = 0;
  const std::size_t n = s.size();
  while (i < n) {
    std::string token;
    enum { kNone, kWord, kNumber, kWordDot, kNumberDot } state = kNone;
    bool seen_letters = false;
    while (i < n) {
      const char c = s[i];
      if (state == kNone) {
        token = std::string(1, c);
        ++i;
        if (is_alpha(c)) {
          state = kWord;
        } else if (is_digit(c)) {
          state = kNumber;
        } else if (is_space(c)) {
          token = " ";
          break;
        } else {
          break;
        }
      } else if (state == kWord) {
        seen_letters = true;
        if (is_alpha(c)) {
          token += c;
          ++i;
        } else if (c == '.') {
          token += c;
          state = kWordDot;
          ++i;
        } else {
          break;
        }
      } else if (state == kNumber) {
        if (is_digit(c)) {
          token += c;
          ++i;
        } else if (c == '.' || (c == ',' && token.size() >= 2)) {
          token += c;
          state = kNumberDot;
          ++i;
        } else {
          break;
        }
      } else if (state == kWordDot) {
        seen_letters = true;
        if (c == '.' || is_alpha(c)) {
          token += c;
          ++i;
        } else if (is_digit(c) && token.back() == '.') {
          token += c;
          state = kNumberDot;
          ++i;
        } else {
          break;
        }
      } else {
        if (c == '.' || is_digit(c)) {
          token += c;
          ++i;
        } else if (is_alpha(c) && token.back() == '.') {
          token += c;
          state = kWordDot;
          ++i;
        } else {
          break;
        }
      }
    }
    if (token.empty()) break;
    std::vector<std::string> pending;
    if ((state == kWordDot || state == kNumberDot) &&
        (seen_letters || std::count(token.begin(), token.end(), '.') > 1 || token.back() == '.' ||
         token.back() == ',')) {
      // re.split("([.,])"): the pieces and the separators, empty pieces dropped.
      std::vector<std::string> parts;
      std::string piece;
      for (const char c : token) {
        if (c == '.' || c == ',') {
          parts.push_back(piece);
          parts.emplace_back(1, c);
          piece.clear();
        } else {
          piece += c;
        }
      }
      parts.push_back(piece);
      token = parts[0];
      for (std::size_t k = 1; k < parts.size(); ++k)
        if (!parts[k].empty()) pending.push_back(parts[k]);
    }
    if (state == kNumberDot && token.find('.') == std::string::npos)
      std::replace(token.begin(), token.end(), ',', '.');
    out.push_back(token);
    for (auto& piece : pending) out.push_back(std::move(piece));
  }
  return out;
}

bool punct_run(char c) { return c == '.' || c == '/' || c == ':'; }
bool other_run(char c) { return !is_digit(c) && !is_alpha(c) && !punct_run(c) && !is_space(c); }

// pandas' own lexer of the format guess: a regular expression's tokens
// (one whitespace character, a decimal number standing alone, digits,
// letters, runs of "./:", runs of anything else), then "59", ",", "456"
// joined as "59.456".
std::vector<std::string> pandas_lex(std::string_view s) {
  std::vector<std::string> tokens;
  std::size_t i = 0;
  const std::size_t n = s.size();
  const auto digit_run = [&](std::size_t at) {
    std::size_t end = at;
    while (end < n && is_digit(s[end])) ++end;
    return end;
  };
  while (i < n) {
    const char c = s[i];
    std::size_t end = i + 1;
    if (is_space(c)) {
      end = i + 1;
    } else if (is_digit(c)) {
      end = digit_run(i);
      const bool behind_ok = i == 0 || !(s[i - 1] == '.' || is_digit(s[i - 1]));
      if (behind_ok && end < n && s[end] == '.' && end + 1 < n && is_digit(s[end + 1])) {
        const auto fraction_end = digit_run(end + 1);
        if (fraction_end == n || s[fraction_end] != '.') end = fraction_end;
      }
    } else if (is_alpha(c)) {
      while (end < n && is_alpha(s[end])) ++end;
    } else if (punct_run(c)) {
      while (end < n && punct_run(s[end])) ++end;
    } else {
      while (end < n && other_run(s[end])) ++end;
    }
    tokens.emplace_back(s.substr(i, end - i));
    i = end;
  }
  // The kludge walks a copy of the list: a joined token's neighbours are
  // still read from the copy.
  const auto copy = tokens;
  std::vector<bool> dropped(tokens.size(), false);
  for (std::size_t k = 0; k + 2 < copy.size(); ++k) {
    const std::string& token = copy[k];
    if (all_digits(token) && !dropped[k + 1] && tokens[k + 1] == "," && !dropped[k + 2] &&
        all_digits(tokens[k + 2])) {
      tokens[k] = token + "." + tokens[k + 2];
      dropped[k] = false;
      dropped[k + 1] = dropped[k + 2] = true;
    }
  }
  std::vector<std::string> out;
  for (std::size_t k = 0; k < tokens.size(); ++k)
    if (!dropped[k]) out.push_back(tokens[k]);
  return out;
}

// ------------------------------------------------------------ dateutil

const std::array<std::vector<const char*>, 7> kWeekdays = {{{"mon", "monday"},
                                                            {"tue", "tuesday"},
                                                            {"wed", "wednesday"},
                                                            {"thu", "thursday"},
                                                            {"fri", "friday"},
                                                            {"sat", "saturday"},
                                                            {"sun", "sunday"}}};
const std::array<std::vector<const char*>, 12> kMonths = {{{"jan", "january"},
                                                           {"feb", "february"},
                                                           {"mar", "march"},
                                                           {"apr", "april"},
                                                           {"may", "may"},
                                                           {"jun", "june"},
                                                           {"jul", "july"},
                                                           {"aug", "august"},
                                                           {"sep", "sept", "september"},
                                                           {"oct", "october"},
                                                           {"nov", "november"},
                                                           {"dec", "december"}}};
const char* const kMonthAbbr[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char* const kMonthFull[] = {"January", "February", "March",     "April",   "May",      "June",
                                  "July",    "August",   "September", "October", "November", "December"};
const char* const kWeekdayAbbr[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char* const kWeekdayFull[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

template <std::size_t N>
std::optional<int> lookup(const std::array<std::vector<const char*>, N>& table, std::string_view name) {
  const auto key = lower(name);
  for (std::size_t i = 0; i < N; ++i)
    for (const auto* candidate : table[i])
      if (key == candidate) return static_cast<int>(i);
  return std::nullopt;
}

std::optional<int> weekday(std::string_view t) { return lookup(kWeekdays, t); }
std::optional<int> month(std::string_view t) {
  const auto index = lookup(kMonths, t);
  return index ? std::optional<int>(*index + 1) : std::nullopt;
}
std::optional<int> hms(std::string_view t) {
  const auto key = lower(t);
  if (key == "h" || key == "hour" || key == "hours") return 0;
  if (key == "m" || key == "minute" || key == "minutes") return 1;
  if (key == "s" || key == "second" || key == "seconds") return 2;
  return std::nullopt;
}
std::optional<int> ampm(std::string_view t) {
  const auto key = lower(t);
  if (key == "am" || key == "a") return 0;
  if (key == "pm" || key == "p") return 1;
  return std::nullopt;
}
bool jump(std::string_view t) {
  static const std::set<std::string> kJump = {" ",  ".",  ",",  ";",  "-",  "/",  "'",  "at", "on",
                                              "and", "ad", "m",  "t",  "of", "st", "nd", "rd", "th"};
  return kJump.count(lower(t)) != 0;
}
bool utc_zone(std::string_view t) { return t == "UTC" || t == "GMT" || t == "Z" || t == "z"; }

// A numeric token's Decimal: its integer part and fraction digits (no
// trailing zeros).
struct Dec {
  std::int64_t whole = 0;
  std::string fraction;
};

std::optional<Dec> to_dec(std::string_view t) {
  const auto dot = t.find('.');
  const auto whole = t.substr(0, dot);
  const auto fraction = dot == std::string_view::npos ? std::string_view() : t.substr(dot + 1);
  if (whole.empty() && fraction.empty()) return std::nullopt;
  if (!std::all_of(whole.begin(), whole.end(), is_digit) ||
      !std::all_of(fraction.begin(), fraction.end(), is_digit))
    return std::nullopt;
  if (dot != std::string_view::npos && fraction.find('.') != std::string_view::npos) return std::nullopt;
  Dec value;
  if (!whole.empty()) value.whole = *py_int(whole);
  value.fraction = std::string(fraction);
  while (!value.fraction.empty() && value.fraction.back() == '0') value.fraction.pop_back();
  return value;
}

Dec need_dec(std::string_view t) {
  const auto value = to_dec(t);
  if (!value) throw ParseFailure{};
  return *value;
}

// value <= n, value > n for an integer n >= 0.
bool dec_le(const Dec& value, std::int64_t n) {
  return value.whole < n || (value.whole == n && value.fraction.empty());
}
bool dec_ge(const Dec& value, std::int64_t n) { return value.whole >= n; }

// int(60 * (value % 1)).
std::int64_t sixty_times_fraction(const std::string& fraction) {
  // 60 * 0.d1d2..dk = (60 * D) / 10^k; the integer part of the product's digits.
  std::string product;
  int carry = 0;
  for (auto it = fraction.rbegin(); it != fraction.rend(); ++it) {
    const int digit_value = (*it - '0') * 60 + carry;
    product.insert(product.begin(), static_cast<char>('0' + digit_value % 10));
    carry = digit_value / 10;
  }
  std::string head = std::to_string(carry);
  // The product is head followed by `product` (k digits); the integer part is head.
  return std::stoll(head);
}

thread_local std::int64_t current_year_value = 2026;
// Round EL: errors="coerce": a value that does not parse is NaT.
thread_local bool coerce_value = false;
// Today's date for texts that start with a time (pandas reads them on it).
thread_local Date today_value{2026, 1, 1};

std::int64_t convert_year(std::int64_t year, bool century) {
  if (year < 100 && !century) {
    year += current_year_value / 100 * 100;
    if (year >= current_year_value + 50) year -= 100;
    else if (year < current_year_value - 50) year += 100;
  }
  return year;
}

struct Res {
  std::optional<std::int64_t> year, month, day, hour, minute, second, microsecond;
  std::optional<int> weekday, ampm;
  std::optional<std::string> tzname;
  std::optional<std::int64_t> tzoffset;
  bool century = false;
};

struct Ymd {
  std::vector<std::int64_t> values;
  bool century = false;
  std::optional<std::size_t> d, m, y;

  std::size_t size() const { return values.size(); }
  bool could_be_day(const Dec& value) const {
    if (d) return false;
    if (!m) return dec_ge(value, 1) && dec_le(value, 31);
    if (!y) return dec_ge(value, 1) && dec_le(value, month_days(2000, values[*m]));
    return dec_ge(value, 1) && dec_le(value, month_days(values[*y], values[*m]));
  }
  void label(char which) {
    if (which == 'M') {
      if (m) throw ParseFailure{};
      m = values.size() - 1;
    } else if (which == 'Y') {
      if (y) throw ParseFailure{};
      y = values.size() - 1;
    }
  }
  void append_text(std::string_view text, char which = 0) {
    if (all_digits(text) && text.size() > 2) {
      century = true;
      which = 'Y';
    }
    values.push_back(need_int(text));
    label(which);
  }
  void append_dec(const Dec& value) {
    char which = 0;
    if (!dec_le(value, 100)) {
      century = true;
      which = 'Y';
    }
    values.push_back(value.whole);
    label(which);
  }
  void append_month(int value) {
    values.push_back(value);
    label('M');
  }
  // (year, month, day).
  std::array<std::optional<std::int64_t>, 3> resolve() const {
    std::map<char, std::size_t> strids;
    if (y) strids['y'] = *y;
    if (m) strids['m'] = *m;
    if (d) strids['d'] = *d;
    const auto n = values.size();
    std::optional<std::int64_t> year, month_value, day_value;
    if ((n == strids.size() && n > 0) || (n == 3 && strids.size() == 2)) {
      if (n == 3 && strids.size() == 2) {
        std::size_t missing = 0;
        for (std::size_t x = 0; x < 3; ++x) {
          bool used = false;
          for (const auto& [key, index] : strids) used = used || index == x;
          if (!used) missing = x;
        }
        for (const char key : {'y', 'm', 'd'})
          if (!strids.count(key)) {
            strids[key] = missing;
            break;
          }
      }
      if (strids.count('y')) year = values[strids['y']];
      if (strids.count('m')) month_value = values[strids['m']];
      if (strids.count('d')) day_value = values[strids['d']];
      return {year, month_value, day_value};
    }
    if (n > 3) throw ParseFailure{};
    if (n == 1 || (m && n == 2)) {
      std::int64_t other;
      if (m) {
        month_value = values[*m];
        other = values[*m == 0 ? n - 1 : *m - 1];
      } else {
        other = values[0];
      }
      if (n > 1 || !m) {
        if (other > 31) year = other;
        else day_value = other;
      }
    } else if (n == 2) {
      if (values[0] > 31) year = values[0], month_value = values[1];
      else if (values[1] > 31) month_value = values[0], year = values[1];
      else month_value = values[0], day_value = values[1];
    } else if (n == 3) {
      const auto a = values[0], b = values[1], c = values[2];
      if (m && *m == 0) {
        if (b > 31) month_value = a, year = b, day_value = c;
        else month_value = a, day_value = b, year = c;
      } else if (m && *m == 1) {
        if (a > 31) year = a, month_value = b, day_value = c;
        else day_value = a, month_value = b, year = c;
      } else if (m && *m == 2) {
        if (b > 31) day_value = a, year = b, month_value = c;
        else year = a, day_value = b, month_value = c;
      } else {
        if (a > 31 || (y && *y == 0)) year = a, month_value = b, day_value = c;
        else if (a > 12) day_value = a, month_value = b, year = c;
        else month_value = a, day_value = b, year = c;
      }
    }
    return {year, month_value, day_value};
  }
};

std::pair<std::int64_t, std::int64_t> parsems(std::string_view value) {
  const auto dot = value.find('.');
  if (dot == std::string_view::npos) return {need_int(value), 0};
  if (value.find('.', dot + 1) != std::string_view::npos) throw ParseFailure{};
  std::string fraction(value.substr(dot + 1));
  fraction.resize(std::max<std::size_t>(fraction.size(), 6), '0');
  return {need_int(value.substr(0, dot)), need_int(std::string_view(fraction).substr(0, 6))};
}

void min_sec(const Dec& value, Res& res) {
  res.minute = value.whole;
  res.second.reset();
  if (!value.fraction.empty()) res.second = sixty_times_fraction(value.fraction);
}

std::int64_t adjust_ampm(std::int64_t hour, int which) {
  if (hour < 12 && which == 1) hour += 12;
  else if (hour == 12 && which == 0) hour = 0;
  return hour;
}

bool could_be_tzname(const Res& res, std::string_view token) {
  return res.hour && !res.tzname && !res.tzoffset && token.size() <= 5 &&
         (std::all_of(token.begin(), token.end(), is_upper) || utc_zone(token));
}

std::optional<std::size_t> find_hms_index(std::size_t i, const std::vector<std::string>& l) {
  const auto n = l.size();
  if (i + 1 < n && hms(l[i + 1])) return i + 1;
  if (i + 2 < n && l[i + 1] == " " && hms(l[i + 2])) return i + 2;
  if (i > 0 && hms(l[i - 1])) return i - 1;
  if (1 < i && i == n - 1 && l[i - 1] == " " && hms(l[i - 2])) return i - 2;
  return std::nullopt;
}

const std::string& at(const std::vector<std::string>& l, std::size_t i) {
  if (i >= l.size()) throw ParseFailure{};  // IndexError
  return l[i];
}

std::size_t numeric_token(const std::vector<std::string>& l, std::size_t i, Ymd& ymd, Res& res, bool fuzzy) {
  const std::string& vr = l[i];
  const Dec v = need_dec(vr);
  const auto ln = vr.size();
  const auto n = l.size();
  const std::string_view view(vr);
  if (ymd.size() == 3 && (ln == 2 || ln == 4) && !res.hour &&
      (i + 1 >= n || (l[i + 1] != ":" && !hms(l[i + 1])))) {
    res.hour = need_int(view.substr(0, 2));
    if (ln == 4) res.minute = need_int(view.substr(2));
  } else if (ln == 6 || (ln > 6 && vr.find('.') == 6)) {
    if (ymd.size() == 0 && vr.find('.') == std::string::npos) {
      ymd.append_text(view.substr(0, 2));
      ymd.append_text(view.substr(2, 2));
      ymd.append_text(view.substr(4));
    } else {
      res.hour = need_int(view.substr(0, 2));
      res.minute = need_int(view.substr(2, 2));
      const auto [second, micro] = parsems(view.substr(4));
      res.second = second;
      res.microsecond = micro;
    }
  } else if (ln == 8 || ln == 12 || ln == 14) {
    ymd.append_text(view.substr(0, 4), 'Y');
    ymd.append_text(view.substr(4, 2));
    ymd.append_text(view.substr(6, 2));
    if (ln > 8) {
      res.hour = need_int(view.substr(8, 2));
      res.minute = need_int(view.substr(10, 2));
      if (ln > 12) res.second = need_int(view.substr(12));
    }
  } else if (const auto hms_index = find_hms_index(i, l)) {
    int which;
    if (*hms_index > i) {
      which = *hms(l[*hms_index]);
      i = *hms_index;
    } else {
      which = *hms(l[*hms_index]) + 1;
    }
    if (which == 0) {
      res.hour = v.whole;
      if (!v.fraction.empty()) res.minute = sixty_times_fraction(v.fraction);
    } else if (which == 1) {
      min_sec(v, res);
    } else if (which == 2) {
      const auto [second, micro] = parsems(vr);
      res.second = second;
      res.microsecond = micro;
    }
  } else if (i + 2 < n && l[i + 1] == ":") {
    res.hour = v.whole;
    min_sec(need_dec(l[i + 2]), res);
    if (i + 4 < n && l[i + 3] == ":") {
      const auto [second, micro] = parsems(l[i + 4]);
      res.second = second;
      res.microsecond = micro;
      i += 2;
    }
    i += 2;
  } else if (i + 1 < n && (l[i + 1] == "-" || l[i + 1] == "/" || l[i + 1] == ".")) {
    const std::string separator = l[i + 1];
    ymd.append_text(vr);
    if (i + 2 < n && !jump(l[i + 2])) {
      if (all_digits(l[i + 2])) {
        ymd.append_text(l[i + 2]);
      } else if (const auto m = month(l[i + 2])) {
        ymd.append_month(*m);
      } else {
        throw ParseFailure{};
      }
      if (i + 3 < n && l[i + 3] == separator) {
        const auto& fourth = at(l, i + 4);
        if (const auto m = month(fourth)) ymd.append_month(*m);
        else ymd.append_text(fourth);
        i += 2;
      }
      i += 1;
    }
    i += 1;
  } else if (i + 1 >= n || jump(l[i + 1])) {
    if (i + 2 < n && ampm(l[i + 2])) {
      res.hour = adjust_ampm(v.whole, *ampm(l[i + 2]));
      i += 1;
    } else {
      ymd.append_dec(v);
    }
    i += 1;
  } else if (ampm(l[i + 1]) && v.whole < 24) {
    res.hour = adjust_ampm(v.whole, *ampm(l[i + 1]));
    i += 1;
  } else if (ymd.could_be_day(v)) {
    ymd.append_dec(v);
  } else if (!fuzzy) {
    throw ParseFailure{};
  }
  return i;
}

bool float_token(const std::string& t) {
  if (to_dec(t)) return true;
  const auto key = lower(t);
  return key == "nan" || key == "inf" || key == "infinity";
}

// dateutil's _parse (dayfirst and yearfirst False) with its validate step;
// nullopt where it returns None. Fuzzy (round EB, read_ocel2_xml) skips a
// token it cannot place, an AM/PM without a 12-hour clock before it or after
// another one, and a number that fits no field.
std::optional<Res> dateutil_parse(std::string_view s, bool fuzzy = false) {
  auto l = dateutil_lex(s);
  Res res;
  Ymd ymd;
  const auto n = l.size();
  try {
    std::size_t i = 0;
    while (i < n) {
      const std::string t = l[i];
      if (float_token(t)) {
        i = numeric_token(l, i, ymd, res, fuzzy);
      } else if (const auto day_of_week = weekday(t)) {
        res.weekday = *day_of_week;
      } else if (const auto month_value = month(t)) {
        ymd.append_month(*month_value);
        if (i + 1 < n) {
          if (l[i + 1] == "-" || l[i + 1] == "/") {
            const std::string separator = l[i + 1];
            ymd.append_text(at(l, i + 2));
            if (i + 3 < n && l[i + 3] == separator) {
              ymd.append_text(at(l, i + 4));
              i += 2;
            }
            i += 2;
          } else if (i + 4 < n && l[i + 1] == " " && l[i + 3] == " " && lower(l[i + 2]) == "of") {
            if (all_digits(l[i + 4])) ymd.append_text(std::to_string(convert_year(need_int(l[i + 4]), false)), 'Y');
            i += 4;
          }
        }
      } else if (const auto which = ampm(t)) {
        bool valid = !(fuzzy && res.ampm);
        if (!res.hour || *res.hour < 0 || *res.hour > 12) {
          if (!fuzzy) throw ParseFailure{};
          valid = false;
        }
        if (valid) {
          res.hour = adjust_ampm(*res.hour, *which);
          res.ampm = *which;
        }
      } else if (could_be_tzname(res, t)) {
        res.tzname = t;
        res.tzoffset.reset();
        if (utc_zone(t)) res.tzoffset = 0;
        if (i + 1 < n && (l[i + 1] == "+" || l[i + 1] == "-")) {
          l[i + 1] = l[i + 1] == "+" ? "-" : "+";
          res.tzoffset.reset();
          if (utc_zone(t)) res.tzname.reset();
        }
      } else if (res.hour && (t == "+" || t == "-")) {
        const int sign = t == "+" ? 1 : -1;
        const auto& next = at(l, i + 1);
        std::int64_t hours, minutes;
        if (next.size() == 4) {
          hours = need_int(std::string_view(next).substr(0, 2));
          minutes = need_int(std::string_view(next).substr(2));
        } else if (i + 2 < n && l[i + 2] == ":") {
          hours = need_int(next);
          minutes = need_int(at(l, i + 3));
          i += 2;
        } else if (next.size() <= 2) {
          hours = need_int(std::string_view(next).substr(0, 2));
          minutes = 0;
        } else {
          throw ParseFailure{};
        }
        res.tzoffset = sign * (hours * 3600 + minutes * 60);
        if (i + 5 < n && jump(l[i + 2]) && l[i + 3] == "(" && l[i + 5] == ")" && l[i + 4].size() >= 3 &&
            res.hour && !res.tzname && l[i + 4].size() <= 5 &&
            (std::all_of(l[i + 4].begin(), l[i + 4].end(), is_upper) || utc_zone(l[i + 4]))) {
          res.tzname = l[i + 4];
          i += 4;
        }
        i += 1;
      } else if (!jump(t) && !fuzzy) {
        throw ParseFailure{};
      }
      i += 1;
    }
    const auto [year, month_value, day_value] = ymd.resolve();
    res.century = ymd.century;
    res.year = year;
    res.month = month_value;
    res.day = day_value;
  } catch (const ParseFailure&) {
    return std::nullopt;
  }
  if (res.year) res.year = convert_year(*res.year, res.century);
  if ((res.tzoffset && *res.tzoffset == 0 && (!res.tzname || res.tzname->empty())) ||
      (res.tzname && (*res.tzname == "Z" || *res.tzname == "z"))) {
    res.tzname = "UTC";
    res.tzoffset = 0;
  } else if ((!res.tzoffset || *res.tzoffset != 0) && res.tzname && !res.tzname->empty() &&
             utc_zone(*res.tzname)) {
    res.tzoffset = 0;
  }
  return res;
}

// pandas' dateutil_parse on datetime(1, 1, 1), or on today's date at
// midnight for a text that starts with a time.
struct Reading {
  std::int64_t year = 1, month = 1, day = 1, hour = 0, minute = 0, second = 0, microsecond = 0;
  std::optional<int> offset_minutes;
  std::string tzname;
};

Reading pandas_dateutil(std::string_view text, const Date* on = nullptr) {
  const auto res = dateutil_parse(text);
  if (!res)
    throw Error{"DateParseError: Unknown datetime string format, unable to parse: " + std::string(text)};
  const std::optional<std::int64_t>* fields[] = {&res->year, &res->month, &res->day, &res->hour,
                                                 &res->minute, &res->second, &res->microsecond};
  bool any = false;
  for (const auto* field : fields) {
    if (field->has_value()) {
      any = true;
      if (**field > 2147483647)
        throw Error{"OutOfBoundsDatetime: Parsing \"" + std::string(text) + "\" to datetime overflows"};
    }
  }
  if (!any) throw Error{"DateParseError: Unable to parse datetime string: " + std::string(text)};
  Reading reading;
  reading.year = res->year.value_or(on ? on->year : 1);
  reading.month = res->month.value_or(on ? on->month : 1);
  reading.day = res->day.value_or(on ? on->day : 1);
  reading.hour = res->hour.value_or(0);
  reading.minute = res->minute.value_or(0);
  reading.second = res->second.value_or(0);
  reading.microsecond = res->microsecond.value_or(0);
  const auto fail = [&](const std::string& what) {
    throw Error{"DateParseError: " + what + ": " + std::string(text)};
  };
  if (reading.year < 1 || reading.year > 9999) fail("year " + std::to_string(reading.year) + " is out of range");
  if (reading.month < 1 || reading.month > 12) fail("month must be in 1..12");
  if (reading.day < 1 || reading.day > month_days(reading.year, reading.month)) fail("day is out of range for month");
  if (reading.hour > 23) fail("hour must be in 0..23");
  if (reading.minute > 59) fail("minute must be in 0..59");
  if (reading.second > 59) fail("second must be in 0..59");
  if (res->weekday && !res->day)
    throw Error{"ValueError: Parsing datetimes with weekday but no day information is not supported"};
  if ((res->tzname && *res->tzname == "UTC") || (res->tzoffset && *res->tzoffset == 0)) {
    reading.offset_minutes = 0;
  } else if (res->tzoffset && *res->tzoffset != 0) {
    if (std::llabs(*res->tzoffset) >= 86400)
      throw Error{"ValueError: Parsed string \"" + std::string(text) +
                  "\" gives an invalid tzoffset, which must be between -timedelta(hours=24) and "
                  "timedelta(hours=24)"};
    if (*res->tzoffset % 60 != 0) throw Outside{};
    reading.offset_minutes = static_cast<int>(*res->tzoffset / 60);
    reading.tzname = res->tzname.value_or("");
  } else if (res->tzname) {
    throw Error{"ValueError: Parsed string \"" + std::string(text) + "\" included an un-recognized timezone \"" +
                *res->tzname + "\"."};
  }
  return reading;
}

// ------------------------------------------------------------ strftime and the guess

std::string zfill(const std::string& text, std::size_t width) {
  if (text.size() >= width) return text;
  const std::string zeros(width - text.size(), '0');
  if (!text.empty() && (text[0] == '+' || text[0] == '-')) return text.substr(0, 1) + zeros + text.substr(1);
  return zeros + text;
}

std::string strftime_of(const Reading& r, std::string_view format) {
  std::string out;
  for (std::size_t i = 0; i < format.size(); ++i) {
    if (format[i] != '%' || i + 1 >= format.size()) {
      out += format[i];
      continue;
    }
    const char c = format[++i];
    switch (c) {
      case 'Y': out += r.year >= 1000 ? std::to_string(r.year) : std::to_string(r.year); break;
      case 'm': out += two(r.month); break;
      case 'd': out += two(r.day); break;
      case 'H': out += two(r.hour); break;
      case 'M': out += two(r.minute); break;
      case 'S': out += two(r.second); break;
      case 'f': {
        std::string micro = std::to_string(r.microsecond);
        out += std::string(6 - std::min<std::size_t>(6, micro.size()), '0') + micro;
        break;
      }
      case 'b': out += kMonthAbbr[r.month - 1]; break;
      case 'B': out += kMonthFull[r.month - 1]; break;
      case 'a': out += kWeekdayAbbr[weekday_of(r.year, static_cast<int>(r.month), static_cast<int>(r.day))]; break;
      case 'A': out += kWeekdayFull[weekday_of(r.year, static_cast<int>(r.month), static_cast<int>(r.day))]; break;
      case 'p': out += r.hour < 12 ? "AM" : "PM"; break;
      case 'z':
        if (r.offset_minutes) {
          const int offset = *r.offset_minutes;
          out += offset >= 0 ? "+" : "-";
          out += two(std::abs(offset) / 60) + two(std::abs(offset) % 60);
        }
        break;
      case 'Z':
        if (r.offset_minutes) out += *r.offset_minutes == 0 ? std::string("UTC") : r.tzname;
        break;
      default: throw Outside{};
    }
  }
  return out;
}

std::string fill_token(const std::string& token, std::size_t padding) {
  // re.search(r"\d+\.\d+", token)
  bool decimal = false;
  for (std::size_t i = 0; i < token.size() && !decimal; ++i) {
    if (token[i] != '.') continue;
    decimal = i > 0 && is_digit(token[i - 1]) && i + 1 < token.size() && is_digit(token[i + 1]);
  }
  if (!decimal) return zfill(token, padding);
  const auto dot = token.find('.');
  std::string whole = token.substr(0, dot);
  std::string fraction = token.substr(dot + 1);
  std::size_t start = 0;
  while (start + 1 < whole.size() && whole[start] == '0') ++start;
  whole = zfill(whole.substr(start), 2);
  fraction.resize(std::max<std::size_t>(fraction.size(), 9), '0');
  return whole + "." + fraction.substr(0, 6);
}

struct Attribute {
  std::vector<const char*> attrs;
  const char* format;
  std::size_t padding;
};

const std::vector<Attribute>& attributes() {
  static const std::vector<Attribute> kAttributes = {
      {{"year", "month", "day", "hour", "minute", "second"}, "%Y%m%d%H%M%S", 0},
      {{"year", "month", "day", "hour", "minute"}, "%Y%m%d%H%M", 0},
      {{"year", "month", "day", "hour"}, "%Y%m%d%H", 0},
      {{"year", "month", "day"}, "%Y%m%d", 0},
      {{"year"}, "%Y", 0},
      {{"hour", "minute", "second"}, "%H%M%S", 0},
      {{"hour", "minute"}, "%H%M", 0},
      {{"month"}, "%B", 0},
      {{"month"}, "%b", 0},
      {{"month"}, "%m", 2},
      {{"day"}, "%d", 2},
      {{"hour"}, "%H", 2},
      {{"minute"}, "%M", 2},
      {{"second"}, "%S", 2},
      {{"second", "microsecond"}, "%S.%f", 0},
      {{"tzinfo"}, "%z", 0},
      {{"tzinfo"}, "%Z", 0},
      {{"day_of_week"}, "%a", 0},
      {{"day_of_week"}, "%A", 0},
      {{"meridiem"}, "%p", 0},
  };
  return kAttributes;
}

// ------------------------------------------------------------ format parsing (ISO parser, strptime rules)

bool format_is_iso(std::string_view format) {
  static const char* kDateSeparators[] = {" ", "/", "\\", "-", ".", ""};
  static const char* kTimeSeparators[] = {" ", "T"};
  static const char* kTails[] = {"", "%z", ".%f", ".%f%z"};
  if (format == "%Y%m") return false;
  for (const auto* date : kDateSeparators)
    for (const auto* time : kTimeSeparators)
      for (const auto* tail : kTails) {
        const std::string iso = std::string("%Y") + date + "%m" + date + "%d" + time + "%H:%M:%S" + tail;
        if (format.size() <= iso.size() && iso.compare(0, format.size(), format) == 0) return true;
      }
  return false;
}

std::vector<std::string> format_tokens(std::string_view format) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < format.size(); ++i) {
    if (format[i] == '%' && i + 1 < format.size() && is_alpha(format[i + 1])) {
      out.emplace_back(format.substr(i, 2));
      ++i;
    } else {
      out.emplace_back(1, format[i]);
    }
  }
  return out;
}

struct Fields {
  int year = 1900, month = 1, day = 1, hour = 0, minute = 0, second = 0;
  std::string fraction;
  std::optional<int> offset_minutes;
  bool utc_name = false;
};

struct Failure {
  bool remains = false;
  std::string rest;
};

using Parsed = std::variant<Fields, Failure>;

std::size_t run(std::string_view text, std::size_t at_index) {
  std::size_t count = 0;
  while (at_index + count < text.size() && is_digit(text[at_index + count])) ++count;
  return count;
}

int number(std::string_view text) {
  int value = 0;
  for (const char c : text) value = value * 10 + (c - '0');
  return value;
}

struct Offset {
  int minutes = 0;
  std::size_t end = 0;
  bool had_minutes = false;
};

// repr() of datetime.timedelta(minutes=minutes), |minutes| >= 1440.
std::string timedelta_repr(std::int64_t minutes) {
  const std::int64_t seconds = minutes * 60;
  const std::int64_t days = seconds >= 0 ? seconds / 86400 : -((-seconds + 86399) / 86400);
  const std::int64_t rest = seconds - days * 86400;
  std::string out = "datetime.timedelta(days=" + std::to_string(days);
  if (rest != 0) out += ", seconds=" + std::to_string(rest);
  return out + ")";
}

Error offset_error(std::int64_t minutes) {
  return Error{"ValueError: offset must be a timedelta strictly between -timedelta(hours=24) and "
               "timedelta(hours=24), not " +
               timedelta_repr(minutes) + "." + kSuggestions};
}

// pandas' ISO offset: Z, or a sign, one or two hour digits, then an
// optional colon and one or two minute digits. Minutes past 59 and offsets
// of a day or more do not match (the strptime rules raise the latter with a
// format, dateutil without one).
std::optional<Offset> iso_offset(std::string_view text, std::size_t pos) {
  const auto n = text.size();
  if (pos < n && text[pos] == 'Z') return Offset{0, pos + 1, true};
  if (pos >= n || (text[pos] != '+' && text[pos] != '-')) return std::nullopt;
  const int sign = text[pos] == '+' ? 1 : -1;
  ++pos;
  auto count = std::min<std::size_t>(run(text, pos), 2);
  if (count == 0) return std::nullopt;
  const int hours = number(text.substr(pos, count));
  pos += count;
  int minutes = 0;
  bool had = false;
  if (pos < n && text[pos] == ':') {
    ++pos;
    count = std::min<std::size_t>(run(text, pos), 2);
    if (count == 0) return std::nullopt;
    minutes = number(text.substr(pos, count));
    pos += count;
    had = true;
  } else if (pos < n && is_digit(text[pos])) {
    count = std::min<std::size_t>(run(text, pos), 2);
    minutes = number(text.substr(pos, count));
    pos += count;
    had = true;
  }
  if (minutes >= 60 || hours * 60 + minutes >= 1440) return std::nullopt;
  return Offset{sign * (hours * 60 + minutes), pos, had};
}

// pandas' ISO parser held to an ISO format.
std::optional<Fields> iso_parse(std::string_view text, const std::vector<std::string>& format) {
  Fields fields;
  std::size_t pos = 0;
  bool has_month = false, has_hour = false, has_minute = false, has_second = false, has_day = false;
  for (std::size_t i = 0; i < format.size(); ++i) {
    const auto& token = format[i];
    if (token == "%Y") {
      if (pos < text.size() && text[pos] == '-') throw Outside{};
      if (run(text, pos) < 4) return std::nullopt;
      fields.year = number(text.substr(pos, 4));
      pos += 4;
    } else if (token == "%m" || token == "%d" || token == "%H" || token == "%M" || token == "%S") {
      const auto count = std::min<std::size_t>(run(text, pos), 2);
      if (count == 0) return std::nullopt;
      const int value = number(text.substr(pos, count));
      pos += count;
      if (token == "%m") fields.month = value, has_month = true;
      else if (token == "%d") fields.day = value, has_day = true;
      else if (token == "%H") fields.hour = value, has_hour = true;
      else if (token == "%M") fields.minute = value, has_minute = true;
      else fields.second = value, has_second = true;
    } else if (token == "%f") {
      const auto count = std::min<std::size_t>(run(text, pos), 9);
      fields.fraction = std::string(text.substr(pos, count));
      pos += count;
      const auto skipped = pos + run(text, pos);
      if (skipped == text.size() || (i + 1 < format.size() && format[i + 1] == "%z")) pos = skipped;
    } else if (token == "%z") {
      const auto offset = iso_offset(text, pos);
      if (!offset) return std::nullopt;
      fields.offset_minutes = offset->minutes;
      pos = offset->end;
    } else {
      if (pos >= text.size() || text[pos] != token[0]) return std::nullopt;
      ++pos;
    }
  }
  (void)has_month, (void)has_day, (void)has_hour, (void)has_minute, (void)has_second;
  if (pos != text.size()) return std::nullopt;
  if (fields.month < 1 || fields.month > 12 || fields.hour > 23 || fields.minute > 59 || fields.second > 59)
    return std::nullopt;
  if (fields.day < 1 || fields.day > month_days(fields.year, fields.month)) return std::nullopt;
  return fields;
}

std::string case_free(std::string_view word) {
  std::string out;
  for (const char c : word) {
    if (is_alpha(c)) {
      out += '[';
      out += to_lower(c);
      out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      out += ']';
    } else {
      out += c;
    }
  }
  return out;
}

std::string alternatives(std::vector<std::string> names) {
  std::stable_sort(names.begin(), names.end(),
                   [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
  std::string out = "(";
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i) out += '|';
    out += case_free(names[i]);
  }
  return out + ")";
}

std::vector<std::string> names_of(const char* const* table, std::size_t n) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < n; ++i) out.push_back(lower(table[i]));
  return out;
}

// The strptime rules: Python's regular expressions for the fields (pandas'
// %f takes up to nine digits), matched case-insensitively from the start,
// a run of format whitespace matching any run of whitespace.
const std::regex& strptime_regex(const std::vector<std::string>& format) {
  static thread_local std::map<std::vector<std::string>, std::regex> cache;
  const auto found = cache.find(format);
  if (found != cache.end()) return found->second;
  static const std::string kWeekdayAbbrRegex = alternatives(names_of(kWeekdayAbbr, 7));
  static const std::string kWeekdayFullRegex = alternatives(names_of(kWeekdayFull, 7));
  static const std::string kMonthAbbrRegex = alternatives(names_of(kMonthAbbr, 12));
  static const std::string kMonthFullRegex = alternatives(names_of(kMonthFull, 12));
  static const std::string kAmPmRegex = alternatives({"am", "pm"});
  std::string pattern;
  bool space = false;
  for (const auto& token : format) {
    if (token.size() == 1 && is_space(token[0])) {
      if (!space) pattern += "\\s+";
      space = true;
      continue;
    }
    space = false;
    if (token == "%d") pattern += "(3[0-1]|[1-2]\\d|0[1-9]|[1-9]| [1-9])";
    else if (token == "%f") pattern += "([0-9]{1,9})";
    else if (token == "%H") pattern += "(2[0-3]|[0-1]\\d|\\d)";
    else if (token == "%I") pattern += "(1[0-2]|0[1-9]|[1-9])";
    else if (token == "%m") pattern += "(1[0-2]|0[1-9]|[1-9])";
    else if (token == "%M") pattern += "([0-5]\\d|\\d)";
    else if (token == "%S") pattern += "(6[0-1]|[0-5]\\d|\\d)";
    else if (token == "%Y") pattern += "(\\d\\d\\d\\d)";
    else if (token == "%z") pattern += "([+-]\\d\\d:?[0-5]\\d(?::?[0-5]\\d(?:\\.\\d{1,6})?)?|Z)";
    else if (token == "%Z") pattern += "([uU][tT][cC]|[gG][mM][tT])";
    else if (token == "%a") pattern += kWeekdayAbbrRegex;
    else if (token == "%A") pattern += kWeekdayFullRegex;
    else if (token == "%b") pattern += kMonthAbbrRegex;
    else if (token == "%B") pattern += kMonthFullRegex;
    else if (token == "%p") pattern += kAmPmRegex;
    else if (token.size() == 2 && token[0] == '%') throw Outside{};
    else {
      const char c = token[0];
      static const std::string kSyntax = "\\^$.|?*+()[]{}";
      if (is_alpha(c)) pattern += case_free(token);
      else if (kSyntax.find(c) != std::string::npos) pattern += std::string("\\") + c;
      else pattern += c;
    }
  }
  return cache.emplace(format, std::regex(pattern, std::regex::ECMAScript)).first->second;
}

int index_of(const char* const* table, std::size_t n, const std::string& value) {
  const auto key = lower(value);
  for (std::size_t i = 0; i < n; ++i)
    if (key == lower(table[i])) return static_cast<int>(i);
  return 0;
}

Parsed strptime_parse(std::string_view text, const std::vector<std::string>& format) {
  const auto& regex = strptime_regex(format);
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(text.begin(), text.end(), match, regex, std::regex_constants::match_continuous))
    return Failure{};
  const auto end = static_cast<std::size_t>(match.length(0));
  if (end != text.size()) return Failure{true, std::string(text.substr(end))};
  Fields fields;
  std::size_t group = 1;
  std::optional<int> twelve_hour;
  std::string meridiem;
  for (const auto& token : format) {
    if (token.size() != 2 || token[0] != '%') continue;
    const std::string value = match[group++].str();
    if (token == "%z") {
      if (value == "Z") {
        fields.offset_minutes = 0;
      } else {
        std::string offset;
        for (const char c : value.substr(1))
          if (c != ':') offset += c;
        if (offset.size() > 4) throw Outside{};
        fields.offset_minutes = (value[0] == '+' ? 1 : -1) *
                                (number(std::string_view(offset).substr(0, 2)) * 60 +
                                 number(std::string_view(offset).substr(2, 2)));
        if (std::abs(*fields.offset_minutes) >= 1440) throw offset_error(*fields.offset_minutes);
      }
    } else if (token == "%Z") {
      fields.utc_name = true;
    } else if (token == "%f") {
      fields.fraction = value;
    } else if (token == "%b") {
      fields.month = index_of(kMonthAbbr, 12, value) + 1;
    } else if (token == "%B") {
      fields.month = index_of(kMonthFull, 12, value) + 1;
    } else if (token == "%a" || token == "%A") {
    } else if (token == "%p") {
      meridiem = lower(value);
    } else if (token == "%I") {
      twelve_hour = number(value);
    } else {
      const int number_value = number(value[0] == ' ' ? std::string_view(value).substr(1) : value);
      if (token == "%Y") fields.year = number_value;
      else if (token == "%m") fields.month = number_value;
      else if (token == "%d") fields.day = number_value;
      else if (token == "%H") fields.hour = number_value;
      else if (token == "%M") fields.minute = number_value;
      else fields.second = number_value;
    }
  }
  if (twelve_hour) {
    int hour = *twelve_hour;
    if (meridiem.empty() || meridiem == "am") {
      if (hour == 12) hour = 0;
    } else if (hour != 12) {
      hour += 12;
    }
    fields.hour = hour;
  }
  return fields;
}

Parsed parse(std::string_view text, const std::string& format) {
  const auto split_format = format_tokens(format);
  if (format_is_iso(format))
    if (auto fields = iso_parse(text, split_format)) return *fields;
  return strptime_parse(text, split_format);
}

// The format pandas guesses for the first text, nullopt for none.
std::optional<std::string> guess_format(std::string_view text) {
  Reading reading;
  try {
    reading = pandas_dateutil(text);
  } catch (const Error&) {
    return std::nullopt;
  }
  auto tokens = pandas_lex(text);
  if (reading.offset_minutes) {
    std::optional<std::ptrdiff_t> offset_index;
    const auto n = static_cast<std::ptrdiff_t>(tokens.size());
    if (n > 0 && tokens[n - 1] == "Z") offset_index = n - 1;
    else if (n > 1 && (tokens[n - 2] == "+" || tokens[n - 2] == "-")) offset_index = n - 2;
    else if (n > 3 && (tokens[n - 4] == "+" || tokens[n - 4] == "-")) offset_index = n - 4;
    if (offset_index) {
      tokens[*offset_index] = strftime_of(reading, "%z");
      tokens.resize(static_cast<std::size_t>(*offset_index + 1));
    }
  }
  std::vector<std::optional<std::string>> guessed(tokens.size());
  std::set<std::string> found;
  for (const auto& attribute : attributes()) {
    bool seen = false;
    for (const auto* name : attribute.attrs) seen = seen || found.count(name) != 0;
    if (seen) continue;
    const std::string format = attribute.format;
    if (!reading.offset_minutes && (format == "%Z" || format == "%z")) continue;
    const auto formatted = strftime_of(reading, format);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
      const auto filled = fill_token(tokens[i], attribute.padding);
      if (!guessed[i] && filled == formatted) {
        guessed[i] = format;
        tokens[i] = filled;
        for (const auto* name : attribute.attrs) found.insert(name);
        break;
      }
    }
  }
  const bool ymd = found.count("year") && found.count("month") && found.count("day");
  const bool year_only = guessed.size() == 1 && guessed[0] && *guessed[0] == "%Y";
  const bool year_month = guessed.size() == 3 && guessed[0] && *guessed[0] == "%Y" && !guessed[1] &&
                          guessed[2] && *guessed[2] == "%m" && tokens[1] == "-";
  if (!ymd && !year_only && !year_month) return std::nullopt;
  std::vector<std::string> output;
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    if (guessed[i]) {
      output.push_back(*guessed[i]);
      continue;
    }
    if (float_token(tokens[i])) return std::nullopt;
    output.push_back(tokens[i]);
  }
  const auto has = [&](const char* format) { return std::find(output.begin(), output.end(), format) != output.end(); };
  if (has("%p") && has("%H")) *std::find(output.begin(), output.end(), "%H") = "%I";
  std::string format;
  for (const auto& piece : output) format += piece;
  const auto parsed = parse(text, format);
  if (!std::holds_alternative<Fields>(parsed)) return std::nullopt;
  const auto& fields = std::get<Fields>(parsed);
  if (fields.fraction.size() > 6 && (fields.year < 1678 || fields.year > 2261)) {
    if (fields.year == 1677 || fields.year == 2262) throw Outside{};
    return std::nullopt;
  }
  std::string joined;
  for (const auto& token : tokens) joined += token;
  if (strftime_of(reading, format) == joined) return format;
  return std::nullopt;
}

// ------------------------------------------------------------ element by element

// pandas' ISO parser without a format.
std::optional<Fields> iso_any(std::string_view t) {
  std::size_t i = 0;
  const auto n = t.size();
  while (i < n && is_space(t[i])) ++i;
  Fields v;
  v.year = 0;
  // A sign before an ISO text is a negative year (outside the model);
  // before anything else it is no ISO text ("-198013445166" goes on to
  // the not-likely check).
  const std::size_t j = i < n && t[i] == '-' ? i + 1 : i;
  if (j > i && run(t, j) >= 4) {
    if (iso_any(t.substr(j))) throw Outside{};
    return std::nullopt;
  }
  if (run(t, j) < 4) {
    // Fewer than four digits read as year 0 when a separator follows
    // (outside the model); otherwise the text is no ISO one.
    if (j < n && (t[j] == '-' || t[j] == '.' || t[j] == '/' || t[j] == '\\' || t[j] == ' ') &&
        iso_any("2000" + std::string(t.substr(j))))
      throw Outside{};
    return std::nullopt;
  }
  // Five to seven digits are no year here (dateutil reads them).
  if (i + 4 < n && is_digit(t[i + 4]) && !(i + 8 <= n && run(t, i) >= 8)) return std::nullopt;
  v.year = number(t.substr(i, 4));
  if (v.year == 0) throw Outside{};
  i += 4;
  if (i == n) return v;
  char separator = 0;
  if (t[i] == ' ' || t[i] == '/' || t[i] == '\\' || t[i] == '-' || t[i] == '.') separator = t[i++];
  auto count = std::min<std::size_t>(run(t, i), 2);
  if (count < (separator ? 1u : 2u)) return std::nullopt;
  v.month = number(t.substr(i, count));
  i += count;
  if (v.month < 1 || v.month > 12) return std::nullopt;
  if (i == n) return separator ? std::optional<Fields>(v) : std::nullopt;
  if (separator) {
    if (t[i] != separator) return std::nullopt;
    ++i;
  }
  count = std::min<std::size_t>(run(t, i), 2);
  if (count < (separator ? 1u : 2u)) return std::nullopt;
  v.day = number(t.substr(i, count));
  i += count;
  if (v.day < 1 || v.day > month_days(v.year, v.month)) return std::nullopt;
  if (i == n) return v;
  if (t[i] != 'T' && t[i] != ' ') return std::nullopt;
  ++i;
  count = std::min<std::size_t>(run(t, i), 2);
  if (count == 0) return std::nullopt;
  if (count == 1 && !(i + 1 < n && t[i + 1] == ':')) return std::nullopt;
  v.hour = number(t.substr(i, count));
  i += count;
  if (v.hour > 23) return std::nullopt;
  if (i < n && t[i] == ':') {
    ++i;
    count = std::min<std::size_t>(run(t, i), 2);
    if (count == 0) return std::nullopt;
    v.minute = number(t.substr(i, count));
    i += count;
    if (v.minute > 59) return std::nullopt;
    if (i < n && t[i] == ':') {
      ++i;
      count = std::min<std::size_t>(run(t, i), 2);
      if (count == 0) return std::nullopt;
      v.second = number(t.substr(i, count));
      i += count;
      if (v.second > 59) return std::nullopt;
      if (i < n && t[i] == '.') {
        ++i;
        const auto digits = run(t, i);
        v.fraction = std::string(t.substr(i, digits));
        i += digits;
      }
    }
  }
  while (i < n && is_space(t[i])) ++i;
  if (i == n) return v;
  const auto offset = iso_offset(t, i);
  if (!offset) return std::nullopt;
  v.offset_minutes = offset->minutes;
  i = offset->end;
  if (offset->had_minutes)
    while (i < n && is_space(t[i])) ++i;
  if (i != n) return std::nullopt;
  return v;
}

bool c_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'; }

// pandas' check before its own readers and dateutil: a text that starts
// with 0 goes on; the single letters a, A, m, M, p, P, t and T do not, nor
// a text that reads as a number (pandas' xstrtod: spaces, a sign, digits
// with one point, an exponent) below 1000.
void check_looks_like_datetime(std::string_view t) {
  if (!t.empty() && t[0] == '0') return;
  const auto refuse = [&]() {
    throw Error{"ValueError: Given date string \"" + std::string(t) + "\" not likely a datetime"};
  };
  if (t.size() == 1 && std::string_view("aAmMpPtT").find(t[0]) != std::string_view::npos) refuse();
  std::size_t i = 0;
  const auto n = t.size();
  while (i < n && c_space(t[i])) ++i;
  const auto start = i;
  if (i < n && (t[i] == '+' || t[i] == '-')) ++i;
  std::size_t digits = 0;
  while (i < n && is_digit(t[i])) ++i, ++digits;
  if (i < n && t[i] == '.') {
    ++i;
    while (i < n && is_digit(t[i])) ++i, ++digits;
  }
  if (digits == 0) return;
  std::size_t end_number = i;
  if (i < n && (t[i] == 'e' || t[i] == 'E')) {
    std::size_t k = i + 1;
    if (k < n && (t[k] == '+' || t[k] == '-')) ++k;
    const auto exponent_digits = run(t, k);
    if (exponent_digits == 0) return;  // xstrtod stops before the 'e': not a number
    end_number = k + exponent_digits;
  }
  i = end_number;
  while (i < n && c_space(t[i])) ++i;
  if (i != n) return;
  const std::string number_text(t.substr(start, end_number - start));
  char* end = nullptr;
  const double value = std::strtod(number_text.c_str(), &end);
  if (!std::isfinite(value)) return;  // xstrtod's range error: goes on
  if (value < 1000) refuse();
}

// pandas' check for a text that starts with a time:
// ^([01]?[0-9]|2[0-3]):([0-5][0-9]).
bool time_first(std::string_view t) {
  const auto hour = [&](std::size_t length) {
    if (length > t.size()) return false;
    const auto h = t.substr(0, length);
    if (!all_digits(h)) return false;
    return length == 1 || h[0] == '0' || h[0] == '1' || (h[0] == '2' && h[1] <= '3');
  };
  for (const std::size_t length : {std::size_t{2}, std::size_t{1}}) {
    if (hour(length) && length + 2 < t.size() && t[length] == ':' && t[length + 1] >= '0' &&
        t[length + 1] <= '5' && is_digit(t[length + 2]))
      return true;
  }
  return false;
}

struct Element {
  Value value;
  std::optional<int> offset_minutes;
  bool nanoseconds = false;
  bool iso = false;  // read by the ISO parser (its range error shows the fields)
};

Element from_reading(std::string_view text, const Reading& reading) {
  Element out;
  out.value.year = static_cast<int>(reading.year);
  out.value.month = static_cast<int>(reading.month);
  out.value.day = static_cast<int>(reading.day);
  out.value.hour = static_cast<int>(reading.hour);
  out.value.minute = static_cast<int>(reading.minute);
  out.value.second = static_cast<int>(reading.second);
  out.value.nanoseconds = static_cast<std::uint32_t>(reading.microsecond * 1000);
  out.offset_minutes = reading.offset_minutes;
  if (reading.microsecond % 1000 == 0) {
    // \d:\d{2}:\d{2}\.(\d+), the first match.
    for (std::size_t i = 0; i + 7 <= text.size(); ++i) {
      if (is_digit(text[i]) && text[i + 1] == ':' && is_digit(text[i + 2]) && is_digit(text[i + 3]) &&
          text[i + 4] == ':' && is_digit(text[i + 5]) && is_digit(text[i + 6]) && i + 7 < text.size() &&
          text[i + 7] == '.' && i + 8 < text.size() && is_digit(text[i + 8])) {
        const auto digits = run(text, i + 8);
        if (digits > 6) {
          std::string extra(text.substr(i + 8 + 6, std::min<std::size_t>(digits - 6, 3)));
          extra.resize(3, '0');
          out.nanoseconds = true;
          out.value.nanoseconds += static_cast<std::uint32_t>(number(extra));
        }
        break;
      }
    }
  }
  return out;
}

std::optional<std::array<int, 3>> delimited(std::string_view t) {
  const auto n = t.size();
  const auto delimiter = [](char c) { return c == ' ' || c == '/' || c == '-' || c == '.'; };
  const auto parse_digits = [&](std::size_t from, std::size_t length) {
    const auto piece = t.substr(from, length);
    return piece.size() == length && all_digits(piece) ? number(piece) : -1;
  };
  int month_value = 0, day_value = 1, year = 0;
  bool can_swap = false;
  if (n == 10 && delimiter(t[2]) && delimiter(t[5])) {
    month_value = parse_digits(0, 2), day_value = parse_digits(3, 2), year = parse_digits(6, 4), can_swap = true;
  } else if (n == 9 && delimiter(t[1]) && delimiter(t[4])) {
    month_value = parse_digits(0, 1), day_value = parse_digits(2, 2), year = parse_digits(5, 4), can_swap = true;
  } else if (n == 9 && delimiter(t[2]) && delimiter(t[4])) {
    month_value = parse_digits(0, 2), day_value = parse_digits(3, 1), year = parse_digits(5, 4), can_swap = true;
  } else if (n == 8 && delimiter(t[1]) && delimiter(t[3])) {
    month_value = parse_digits(0, 1), day_value = parse_digits(2, 1), year = parse_digits(4, 4), can_swap = true;
  } else if (n == 7 && delimiter(t[2])) {
    if (t[2] == '.') return std::nullopt;
    month_value = parse_digits(0, 2), year = parse_digits(3, 4);
  } else {
    return std::nullopt;
  }
  if (month_value < 0 || day_value < 0 || year < 1000) return std::nullopt;
  if (month_value >= 1 && month_value <= 31 && day_value >= 1 && day_value <= 31 &&
      (month_value <= 12 || day_value <= 12)) {
    if (month_value > 12 && can_swap) std::swap(month_value, day_value);
    if (month_value > 12) throw Error{"ValueError: month must be in 1..12"};
    if (day_value > month_days(year, month_value)) throw Error{"ValueError: day is out of range for month"};
    return std::array<int, 3>{year, month_value, day_value};
  }
  throw Error{"DateParseError: Invalid date specified (" + std::to_string(month_value) + "/" +
              std::to_string(day_value) + ")"};
}

// Python int() of a short text: spaces around, a sign, digits with single
// underscores between them; nullopt where int() raises.
std::optional<int> python_int(std::string_view t) {
  std::size_t i = 0, n = t.size();
  while (i < n && c_space(t[i])) ++i;
  while (n > i && c_space(t[n - 1])) --n;
  bool negative = false;
  if (i < n && (t[i] == '+' || t[i] == '-')) negative = t[i++] == '-';
  if (i == n || !is_digit(t[i]) || !is_digit(t[n - 1])) return std::nullopt;
  int value = 0;
  for (; i < n; ++i) {
    if (t[i] == '_') {
      if (!is_digit(t[i + 1])) return std::nullopt;
      continue;
    }
    if (!is_digit(t[i])) return std::nullopt;
    value = value * 10 + (t[i] - '0');
  }
  return negative ? -value : value;
}

// pandas' quarter reader on a text of four to seven characters with a Q
// (any case) at index 1 to 5: nullopt to go on, or the quarter's first day.
// The quarter digit is read as a character code minus '0'.
std::optional<std::array<int, 3>> quarter(std::string_view t) {
  std::string u(t);
  for (auto& c : u) c = c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
  const auto n = u.size();
  const auto i = u.find('Q', 1);
  if (i == std::string::npos || i >= 6) return std::nullopt;
  int q = 0;
  std::optional<int> year;
  const std::string_view s(u);
  if (i == 1) {
    q = static_cast<unsigned char>(u[0]) - '0';
    if (n == 4 || (n == 5 && u[i + 1] == '-')) {
      year = python_int(s.substr(n - 2));
      if (year) *year += 2000;
    } else if (n == 6 || (n == 7 && u[i + 1] == '-')) {
      year = python_int(s.substr(n - 4));
    } else {
      return std::nullopt;
    }
  } else if (i == 2 || i == 3) {
    if (!(n == 4 || (n == 5 && u[i - 1] == '-'))) return std::nullopt;
    q = static_cast<unsigned char>(u[n - 1]) - '0';
    year = python_int(s.substr(0, 2));
    if (year) *year += 2000;
  } else {
    if (!(n == 6 || (n == 7 && u[i - 1] == '-'))) return std::nullopt;
    q = static_cast<unsigned char>(u[n - 1]) - '0';
    year = python_int(s.substr(0, 4));
  }
  if (!year) return std::nullopt;
  if (q < 1 || q > 4)
    throw Error{"DateParseError: Incorrect quarterly string is given, quarter must be between 1 and 4: " + u};
  if (*year < 1 || *year > 9999) return std::nullopt;
  return std::array<int, 3>{*year, (q - 1) * 3 + 1, 1};
}

std::optional<std::array<int, 3>> year_or_month(std::string_view t) {
  if (t.size() == 4) {
    const auto year = python_int(t);
    if (year && *year >= 1 && *year <= 9999) return std::array<int, 3>{*year, 1, 1};
  }
  if (t.size() >= 4 && t.size() <= 7 && lower(t).find('q') != std::string::npos)
    if (const auto q = quarter(t)) return q;
  // "%Y-%m": four digits, "-", then 1[0-2]|0[1-9]|[1-9] to the end.
  if (t.size() >= 6 && t.size() <= 7 && all_digits(t.substr(0, 4)) && t[4] == '-') {
    const auto m = t.substr(5);
    const bool ok = (m.size() == 2 && ((m[0] == '1' && m[1] >= '0' && m[1] <= '2') ||
                                       (m[0] == '0' && m[1] >= '1' && m[1] <= '9'))) ||
                    (m.size() == 1 && m[0] >= '1' && m[0] <= '9');
    if (ok) return std::array<int, 3>{number(t.substr(0, 4)), number(m), 1};
  }
  // "%b %Y" (any whitespace run) and "%b-%Y".
  if (t.size() >= 8) {
    const auto name = lower(t.substr(0, 3));
    int index = -1;
    for (int k = 0; k < 12; ++k)
      if (name == lower(kMonthAbbr[k])) index = k;
    if (index >= 0) {
      std::size_t i = 3;
      if (t[i] == '-') {
        ++i;
      } else {
        const auto start = i;
        while (i < t.size() && is_space(t[i])) ++i;
        if (i == start) return std::nullopt;
      }
      const auto year = t.substr(i);
      if (year.size() == 4 && all_digits(year)) return std::array<int, 3>{number(year), index + 1, 1};
    }
  }
  return std::nullopt;
}

// One text without a format (pandas' ISO parser, its checks and readers,
// then dateutil); a text that starts with a time reads on today's date.
Element element(std::string_view t) {
  if (const auto v = iso_any(t)) {
    Element out;
    out.value.year = v->year;
    out.value.month = v->month;
    out.value.day = v->day;
    out.value.hour = v->hour;
    out.value.minute = v->minute;
    out.value.second = v->second;
    std::string fraction = v->fraction.substr(0, std::min<std::size_t>(v->fraction.size(), 9));
    fraction.resize(9, '0');
    out.value.nanoseconds = static_cast<std::uint32_t>(number(fraction));
    out.offset_minutes = v->offset_minutes;
    out.nanoseconds = v->fraction.size() > 6;
    out.iso = true;
    return out;
  }
  check_looks_like_datetime(t);
  if (time_first(t)) return from_reading(t, pandas_dateutil(t, &today_value));
  if (const auto d = delimited(t)) {
    Element out;
    out.value = Value{(*d)[0], (*d)[1], (*d)[2], 0, 0, 0, 0};
    return out;
  }
  if (const auto d = year_or_month(t)) {
    Element out;
    out.value = Value{(*d)[0], (*d)[1], (*d)[2], 0, 0, 0, 0};
    return out;
  }
  return from_reading(t, pandas_dateutil(t));
}

bool in_domain(std::string_view text) {
  for (const char c : text)
    if (!((c >= ' ' && c <= '~') || c == '\t') || c == '%') return false;
  return true;
}

bool year_ok(int year) { return year >= 1 && year <= 9999; }

// The nanosecond range of datetime64[ns] (NaT's value excluded), on the
// fields as written.
using Key = std::array<std::int64_t, 7>;
Key key_of(const Value& v) {
  return {v.year, v.month, v.day, v.hour, v.minute, v.second, static_cast<std::int64_t>(v.nanoseconds)};
}
bool in_ns_range(const Value& v) {
  static constexpr Key kMin{1677, 9, 21, 0, 12, 43, 145224193};
  static constexpr Key kMax{2262, 4, 11, 23, 47, 16, 854775807};
  const auto k = key_of(v);
  return !(k < kMin) && !(kMax < k);
}

std::string out_of_bounds(const std::string& what) {
  return "OutOfBoundsDatetime: Out of bounds nanosecond timestamp" + (what.empty() ? "" : ": " + what);
}

// The UTC instant of an aware value: outside the model when it leaves years
// 1-9999, or the nanosecond range in a nanosecond column.
void check_utc(const Value& v, std::optional<int> offset, bool nanoseconds) {
  if (!offset || *offset == 0) return;
  const std::int64_t seconds =
      days_from_civil(v.year, static_cast<unsigned>(v.month), static_cast<unsigned>(v.day)) * 86400 +
      v.hour * 3600 + v.minute * 60 + v.second - static_cast<std::int64_t>(*offset) * 60;
  const std::int64_t first = days_from_civil(1, 1, 1) * 86400, end = days_from_civil(10000, 1, 1) * 86400;
  if (seconds < first || seconds >= end) throw Outside{};
  if (nanoseconds) {
    // datetime64[ns]: -9223372036854775807 to 9223372036854775807 ns.
    const std::int64_t low = -9223372037, high = 9223372036;
    if (seconds < low || seconds > high) throw Outside{};
    if (seconds == low && v.nanoseconds < 145224193) throw Outside{};
    if (seconds == high && v.nanoseconds > 854775807) throw Outside{};
  }
}

// pandas parses the column once with the finest unit seen so far (values
// after the first nanosecond one must fit nanoseconds then), and again in
// nanoseconds when that unit changed after the first value; mixed zones
// raise in between.
struct Pass {
  bool nanoseconds = false, changed = false, seen = false;
  std::set<std::optional<int>> offsets;
  std::vector<std::pair<Value, std::string_view>> held;
  std::vector<std::optional<int>> held_offsets;
};

Result finish(Pass& pass, Result result, const char* suffix) {
  if (pass.offsets.size() > 1) throw Error{kMixed};
  if (pass.changed)
    for (const auto& [value, text] : pass.held)
      if (!in_ns_range(value)) throw Error{out_of_bounds(std::string(text)) + suffix};
  for (std::size_t k = 0; k < pass.held.size(); ++k)
    check_utc(pass.held[k].first, pass.held_offsets[k], pass.nanoseconds);
  result.nanoseconds = pass.nanoseconds;
  result.offset_minutes = pass.offsets.empty() ? std::nullopt : *pass.offsets.begin();
  return result;
}

Result with_format(const std::vector<std::string_view>& texts, const std::string& format) {
  Result result;
  Pass pass;
  const auto fail = [&](const std::string& message) { throw Error{"ValueError: " + message + kSuggestions}; };
  for (const auto text : texts) {
    if (text.empty()) {
      result.values.emplace_back();
      continue;
    }
    Parsed parsed;
    try {
      parsed = parse(text, format);
    } catch (const Error&) {
      if (!coerce_value) throw;
      result.values.emplace_back();
      continue;
    }
    if (coerce_value && (std::holds_alternative<Failure>(parsed) ||
                         (std::get<Fields>(parsed).day > month_days(std::get<Fields>(parsed).year,
                                                                    std::get<Fields>(parsed).month)))) {
      result.values.emplace_back();
      continue;
    }
    if (const auto* failure = std::get_if<Failure>(&parsed)) {
      if (failure->remains)
        fail("unconverted data remains when parsing with format \"" + format + "\": \"" + failure->rest + "\"");
      fail("time data \"" + std::string(text) + "\" doesn't match format \"" + format + "\"");
    }
    const auto& fields = std::get<Fields>(parsed);
    if (!year_ok(fields.year)) throw Outside{};
    if (fields.day > month_days(fields.year, fields.month)) fail("day is out of range for month");
    std::string fraction = fields.fraction;
    fraction.resize(9, '0');
    const Value value{fields.year, fields.month, fields.day, fields.hour, fields.minute, fields.second,
                      static_cast<std::uint32_t>(number(fraction))};
    if (coerce_value && (fields.fraction.size() > 6 || pass.nanoseconds) && !in_ns_range(value)) {
      result.values.emplace_back();
      continue;
    }
    if (fields.fraction.size() > 6 && !pass.nanoseconds) {
      pass.nanoseconds = true;
      pass.changed = pass.seen;
    }
    if (pass.nanoseconds && !in_ns_range(value)) throw Error{out_of_bounds(std::string(text)) + kSuggestions};
    pass.seen = true;
    const std::optional<int> offset = fields.utc_name ? std::optional<int>(0) : fields.offset_minutes;
    if (pass.nanoseconds) check_utc(value, offset, true);
    pass.offsets.insert(offset);
    pass.held.emplace_back(value, text);
    pass.held_offsets.push_back(offset);
    result.values.emplace_back(value);
  }
  return finish(pass, std::move(result), kSuggestions);
}

Result element_by_element(const std::vector<std::string_view>& texts) {
  Result result;
  Pass pass;
  for (const auto text : texts) {
    if (text.empty()) {
      result.values.emplace_back();
      continue;
    }
    std::optional<Element> coerced;
    if (coerce_value) {
      try {
        coerced = element(text);
      } catch (const Error&) {
        result.values.emplace_back();
        continue;
      }
      if ((coerced->nanoseconds || pass.nanoseconds) && !in_ns_range(coerced->value)) {
        result.values.emplace_back();
        continue;
      }
    }
    const auto parsed = coerced ? *coerced : element(text);
    const auto& value = parsed.value;
    if (!year_ok(value.year)) throw Outside{};
    if (parsed.nanoseconds) {
      if (!in_ns_range(value)) {
        // The ISO parser's range error shows the fields, dateutil's none.
        std::string fields;
        if (parsed.iso)
          fields = std::to_string(value.year) + "-" + two(value.month) + "-" + two(value.day) + " " +
                   two(value.hour) + ":" + two(value.minute) + ":" + two(value.second);
        throw Error{out_of_bounds(fields)};
      }
      if (!pass.nanoseconds) {
        pass.nanoseconds = true;
        pass.changed = pass.seen;
      }
    }
    if (pass.nanoseconds && !in_ns_range(value)) throw Error{out_of_bounds(std::string(text))};
    pass.seen = true;
    if (pass.nanoseconds) check_utc(value, parsed.offset_minutes, true);
    pass.offsets.insert(parsed.offset_minutes);
    pass.held.emplace_back(value, text);
    pass.held_offsets.push_back(parsed.offset_minutes);
    result.values.emplace_back(value);
  }
  return finish(pass, std::move(result), "");
}

Date local_today() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  return Date{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

int local_year() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  return local.tm_year + 1900;
}

}  // namespace

namespace {

std::optional<Result> to_datetime_on(const std::vector<std::string_view>& cells, int current_year, Date today) {
  // pandas' NaT spellings are missing values, for the guess too.
  std::vector<std::string_view> texts;
  texts.reserve(cells.size());
  for (const auto cell : cells) {
    const bool nat = cell == "NaT" || cell == "nat" || cell == "NAT" || cell == "nan" || cell == "NaN" ||
                     cell == "NAN";
    texts.push_back(nat ? std::string_view() : cell);
  }
  std::optional<std::string_view> first;
  for (const auto text : texts) {
    if (text.empty()) continue;
    if (!in_domain(text)) return std::nullopt;
    if (!first) first = text;
  }
  if (!first) return std::nullopt;
  current_year_value = current_year;
  today_value = today;
  try {
    const auto format = guess_format(*first);
    return format ? with_format(texts, *format) : element_by_element(texts);
  } catch (const Outside&) {
    return std::nullopt;
  } catch (const Error& error) {
    throw std::invalid_argument(error.message);
  }
}

}  // namespace

std::optional<Result> to_datetime(const std::vector<std::string_view>& texts, int current_year) {
  return to_datetime_on(texts, current_year, local_today());
}

std::optional<Result> to_datetime(const std::vector<std::string_view>& texts, Date today) {
  return to_datetime_on(texts, today.year, today);
}

std::optional<Result> to_datetime(const std::vector<std::string_view>& texts) {
  return to_datetime_on(texts, local_year(), local_today());
}

std::optional<Result> to_datetime_coerce(const std::vector<std::string_view>& texts, Date today) {
  struct Reset {
    ~Reset() { coerce_value = false; }
  } reset;
  coerce_value = true;
  return to_datetime_on(texts, today.year, today);
}

std::optional<Result> to_datetime_coerce(const std::vector<std::string_view>& texts) {
  return to_datetime_coerce(texts, local_today());
}

namespace {

// The host's local zone names as Python's time.tzname holds them: strftime's
// %Z on January 1 and July 1 of this year.
std::array<std::string, 2> local_zone_names() {
  std::array<std::string, 2> names;
  const int year = local_year();
  for (int i = 0; i < 2; ++i) {
    std::tm when{};
    when.tm_year = year - 1900;
    when.tm_mon = i == 0 ? 0 : 6;
    when.tm_mday = 1;
    when.tm_hour = 12;
    when.tm_isdst = -1;
    std::mktime(&when);
    char buffer[128];
    const auto length = std::strftime(buffer, sizeof buffer, "%Z", &when);
    names[static_cast<std::size_t>(i)] = std::string(buffer, length);
  }
  return names;
}

}  // namespace

std::optional<DateutilReading> dateutil_parser_parse(std::string_view text, bool fuzzy, Date today) {
  // dateutil's lexer reads every whitespace character (str.isspace) as one
  // space token.
  std::string spaced(text);
  for (char& c : spaced)
    if (c == '\n' || c == '\r' || c == '\v' || c == '\f' || (c >= '\x1c' && c <= '\x1f')) c = ' ';
  for (const char c : spaced)
    if (!((c >= ' ' && c <= '~') || c == '\t')) return std::nullopt;
  current_year_value = today.year;
  std::optional<Res> res;
  try {
    res = dateutil_parse(spaced, fuzzy);
  } catch (const Outside&) {
    return std::nullopt;
  }
  const std::string original(text);
  const auto fail = [&](const std::string& message) -> void {
    throw std::invalid_argument(message + ": " + original);
  };
  if (!res) fail("Unknown string format");
  if (!res->year && !res->month && !res->day && !res->hour && !res->minute && !res->second &&
      !res->microsecond && !res->weekday && !res->tzname && !res->tzoffset && !res->ampm)
    fail("String does not contain a date");
  // _build_naive on today at midnight: a missing day takes today's, cut to
  // the month's length (calendar.monthrange, which raises on a bad month).
  std::int64_t year = res->year.value_or(today.year), month = res->month.value_or(today.month);
  std::int64_t day = res->day.value_or(today.day);
  if (!res->day) {
    if (month < 1 || month > 12) fail("bad month number " + std::to_string(month) + "; must be 1-12");
    const auto last = month_days(year >= 1 && year <= 9999 ? year : 2000 + ((year % 400) + 400) % 400, month);
    if (day > last) day = last;
  }
  const std::int64_t hour = res->hour.value_or(0), minute = res->minute.value_or(0);
  const std::int64_t second = res->second.value_or(0), microsecond = res->microsecond.value_or(0);
  // datetime.replace converts every field to a C int first.
  for (const auto value : {year, month, day, hour, minute, second, microsecond})
    if (value > 2147483647) throw std::invalid_argument("signed integer is greater than maximum");
  if (year < 1 || year > 9999) fail("year " + std::to_string(year) + " is out of range");
  if (month < 1 || month > 12) fail("month must be in 1..12");
  if (day < 1 || day > month_days(year, month)) fail("day is out of range for month");
  if (hour > 23) fail("hour must be in 0..23");
  if (minute > 59) fail("minute must be in 0..59");
  if (second > 59) fail("second must be in 0..59");
  if (microsecond > 999999) fail("microsecond must be in 0..999999");
  // A weekday without a day moves forward to that weekday (relativedelta).
  if (res->weekday && !res->day) {
    const int ahead = ((*res->weekday - weekday_of(year, static_cast<int>(month), static_cast<int>(day))) % 7 + 7) % 7;
    auto days = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) + ahead;
    if (days > days_from_civil(9999, 12, 31)) throw std::invalid_argument("date value out of range");
    // civil_from_days (Howard Hinnant).
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const std::int64_t doe = days - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp < 10 ? mp + 3 : mp - 9;
    year = yoe + era * 400 + (month <= 2);
  }
  DateutilReading reading;
  reading.value = Value{static_cast<int>(year), static_cast<int>(month), static_cast<int>(day),
                        static_cast<int>(hour), static_cast<int>(minute), static_cast<int>(second),
                        static_cast<std::uint32_t>(microsecond * 1000)};
  // _build_tzaware without tzinfos: a name of the host's zone is the local
  // zone (outside the model unless UTC, GMT or Z), offset 0 UTC, another
  // offset a fixed zone, an unknown name naive (dateutil only warns).
  if (res->tzname) {
    const auto names = local_zone_names();
    if (*res->tzname == names[0] || *res->tzname == names[1]) {
      if (!utc_zone(*res->tzname)) return std::nullopt;
      reading.offset_seconds = 0;
      return reading;
    }
  }
  if (res->tzoffset) reading.offset_seconds = *res->tzoffset;
  return reading;
}

std::optional<DateutilReading> dateutil_parser_parse(std::string_view text, bool fuzzy) {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  return dateutil_parser_parse(text, fuzzy, Date{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
}

namespace {

UtcInstant epoch_nanoseconds(std::int64_t value) {
  std::int64_t seconds = value / 1000000000;
  std::int64_t rest = value % 1000000000;
  if (rest < 0) {
    rest += 1000000000;
    --seconds;
  }
  return UtcInstant{seconds, static_cast<std::uint32_t>(rest), false};
}


}  // namespace

std::optional<std::vector<std::optional<UtcInstant>>> to_datetime_utc_mixed(const std::vector<Cell>& cells,
                                                                            Date today) {
  std::vector<std::optional<UtcInstant>> out;
  out.reserve(cells.size());
  bool numbers = false, years_out = false, nanoseconds = false, outside = false;
  std::optional<std::string> raised;  // an error raises whatever the other values are
  current_year_value = today.year;
  today_value = today;
  for (const auto& cell : cells) {
    out.emplace_back();
    switch (cell.kind) {
      case Cell::Kind::kNull:
        break;
      case Cell::Kind::kBlob:
        if (!raised) raised = "TypeError: <class 'bytes'> is not convertible to datetime";
        break;
      case Cell::Kind::kInteger:
        if (cell.integer == std::numeric_limits<std::int64_t>::min()) {
          outside = true;  // NaT's value
          break;
        }
        numbers = true;
        out.back() = epoch_nanoseconds(cell.integer);
        break;
      case Cell::Kind::kReal:
        if (std::isnan(cell.real)) break;
        // Past int64 (or infinite) the nanoseconds overflow, which raises.
        if (!(cell.real < 9223372036854775808.0 && cell.real > -9223372036854775808.0)) {
          if (!raised) raised = "OutOfBoundsDatetime: cannot convert input with unit 'ns'";
          break;
        }
        numbers = true;
        out.back() = epoch_nanoseconds(static_cast<std::int64_t>(cell.real));
        break;
      case Cell::Kind::kText: {
        const auto t = cell.text;
        if (t.empty() || t == "NaT" || t == "nat" || t == "NAT" || t == "nan" || t == "NaN" || t == "NAN") break;
        if (!in_domain(t)) {
          outside = true;
          break;
        }
        try {
          const auto parsed = element(t);
          const auto& v = parsed.value;
          // Microseconds hold any year from 1 (pandas' dateutil default).
          if (v.year < 1 || v.year > 9999) {
            outside = true;
            break;
          }
          if (v.year < 1678 || v.year > 2261) years_out = true;
          if (parsed.nanoseconds) nanoseconds = true;
          std::int64_t seconds =
              days_from_civil(v.year, static_cast<unsigned>(v.month), static_cast<unsigned>(v.day)) * 86400 +
              v.hour * 3600 + v.minute * 60 + v.second;
          if (parsed.offset_minutes) seconds -= static_cast<std::int64_t>(*parsed.offset_minutes) * 60;
          out.back() = UtcInstant{seconds, v.nanoseconds, parsed.offset_minutes.has_value()};
        } catch (const LongInteger&) {
          // dateutil puts such a number in no field: pandas raises (which
          // error does not matter here, the column is kept either way).
          if (!raised) raised = "OutOfBoundsDatetime: Parsing \"" + std::string(t) + "\" to datetime overflows";
        } catch (const Outside&) {
          outside = true;
        } catch (const Error& error) {
          if (!raised) raised = error.message;
        } catch (const std::invalid_argument& error) {
          if (!raised) raised = error.what();
        }
        break;
      }
    }
  }
  // Numbers or a nanosecond text make the column nanoseconds, where such a
  // year overflows.
  if (!raised && years_out && (numbers || nanoseconds))
    raised = "OutOfBoundsDatetime: Out of bounds nanosecond timestamp";
  if (raised) throw std::invalid_argument(*raised);
  if (outside) return std::nullopt;
  return out;
}

std::optional<std::vector<std::optional<UtcInstant>>> to_datetime_utc_mixed(const std::vector<Cell>& cells) {
  return to_datetime_utc_mixed(cells, local_today());
}

// Round FK: whether a cell makes to_datetime_utc_mixed's column nanoseconds
// (a number, or a text read at nanoseconds; see the column flag there).
bool utc_mixed_nanosecond_cell(const Cell& cell) {
  switch (cell.kind) {
    case Cell::Kind::kInteger: return cell.integer != std::numeric_limits<std::int64_t>::min();
    case Cell::Kind::kReal:
      return !std::isnan(cell.real) && cell.real < 9223372036854775808.0 && cell.real > -9223372036854775808.0;
    case Cell::Kind::kText: {
      const auto t = cell.text;
      // Only a run of seven or more digits after a dot can read so.
      bool run = false;
      for (std::size_t dot = t.find('.'); dot != std::string_view::npos && !run; dot = t.find('.', dot + 1)) {
        std::size_t digits = 0;
        while (dot + 1 + digits < t.size() && t[dot + 1 + digits] >= '0' && t[dot + 1 + digits] <= '9') ++digits;
        run = digits > 6;
      }
      if (!run || !in_domain(t)) return false;
      const auto today = local_today();
      current_year_value = today.year;
      today_value = today;
      try {
        return element(t).nanoseconds;
      } catch (...) {
        return false;
      }
    }
    default: return false;
  }
}

}  // namespace minera::pandas_to_datetime
