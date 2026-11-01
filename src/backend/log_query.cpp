#include "minera/log_query.hpp"
#include "minera/log_conversion.hpp"
#include "minera/event_table_json.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#if defined(MINERA_HAS_SQLITE)
#if defined(_WIN32)
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#endif
#include "full_double.hpp"

namespace minera {
namespace {
std::string quote(std::string_view text) {
  std::string out="\"";constexpr char hex[]="0123456789abcdef";
  for(const unsigned char c:text){switch(c){case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;default:if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=static_cast<char>(c);}}
  return out+'"';
}
std::string identifier(std::string_view name) {
  if(name.find('\0')!=std::string_view::npos)throw std::invalid_argument("SQL identifiers must not contain NUL");
  std::string out="\"";for(char c:name){out+=c;if(c=='"')out+='"';}return out+'"';
}
std::string lower(std::string text){for(char& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
QueryValue scalar(const std::optional<EventAttribute>& cell) {
  if(!cell||cell->is_null||cell->kind==AttributeKind::kNA)return std::monostate{};
  if(cell->container!=AttributeContainerKind::kScalar){
    EventTable one;one.columns={cell->key};one.rows={{cell}};
    return serialize_event_table_json(one);
  }
  if(auto p=std::get_if<std::int64_t>(&cell->typed_value))return *p;
  if(auto p=std::get_if<std::uint64_t>(&cell->typed_value))return *p<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())?QueryValue(static_cast<std::int64_t>(*p)):QueryValue(std::to_string(*p));
  if(auto p=std::get_if<double>(&cell->typed_value))return *p;
  if(auto p=std::get_if<bool>(&cell->typed_value))return static_cast<std::int64_t>(*p);
  if(auto p=std::get_if<NativeDate>(&cell->typed_value))return p->value;
  if(cell->kind==AttributeKind::kInt){std::int64_t value=0;auto parsed=std::from_chars(cell->value.data(),cell->value.data()+cell->value.size(),value);if(parsed.ec==std::errc{}&&parsed.ptr==cell->value.data()+cell->value.size())return value;}
  if(cell->kind==AttributeKind::kBoolean){const auto text=lower(cell->value);if(text=="true"||text=="1")return std::int64_t{1};if(text=="false"||text=="0")return std::int64_t{0};throw std::invalid_argument("invalid boolean query cell");}
  if(cell->kind==AttributeKind::kFloat){std::size_t used=0;const auto value=::minera::detail::stod(cell->value,&used);if(used==cell->value.size())return value;}
  return cell->value;
}
void write_value(std::ostream& output,const QueryValue& value){
  if(std::holds_alternative<std::monostate>(value)){output<<"null";return;}
  if(auto p=std::get_if<std::int64_t>(&value)){if(*p>=-9007199254740991LL&&*p<=9007199254740991LL)output<<*p;else output<<"{\"type\":\"integer\",\"value\":"<<quote(std::to_string(*p))<<'}';return;}
  if(auto p=std::get_if<double>(&value)){if(std::isfinite(*p))output<<std::setprecision(17)<<*p;else output<<"{\"type\":\"float\",\"value\":"<<quote(std::isnan(*p)?"NaN":*p<0?"-Infinity":"Infinity")<<'}';return;}
  if(auto p=std::get_if<std::string>(&value)){output<<quote(*p);return;}
  output<<"{\"type\":\"blob\",\"value\":"<<quote(std::get<QueryBlob>(value).hex)<<'}';
}
#if defined(MINERA_HAS_SQLITE)
struct Database {sqlite3* value=nullptr;Database(){if(sqlite3_open(":memory:",&value)!=SQLITE_OK){sqlite3_close(value);value=nullptr;throw std::runtime_error("could not create SQLite query database");}}~Database(){sqlite3_close(value);}};
struct Statement {sqlite3_stmt* value=nullptr;~Statement(){sqlite3_finalize(value);}};
void require_ok(sqlite3* database,int status){if(status!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(database));}
void bind_query_value(sqlite3_stmt* statement,int index,const QueryValue& value){
  int status=SQLITE_OK;
  if(std::holds_alternative<std::monostate>(value))status=sqlite3_bind_null(statement,index);
  else if(auto p=std::get_if<std::int64_t>(&value))status=sqlite3_bind_int64(statement,index,*p);
  else if(auto p=std::get_if<double>(&value))status=sqlite3_bind_double(statement,index,*p);
  else if(auto p=std::get_if<std::string>(&value))status=sqlite3_bind_text(statement,index,p->data(),static_cast<int>(p->size()),SQLITE_TRANSIENT);
  else throw std::invalid_argument("blob query parameters are not supported");
  if(status!=SQLITE_OK)throw std::runtime_error("could not bind query value");
}
int authorize(void*,int action,const char*,const char* function,const char*,const char*){
  if(action==SQLITE_SELECT||action==SQLITE_READ||action==SQLITE_RECURSIVE)return SQLITE_OK;
  if(action==SQLITE_FUNCTION)return function&&lower(function)=="load_extension"?SQLITE_DENY:SQLITE_OK;
  return SQLITE_DENY;
}
struct Budget {std::size_t used=0;std::size_t maximum=0;};
int progress(void* value){auto* budget=static_cast<Budget*>(value);budget->used+=1000;return budget->used>budget->maximum;}
QueryValue value(sqlite3_stmt* statement,int column){
  switch(sqlite3_column_type(statement,column)){
   case SQLITE_NULL:return std::monostate{};
   case SQLITE_INTEGER:return static_cast<std::int64_t>(sqlite3_column_int64(statement,column));
   case SQLITE_FLOAT:return sqlite3_column_double(statement,column);
   case SQLITE_TEXT:{const auto* text=sqlite3_column_text(statement,column);return std::string(reinterpret_cast<const char*>(text),sqlite3_column_bytes(statement,column));}
   default:{constexpr char hex[]="0123456789abcdef";std::string out;const auto* data=static_cast<const unsigned char*>(sqlite3_column_blob(statement,column));for(int i=0;i<sqlite3_column_bytes(statement,column);++i){out+=hex[data[i]>>4];out+=hex[data[i]&15];}return QueryBlob{out};}
  }
}
#endif
} // namespace

bool sqlite_log_query_available(){
#if defined(MINERA_HAS_SQLITE)
 return true;
#else
 return false;
#endif
}

LogQueryTable log_query_table(const EventLog& log){
  LogConversionOptions options;options.variant=LogConversionVariant::kToDataFrame;
  LogQueryTable result;result.table=convert_event_log(log,options).table;
  result.event_index_column="__minera_event_index";
  const auto taken=[&]{return std::any_of(result.table.columns.begin(),result.table.columns.end(),[&](const auto& name){return lower(name)==lower(result.event_index_column);});};
  while(taken())result.event_index_column+='_';
  result.table.columns.push_back(result.event_index_column);
  if(!result.table.dtypes.empty())result.table.dtypes.push_back("int64");
  if(!result.table.categorical_dtypes.empty())result.table.categorical_dtypes.push_back(std::nullopt);
  for(std::size_t i=0;i<result.table.rows.size();++i){EventAttribute attribute;attribute.key=result.event_index_column;attribute.value=std::to_string(i);attribute.kind=AttributeKind::kInt;attribute.typed_value=static_cast<std::int64_t>(i);result.table.rows[i].push_back(attribute);}
  result.table.validate();return result;
}

LogQueryResult query_log_sqlite(const LogQueryTable& input,std::string_view sql,const LogQueryOptions& options){
#if !defined(MINERA_HAS_SQLITE)
 (void)input;(void)sql;(void)options;throw std::runtime_error("SQLite query support is unavailable in this build");
#else
 input.table.validate();if(sql.empty()||sql.find('\0')!=std::string_view::npos)throw std::invalid_argument("query SQL must be nonempty and contain no NUL");
 if(!options.max_rows||options.max_rows>1000000||!options.max_vm_steps||!options.max_output_bytes)throw std::invalid_argument("invalid SQL query limits");
 const auto output_limit=[&]{return std::length_error("SQL query result exceeds the "+std::to_string(options.max_output_bytes)+"-byte output limit; select fewer rows or columns");};
 Database database;std::string create="CREATE TABLE "+identifier(options.table_name)+" (";
 for(std::size_t c=0;c<input.table.columns.size();++c){if(c)create+=',';create+=identifier(input.table.columns[c]);}create+=')';
 require_ok(database.value,sqlite3_exec(database.value,create.c_str(),nullptr,nullptr,nullptr));
 std::string insert="INSERT INTO "+identifier(options.table_name)+" VALUES (";
 for(std::size_t c=0;c<input.table.columns.size();++c){if(c)insert+=',';insert+='?';}insert+=')';
 Statement write;require_ok(database.value,sqlite3_prepare_v2(database.value,insert.c_str(),-1,&write.value,nullptr));
 require_ok(database.value,sqlite3_exec(database.value,"BEGIN",nullptr,nullptr,nullptr));
 for(const auto& row:input.table.rows){for(std::size_t c=0;c<row.size();++c)bind_query_value(write.value,static_cast<int>(c+1),scalar(row[c]));if(sqlite3_step(write.value)!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(database.value));sqlite3_reset(write.value);sqlite3_clear_bindings(write.value);}
 require_ok(database.value,sqlite3_exec(database.value,"COMMIT",nullptr,nullptr,nullptr));
 // Set after loading the source table: no single string or blob produced by
 // the query (zeroblob, randomblob, replace, group_concat...) may exceed the
 // output limit, so SQLite fails before allocating it.
 sqlite3_limit(database.value,SQLITE_LIMIT_LENGTH,static_cast<int>(std::min<std::size_t>(options.max_output_bytes,0x7fffffff)));
 sqlite3_set_authorizer(database.value,authorize,nullptr);Budget budget{0,options.max_vm_steps};sqlite3_progress_handler(database.value,1000,progress,&budget);
 Statement statement;const char* tail=nullptr;const std::string text(sql);require_ok(database.value,sqlite3_prepare_v2(database.value,text.c_str(),-1,&statement.value,&tail));
 if(!statement.value||!sqlite3_stmt_readonly(statement.value)||sqlite3_column_count(statement.value)==0)throw std::invalid_argument("expected one read-only result query");
 Statement extra;require_ok(database.value,sqlite3_prepare_v2(database.value,tail,-1,&extra.value,nullptr));if(extra.value)throw std::invalid_argument("only one SQL query is accepted");
 if(static_cast<std::size_t>(sqlite3_bind_parameter_count(statement.value))!=options.parameters.size())throw std::invalid_argument("query parameter count does not match");
 for(std::size_t i=0;i<options.parameters.size();++i)bind_query_value(statement.value,static_cast<int>(i+1),options.parameters[i]);
 LogQueryResult result;result.sql=text;result.event_index_column=input.event_index_column;int index_column=-1,index_columns=0;
 for(int c=0;c<sqlite3_column_count(statement.value);++c){result.columns.emplace_back(sqlite3_column_name(statement.value,c));if(result.columns.back()==input.event_index_column){index_column=c;++index_columns;}}
 if(index_columns!=1)index_column=-1;
 result.can_filter=index_column>=0;std::set<std::size_t> selected;
 int status=SQLITE_ROW;std::size_t output_bytes=0;
 while((status=sqlite3_step(statement.value))==SQLITE_ROW){
  if(result.rows.size()>=options.max_rows){result.output_truncated=true;break;}
  std::vector<QueryValue> row;for(int c=0;c<sqlite3_column_count(statement.value);++c){row.push_back(value(statement.value,c));if(const auto* text=std::get_if<std::string>(&row.back()))output_bytes+=text->size();else if(const auto* blob=std::get_if<QueryBlob>(&row.back()))output_bytes+=blob->hex.size();else output_bytes+=8;}
  if(output_bytes>options.max_output_bytes)throw output_limit();
  if(index_column>=0){const auto* index=std::get_if<std::int64_t>(&row[index_column]);if(index&&*index>=0&&static_cast<std::uint64_t>(*index)<input.table.rows.size())selected.insert(static_cast<std::size_t>(*index));else result.can_filter=false;}
  result.rows.push_back(std::move(row));
 }
 if(status==SQLITE_TOOBIG)throw output_limit();
 if(status!=SQLITE_DONE&&status!=SQLITE_ROW)throw std::runtime_error(sqlite3_errmsg(database.value));
 if(result.can_filter&&!result.output_truncated)result.selected_event_indices.assign(selected.begin(),selected.end());
 return result;
#endif
}

EventLog filter_query_events(const EventLog& log,const std::vector<std::size_t>& indices,bool whole_cases){
 std::set<std::size_t> selected(indices.begin(),indices.end());for(auto index:selected)if(index>=log.event_count())throw std::invalid_argument("selected event index is outside the source log");
 EventLog result;copy_event_log_context(log,result);std::size_t global=0;
 for(const auto& trace:log.traces){Trace copy=trace;copy.events.clear();bool matched=false;for(const auto& event:trace.events){if(selected.contains(global++)){matched=true;copy.events.push_back(event);}}if(matched){if(whole_cases)copy.events=trace.events;result.traces.push_back(std::move(copy));}}
 return result;
}

std::string log_query_json(const LogQueryResult& result){
 std::ostringstream out;out<<"{\"family\":\"LOG_QUERY\",\"engine\":"<<quote(result.engine)<<",\"sql\":"<<quote(result.sql)<<",\"event_index_column\":"<<quote(result.event_index_column)<<",\"output_truncated\":"<<(result.output_truncated?"true":"false")<<",\"can_filter\":"<<(result.can_filter?"true":"false")<<",\"columns\":[";
 for(std::size_t i=0;i<result.columns.size();++i){if(i)out<<',';out<<quote(result.columns[i]);}out<<"],\"rows\":[";
 for(std::size_t r=0;r<result.rows.size();++r){if(r)out<<',';out<<'[';for(std::size_t c=0;c<result.rows[r].size();++c){if(c)out<<',';write_value(out,result.rows[r][c]);}out<<']';}out<<"],\"selected_event_indices\":[";
 for(std::size_t i=0;i<result.selected_event_indices.size();++i){if(i)out<<',';out<<result.selected_event_indices[i];}out<<"]}";return out.str();
}
std::string log_query_table_json(const LogQueryTable& input){return "{\"family\":\"LOG_QUERY_TABLE\",\"event_index_column\":"+quote(input.event_index_column)+",\"table\":"+serialize_event_table_json(input.table)+'}';}
} // namespace minera
