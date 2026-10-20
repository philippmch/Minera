#include "minera/case_description.hpp"
#include "root_cause_trace_key.hpp"
#include "root_cause_unicode.hpp"
#include <algorithm>
#include <map>

namespace minera { namespace {
std::string case_string_repr(std::string_view value){
 const char delimiter=value.find('\'')!=std::string_view::npos&&value.find('"')==std::string_view::npos?'"':'\'';
 std::string out(1,delimiter);constexpr char hex[]="0123456789abcdef";
 for(std::size_t i=0;i<value.size();){const auto start=i;const auto first=static_cast<unsigned char>(value[i++]);char32_t code=first;unsigned count=0;
  if(first<0x80){}else if(first>=0xc2&&first<=0xdf){code=first&31;count=1;}else if(first>=0xe0&&first<=0xef){code=first&15;count=2;}else if(first>=0xf0&&first<=0xf4){code=first&7;count=3;}else throw std::invalid_argument("case description: invalid string UTF-8");
  for(unsigned j=0;j<count;++j){if(i==value.size())throw std::invalid_argument("case description: short string UTF-8");const auto next=static_cast<unsigned char>(value[i++]);if((next&0xc0)!=0x80)throw std::invalid_argument("case description: invalid string continuation");code=(code<<6)|(next&63);}
  if((count==1&&code<0x80)||(count==2&&code<0x800)||(count==3&&code<0x10000)||code>0x10ffff)throw std::invalid_argument("case description: invalid string code point");
  if(code=='\\'||code==static_cast<unsigned char>(delimiter)){out+='\\';out+=static_cast<char>(code);}
  else if(code=='\n')out+="\\n";else if(code=='\r')out+="\\r";else if(code=='\t')out+="\\t";
  else if(!root_cause_unicode_printable(code)){const int digits=code<=255?2:code<=65535?4:8;out+=digits==2?"\\x":digits==4?"\\u":"\\U";for(int shift=(digits-1)*4;shift>=0;shift-=4)out+=hex[(code>>shift)&15];}
  else out.append(value.substr(start,i-start));
 }
 return out+delimiter;
}
std::string scalar(const CaseDescriptionValue& value,bool nested){
 if(value.kind==CaseValueKind::kNull)return "None";
 if(value.kind==CaseValueKind::kString)return nested?case_string_repr(value.text):value.text;
 EventAttribute attribute;
 switch(value.kind){
 case CaseValueKind::kBoolean:attribute.typed_value=value.boolean;break;
 case CaseValueKind::kInteger:attribute.kind=AttributeKind::kInt;attribute.value=value.text;break;
 case CaseValueKind::kFloat:attribute.typed_value=value.real;break;
 case CaseValueKind::kBytes:attribute=make_bytes_attribute("",value.text);break;
 case CaseValueKind::kDate:attribute.kind=AttributeKind::kDate;attribute.value=value.text;break;
 default:throw std::invalid_argument("case description: invalid scalar kind");
 }
 if(nested&&value.kind==CaseValueKind::kDate){
  EventAttribute tuple;tuple.container=AttributeContainerKind::kTuple;tuple.children={attribute};
  auto text=root_cause_attribute_category(tuple);return text.substr(1,text.size()-3);
 }
 return root_cause_attribute_category(attribute);
}
std::string representation(const CaseDescriptionValue& value,bool nested){
 const auto kind=value.kind;
 if(kind!=CaseValueKind::kList&&kind!=CaseValueKind::kTuple&&kind!=CaseValueKind::kDictionary)return scalar(value,nested);
 if(kind==CaseValueKind::kDictionary&&value.items.size()%2)throw std::invalid_argument("case description: dictionary requires key/value pairs");
 std::string out=kind==CaseValueKind::kList?"[":kind==CaseValueKind::kTuple?"(":"{";
 const auto step=kind==CaseValueKind::kDictionary?2u:1u;
 for(std::size_t i=0;i<value.items.size();i+=step){if(i)out+=", ";out+=representation(value.items[i],true);if(step==2)out+=": "+representation(value.items[i+1],true);}
 if(kind==CaseValueKind::kTuple&&value.items.size()==1)out+=',';
 return out+(kind==CaseValueKind::kList?"]":kind==CaseValueKind::kTuple?")":"}");
}
// Dictionary adapters reject duplicate keys. Direct callers receive the same
// overwrite-position behavior as ordered Python dictionary assignment.
CaseDescriptionAttributes dictionary(const CaseDescriptionAttributes& attributes){
 CaseDescriptionAttributes out;std::map<std::string,std::size_t> indexes;
 for(const auto& entry:attributes){const auto [it,inserted]=indexes.emplace(entry.first,out.size());if(inserted)out.push_back(entry);else out[it->second].second=entry.second;}return out;
}
const CaseDescriptionValue& selected(const CaseDescriptionAttributes& attrs,const std::string& key){
 for(const auto& [name,value]:attrs)if(name==key)return value;throw CaseDescriptionKeyError(key);
}
} // namespace
CaseDescriptionKeyError::CaseDescriptionKeyError(std::string value):std::out_of_range(case_string_repr(value)),key(std::move(value)){}
LlmContextResult abstract_case(const CaseDescription& supplied,const CaseDescriptionOptions& options){
 std::string text="\n";
 if(options.include_header)text+=options.include_case_attributes?"If I have a case with the following (case) attributes:":"If I have a case";
 text+="\n\n";
 if(options.include_case_attributes)for(const auto& [key,value]:dictionary(supplied.attributes))text+=key+" = "+representation(value,false)+"\n";
 text+='\n';
 if(options.include_header)text+="\nthe case contains the following events (the activity of the event is reported first):\n\n";
 for(const auto& original:supplied.events){const auto event=dictionary(original);text+=representation(selected(event,options.activity_key),false)+" ";
  std::vector<std::pair<std::string,std::string>> extras;
  if(options.include_event_attributes)for(const auto& [key,value]:event)if(key!=options.activity_key&&key!=options.timestamp_key)extras.emplace_back(key,representation(value,false));
  std::sort(extras.begin(),extras.end(),[](const auto& a,const auto& b){return a.first<b.first;});
  if(options.include_timestamp||!extras.empty()){
   text+="( ";if(options.include_timestamp)text+="timestamp = "+representation(selected(event,options.timestamp_key),false)+" ;  ";
   for(const auto& [key,value]:extras)text+=key+" = "+value+" ;  ";
   text.pop_back();text+=")";
  }
  text+='\n';
 }
 text+='\n';const auto count=std::count_if(text.begin(),text.end(),[](unsigned char c){return (c&0xc0)!=0x80;});
 return {"case",std::move(text),static_cast<std::size_t>(count),false};
}
} // namespace minera
