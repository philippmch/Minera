#include "minera/ocel_csv.hpp"
#include "minera/ocel_statistics.hpp"
#include "minera/ocel_transform.hpp"
#include "minera/ocel_xml.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace {

template <typename F>
void throws(F&& function) {
  bool rejected = false;
  try {
    function();
  } catch (const std::exception&) {
    rejected = true;
  }
  assert(rejected);
}

// Captured with PM4Py 2.7.22: read_ocel2_json on kFixture, then write_ocel
// to CSV. Fixed columns come first in the order the reference frame holds
// them, attributes in first-appearance order, object types in relation
// order; values print as Python's str, list cells as the list's repr,
// timestamps as pandas prints them, and the integer 3 of a float column
// as 3.0.
constexpr const char* kFixture = R"REF({"objectTypes":[{"name":"order","attributes":[{"name":"color","type":"string"}]},{"name":"item","attributes":[{"name":"weight","type":"float"}]}],
 "eventTypes":[{"name":"create","attributes":[]},{"name":"pick, item","attributes":[]}],
 "objects":[
  {"id":"o1","type":"order","attributes":[{"name":"color","time":"1970-01-01T00:00:00Z","value":"red"},{"name":"color","time":"2020-01-02T00:00:00Z","value":"blue"}],"relationships":[{"objectId":"i1","qualifier":"contains"}]},
  {"id":"i1","type":"item","attributes":[{"name":"weight","time":"1970-01-01T00:00:00Z","value":2.5}]},
  {"id":"i'2","type":"item","attributes":[{"name":"weight","time":"1970-01-01T00:00:00Z","value":3}]}],
 "events":[
  {"id":"e1","type":"create","time":"2020-01-01T00:00:00Z","attributes":[{"name":"n","value":4},{"name":"flag","value":false},{"name":"cost","value":0.1},{"name":"note","value":"a<b&\"c\"\nd"}],"relationships":[{"objectId":"o1","qualifier":"q"}]},
  {"id":"e2","type":"pick, item","time":"2020-01-01T00:00:01.250000Z","attributes":[{"name":"n","value":5},{"name":"flag","value":true}],"relationships":[{"objectId":"i'2","qualifier":""}]},
  {"id":"e3","type":"create","time":"2020-01-01T00:00:02.000001Z","attributes":[{"name":"n","value":-6},{"name":"flag","value":true},{"name":"cost","value":1e20}],"relationships":[{"objectId":"i1","qualifier":""}]},
  {"id":"e4","type":"pick, item","time":"2020-01-01T00:00:03Z","attributes":[{"name":"n","value":7},{"name":"flag","value":false}],"relationships":[{"objectId":"o1","qualifier":"r"}]}]})REF";
constexpr const char* kReferenceEvents = R"REF(ocel:eid,ocel:timestamp,ocel:activity,n,flag,cost,note,ocel:type:order,ocel:type:item
e1,2020-01-01 00:00:00+00:00,create,4,False,0.1,"a<b&""c""
d",['o1'],
e2,2020-01-01 00:00:01.250000+00:00,"pick, item",5,True,,,,"[""i'2""]"
e3,2020-01-01 00:00:02.000001+00:00,create,-6,True,1e+20,,,['i1']
e4,2020-01-01 00:00:03+00:00,"pick, item",7,False,,,['o1'],
)REF";
constexpr const char* kReferenceObjects = R"REF(ocel:oid,ocel:type,color,weight
o1,order,red,
i1,item,,2.5
i'2,item,,3.0
)REF";

void reference_capture_tests() {
  const auto log = minera::parse_ocel_json(kFixture);
  // Qualifiers, the O2O link and the object change need the loss allowance.
  throws([&] { (void)minera::serialize_ocel_csv(log); });
  minera::OcelCsvExportOptions lossy;
  lossy.allow_lossy = true;
  assert(minera::serialize_ocel_csv(log, lossy) == kReferenceEvents);
  assert(minera::serialize_ocel_objects_csv(log, lossy) == kReferenceObjects);
  const auto reread = minera::parse_ocel_csv(kReferenceEvents, kReferenceObjects);
  assert(reread.events.size() == 4 && reread.objects.size() == 3 && reread.relations.size() == 4);
  bool quoted_id = false;
  for (const auto& relation : reread.relations) quoted_id = quoted_id || relation.object_id == "i'2";
  assert(quoted_id);
  assert(std::get<std::string>(reread.events[0].attributes[3].value) == "a<b&\"c\"\nd");
  assert(std::get<std::string>(reread.events[1].attributes[1].value) == "True");
}

// Row shapes and quoting follow pandas.read_csv(index_col=False), which
// pm4py.read_ocel_csv uses for both tables (observed on pm4py 2.7.22 with
// pandas 3.0.5): blank lines are skipped but counted, the first data row
// fixes the field count, fields past the header are dropped, a later longer
// row is refused with read_csv's message, short rows read missing cells, and
// a row whose id or activity (object id or type) is missing is dropped.
void follows_reference_csv_row_shapes() {
  const std::string header = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order,cost\n";
  const auto refused = [](const std::string& events, const std::string& expected,
                          const std::string* objects = nullptr) {
    try {
      (void)(objects ? minera::parse_ocel_csv(events, *objects) : minera::parse_ocel_csv(events));
    } catch (const std::invalid_argument& error) {
      return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
  };
  const auto ids = [](const minera::Ocel& log) {
    std::string out;
    for (const auto& event : log.events) out += event.id + ":" + event.activity + "(" +
        std::to_string(event.attributes.size()) + ")";
    out += "|";
    for (const auto& relation : log.relations) out += relation.event_id + ">" + relation.object_id + ";";
    out += "|";
    for (const auto& object : log.objects) out += object.id + ":" + object.type + "(" +
        std::to_string(object.attributes.size()) + ")";
    return out;
  };
  const std::string row1 = "e1,2024-01-01T00:00:00Z,A,['o1'],5\n";
  // A short row reads its missing cells as missing values.
  assert(ids(minera::parse_ocel_csv(header + row1 + "e2,2024-01-02T00:00:00Z,B\n")) ==
         "e1:A(1)e2:B(0)|e1>o1;|o1:order(0)");
  // A missing id or activity drops the row; without an object table its
  // objects are still listed.
  assert(ids(minera::parse_ocel_csv(header + row1 + "e2,2024-01-02T00:00:00Z\n")) ==
         "e1:A(1)|e1>o1;|o1:order(0)");
  assert(ids(minera::parse_ocel_csv(header + "e0,2024-01-01T00:00:00Z,,['o9'],5\n" +
                                    ",2024-01-01T00:00:00Z,B,['o8'],5\n" + row1)) ==
         "e1:A(1)|e1>o1;|o9:order(0)o8:order(0)o1:order(0)");
  // A missing timestamp is NaT in the reference, which a native event cannot hold.
  assert(refused(header + "e1,,A,['o1'],5\n", "has no timestamp"));
  // The reference parses timestamps before it drops rows, so a dropped row's
  // unreadable timestamp refuses the file.
  assert(refused(header + row1 + "e2,extra,,['o1'],5\n", "not ISO-8601: extra"));
  assert(refused(header + row1 + ",extra,B,['o1'],5\n", "not ISO-8601: extra"));
  // Fields past the header are dropped when the first data row is longer.
  assert(ids(minera::parse_ocel_csv(header + "e1,2024-01-01T00:00:00Z,A,['o1'],5,x,y\n"
                                             "e2,2024-01-02T00:00:00Z,B,['o1'],6,z\n")) ==
         "e1:A(1)e2:B(1)|e1>o1;e2>o1;|o1:order(0)");
  // A later row longer than the first data row is refused like read_csv.
  assert(refused(header + row1 + "e2,2024-01-02T00:00:00Z,B,['o1'],6,x\n",
                 "Expected 5 fields in line 3, saw 6"));
  assert(refused(header + "e1,2024-01-01T00:00:00Z,A,['o1'],5,x\n"
                          "e2,2024-01-02T00:00:00Z,B,['o1'],6,y,z\n",
                 "Expected 6 fields in line 3, saw 7"));
  assert(refused(header + "e1,2024-01-01T00:00:00Z\ne2,2024-01-02T00:00:00Z,B,['o1'],6,y\n",
                 "Expected 5 fields in line 3, saw 6"));
  // Blank and whitespace-only lines are skipped but count as lines; a quoted
  // field spanning lines is one.
  assert(ids(minera::parse_ocel_csv("\n" + header + "\n  \n\t\n" + row1 + "\n")) ==
         "e1:A(1)|e1>o1;|o1:order(0)");
  assert(refused("\n" + header + row1 + "\n  \ne2,2024-01-02T00:00:00Z,B,['o1'],6,x\n",
                 "Expected 5 fields in line 6, saw 6"));
  assert(refused(header + "e1,2024-01-01T00:00:00Z,\"A\nB\",['o1'],5\n"
                          "e2,2024-01-02T00:00:00Z,B,['o1'],6,x\n",
                 "Expected 5 fields in line 3, saw 6"));
  // Quoting: a quote inside an unquoted field is literal, text after a
  // closing quote is appended, "" is one quote, and an open quote is an error.
  const auto activity_of = [&](const std::string& cell) {
    return minera::parse_ocel_csv(header + "e1,2024-01-01T00:00:00Z," + cell + ",['o1'],5\n")
        .events.at(0).activity;
  };
  assert(activity_of("A\"x") == "A\"x");
  assert(activity_of("\"A\"x") == "Ax");
  assert(activity_of("\"A,B\"x") == "A,Bx");
  assert(activity_of("\"A\"\"B\"") == "A\"B");
  assert(activity_of("\"A\nB\"") == "A\nB");
  assert(refused(header + row1 + "e2,2024-01-02T00:00:00Z,\"B,['o1'],6\n", "unterminated quote"));
  // The object table follows the same shapes; a missing id or type drops the row.
  const std::string objects = "ocel:oid,ocel:type,size\n,order,3\no2,,3\no3\no1,order\n\n";
  assert(ids(minera::parse_ocel_csv(header + row1, objects)) == "e1:A(1)|e1>o1;|o1:order(0)");
  const std::string long_first = "ocel:oid,ocel:type,size\no1,order,3,x\n";
  assert(ids(minera::parse_ocel_csv(header + row1, long_first)) == "e1:A(1)|e1>o1;|o1:order(1)");
  const std::string long_later = "ocel:oid,ocel:type,size\no1,order,3\no2,order,4,x\n";
  assert(refused(header + row1, "object CSV line 3 has 4 fields", &long_later));
  assert(refused(header + row1, "Expected 3 fields in line 3, saw 4", &long_later));
}

// An object listed twice in one cell is two relations, as pm4py.read_ocel_csv
// keeps one relation row per list entry. Observed on pm4py 2.7.22 for this
// table: the relations below in this order, objects_ot_count counting each
// repeat, ocel_flattening repeating the event, write_ocel_csv writing the
// repeats back into the cell, and write_ocel2_xml / write_ocel2_sqlite (and
// their readers) keeping them.
void keeps_repeated_relations() {
  const std::string events =
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order,ocel:type:item\n"
      "e1,2024-01-01T00:00:00,Create,\"['o1', 'o1']\",\"['i1']\"\n"
      "e2,2024-01-02T00:00:00,Pack,['o1'],\"['i1', 'i2', 'i1']\"\n"
      "e3,2024-01-03T00:00:00,Ship,\"['o1', 'o1', 'o1']\",['i2']\n";
  const std::string objects = "ocel:oid,ocel:type\no1,order\ni1,item\ni2,item\n";
  const auto relations = [](const minera::Ocel& log) {
    std::string out;
    for (const auto& relation : log.relations) out += relation.event_id + ">" + relation.object_id + ";";
    return out;
  };
  const std::string expected =
      "e1>o1;e1>o1;e1>i1;e2>o1;e2>i1;e2>i2;e2>i1;e3>o1;e3>o1;e3>o1;e3>i2;";
  const auto log = minera::parse_ocel_csv(events, objects);
  assert(relations(log) == expected);
  assert(log.objects.size() == 3);
  // Without an object table each object is listed once.
  const auto event_only = minera::parse_ocel_csv(events);
  assert(relations(event_only) == expected && event_only.objects.size() == 3);

  const auto counts = minera::objects_ot_count(log);
  assert(counts.at("e1").at("order") == 2 && counts.at("e1").at("item") == 1);
  assert(counts.at("e2").at("item") == 3 && counts.at("e3").at("order") == 3);
  const auto flat = minera::flatten_ocel_to_event_log(log, "order");
  assert(flat.traces.size() == 1 && flat.traces[0].events.size() == 6);

  // The writer needs no loss allowance for repeats and writes them back.
  const auto written = minera::serialize_ocel_csv(log);
  assert(written.find("\"['o1', 'o1']\"") != std::string::npos);
  assert(written.find("\"['i1', 'i2', 'i1']\"") != std::string::npos);
  assert(written.find("\"['o1', 'o1', 'o1']\"") != std::string::npos);
  assert(relations(minera::parse_ocel_csv(written, minera::serialize_ocel_objects_csv(log))) ==
         expected);
  minera::OcelXmlExportOptions standard_xml;
  standard_xml.format = minera::OcelXmlFormat::kStandard;
  assert(relations(minera::parse_ocel_xml(minera::serialize_ocel_xml(log, standard_xml))) ==
         expected);
}

// Object cells as pm4py.read_ocel_csv reads them, observed on pm4py 2.7.22:
// a cell that is no Python list literal lists no objects (its event stays),
// elements become ids as str() prints them, None and '' are skipped, and a
// list, set or dict element raises TypeError (refused here).
void reads_object_cells_as_pandas() {
  const auto ids = [](const std::string& cell, minera::OcelCsvImportOptions options = {}) {
    std::string events = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n";
    events += "e0,2024-01-01T00:00:00Z,a," + cell + "\ne1,2024-01-02T00:00:00Z,a,['z']\n";
    const auto log = minera::parse_ocel_csv(events, options);
    assert(log.events.size() == 2);
    std::string out;
    for (const auto& relation : log.relations)
      if (relation.event_id == "e0") out += relation.object_id + ";";
    return out;
  };
  const auto refused = [&](const std::string& cell, const std::string& expected) {
    try {
      (void)ids(cell);
    } catch (const std::invalid_argument& error) {
      return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
  };
  // No Python literal, or no list: no objects.
  for (const char* cell : {"[o1]", "\"['o1'\"", "[1+1]", "\"[float('nan')]\"", "\"('o1',)\"",
                           "abc", "7", "\"['o1']x\"", "[007]", "[--1]", "[2j+1]", "[+True]",
                           "\"[f'x']\"", "\"['a' b'y']\"", "\"[1 2]\"", "\"[1,,2]\"", "\"['o1'][0]\"",
                           "\"['\\U00110000']\"", "\"['o1'] \\\n\""})
    assert(ids(cell).empty());
  minera::OcelCsvImportOptions untrimmed;
  untrimmed.trim_fields = false;
  assert(ids("\" ['o1']\"", untrimmed).empty());
  assert(ids("\"['o1'] \"", untrimmed) == "o1;");
  // Elements as str() prints them.
  assert(ids("\"['o1', None, '', 'o2', b'']\"") == "o1;o2;");
  assert(ids("\"['o1','o1']\"") == "o1;o1;");
  assert(ids("\"[0, -1, +3, 0x10, 0b11, 0o7, 1_000, 00, -(1)]\"") == "0;-1;3;16;3;7;1000;0;-1;");
  assert(ids("[12345678901234567890]") == "12345678901234567890;");
  assert(ids("[0xFFFFFFFFFFFFFFFFFFFF]") == "1208925819614629174706175;");
  assert(ids("\"[1e3, 1.5e-7, 1e16, 1., .5, -0.0, 1e999, 1_0.5]\"") ==
         "1000.0;1.5e-07;1e+16;1.0;0.5;-0.0;inf;10.5;");
  assert(ids("\"[True, ..., 3j, -1j, 1+2j, 1.5-0j, 1e5j]\"") ==
         "True;Ellipsis;3j;(-0-1j);(1+2j);(1.5+0j);100000j;");
  assert(ids("\"[b'o1', r'a\\b', u'x', rb'y', 'p' 'q', 'a' \"\"b\"\"]\"") == "o1;a\\b;x;y;pq;ab;");
  assert(ids("\"['\\x41', '\\101', '\\u00e9', 'a\\q', 'o\\'1', \"\"\"\"\"\"a\nb\"\"\"\"\"\"]\"") ==
         "A;A;\xc3\xa9;a\\q;o'1;a\nb;");
  assert(ids("\"['o1', # c\n 'o2',]\"") == "o1;o2;");
  assert(ids("\"['o1'] # c\"") == "o1;");
  assert(ids("[]") == "" && ids("\"[ ]\"") == "");
  // The reference raises.
  assert(refused("[[]]", "unhashable type: 'list'"));
  assert(refused("\"['o1', ['o2']]\"", "unhashable type: 'list'"));
  assert(refused("\"[{'a'}]\"", "unhashable type: 'set'"));
  assert(refused("\"[set()]\"", "unhashable type: 'set'"));
  assert(refused("\"[{'a': 1}]\"", "unhashable type: 'dict'"));
  assert(refused("\"['o1'],\"", "unhashable type: 'list'"));
  assert(refused("\"[b'\\xff']\"", "UnicodeDecodeError"));
  // Declared: what this reader does not support.
  assert(refused("\"[('a',)]\"", "does not support"));
  assert(refused("\"['\\N{LATIN SMALL LETTER E WITH ACUTE}']\"", "does not support"));
  assert(refused("\"['\\N{bullet}', 'o2']\"", "does not support"));
  // A malformed \N escape, or one in a cell that is no literal, lists no
  // objects, as pm4py.read_ocel_csv lists none (observed: the cell is a
  // SyntaxError); raw strings and bytes keep the escape as text.
  for (const char* cell : {"\"['\\N{BULLET']\"", "\"['\\N']\"", "\"['\\Nx']\"", "\"['\\N{}']\"",
                           "\"['\\N{BUL_LET}']\"", "\"['\\N{BULLET}'\"", "\"['\\N{BULLET}'] x\"",
                           "\"['\\N{BULLET}', o3]\""})
    assert(ids(cell).empty());
  assert(ids("\"[r'\\N{BULLET}', b'\\N{X}']\"") == "\\N{BULLET};\\N{X};");
  assert(refused("\"[1, 1.0]\"", "does not support"));
  assert(refused("\"[1, '1']\"", "does not support"));

  // A dropped row's cell (no object table) lists objects the same way.
  const auto dropped = minera::parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n"
      ",2024-01-01T00:00:00Z,a,[o1]\n,2024-01-01T00:00:00Z,a,\"['o2']\"\n"
      "e1,2024-01-02T00:00:00Z,a,['z']\n");
  assert(dropped.objects.size() == 2 && dropped.objects[0].id == "o2");

  // A delimiter right after a blank line a lone \r ends is dropped, as
  // pandas drops it ("\r\r,5,6" reads as 5,6), before the header too; a line
  // starting with spaces after such a blank line is refused.
  const std::string header = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order";
  const auto shifted = minera::parse_ocel_csv(
      "\r," + header + "\re1,2024-01-01T00:00:00Z,a,['o1']\r\r,e2,2024-01-02T00:00:00Z,b,['o2']\r"
      "\n\r \r,e3,2024-01-03T00:00:00Z,c,['o3']\r\r\n,2024-01-04T00:00:00Z,d,['o4']\r");
  assert(shifted.events.size() == 3 && shifted.objects.size() == 4);
  assert(shifted.events[0].id == "e1" && shifted.events[1].id == "e2" && shifted.events[2].id == "e3");
  bool spaces_refused = false;
  try {
    (void)minera::parse_ocel_csv(header + "\re1,2024-01-01T00:00:00Z,a,['o1']\r\r e2,2024-01-02T00:00:00Z,b,[]\r");
  } catch (const std::invalid_argument& error) {
    spaces_refused = std::string(error.what()).find("lone carriage return") != std::string::npos;
  }
  assert(spaces_refused);
  // Data rows ended by a lone \r and \r\n-ended blank lines keep their cells.
  const auto plain = minera::parse_ocel_csv(header + "\re1,2024-01-01T00:00:00Z,a,['o1']\r,2024-01-02T00:00:00Z,b,['o2']\r");
  assert(plain.events.size() == 1 && plain.objects.size() == 2);
}

// The event timestamp column as DataFrame.to_csv writes it, observed on
// pm4py 2.7.22 (read_ocel_csv, then write_ocel_csv): a naive column of
// midnights as dates alone (the year unpadded), any other naive column with
// the fraction digits its most precise value needs on every value, an aware
// column value by value in the offset its values share.
void writes_timestamps_as_pandas() {
  const auto written = [](std::initializer_list<const char*> timestamps) {
    std::string events = "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n";
    int index = 0;
    for (const auto* timestamp : timestamps)
      events += "e" + std::to_string(index++) + "," + timestamp + ",a,['o1']\n";
    const auto text = minera::serialize_ocel_csv(minera::parse_ocel_csv(events));
    std::string out;
    std::size_t line = text.find('\n') + 1;
    while (line < text.size()) {
      const auto first = text.find(',', line) + 1;
      out += text.substr(first, text.find(',', first) - first) + ";";
      line = text.find('\n', line) + 1;
    }
    return out;
  };
  assert(written({"2024-01-01 00:00:00", "2024-01-02 00:00:00"}) == "2024-01-01;2024-01-02;");
  assert(written({"2024-01-01T00:00:00", "2024-01-02T00:00:00"}) == "2024-01-01;2024-01-02;");
  assert(written({"0999-01-01 00:00:00", "2024-01-02 00:00:00"}) == "999-01-01;2024-01-02;");
  assert(written({"0999-01-01 10:00:00", "2024-01-02 00:00:00"}) ==
         "999-01-01 10:00:00;2024-01-02 00:00:00;");
  assert(written({"2024-01-01 00:00:00", "2024-01-02 10:00:00"}) ==
         "2024-01-01 00:00:00;2024-01-02 10:00:00;");
  assert(written({"2024-01-01 00:00:00.000", "2024-01-02 00:00:00.123"}) ==
         "2024-01-01 00:00:00.000;2024-01-02 00:00:00.123;");
  assert(written({"2024-01-01 00:00:00.000000", "2024-01-02 00:00:00.000001"}) ==
         "2024-01-01 00:00:00.000000;2024-01-02 00:00:00.000001;");
  assert(written({"2024-01-01 00:00:00.100000000", "2024-01-02 00:00:00.000000000"}) ==
         "2024-01-01 00:00:00.100;2024-01-02 00:00:00.000;");
  assert(written({"2024-01-01 00:00:00.000000001", "2024-01-02 00:00:00.000000000"}) ==
         "2024-01-01 00:00:00.000000001;2024-01-02 00:00:00.000000000;");
  assert(written({"2024-01-01T00:00:00.500000Z", "2024-01-02T00:00:00.000000Z"}) ==
         "2024-01-01 00:00:00.500000+00:00;2024-01-02 00:00:00+00:00;");
  assert(written({"2024-01-01T00:00:00.500000+02:00", "2024-01-02T00:00:00.000000+02:00"}) ==
         "2024-01-01 00:00:00.500000+02:00;2024-01-02 00:00:00+02:00;");
  assert(written({"2024-01-01T00:00:00-05:30", "2024-01-02T00:00:00-05:30"}) ==
         "2024-01-01 00:00:00-05:30;2024-01-02 00:00:00-05:30;");
}

// Header shapes as pandas.read_csv names them and pm4py.read_ocel_csv then
// selects the columns (observed black-box): an empty name is "Unnamed: i",
// a repeat "name.1"; a repeated "ocel:type:order" lists objects of type
// "order.1", any unselected column (also "ocel:eid.1", "ocel:foo") is an
// attribute, and a bare "ocel:type:" column lists nothing.
void names_columns_as_pandas() {
  const auto text = [](const minera::OcelValue& value) { return std::get<std::string>(value); };
  {
    const auto log = minera::parse_ocel_csv(
        ",ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order,\n0,e1,2024-01-01,A,['o1'],w\n");
    assert(log.events.size() == 1 && log.events[0].attributes.size() == 2);
    assert(log.events[0].attributes[0].key == "Unnamed: 0" && text(log.events[0].attributes[0].value) == "0");
    assert(log.events[0].attributes[1].key == "Unnamed: 5" && text(log.events[0].attributes[1].value) == "w");
    assert(log.objects.size() == 1 && log.objects[0].type == "order" && log.relations.size() == 1);
  }
  {
    const auto log = minera::parse_ocel_csv(
        "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order,ocel:type:order,ocel:eid,ocel:foo\n"
        "e1,2024-01-01,A,['o1'],['o2'],x,y\n");
    assert(log.objects.size() == 2 && log.objects[0].type == "order" && log.objects[1].type == "order.1");
    assert(log.relations.size() == 2);
    assert(log.events[0].attributes.size() == 2);
    assert(log.events[0].attributes[0].key == "ocel:eid.1" && text(log.events[0].attributes[0].value) == "x");
    assert(log.events[0].attributes[1].key == "ocel:foo");
  }
  {
    const auto log = minera::parse_ocel_csv(
        "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:\ne1,2024-01-01,A,['o1']\n");
    assert(log.events.size() == 1 && log.events[0].attributes.empty());
    assert(log.objects.empty() && log.relations.empty());
  }
  {
    const auto log = minera::parse_ocel_csv(
        "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\ne1,2024-01-01,A,['o1']\n",
        ",ocel:oid,ocel:type,ocel:oid\n0,o1,order,z\n");
    assert(log.objects.size() == 1 && log.objects[0].attributes.size() == 2);
    assert(log.objects[0].attributes[0].key == "Unnamed: 0");
    assert(log.objects[0].attributes[1].key == "ocel:oid.1" && text(log.objects[0].attributes[1].value) == "z");
  }
}

// An object id under two types: pm4py.read_ocel_csv lists it once per type
// and keeps each relation's column type (observed black-box), which a native
// OCEL cannot hold, so it is refused (declared), with or without an object
// table.
void refuses_an_id_under_two_types() {
  const std::string events =
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order,ocel:type:item\n"
      "e1,2024-01-01,A,['o1'],['o1']\n";
  const auto message = [](auto&& read) {
    try {
      (void)read();
    } catch (const std::invalid_argument& error) {
      return std::string(error.what());
    }
    return std::string();
  };
  assert(message([&] { return minera::parse_ocel_csv(events); }).starts_with(
      "object id appears under multiple types: o1 (order and item)"));
  assert(message([&] {
           return minera::parse_ocel_csv(
               "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:item\ne1,2024-01-01,A,['o1']\n",
               "ocel:oid,ocel:type\no1,order\n");
         }).starts_with("object id appears under multiple types: o1 (order and item)"));
  assert(message([&] {
           return minera::parse_ocel_csv(
               "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:item\ne1,2024-01-01,A,['o1']\n",
               "ocel:oid,ocel:type\no1,order\no1,item\n");
         }).starts_with("duplicate object CSV id: o1"));
}

// A naive OCEL 2.0 JSON or XML timestamp is stored as UTC, as the reference
// stores it, so a CSV written from it prints "+00:00" as pm4py's writer does
// ("2024-01-01 00:00:00+00:00", not the naive date "2024-01-01").
void writes_naive_json_and_xml_timestamps_as_utc() {
  const auto log = minera::parse_ocel_json(
      R"({"objectTypes":[{"name":"order","attributes":[]}],"eventTypes":[{"name":"A","attributes":[]}],)"
      R"("objects":[{"id":"o1","type":"order"}],"events":[)"
      R"({"id":"e1","type":"A","time":"2024-01-01T00:00:00","relationships":[{"objectId":"o1","qualifier":""}]},)"
      R"({"id":"e2","type":"A","time":"2024-01-02T10:30:00.123456","relationships":[{"objectId":"o1","qualifier":""}]}]})");
  const auto csv = minera::serialize_ocel_csv(log);
  assert(csv.find("e1,2024-01-01 00:00:00+00:00,A") != std::string::npos);
  assert(csv.find("e2,2024-01-02 10:30:00.123456+00:00,A") != std::string::npos);
  minera::OcelXmlImportOptions xml_options;
  const auto xml = minera::parse_ocel_xml(
      "<log><events><event><string key=\"id\" value=\"e1\"/><date key=\"timestamp\" "
      "value=\"2024-01-01T00:00:00\"/><string key=\"activity\" value=\"A\"/><list key=\"omap\">"
      "<string key=\"object-id\" value=\"o1\"/></list><list key=\"vmap\"/></event></events>"
      "<objects><object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"order\"/>"
      "<list key=\"ovmap\"/></object></objects></log>",
      xml_options);
  // The frame of an XML read is activity first, so its CSV columns are too.
  assert(minera::serialize_ocel_csv(xml).find("e1,A,2024-01-01 00:00:00+00:00") != std::string::npos);
  // pm4py.read_ocel_csv keeps naive times naive; write_ocel_xml still
  // appends "Z" and write_ocel2_xml prints the naive isoformat.
  const auto naive = minera::parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n"
      "e1,2024-01-01 00:00:00,A,['o1']\n");
  minera::OcelXmlExportOptions classic_xml;
  classic_xml.allow_lossy = true;
  assert(minera::serialize_ocel_xml(naive, classic_xml)
             .find("<date key=\"timestamp\" value=\"2024-01-01T00:00:00Z\"/>") != std::string::npos);
  minera::OcelXmlExportOptions standard_xml = classic_xml;
  standard_xml.format = minera::OcelXmlFormat::kStandard;
  const auto standard_text = minera::serialize_ocel_xml(naive, standard_xml);
  assert(standard_text.find("time=\"2024-01-01T00:00:00\"") != std::string::npos);
}

// pm4py.read_ocel_csv keeps a "+02:00" column as datetime64[us, UTC+02:00]
// and the XML writers print its wall clock: "+02:00" in OCEL 2.0 XML, the
// wall clock labelled "Z" in OCEL 1.0 XML (observed black-box). Times that
// do not share one offset stay UTC (the reference refuses to read them).
void writes_offset_csv_wall_clock_in_xml() {
  const auto log = minera::parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,d,ocel:type:order\n"
      "e1,2024-01-01 10:00:00+02:00,A,2024-05-01 10:00:00+02:00,['o1']\n"
      "e2,2024-01-01 11:30:00+02:00,B,x,\"['o1', 'o2']\"\n");
  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  standard.allow_lossy = true;
  const auto xml2 = minera::serialize_ocel_xml(log, standard);
  assert(xml2.find("time=\"2024-01-01T10:00:00+02:00\"") != std::string::npos);
  assert(xml2.find("time=\"2024-01-01T11:30:00+02:00\"") != std::string::npos);
  // A text attribute stays text.
  assert(xml2.find(">2024-05-01 10:00:00+02:00</attribute>") != std::string::npos);
  minera::OcelXmlExportOptions classic;
  classic.allow_lossy = true;
  const auto xml1 = minera::serialize_ocel_xml(log, classic);
  assert(xml1.find("<date key=\"timestamp\" value=\"2024-01-01T10:00:00Z\"/>") != std::string::npos);
  assert(xml1.find("<date key=\"timestamp\" value=\"2024-01-01T11:30:00Z\"/>") != std::string::npos);
  assert(minera::serialize_ocel_csv(log).find("e1,2024-01-01 10:00:00+02:00,A") != std::string::npos);
  const auto mixed = minera::parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n"
      "e1,2024-01-01 10:00:00+02:00,A,['o1']\n"
      "e2,2024-01-01 11:30:00Z,B,['o1']\n");
  assert(minera::serialize_ocel_xml(mixed, standard).find("time=\"2024-01-01T08:00:00+00:00\"") !=
         std::string::npos);
}

// Date-typed attribute values: pm4py.read_ocel2_json, read_ocel2_xml and
// read_ocel_xml convert them to UTC timestamps (the wall clock, its offset
// dropped; a naive text is UTC), while read_ocel_json, read_ocel2_sqlite and
// read_ocel_csv keep the text. The reference writers print a datetime64
// column as OCEL 2.0 XML "2024-02-01T08:30:00+00:00", OCEL 1.0 XML <date>
// "...Z", CSV and SQLite "2024-02-01 08:30:00+00:00" (observed black-box on
// the same fixture).
void date_attributes_read_as_reference() {
  const std::string standard_json =
      R"({"objectTypes":[{"name":"order","attributes":[{"name":"od","type":"date"},{"name":"p","type":"string"}]}],)"
      R"("eventTypes":[{"name":"A","attributes":[{"name":"d","type":"date"},{"name":"s","type":"string"}]}],)"
      R"("objects":[{"id":"o1","type":"order","attributes":[{"name":"od","value":"2024-03-01T10:00:00+02:00","time":"1970-01-01T00:00:00Z"},)"
      R"({"name":"p","value":"2024-03-01T10:00:00+02:00","time":"1970-01-01T00:00:00Z"},{"name":"od","value":"2024-03-05","time":"2024-01-05T12:00:00Z"}]},)"
      R"({"id":"o2","type":"order","attributes":[{"name":"od","value":"notadate","time":"1970-01-01T00:00:00Z"}]}],)"
      R"("events":[{"id":"e1","type":"A","time":"2024-01-01T00:00:00Z","attributes":[{"name":"d","value":"2024-02-01T08:30:00+02:00"},{"name":"s","value":"2024-02-01"}],)"
      R"("relationships":[{"objectId":"o1","qualifier":"q1"},{"objectId":"o2","qualifier":"q2"}]},)"
      R"({"id":"e2","type":"A","time":"2024-01-02T00:00:00Z","attributes":[{"name":"d","value":"2024-02-02T08:30:00.123456"}],"relationships":[{"objectId":"o1","qualifier":"q"}]}]})";
  const auto log = minera::parse_ocel_json(standard_json);
  const auto text = [](const minera::OcelValue& value) { return std::get<std::string>(value); };
  assert(text(log.events[0].attributes[0].value) == "2024-02-01T08:30:00Z");
  assert(text(log.events[0].attributes[1].value) == "2024-02-01");  // a string attribute
  assert(text(log.events[1].attributes[0].value) == "2024-02-02T08:30:00.123456Z");
  assert(text(log.objects[0].attributes[0].value) == "2024-03-01T10:00:00Z");
  assert(text(log.objects[0].attributes[1].value) == "2024-03-01T10:00:00+02:00");
  assert(text(log.objects[1].attributes[0].value) == "notadate");  // kept, as the reference keeps it
  assert(log.object_changes.size() == 1 &&
         text(log.object_changes[0].value) == "2024-03-05T00:00:00+00:00");

  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  standard.allow_lossy = true;
  const auto xml2 = minera::serialize_ocel_xml(log, standard);
  assert(xml2.find(">2024-02-01T08:30:00+00:00</attribute>") != std::string::npos);
  assert(xml2.find(">2024-02-02T08:30:00.123456+00:00</attribute>") != std::string::npos);
  assert(xml2.find(">2024-03-05T00:00:00+00:00</attribute>") != std::string::npos);
  minera::OcelXmlExportOptions classic;
  classic.allow_lossy = true;
  const auto xml1 = minera::serialize_ocel_xml(log, classic);
  assert(xml1.find("<date key=\"d\" value=\"2024-02-01T08:30:00Z\"/>") != std::string::npos);
  // "od" mixes a date with "notadate": the reference writes the mixed
  // object column's date as a string (observed black-box on this fixture).
  assert(xml1.find("<string key=\"od\" value=\"2024-03-01T10:00:00+00:00\"/>") != std::string::npos);
  assert(xml1.find("<string key=\"s\" value=\"2024-02-01\"/>") != std::string::npos);
  minera::OcelCsvExportOptions csv_options;
  csv_options.allow_lossy = true;
  const auto csv = minera::serialize_ocel_csv(log, csv_options);
  assert(csv.find(",2024-02-01 08:30:00+00:00,2024-02-01,") != std::string::npos);
  assert(csv.find(",2024-02-02 08:30:00.123456+00:00,,") != std::string::npos);
  assert(minera::serialize_ocel_objects_csv(log, csv_options)
             .find("o1,order,2024-03-01 10:00:00+00:00,") != std::string::npos);

  // Both XML readers convert too, and read back what the writers wrote
  // (without the unreadable value, which both reference XML readers refuse).
  minera::OcelXmlImportOptions xml_options;
  auto readable = log;
  readable.objects[1].attributes.clear();
  const auto readable_xml2 = minera::serialize_ocel_xml(readable, standard);
  const auto readable_xml1 = minera::serialize_ocel_xml(readable, classic);
  for (const auto* written : {&readable_xml2, &readable_xml1}) {
    const auto back = minera::parse_ocel_xml(*written, xml_options);
    const auto& value = back.events[0].attributes[0].value;
    assert(text(value) == "2024-02-01T08:30:00+00:00" || text(value) == "2024-02-01T08:30:00Z");
  }
  const auto classic_xml = std::string(
      "<log><events><event><string key=\"id\" value=\"e1\"/><date key=\"timestamp\" "
      "value=\"2024-01-01T00:00:00Z\"/><string key=\"activity\" value=\"A\"/><list key=\"omap\">"
      "<string key=\"object-id\" value=\"o1\"/></list><list key=\"vmap\">"
      "<date key=\"d\" value=\"DATE\"/></list></event></events>"
      "<objects><object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"order\"/>"
      "<list key=\"ovmap\"/></object></objects></log>");
  const auto with = [&](std::string value) {
    auto document = classic_xml;
    document.replace(document.find("DATE"), 4, value);
    return document;
  };
  const auto offset = minera::parse_ocel_xml(with("2024-02-01T08:30:00+02:00"), xml_options);
  assert(text(offset.events[0].attributes[0].value) == "2024-02-01T08:30:00Z");
  assert(offset.event_type_attributes.at("A").at("d") == "date");
  bool refused = false;
  try {
    (void)minera::parse_ocel_xml(with("notadate"), xml_options);
  } catch (const std::invalid_argument& error) {
    refused = std::string(error.what()) == "Invalid isoformat string: 'notadate'";
  }
  assert(refused);
  auto bad_standard = readable_xml2;
  bad_standard.replace(bad_standard.find(">2024-02-01T08:30:00+00:00<"), 27, ">notadate<");
  refused = false;
  try {
    (void)minera::parse_ocel_xml(bad_standard, xml_options);
  } catch (const std::invalid_argument& error) {
    refused = std::string(error.what()) == "String does not contain a date: notadate";
  }
  assert(refused);

  // read_ocel_json and read_ocel_csv keep date texts.
  const auto classic_json = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:timestamp":"2024-01-01T00:00:00Z","ocel:activity":"A",)"
      R"("ocel:vmap":{"d":"2024-02-01T08:30:00+02:00"},"ocel:omap":["o1"]}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"order","ocel:ovmap":{}}},"ocel:eventTypes":{"A":{"d":"date"}}})");
  assert(text(classic_json.events[0].attributes[0].value) == "2024-02-01T08:30:00+02:00");
}

// Round O review items (observed black-box on pm4py 2.7.22): read_ocel_xml
// converts each <date> element, so a <string> under the same key stays a
// string and the column mixes both (write_ocel_xml then prints the date as
// the <string> of its isoformat); read_ocel2_json falls back to dateutil
// (a naive value stays naive, its change value is stamped UTC, a text
// dateutil cannot read stays); read_ocel2_xml reads a padded date text
// through dateutil as a naive value; the classic transport keeps a shared
// event-time offset.
void round_o_date_and_offset_shapes() {
  const auto text = [](const minera::OcelValue& value) { return std::get<std::string>(value); };
  minera::OcelXmlImportOptions xml_options;
  const std::string mixed_xml =
      "<log><events>"
      "<event><string key=\"id\" value=\"e1\"/><date key=\"timestamp\" value=\"2024-01-01T00:00:00Z\"/>"
      "<string key=\"activity\" value=\"A\"/><list key=\"omap\"><string key=\"object-id\" value=\"o1\"/></list>"
      "<list key=\"vmap\"><date key=\"d\" value=\"2024-02-01T08:30:00+02:00\"/></list></event>"
      "<event><string key=\"id\" value=\"e2\"/><date key=\"timestamp\" value=\"2024-01-02T00:00:00Z\"/>"
      "<string key=\"activity\" value=\"A\"/><list key=\"omap\"><string key=\"object-id\" value=\"o1\"/></list>"
      "<list key=\"vmap\"><string key=\"d\" value=\"hello\"/></list></event>"
      "</events><objects><object><string key=\"id\" value=\"o1\"/><string key=\"type\" value=\"order\"/>"
      "<list key=\"ovmap\"><string key=\"s\" value=\"2024-03-01\"/></list></object></objects></log>";
  const auto mixed = minera::parse_ocel_xml(mixed_xml, xml_options);
  assert(text(mixed.events[0].attributes[0].value) == "2024-02-01T08:30:00Z");
  assert(text(mixed.events[1].attributes[0].value) == "hello");
  assert(text(mixed.objects[0].attributes[0].value) == "2024-03-01");  // a <string> stays
  minera::OcelXmlExportOptions classic;
  classic.allow_lossy = true;
  const auto written = minera::serialize_ocel_xml(mixed, classic);
  assert(written.find("<string key=\"d\" value=\"2024-02-01T08:30:00+00:00\"/>") != std::string::npos);
  assert(written.find("<string key=\"d\" value=\"hello\"/>") != std::string::npos);

  const auto json = [](const std::string& value) {
    return R"({"objectTypes":[{"name":"order","attributes":[{"name":"d","type":"date"}]}],)"
           R"("eventTypes":[{"name":"A","attributes":[{"name":"d","type":"date"}]}],)"
           R"("objects":[{"id":"o1","type":"order","attributes":[{"name":"d","time":"1970-01-01T00:00:00Z","value":")" +
           value + R"("},{"name":"d","time":"2024-03-01T00:00:00Z","value":")" + value +
           R"("}]}],"events":[{"id":"e1","type":"A","time":"2024-01-01T10:00:00Z","attributes":[{"name":"d","value":")" +
           value + R"("}],"relationships":[{"objectId":"o1","qualifier":""}]}]})";
  };
  const auto naive = minera::parse_ocel_json(json("Feb 1 2024"));
  assert(text(naive.events[0].attributes[0].value) == "2024-02-01T00:00:00");
  assert(text(naive.objects[0].attributes[0].value) == "2024-02-01T00:00:00");
  assert(text(naive.object_changes[0].value) == "2024-02-01T00:00:00+00:00");
  const auto aware = minera::parse_ocel_json(json("2024/02/01 08:30 +0200"));
  assert(text(aware.events[0].attributes[0].value) == "2024-02-01T06:30:00Z");
  assert(text(aware.object_changes[0].value) == "2024-02-01T06:30:00+00:00");
  const auto padded = minera::parse_ocel_json(json(" 2024-02-01"));
  assert(text(padded.events[0].attributes[0].value) == "2024-02-01T00:00:00");
  assert(text(minera::parse_ocel_json(json("Feb 30 2024")).events[0].attributes[0].value) ==
         "Feb 30 2024");
  // Shapes from the 920-string fuzz (reference readings): basic dates and
  // clocks, a lowercase z, a space-separated clock respelled, and an
  // all-numeric date followed by -HHMM read as a naive clock.
  const auto first = [&](const std::string& value) {
    return text(minera::parse_ocel_json(json(value)).events[0].attributes[0].value);
  };
  // Unpadded, fromisoformat reads the basic shape: the wall clock as UTC.
  assert(first("20231104") == "2023-11-04T00:00:00Z");
  assert(first("20200226T0523-0530") == "2020-02-26T05:23:00Z");
  // Padded, only dateutil reads it: naive stays naive, an offset applies.
  assert(first(" 20231104") == "2023-11-04T00:00:00");
  assert(first(" 20200226T0523-0530") == "2020-02-26T10:53:00Z");
  assert(first("20230221 200601,25 GMT") == "2023-02-21T20:06:01.250000Z");
  assert(first("1 feb 2020T21:13z") == "2020-02-01T21:13:00Z");
  assert(first("1999-09-08 13:37Z") == "1999-09-08T13:37:00Z");
  assert(first("25.6.2023-0530") == "2023-06-25T05:30:00");
  assert(first("20 MAR 2024-0530") == "20 MAR 2024-0530");
  assert(first("2023/6/24 Z") == "2023/6/24 Z");  // a zone without a clock stays text

  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  standard.allow_lossy = true;
  auto padded_xml = minera::serialize_ocel_xml(minera::parse_ocel_json(json("2024-02-01T00:00:00Z")), standard);
  for (std::size_t at = 0; (at = padded_xml.find(">2024-02-01T00:00:00+00:00<", at)) != std::string::npos; ++at)
    padded_xml.replace(at, 27, "> 2024-02-01<");
  const auto padded_back = minera::parse_ocel_xml(padded_xml, xml_options);
  assert(text(padded_back.events[0].attributes[0].value) == "2024-02-01T00:00:00");
  assert(minera::serialize_ocel_xml(padded_back, standard).find(">2024-02-01T00:00:00</attribute>") !=
         std::string::npos);

  const auto offset = minera::parse_ocel_csv(
      "ocel:eid,ocel:timestamp,ocel:activity,ocel:type:order\n"
      "e1,2024-01-01 10:00:00+02:00,A,['o1']\n"
      "e2,2024-01-01 11:30:00+02:00,B,['o1']\n");
  const auto transport = minera::serialize_ocel_json(offset, minera::OcelJsonFormat::kClassic);
  assert(transport.find("\"minera:eventTimeOffset\":true") != std::string::npos);
  const auto passed = minera::parse_ocel_json(transport);
  assert(passed.events[0].timestamp == "2024-01-01 10:00:00+02:00");
  assert(minera::serialize_ocel_xml(passed, standard).find("time=\"2024-01-01T10:00:00+02:00\"") !=
         std::string::npos);
  // Without the marker the classic reader stores the wall clock stamped UTC.
  auto unmarked = transport;
  unmarked.erase(unmarked.find(",\"minera:eventTimeOffset\":true"), 30);
  assert(minera::parse_ocel_json(unmarked).events[0].timestamp == "2024-01-01T10:00:00+00:00");
  // A download (CLI JSON output, export) is written without the marker.
  const auto file = minera::serialize_ocel_json(offset, minera::OcelJsonFormat::kClassic, false);
  assert(file == unmarked);

  // The reference CSV reader keeps the file's column order, and its CSV and
  // OCEL 1.0 XML writers follow it (observed black-box).
  const auto activity_first = minera::parse_ocel_csv(
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order\n"
      "e1,Create,2024-01-01 10:00:00+00:00,['o1']\n");
  assert(activity_first.event_activity_first);
  assert(minera::serialize_ocel_csv(activity_first).rfind(
             "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order\n"
             "e1,Create,2024-01-01 10:00:00+00:00,['o1']\n", 0) == 0);
  const auto xml1 = minera::serialize_ocel_xml(activity_first, classic);
  assert(xml1.find("<string key=\"activity\"") < xml1.find("<date key=\"timestamp\""));
  assert(!offset.event_activity_first);
  assert(minera::serialize_ocel_csv(offset).rfind("ocel:eid,ocel:timestamp,ocel:activity,", 0) == 0);
}

// Round M review items (observed black-box on pm4py 2.7.22):
// "ocel:"-prefixed attributes are dropped by write_ocel2_json and
// write_ocel2_xml, and write_ocel_xml writes them on the event without the
// prefix (outside vmap) and drops them from objects; write_ocel2_json writes
// only the changes of a field without a static value; read_ocel_json stores
// an object change time with an offset as its UTC instant; a naive basic ISO
// event time reads as "...Z".
void reference_writer_shapes() {
  const auto log = minera::parse_ocel_csv(
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:A,,ocel:foo,ocel:type:,ocel:eid,ocel:oid\n"
      "e1,go,2020-01-01 00:00:00,\"['o1']\",v,w,\"['o3']\",e9,zz\n",
      "ocel:oid,ocel:type,ocel:oid.1,q\no1,A,x1,z1\n");
  const auto standard_json = minera::serialize_ocel_json(log, minera::OcelJsonFormat::kStandard);
  assert(standard_json.find("ocel:foo") == std::string::npos &&
         standard_json.find("ocel:eid.1") == std::string::npos &&
         standard_json.find("ocel:oid.1") == std::string::npos &&
         standard_json.find("\"ocel:oid\"") == std::string::npos);
  assert(standard_json.find(R"({"name":"Unnamed: 4","value":"v"})") != std::string::npos);
  minera::OcelXmlExportOptions standard;
  standard.format = minera::OcelXmlFormat::kStandard;
  standard.allow_lossy = true;
  const auto xml2 = minera::serialize_ocel_xml(log, standard);
  assert(xml2.find("ocel:") == std::string::npos);
  assert(xml2.find("name=\"Unnamed: 4\"") != std::string::npos);
  minera::OcelXmlExportOptions classic;
  classic.allow_lossy = true;
  const auto xml1 = minera::serialize_ocel_xml(log, classic);
  assert(xml1.find("      <string key=\"foo\" value=\"w\"/>\n      <string key=\"eid.1\" value=\"e9\"/>\n"
                   "      <string key=\"oid\" value=\"zz\"/>\n      <list key=\"omap\">") != std::string::npos);
  assert(xml1.find("ocel:") == std::string::npos && xml1.find("x1") == std::string::npos);
  assert(xml1.find("<string key=\"attribute-name\" value=\"Unnamed: 4\"/>") != std::string::npos);
  // Declared: classic JSON, the native transport, keeps them in vmap.
  assert(minera::serialize_ocel_json(log).find("\"ocel:foo\":\"w\"") != std::string::npos);

  const auto changes_only = minera::parse_ocel_json(
      R"({"objectTypes":[{"name":"A","attributes":[{"name":"p","type":"string"}]},{"name":"B","attributes":[]}],)"
      R"("eventTypes":[{"name":"go","attributes":[]}],)"
      R"("objects":[{"id":"o1","type":"A","attributes":[{"name":"p","time":"1970-01-01T00:00:00Z","value":"x"},)"
      R"({"name":"p","time":"2020-01-03T00:00:00Z","value":"y"}]},{"id":"o1","type":"B","attributes":[]}],)"
      R"("events":[{"id":"e1","type":"go","time":"2020-01-01T10:00:00Z","attributes":[],"relationships":[{"objectId":"o1","qualifier":""}]}]})");
  const auto written = minera::serialize_ocel_json(changes_only, minera::OcelJsonFormat::kStandard);
  assert(written.find("\"value\":null") == std::string::npos);
  assert(written.find(R"("attributes":[{"name":"p","value":"y","time":"2020-01-03T00:00:00Z"}])") !=
         std::string::npos);

  const auto classic_changes = minera::parse_ocel_json(
      R"({"ocel:events":{"e1":{"ocel:activity":"go","ocel:timestamp":"2020-01-01T00:00:00","ocel:vmap":{},"ocel:omap":["o1"]}},)"
      R"("ocel:objects":{"o1":{"ocel:type":"A","ocel:ovmap":{"a":1}}},)"
      R"("ocel:objectChanges":[{"ocel:oid":"o1","ocel:timestamp":"2020-01-02T00:00:00","ocel:field":"a","a":5},)"
      R"({"ocel:oid":"o1","ocel:timestamp":"2020-01-03T00:00:00+01:00","ocel:field":"a","a":6}]})");
  assert(classic_changes.object_changes.size() == 2);
  assert(classic_changes.object_changes[0].timestamp == "2020-01-02T00:00:00+00:00");
  assert(classic_changes.object_changes[1].timestamp == "2020-01-02T23:00:00+00:00");

  const auto basic = minera::parse_ocel_json(
      R"({"objectTypes":[{"name":"A","attributes":[]}],"eventTypes":[{"name":"go","attributes":[]}],)"
      R"("objects":[{"id":"o1","type":"A"}],"events":[{"id":"e1","type":"go","time":"20200101T100000",)"
      R"("relationships":[{"objectId":"o1","qualifier":""}]}]})");
  assert(basic.events[0].timestamp == "2020-01-01T10:00:00Z");
}

}  // namespace

int main() {
  round_o_date_and_offset_shapes();
  const std::string events =
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:item,ocel:type:order,amount,note\n"
      "e2,Ship,2024-01-02T00:00:00Z,\"['i2','i1']\",['o1'],2.5,\"quoted, value\"\n"
      "e1,Create,2024-01-01T00:00:00Z,[],['o1'],2,\n";
  const std::string objects =
      "ocel:oid,ocel:type,status\n"
      "o1,order,open\n"
      "i1,item,ready\n"
      "i2,item,\"in use\"\n";

  const auto log = minera::parse_ocel_csv(events, objects);
  assert(log.events.size() == 2);
  assert(log.events[0].id == "e1" && log.events[1].id == "e2");
  // Attribute cells stay text, as the reference reader keeps them; an empty
  // cell is no attribute.
  assert(log.events[0].attributes.size() == 1);
  assert(std::get<std::string>(log.events[0].attributes[0].value) == "2");
  assert(std::get<std::string>(log.events[1].attributes[0].value) == "2.5");
  assert(log.events[1].attributes[1].key == "note");
  assert(std::get<std::string>(log.events[1].attributes[1].value) ==
         "quoted, value");
  assert(log.objects.size() == 3);
  assert(log.relations.size() == 4);
  // Objects keep the object table's order.
  assert(log.objects[0].id == "o1" && log.objects[0].type == "order");
  assert(log.objects[1].id == "i1" &&
         std::get<std::string>(log.objects[1].attributes[0].value) == "ready");

  minera::OcelCsvExportOptions event_only_options;
  event_only_options.allow_lossy = true;
  const auto exported_events = minera::serialize_ocel_csv(log, event_only_options);
  const auto exported_objects = minera::serialize_ocel_objects_csv(log);
  assert(exported_events == minera::serialize_ocel_csv(log, event_only_options));
  const auto round = minera::parse_ocel_csv(exported_events, exported_objects);
  assert(round.events.size() == log.events.size());
  assert(round.objects.size() == log.objects.size());
  assert(round.relations.size() == log.relations.size());
  assert(round.events[1].id == "e2");
  assert(std::get<std::string>(round.events[1].attributes[0].value) == "2.5");
  assert(std::get<std::string>(round.objects[0].attributes[0].value) ==
         "open");
  reference_capture_tests();
  follows_reference_csv_row_shapes();
  keeps_repeated_relations();
  writes_timestamps_as_pandas();
  reads_object_cells_as_pandas();
  names_columns_as_pandas();
  refuses_an_id_under_two_types();
  writes_naive_json_and_xml_timestamps_as_utc();
  writes_offset_csv_wall_clock_in_xml();
  date_attributes_read_as_reference();
  reference_writer_shapes();

  minera::OcelCsvImportOptions custom_import;
  custom_import.event_id_column = "event_id";
  custom_import.activity_column = "activity_name";
  custom_import.timestamp_column = "occurred_at";
  custom_import.object_type_prefix = "ref.";
  custom_import.object_id_column = "object_id";
  custom_import.object_type_column = "object_kind";
  custom_import.delimiter = ';';
  const auto custom = minera::parse_ocel_csv(
      "event_id;activity_name;occurred_at;ref.item;score\n"
      " e1 ; Pick ; 2024-01-01T00:00:00Z ; ['i1'] ; 4 \n",
      "object_id;object_kind;state\n"
      "i1;item;ready\n",
      custom_import);
  assert(custom.events.size() == 1 && custom.events[0].id == "e1");
  assert(custom.events[0].activity == "Pick");
  assert(custom.objects.size() == 1 && custom.objects[0].id == "i1");
  assert(custom.relations.size() == 1);

  minera::OcelCsvExportOptions custom_export;
  custom_export.event_id_column = custom_import.event_id_column;
  custom_export.activity_column = custom_import.activity_column;
  custom_export.timestamp_column = custom_import.timestamp_column;
  custom_export.object_type_prefix = custom_import.object_type_prefix;
  custom_export.object_id_column = custom_import.object_id_column;
  custom_export.object_type_column = custom_import.object_type_column;
  custom_export.delimiter = custom_import.delimiter;
  custom_export.allow_lossy = true;
  const auto custom_events = minera::serialize_ocel_csv(custom, custom_export);
  const auto custom_objects = minera::serialize_ocel_objects_csv(custom, custom_export);
  // The input lists the activity before the time, so the output does too.
  assert(custom_events.rfind("event_id;activity_name;occurred_at;score;ref.item\n", 0) == 0);
  assert(custom_objects.rfind("object_id;object_kind;state", 0) == 0);
  const auto custom_roundtrip =
      minera::parse_ocel_csv(custom_events, custom_objects, custom_import);
  assert(custom_roundtrip.events[0].id == "e1");
  assert(custom_roundtrip.objects[0].type == "item");

  throws([&] {
    auto invalid = custom_import;
    invalid.delimiter = '"';
    (void)minera::parse_ocel_csv("event_id;activity_name;occurred_at\n", invalid);
  });
  throws([&] {
    auto invalid = custom_import;
    invalid.delimiter = '\n';
    (void)minera::parse_ocel_csv("event_id;activity_name;occurred_at\n", invalid);
  });

  const std::string synthesized =
      "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order\n"
      "e1,A,2024-01-01T00:00:00Z,['o9']\n";
  const auto synthesized_log = minera::parse_ocel_csv(synthesized);
  assert(synthesized_log.objects.size() == 1 &&
         synthesized_log.objects[0].id == "o9" &&
         synthesized_log.objects[0].type == "order");

  // A repeated column is "ocel:eid.1", as read_csv names it (pm4py reads an
  // empty log from this header).
  assert(minera::parse_ocel_csv("ocel:eid,ocel:eid,ocel:activity,ocel:timestamp\n").events.empty());
  throws([&] {
    (void)minera::parse_ocel_csv(
        "ocel:eid,ocel:activity,ocel:timestamp,ocel:type:order,ocel:type:item\n"
        "e1,A,2024-01-01T00:00:00Z,['o1'],['o1']\n");
  });
  throws([&] {
    minera::Ocel invalid;
    invalid.objects.push_back({"o1", "order", {}});
    invalid.events.push_back({"e1", "A", "2024-01-01T00:00:00Z", {}});
    invalid.relations.push_back({"e1", "o1", "qualified"});
    (void)minera::serialize_ocel_csv(invalid);
  });
  throws([&] {
    minera::Ocel invalid;
    invalid.objects.push_back({"o1", "order", {{"status", std::string("open")}}});
    invalid.events.push_back({"e1", "A", "2024-01-01T00:00:00Z", {}});
    invalid.relations.push_back({"e1", "o1", ""});
    (void)minera::serialize_ocel_csv(invalid);
  });
  throws([&] {
    auto invalid = log;
    invalid.e2e.push_back({"e1", "e2", "precedes"});
    (void)minera::serialize_ocel_csv(invalid);
  });

  // A two-file export must not accept suppressed object output when object
  // attributes would otherwise disappear without an error.
  const auto event_path = std::filesystem::temp_directory_path() /
                          "minera_ocel_csv_emit_objects_false_events.csv";
  const auto object_path = std::filesystem::temp_directory_path() /
                           "minera_ocel_csv_emit_objects_false_objects.csv";
  std::filesystem::remove(event_path);
  std::filesystem::remove(object_path);
  auto suppressed_objects = minera::OcelCsvExportOptions{};
  suppressed_objects.emit_objects = false;
  throws([&] {
    minera::export_ocel_csv(log, event_path, object_path, suppressed_objects);
  });
  std::filesystem::remove(event_path);
  std::filesystem::remove(object_path);

  return 0;
}
