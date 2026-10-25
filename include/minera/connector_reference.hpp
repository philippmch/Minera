#pragma once

// algo.connectors of the PM4Py 2.7.22 reference, as observed from the outside
// with stubbed sources (SQLite files, database connections, HTTP responses
// and Windows automation objects). Each connector returns the dataframe the
// reference returns, as a ConnectorReferenceFrame: columns, pandas dtypes,
// the row labels and the rows in order, values rendered as the reference
// prints them (str of a timestamp, repr of a float, True/False, nullopt for a
// missing value).
//
// Browser history (variants.chrome_history and variants.firefox_history)
// reads one database file. Chrome opens it read-only, reads the visits
// joined to their urls (the SQL below) and maps from_visit through the urls
// table's ids (not the visits'), so a referrer is the url whose id equals the
// from_visit value, '' when there is none. Firefox reads moz_historyvisits
// joined to moz_places. Per row, in order, the reference converts the visit
// time (Chrome: 1601-01-01 plus visit_time microseconds, OverflowError
// outside years 1..9999; Firefox: datetime.fromtimestamp(visit_date / 10^6,
// utc), ValueError outside years 1..9999), splits the url (AttributeError
// when it is NULL) and, for Chrome, decodes the transition (low byte; NULL
// raises TypeError, as does a NULL visit time). The part after the last "//"
// is cut at the first '?' into url_wo_parameters, the activity is that
// without commas, and the domain is the part before the first '/'. Rows with
// an empty activity are dropped after these conversions. Chrome's title
// NULL becomes '', a NULL duration 0.0 seconds. Columns are typed as pandas
// infers them from the row values (ints with a NULL become float64, a column
// of NULLs object), and the frame is sorted by timestamp, ties by @@index.
//
// The database connectors (variants.camunda_workflow, sap_accounting and
// sap_o2c) issue one fixed SQL text through the DB-API connection they are
// given (the SAP ones with every "OWNED." replaced by the prefix parameter),
// read the result as pandas.read_sql does, close the connection and return
// pm4py.format_dataframe of that table with the default keys.
//
// variants.github_repo reads the issues of owner/repository page by page
// (GET https://api.github.com/repos/{owner}/{repository}/issues?state=all&
// per_page=100&page=N with {"Authorization": "Bearer " + token} as headers,
// {} without a token) until a page is empty, and fetches each issue's
// timeline_url (issues without that key are skipped). An issue gives a
// "created" row (its created_at, user.login, author_association, title and,
// when the key is present, pull_request.url as case:pull_request); the
// timeline is walked from its last event to its first, and an event holding
// created_at, event and actor gives a row of those, the others are skipped.
// Every key named here is read directly, so a missing one, a null user,
// actor or pull_request, or a created_at that is not a date stops the whole
// extraction: the reference catches it, and the frame holds the rows read
// until then. Dates are parsed by dateutil and the offset is dropped (the
// wall clock is labelled UTC); the native side accepts the ISO 8601 forms
// the API writes (YYYY-MM-DD, optionally followed by T or a space, HH:MM,
// :SS, a fraction truncated to microseconds and Z or an offset), and treats
// anything else as not a date. The frame is pm4py.format_dataframe of the
// rows: cases ordered by their first event (by timestamp, then row), rows
// by case, timestamp and row.
//
// The Windows connectors read COM objects (win32com stubbed). Their values
// are ConnectorReferenceCom: a missing attribute is an absent member, and
// str() of a value is Python's (None, True, 5, repr of a float, the text).
//
// variants.outlook_calendar walks the default calendar folder (9): per item
// ConversationID and Subject (str), CreationTime, LastModificationTime and
// Start (datetimes; an aware one is converted to UTC, a naive one read as
// UTC) and Duration (minutes, int or float) give "Meeting Created", "Last
// Change of Meeting", "Meeting Started" and "Meeting Completed" (Start plus
// Duration); a MeetingStatus equal to 5 adds "Meeting Cancelled" at the last
// modification; a truthy ReminderSet with an int ReminderMinutesBeforeStart
// adds "Meeting Reminder Fired" that many minutes before Start (unless
// include_reminders is false, when ReminderSet is not read). A failure
// before the four rows drops the item; a missing MeetingStatus, ReminderSet
// or minutes (or a minutes value that is no int) drops only its row.
// variants.outlook_mail_extractor walks the sent folder (5) and then the
// inbox (6): an item whose str(Class) is "43" (mail) or "46" (delivery
// report) gives one row "Sent ..."/"Received ..." from Subject, CreationTime,
// Sender.Name ("EMPTY" when that fails), the recipients' names joined by
// " AND " ("EMPTY" when that fails), ConversationID and ConversationTopic;
// any other failure drops the item. variants.windows_events reads every
// Win32_NTLogEvent row's properties as str() in a fixed order, parsing
// TimeGenerated and TimeWritten as they are read (cut at the first '+' or
// '-', then time.strptime with "%Y%m%d%H%M%S.%f"); a missing property or a
// time that does not parse raises. The three frames are ordered as
// format_dataframe orders them, and an empty one raises KeyError as the
// reference does.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace minera {

// Where the reference raises: type is the Python exception class name.
class ConnectorReferenceError : public std::runtime_error {
 public:
  ConnectorReferenceError(std::string type, const std::string& message)
      : std::runtime_error(type + ": " + message), type_(std::move(type)) {}
  [[nodiscard]] const std::string& type() const noexcept { return type_; }

 private:
  std::string type_;
};

struct ConnectorReferenceFrame {
  std::vector<std::string> columns, dtypes;
  std::vector<std::size_t> index;
  std::vector<std::vector<std::optional<std::string>>> rows;
  bool operator==(const ConnectorReferenceFrame&) const = default;
};

// A DB-API value: None, int, float or str.
using ConnectorReferenceCell = std::variant<std::monostate, std::int64_t, double, std::string>;

struct ConnectorReferenceResultSet {
  std::vector<std::string> names;
  std::vector<std::vector<ConnectorReferenceCell>> rows;
};

// The connection a database connector is given: execute runs one SQL text
// and returns the cursor's result; close is called once afterwards.
struct ConnectorReferenceConnection {
  std::function<ConnectorReferenceResultSet(const std::string& sql)> execute;
  std::function<void()> close;
};

// The SQL text a database connector issues.
[[nodiscard]] std::string reference_database_connector_query(const std::string& connector, const std::string& prefix = {});

[[nodiscard]] ConnectorReferenceFrame reference_camunda_workflow(const ConnectorReferenceConnection& connection);
[[nodiscard]] ConnectorReferenceFrame reference_sap_accounting(const ConnectorReferenceConnection& connection,
                                                               const std::string& prefix = {});
[[nodiscard]] ConnectorReferenceFrame reference_sap_o2c(const ConnectorReferenceConnection& connection,
                                                        const std::string& prefix = {});

// A decoded JSON response: members keep their order.
struct ConnectorReferenceJson {
  enum Kind { kNull, kBool, kInt, kFloat, kText, kArray, kObject } kind = kNull;
  bool boolean = false;
  std::int64_t integer = 0;
  double real = 0.0;
  std::string text;
  std::vector<ConnectorReferenceJson> items;
  std::vector<std::pair<std::string, ConnectorReferenceJson>> members;
  // The member named key, nullptr when absent (or when this is no object).
  [[nodiscard]] const ConnectorReferenceJson* find(const std::string& key) const;
};

// requests.get(url, headers=headers).json()
using ConnectorReferenceGet =
    std::function<ConnectorReferenceJson(const std::string& url, const std::map<std::string, std::string>& headers)>;

[[nodiscard]] ConnectorReferenceFrame reference_github_repo(const std::string& owner, const std::string& repository,
                                                            const std::optional<std::string>& token,
                                                            const ConnectorReferenceGet& get);

// A value read from a COM object.
struct ConnectorReferenceCom {
  enum Kind { kNone, kBool, kInt, kFloat, kText, kTime, kObject, kList } kind = kNone;
  bool boolean = false;
  std::int64_t integer = 0;
  double real = 0.0;
  std::string text;
  // kTime: the wall clock in microseconds since 1970-01-01, and the UTC
  // offset in minutes of an aware datetime.
  std::int64_t micros = 0;
  std::optional<int> offset_minutes;
  std::vector<std::pair<std::string, ConnectorReferenceCom>> members;  // kObject attributes
  std::vector<ConnectorReferenceCom> items;                            // kList
  [[nodiscard]] const ConnectorReferenceCom* find(const std::string& name) const;
};

[[nodiscard]] ConnectorReferenceFrame reference_outlook_calendar(const std::vector<ConnectorReferenceCom>& calendar,
                                                                 bool include_reminders = true);
[[nodiscard]] ConnectorReferenceFrame reference_outlook_mail(const std::vector<ConnectorReferenceCom>& sent,
                                                             const std::vector<ConnectorReferenceCom>& inbox);
// util.mail.connect(email_user, mailbox_id), the folder every Outlook
// connector reads (the calendar's email_user and calendar_id parameters, 9 by
// default; the mail extractor always the own inbox, 6, and sent items, 5):
// the COM calls it makes, as text. Without a user the own default folder is
// opened; with one (the empty string included) the user is resolved as a
// recipient and their shared default folder is opened.
[[nodiscard]] std::vector<std::string> reference_mail_connect_calls(const std::optional<std::string>& email_user,
                                                                    int mailbox_id);
// Each row is a kObject of Win32_NTLogEvent properties.
[[nodiscard]] ConnectorReferenceFrame reference_windows_events(const std::vector<ConnectorReferenceCom>& rows);

[[nodiscard]] ConnectorReferenceFrame reference_chrome_history(const std::filesystem::path& database);
[[nodiscard]] ConnectorReferenceFrame reference_firefox_history(const std::filesystem::path& database);

}  // namespace minera
