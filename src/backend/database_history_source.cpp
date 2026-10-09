#include "minera/database_history_source.hpp"
#include "minera/timestamp.hpp"
#include "database_history_queries.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#ifdef MINERA_HAS_SQLITE
#ifdef _WIN32
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#endif
namespace minera {
namespace {
constexpr std::size_t maximum_bytes=512ULL*1024*1024;
std::string quote(const std::string& text){std::string out="\"";const char* h="0123456789abcdef";for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c<32){out+="\\u00";out+=h[c>>4];out+=h[c&15];}else out+=static_cast<char>(c);}return out+'"';}
void replace_all(std::string& text,const std::string& from,const std::string& to){std::size_t i=0;while((i=text.find(from,i))!=std::string::npos){text.replace(i,from.size(),to);i+=to.size();}}
void prefix_valid(const std::string& p){if(p.empty())return;if(p.size()>256||p.back()!='.')throw std::invalid_argument("Database table prefix requires a schema name followed by a dot");bool segment=true;for(unsigned char c:p){if(c=='.'){if(segment)throw std::invalid_argument("Database prefix contains an empty identifier");segment=true;}else{if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')||(segment&&c>='0'&&c<='9'))throw std::invalid_argument("Database prefix requires simple SQL identifiers");segment=false;}}}
std::string normalized_time(std::string value){
 if((value.size()==8||value.size()==15)&&std::all_of(value.begin(),value.begin()+8,[](unsigned char c){return c>='0'&&c<='9';})){const auto date=value.substr(0,4)+'-'+value.substr(4,2)+'-'+value.substr(6,2);value=value.size()==8?date:date+'T'+value.substr(9,2)+':'+value.substr(11,2)+':'+value.substr(13,2);}
 const auto parsed=parse_timestamp_exact(value);if(!parsed)throw std::invalid_argument("Database history: invalid source timestamp");
 using namespace std::chrono;const sys_seconds instant{seconds{parsed->seconds}};const auto day=floor<days>(instant);const year_month_day ymd{day};const hh_mm_ss clock{instant-day};char text[64];std::snprintf(text,sizeof(text),"%04d-%02u-%02uT%02lld:%02lld:%02lld.%09uZ",static_cast<int>(ymd.year()),static_cast<unsigned>(ymd.month()),static_cast<unsigned>(ymd.day()),static_cast<long long>(clock.hours().count()),static_cast<long long>(clock.minutes().count()),static_cast<long long>(clock.seconds().count()),parsed->nanoseconds);return text;
}
std::string table_json(const DatabaseHistoryTable& table,const std::string& family,bool indexes=false){
 std::ostringstream out;out<<"{\"family\":"<<quote(family)<<",\"columns\":[";for(std::size_t i=0;i<table.columns.size();++i){if(i)out<<',';out<<quote(table.columns[i]);}out<<"],\"column_types\":[";for(std::size_t i=0;i<table.column_types.size();++i){if(i)out<<',';out<<quote(table.column_types[i]);}out<<"],\"rows\":[";for(std::size_t r=0;r<table.rows.size();++r){if(r)out<<',';out<<'[';for(std::size_t c=0;c<table.columns.size();++c){if(c)out<<',';const auto& v=table.rows[r][c];if(!v)out<<"null";else if(indexes&&(table.columns[c]=="@@index"||table.columns[c]=="@@case_index"))out<<*v;else out<<quote(*v);}out<<']';}out<<"],\"row_count\":"<<table.rows.size()<<",\"limited\":"<<(table.limited?"true":"false")<<'}';return out.str();
}
#ifdef MINERA_HAS_SQLITE
DatabaseHistoryTable sqlite_read(const DatabaseHistoryOptions& o,const std::string& query){
 sqlite3* raw=nullptr;const auto u8=o.path.u8string();const auto code=sqlite3_open_v2(reinterpret_cast<const char*>(u8.c_str()),&raw,SQLITE_OPEN_READONLY,nullptr);std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,&sqlite3_close);if(code!=SQLITE_OK)throw std::invalid_argument("Database history: could not open the explicit SQLite file read-only");sqlite3_busy_timeout(raw,static_cast<int>(o.timeout_seconds*1000));
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(o.timeout_seconds);sqlite3_progress_handler(raw,1000,[](void* context){return std::chrono::steady_clock::now()>*static_cast<const std::chrono::steady_clock::time_point*>(context)?1:0;},const_cast<std::chrono::steady_clock::time_point*>(&deadline));
 sqlite3_stmt* statement=nullptr;if(sqlite3_prepare_v2(raw,query.c_str(),static_cast<int>(query.size()),&statement,nullptr)!=SQLITE_OK)throw std::invalid_argument("Database history: SQLite schema/query is incompatible with the selected connector");std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> owner(statement,&sqlite3_finalize);if(!sqlite3_stmt_readonly(statement))throw std::invalid_argument("Database history: query is not read-only");DatabaseHistoryTable table;
 for(int i=0;i<sqlite3_column_count(statement);++i){table.columns.emplace_back(sqlite3_column_name(statement,i));const auto* type=sqlite3_column_decltype(statement,i);table.column_types.emplace_back(type?type:"expression");}
 std::size_t bytes=0;for(;;){const auto status=sqlite3_step(statement);if(status==SQLITE_DONE)break;if(status!=SQLITE_ROW)throw std::runtime_error("Database history: SQLite query failed or timed out");if(table.rows.size()==o.max_rows){table.limited=true;break;}std::vector<std::optional<std::string>> row;for(int i=0;i<sqlite3_column_count(statement);++i){if(sqlite3_column_type(statement,i)==SQLITE_NULL)row.push_back(std::nullopt);else{if(sqlite3_column_type(statement,i)==SQLITE_BLOB)throw std::invalid_argument("Database history: connector fields do not accept binary SQL values");const auto* value=sqlite3_column_text(statement,i);const auto size=sqlite3_column_bytes(statement,i);row.emplace_back(std::string(reinterpret_cast<const char*>(value),size));bytes+=static_cast<std::size_t>(size);if(bytes>maximum_bytes)throw std::length_error("Database history: captured values exceed 512 MiB");}}table.rows.push_back(std::move(row));}return table;
}
#endif
#ifdef _WIN32
std::wstring wide(const std::string& v){if(v.empty())return {};const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),nullptr,0);if(!n)throw std::invalid_argument("Database history: invalid UTF-8 input");std::wstring out(n,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),out.data(),n);return out;}
std::string narrow(const std::wstring& v){if(v.empty())return {};const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("Database history: invalid Unicode result");std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,v.data(),static_cast<int>(v.size()),out.data(),n,nullptr,nullptr);return out;}
struct OdbcHandle{SQLSMALLINT type;SQLHANDLE value=SQL_NULL_HANDLE;OdbcHandle(SQLSMALLINT t,SQLHANDLE parent):type(t){if(!SQL_SUCCEEDED(SQLAllocHandle(type,parent,&value)))throw std::runtime_error("Database history: ODBC handle allocation failed");}~OdbcHandle(){if(type==SQL_HANDLE_DBC)SQLDisconnect(value);SQLFreeHandle(type,value);}OdbcHandle(const OdbcHandle&)=delete;};
void require_odbc(SQLRETURN status,SQLSMALLINT type,SQLHANDLE handle,const char* operation){if(SQL_SUCCEEDED(status))return;SQLWCHAR state[6]{};SQLINTEGER native=0;SQLWCHAR discarded[1]{};SQLSMALLINT size=0;SQLGetDiagRecW(type,handle,1,state,&native,discarded,1,&size);std::wstring code(reinterpret_cast<wchar_t*>(state),5);throw std::invalid_argument(std::string("Database history: ODBC ")+operation+" failed (SQLSTATE "+narrow(code)+", code "+std::to_string(native)+"). Check the configured driver, schema and account access.");}
bool optional_timeout(SQLRETURN status,SQLHANDLE statement){if(SQL_SUCCEEDED(status))return true;SQLWCHAR state[6]{};SQLINTEGER native=0;SQLWCHAR discarded[1]{};SQLSMALLINT size=0;SQLGetDiagRecW(SQL_HANDLE_STMT,statement,1,state,&native,discarded,1,&size);const std::wstring code(reinterpret_cast<wchar_t*>(state),5);if(code==L"HYC00"||code==L"HY092")return false;require_odbc(status,SQL_HANDLE_STMT,statement,"query timeout");return false;}
DatabaseHistoryTable odbc_read(const DatabaseHistoryOptions& o,const std::string& query){
 OdbcHandle env(SQL_HANDLE_ENV,SQL_NULL_HANDLE);require_odbc(SQLSetEnvAttr(env.value,SQL_ATTR_ODBC_VERSION,reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3),0),SQL_HANDLE_ENV,env.value,"environment setup");OdbcHandle connection(SQL_HANDLE_DBC,env.value);
 require_odbc(SQLSetConnectAttr(connection.value,SQL_ATTR_LOGIN_TIMEOUT,reinterpret_cast<SQLPOINTER>(static_cast<std::uintptr_t>(o.timeout_seconds)),0),SQL_HANDLE_DBC,connection.value,"login timeout");require_odbc(SQLSetConnectAttr(connection.value,SQL_ATTR_ACCESS_MODE,reinterpret_cast<SQLPOINTER>(SQL_MODE_READ_ONLY),0),SQL_HANDLE_DBC,connection.value,"read-only hint");const auto input=wide(o.connection_string);if(input.size()>32767)throw std::invalid_argument("Database history: connection string is too long");require_odbc(SQLDriverConnectW(connection.value,nullptr,reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(input.c_str())),static_cast<SQLSMALLINT>(input.size()),nullptr,0,nullptr,SQL_DRIVER_NOPROMPT),SQL_HANDLE_DBC,connection.value,"connection");
 OdbcHandle statement(SQL_HANDLE_STMT,connection.value);const bool timeout_applied=optional_timeout(SQLSetStmtAttr(statement.value,SQL_ATTR_QUERY_TIMEOUT,reinterpret_cast<SQLPOINTER>(static_cast<std::uintptr_t>(o.timeout_seconds)),0),statement.value);const auto sql=wide(query);require_odbc(SQLExecDirectW(statement.value,reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(sql.c_str())),static_cast<SQLINTEGER>(sql.size())),SQL_HANDLE_STMT,statement.value,"query");SQLSMALLINT columns=0;require_odbc(SQLNumResultCols(statement.value,&columns),SQL_HANDLE_STMT,statement.value,"column count");DatabaseHistoryTable table;table.query_timeout_applied=timeout_applied;
 for(SQLUSMALLINT i=1;i<=columns;++i){SQLWCHAR name[4096]{};SQLSMALLINT length=0,type=0,scale=0,nullable=0;SQLULEN size=0;require_odbc(SQLDescribeColW(statement.value,i,name,4096,&length,&type,&size,&scale,&nullable),SQL_HANDLE_STMT,statement.value,"column schema");if(length>=4096)throw std::length_error("Database history: column name is too long");table.columns.push_back(narrow(std::wstring(reinterpret_cast<wchar_t*>(name),length)));table.column_types.push_back("ODBC:"+std::to_string(type));}
 std::size_t bytes=0;for(;;){const auto status=SQLFetch(statement.value);if(status==SQL_NO_DATA)break;require_odbc(status,SQL_HANDLE_STMT,statement.value,"row fetch");if(table.rows.size()==o.max_rows){table.limited=true;break;}std::vector<std::optional<std::string>> row;
  for(SQLUSMALLINT i=1;i<=columns;++i){std::wstring value;bool null=false;for(;;){SQLWCHAR chunk[2048]{};SQLLEN size=0;const auto code=SQLGetData(statement.value,i,SQL_C_WCHAR,chunk,sizeof(chunk),&size);if(code==SQL_NO_DATA)break;require_odbc(code,SQL_HANDLE_STMT,statement.value,"column read");if(size==SQL_NULL_DATA){null=true;break;}const auto used=wcsnlen_s(reinterpret_cast<wchar_t*>(chunk),2048);value.append(reinterpret_cast<wchar_t*>(chunk),used);if(value.size()*sizeof(wchar_t)>maximum_bytes)throw std::length_error("Database history: column exceeds 512 MiB");if(code==SQL_SUCCESS)break;if(used==0)throw std::runtime_error("Database history: ODBC returned an incomplete value without progress");}if(null)row.push_back(std::nullopt);else{row.push_back(narrow(value));bytes+=row.back()->size();if(bytes>maximum_bytes)throw std::length_error("Database history: captured values exceed 512 MiB");}}table.rows.push_back(std::move(row));}return table;
}
#endif
}  // namespace
std::string database_history_query(const std::string& connector,const std::string& prefix,bool sqlite_dialect){prefix_valid(prefix);std::string query;if(connector=="camunda_workflow")query=database_history_detail::camunda_workflow_query;else if(connector=="sap_accounting")query=database_history_detail::sap_accounting_query;else if(connector=="sap_o2c")query=database_history_detail::sap_o2c_query;else throw std::invalid_argument("Database history: unsupported connector");replace_all(query,"OWNED.",prefix);
 if(sqlite_dialect&&connector=="sap_o2c"){replace_all(query,"DECODE(T1.TYPE, 'C', 'Create Sales Document', 'D', 'Create Delivery')","CASE T1.TYPE WHEN 'C' THEN 'Create Sales Document' WHEN 'D' THEN 'Create Delivery' END");replace_all(query,"UNION\n    (\n", "UNION\n");replace_all(query,"    )\n    UNION", "\n    UNION");const auto last=query.find_last_not_of(" \r\n\t");if(last!=std::string::npos&&query[last]==')')query.erase(last,1);}
 return query;
}
DatabaseHistoryCapture capture_database_history(const DatabaseHistoryOptions& o,DatabaseHistoryReader reader){
 if((o.driver!="sqlite"&&o.driver!="odbc")||!o.max_rows||o.max_rows>10000000||!o.timeout_seconds||o.timeout_seconds>300)throw std::invalid_argument("Database history: invalid driver, row limit or timeout");if(o.driver=="sqlite"&&(o.path.empty()||!o.connection_string.empty()))throw std::invalid_argument("Database history: SQLite requires an explicit file and no connection credential");if(o.driver=="odbc"&&(o.connection_string.empty()||!o.path.empty()))throw std::invalid_argument("Database history: ODBC requires a connection credential and no file");
 DatabaseHistoryCapture c;c.connector=o.connector;c.driver=o.driver;c.prefix=o.prefix;c.dialect=o.dialect.empty()?(o.driver=="sqlite"?"sqlite":"reference"):o.dialect;if(c.dialect!="reference"&&c.dialect!="sqlite"&&c.dialect!="access")throw std::invalid_argument("Database history: unsupported query dialect");if(c.dialect=="access"&&o.connector!="camunda_workflow")throw std::invalid_argument("Database history: Access query dialect currently supports Camunda history");c.reference_query=database_history_query(o.connector,o.prefix);c.query=database_history_query(o.connector,o.prefix,c.dialect=="sqlite");if(c.dialect=="access"){replace_all(c.query,"    JOIN\n","    INNER JOIN\n");for(const auto& column:{"processID","case:concept:name","concept:name","time:timestamp","org:resource"})replace_all(c.query,"AS \""+std::string(column)+"\"","AS ["+std::string(column)+"]");}
 if(reader)c.table=reader(o,c.query);else if(o.driver=="sqlite"){
#ifdef MINERA_HAS_SQLITE
  c.table=sqlite_read(o,c.query);
#else
  throw std::runtime_error("Database history: SQLite support is unavailable in this build");
#endif
 }else{
#ifdef _WIN32
  c.table=odbc_read(o,c.query);
#else
  throw std::runtime_error("Database history: native ODBC acquisition currently requires Windows");
#endif
 }
 if(c.table.columns.empty()||c.table.column_types.size()!=c.table.columns.size()||c.table.rows.size()>o.max_rows)throw std::invalid_argument("Database history: invalid query table schema or bound");std::map<std::string,std::size_t> indexes;for(std::size_t i=0;i<c.table.columns.size();++i)if(!indexes.emplace(c.table.columns[i],i).second)throw std::invalid_argument("Database history: duplicate query column");auto column=[&](const std::string& key){const auto it=indexes.find(key);if(it==indexes.end())throw std::invalid_argument("Database history: missing query column "+key);return it->second;};const auto id=column("case:concept:name"),activity=column("concept:name"),time=column("time:timestamp"),resource=column("org:resource");
 for(const auto& row:c.table.rows)if(row.size()!=c.table.columns.size())throw std::invalid_argument("Database history: inconsistent query row width");
 c.raw_table_json=table_json(c.table,"DATABASE_HISTORY_CAPTURE");DatabaseHistoryTable formatted=c.table;for(auto& row:formatted.rows){if(!row[id]||row[id]->empty()||!row[activity]||row[activity]->empty()||!row[time])throw std::invalid_argument("Database history: invalid required case/activity/time cell");row[time]=normalized_time(*row[time]);}
 std::stable_sort(formatted.rows.begin(),formatted.rows.end(),[&](const auto& a,const auto& b){return *a[id]!=*b[id]?*a[id]<*b[id]:*a[time]<*b[time];});formatted.columns.insert(formatted.columns.end(),{"@@index","@@case_index"});formatted.column_types.insert(formatted.column_types.end(),{"integer","integer"});c.log.attributes={{"source",o.connector=="camunda_workflow"?"workflow_history":"enterprise_history",AttributeKind::kString},{"database_driver",o.driver,AttributeKind::kString}};std::map<std::string,std::size_t> cases;
 for(std::size_t r=0;r<formatted.rows.size();++r){auto& row=formatted.rows[r];const auto [position,inserted]=cases.emplace(*row[id],c.log.traces.size());if(inserted){Trace trace;trace.case_id=*row[id];trace.case_id_state=FirstClassAttributeState::kPresent;trace.case_id_source_key="concept:name";if(indexes.contains("processID")){const auto& p=row[indexes.at("processID")];trace.attributes.push_back({"case:process_id",p.value_or(""),AttributeKind::kString,!p});}c.log.traces.push_back(std::move(trace));}row.push_back(std::to_string(r));row.push_back(std::to_string(position->second));Event event;event.case_id=*row[id];event.activity=*row[activity];event.timestamp=*row[time];event.resource=row[resource].value_or("");event.input_order=r;event.case_id_state=event.activity_state=event.timestamp_state=FirstClassAttributeState::kPresent;event.resource_state=row[resource]?FirstClassAttributeState::kPresent:FirstClassAttributeState::kNull;event.case_id_source_key="case:concept:name";event.activity_source_key="concept:name";event.timestamp_source_key="time:timestamp";event.resource_source_key="org:resource";
  for(std::size_t i=0;i<formatted.columns.size();++i){EventAttribute a{formatted.columns[i],row[i].value_or(""),i==time?AttributeKind::kDate:i>=c.table.columns.size()?AttributeKind::kInt:AttributeKind::kString,!row[i]};if(i==time)a.typed_value=NativeDate{*row[i],NativeDateResolution::kNanosecond,std::string("UTC")};if(i==resource)event.resource_attribute=a;if(i==time)event.timestamp_attribute=a;event.attributes.push_back(std::move(a));}c.log.traces[position->second].events.push_back(std::move(event));}
 c.formatted_table_json=table_json(formatted,"DATABASE_HISTORY_FORMATTED",true);return c;
}
std::string database_history_capture_json(const DatabaseHistoryCapture& c){std::ostringstream out;out<<"{\"driver\":"<<quote(c.driver=="sqlite"?"sqlite_history":"odbc_history")<<",\"connector\":"<<quote(c.connector)<<",\"prefix\":"<<quote(c.prefix)<<",\"dialect\":"<<quote(c.dialect)<<",\"query\":"<<quote(c.query)<<",\"reference_query\":"<<quote(c.reference_query)<<",\"captured_rows\":"<<c.table.rows.size()<<",\"limited\":"<<(c.table.limited?"true":"false")<<",\"query_timeout_applied\":"<<(c.table.query_timeout_applied?"true":"false")<<",\"has_table_capture\":true,\"table_bytes\":"<<c.raw_table_json.size()<<",\"formatted_bytes\":"<<c.formatted_table_json.size()<<'}';return out.str();}
}  // namespace minera
