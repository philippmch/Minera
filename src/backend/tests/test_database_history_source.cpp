#undef NDEBUG
#include "minera/database_history_source.hpp"
#include "minera/xes_export.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#ifdef MINERA_HAS_SQLITE
#ifdef _WIN32
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif
#endif
using namespace minera;
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
int main(){
 const auto query=database_history_query("sap_accounting","main.");assert(query.find("main.BKPF")!=std::string::npos&&query.find("Clear Vendor Document")!=std::string::npos);rejects([]{(void)database_history_query("sap_o2c","X;DELETE.");});
#ifdef MINERA_HAS_SQLITE
 const auto path=std::filesystem::temp_directory_path()/("minera-database-history-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".sqlite");assert(!std::filesystem::exists(path));sqlite3* db=nullptr;assert(sqlite3_open(path.string().c_str(),&db)==SQLITE_OK);const char* sql="CREATE TABLE act_hi_procinst(PROC_DEF_KEY_ TEXT,PROC_INST_ID_ TEXT);CREATE TABLE act_hi_actinst(PROC_INST_ID_ TEXT,EXECUTION_ID_ TEXT,ACT_NAME_ TEXT,START_TIME_ TEXT,ASSIGNEE_ TEXT);INSERT INTO act_hi_procinst VALUES('p','pi');INSERT INTO act_hi_actinst VALUES('pi','c','B','2024-01-02T00:00:00.000000001Z',NULL);INSERT INTO act_hi_actinst VALUES('pi','c','A','2024-01-01T00:00:00Z','owner');";assert(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);sqlite3_close(db);DatabaseHistoryOptions options;options.connector="camunda_workflow";options.path=path;options.max_rows=1;auto c=capture_database_history(options);assert(c.table.limited&&c.log.event_count()==1&&c.log.traces[0].events[0].activity=="A");options.max_rows=10;c=capture_database_history(options);assert(!c.table.limited&&c.log.event_count()==2&&c.log.traces[0].events[1].resource_state==FirstClassAttributeState::kNull);assert(c.log.traces[0].events[1].timestamp=="2024-01-02T00:00:00.000000001Z");assert(c.formatted_table_json.find("@@case_index")!=std::string::npos);assert(serialize_xes(c.log).find("<string key=\"concept:name\" value=\"c\"")!=std::string::npos);std::filesystem::remove(path);
#endif
 DatabaseHistoryOptions injected;injected.connector="sap_accounting";injected.driver="odbc";injected.connection_string="secret-connection";const auto capture=capture_database_history(injected,[](const auto& o,const auto& sql){assert(o.connection_string=="secret-connection"&&sql.find("FROM BKPF")!=std::string::npos);return DatabaseHistoryTable{{"case:concept:name","concept:name","time:timestamp","org:resource","BLART"},{"text","text","text","text","text"},{{"c","Create Financial Document - X","20240101 100000",std::nullopt,"X"}},false};});assert(capture.log.event_count()==1&&capture.log.traces[0].events[0].timestamp=="2024-01-01T10:00:00.000000000Z");assert(database_history_capture_json(capture).find("secret-connection")==std::string::npos);
 rejects([&]{(void)capture_database_history(injected,[](const auto&,const auto&){return DatabaseHistoryTable{{"case:concept:name","concept:name","time:timestamp","org:resource"},{"text","text","text","text"},{{"short"}},false};});});
}
