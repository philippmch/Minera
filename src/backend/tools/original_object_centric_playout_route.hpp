#pragma once
// Include after original_emd_route.hpp; global native headers and registration
// are listed in integration.json. No discovery/current-session source is used.
void handle_original_object_centric_playout_workbench(
    const httplib::Request& request, httplib::Response& response) {
  using namespace minera;
  try {
    if (!request.is_multipart_form_data())
      throw std::invalid_argument("object-centric playout workbench requires multipart data");
    const auto supplied = [&](std::string_view name) { return active_source_supplied(request,name); };
    const auto field = [&](std::string_view name) { return active_source_field(request,name); };
    using Upload = std::decay_t<decltype(request.get_file_value("model_file"))>;
    std::optional<Upload> upload;
    if (request.has_file("model_file")) upload=request.get_file_value("model_file");
    const auto q=serialize_embedding_workbench_string_json;
    const auto flag=[](bool value){return value?"true":"false";};
    const auto exception_name=[](const std::exception& error){
      return dynamic_cast<const std::invalid_argument*>(&error)?"invalid_argument":
          dynamic_cast<const std::out_of_range*>(&error)?"out_of_range":
          dynamic_cast<const std::overflow_error*>(&error)?"overflow_error":
          dynamic_cast<const std::runtime_error*>(&error)?"runtime_error":"exception";
    };
    const auto error_value=[&](std::string_view type,const std::exception& error){
      return "{\"type\":"+q(type)+",\"message\":"+q(error.what())+
          ",\"exception_type\":"+q(exception_name(error))+'}';
    };
    const auto encoded=[](std::string_view bytes){
      bool text=true;
      for(std::size_t i=0;i<bytes.size()&&text;){
        const auto a=static_cast<unsigned char>(bytes[i++]);if(a<128)continue;
        const int n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:-1;
        if(n<0||i+static_cast<std::size_t>(n)>bytes.size()){text=false;break;}
        unsigned code=a&(n==1?31:n==2?15:7);
        for(int j=0;j<n;++j){const auto b=static_cast<unsigned char>(bytes[i++]);if((b&192)!=128){text=false;break;}code=(code<<6)|(b&63);}
        if((n==1&&code<128)||(n==2&&code<2048)||(n==3&&code<65536)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))text=false;
      }
      std::string data(bytes);
      if(!text){constexpr char hex[]="0123456789abcdef";data.clear();for(const unsigned char c:bytes){data+=hex[c>>4];data+=hex[c&15];}}
      return std::pair{text,std::move(data)};
    };
    const auto original=[&](std::string_view name,std::string_view bytes){
      const auto [text,data]=encoded(bytes);
      return "{\"dataset\":"+q(name)+",\"encoding\":"+q(text?"utf8":"hex")+
          ",\"data\":"+q(data)+",\"bytes\":"+std::to_string(bytes.size())+
          ",\"sha256\":"+q(footprints_workbench_sha256(bytes))+'}';
    };
    std::string raw_controls="[";
    const auto control=[&](const std::string& name,const std::string& bytes){
      if(raw_controls.size()>1)raw_controls+=',';
      raw_controls+="{\"name\":"+q(name)+",\"original\":"+original(name,bytes)+'}';
    };
    for(const auto& [name,value]:request.params)control(name,value);
    for(const auto& [name,value]:request.files)if(name!="model_file")control(name,value.content);
    raw_controls+=']';

    const auto operation=supplied("operation")?field("operation"):"ocpn_result";
    const auto format=supplied("model_format")?field("model_format"):"exact";
    const auto options_text=supplied("options_json")?field("options_json"):"{}";
    const bool is_ocpn=operation=="ocpn_result"||operation=="ocpn_log";
    const bool is_causal=operation=="causal_result"||operation=="causal_log";
    const bool result_surface=operation=="ocpn_result"||operation=="causal_result";
    const auto submitted_options=supplied("options_json")&&encoded(options_text).first?q(options_text):std::string("null");
    std::optional<OcpnGraph> graph;
    std::optional<OcCausalNet> causal;
    std::optional<OcpnPlayoutOptions> options;
    std::optional<OcpnPlayoutResult> result;
    std::optional<Ocel> log;
    std::set<std::string> option_members,model_members;
    std::string before="null",after="null",options_before="null",options_after="null";
    std::string phase="configuration_error",failure="null",calls="[",result_value="null";
    bool model_parsed=false,executed=false,returned=false;
    const auto invoke=[&](std::string_view name,std::string_view stage,std::string_view kind,const auto& make){
      if(calls.size()>1)calls+=',';
      const auto prefix="{\"name\":"+q(name)+",\"stage\":"+q(stage)+",\"input_kind\":"+q(kind)+",\"returned\":";
      try{make();calls+=prefix+"true,\"error\":null}";}
      catch(const std::exception& error){calls+=prefix+"false,\"error\":"+error_value(stage=="primary"?"native_error":"export_error",error)+'}';throw;}
    };
    try {
      static const std::set<std::string> controls={"operation","model_format","options_json"};
      const auto check=[&](const std::string& name){
        if(request.params.count(name)+request.files.count(name)!=1)
          throw std::invalid_argument("duplicate object-centric playout field: "+name);
        if(name=="model_file"){
          if(!upload)throw std::invalid_argument("object-centric playout model_file must be a file");
        }else if(!controls.contains(name))throw std::invalid_argument("unknown object-centric playout control: "+name);
      };
      for(const auto& [name,value]:request.params){(void)value;check(name);}
      for(const auto& [name,value]:request.files){(void)value;check(name);}
      if(!is_ocpn&&!is_causal)throw std::invalid_argument("unsupported object-centric playout operation: "+operation);
      if(format!="exact")throw std::invalid_argument("object-centric playout model_format requires exact");
      if(!upload)throw std::invalid_argument("missing object-centric playout model_file");
      phase="preparation_error";
      if(is_ocpn){graph=parse_oc_playout_workbench_ocpn_json(upload->content);before=serialize_oc_playout_workbench_ocpn_json(*graph);}
      else{causal=parse_oc_playout_workbench_causal_net_json(upload->content);before=serialize_oc_playout_workbench_causal_net_json(*causal);}
      model_parsed=true;
      model_members=footprints_workbench_supplied_members(upload->content);
      phase="configuration_error";
      options=parse_oc_playout_workbench_options_json(options_text,&option_members);
      options_before=serialize_oc_playout_workbench_options_json(*options);
      phase="native_error";
      if(operation=="ocpn_result"){
        invoke("ocpn_extensive_playout_result(graph,options)","primary","OcpnGraph",[&]{executed=true;result=ocpn_extensive_playout_result(*graph,*options);});
      }else if(operation=="ocpn_log"){
        invoke("ocpn_extensive_playout(graph,options)","primary","OcpnGraph",[&]{executed=true;log=ocpn_extensive_playout(*graph,*options);});
      }else if(operation=="causal_result"){
        invoke("oc_causal_net_extensive_playout_result(model,options)","primary","OcCausalNet",[&]{executed=true;result=oc_causal_net_extensive_playout_result(*causal,*options);});
      }else{
        invoke("oc_causal_net_extensive_playout(model,options)","primary","OcCausalNet",[&]{executed=true;log=oc_causal_net_extensive_playout(*causal,*options);});
      }
      returned=true;
      result_value=result?serialize_oc_playout_workbench_result_json(*result):serialize_oc_playout_workbench_ocel_json(*log);
    }catch(const std::exception& error){failure=error_value(phase,error);}

    const std::vector<std::string> artifacts={"model_json","options_json","result_json","model_legacy_json","ocel_classic_json","ocel_standard_json","playout_legacy_json","playout_legacy_default_json"};
    std::map<std::string,std::optional<std::string>> exports;
    std::map<std::string,std::string> export_errors;
    for(const auto& key:artifacts){exports[key]=std::nullopt;export_errors[key]="null";}
    const auto artifact=[&](const std::string& key,const auto& make){
      try{exports[key]=make();}catch(const std::exception& error){export_errors[key]=error_value("export_error",error);}
    };
    if(model_parsed)artifact("model_json",[&]{return before;});
    if(options)artifact("options_json",[&]{return serialize_oc_playout_workbench_options_json(*options);});
    if(returned)artifact("result_json",[&]{return result_value;});
    if(graph)artifact("model_legacy_json",[&]{std::string value;invoke("serialize_ocpn_json(graph)","export","OcpnGraph",[&]{value=serialize_ocpn_json(*graph);});return value;});
    if(causal)artifact("model_legacy_json",[&]{std::string value;invoke("serialize_oc_causal_net_json(model)","export","OcCausalNet",[&]{value=serialize_oc_causal_net_json(*causal);});return value;});
    const Ocel* generated=result?&result->log:log?&*log:nullptr;
    if(returned&&generated){
      artifact("ocel_classic_json",[&]{std::string value;invoke("serialize_ocel_json(log,classic)","export","Ocel",[&]{value=serialize_ocel_json(*generated,OcelJsonFormat::kClassic,false);});return value;});
      artifact("ocel_standard_json",[&]{std::string value;invoke("serialize_ocel_json(log,standard)","export","Ocel",[&]{value=serialize_ocel_json(*generated,OcelJsonFormat::kStandard,false);});return value;});
      artifact("playout_legacy_json",[&]{
        std::string value;
        if(is_ocpn&&result)invoke("serialize_ocpn_playout_json(result,binding)","export","OcpnPlayoutResult",[&]{value=serialize_ocpn_playout_json(*result,options->event_binding);});
        else if(is_ocpn)invoke("serialize_ocpn_playout_json(log,binding)","export","Ocel",[&]{value=serialize_ocpn_playout_json(*generated,options->event_binding);});
        else if(result)invoke("serialize_oc_causal_net_playout_json(result,binding)","export","OcpnPlayoutResult",[&]{value=serialize_oc_causal_net_playout_json(*result,options->event_binding);});
        else invoke("serialize_oc_causal_net_playout_json(log,binding)","export","Ocel",[&]{value=serialize_oc_causal_net_playout_json(*generated,options->event_binding);});
        return value;
      });
      artifact("playout_legacy_default_json",[&]{
        std::string value;
        if(is_ocpn)invoke("serialize_ocpn_playout_json(log)","export","Ocel",[&]{value=serialize_ocpn_playout_json(*generated);});
        else invoke("serialize_oc_causal_net_playout_json(log)","export","Ocel",[&]{value=serialize_oc_causal_net_playout_json(*generated);});
        return value;
      });
    }
    calls+=']';
    std::string exports_json="{",artifact_originals="{";
    for(const auto& key:artifacts){
      if(exports_json.size()>1){exports_json+=',';artifact_originals+=',';}
      exports_json+=q(key)+':'+(exports[key]&&encoded(*exports[key]).first?q(*exports[key]):"null")+','+q(key+"_error")+':'+export_errors[key];
      artifact_originals+=q(key)+':'+(exports[key]?original(key,*exports[key]):"null");
    }
    exports_json+='}';artifact_originals+='}';
    if(graph)after=serialize_oc_playout_workbench_ocpn_json(*graph);
    if(causal)after=serialize_oc_playout_workbench_causal_net_json(*causal);
    if(options)options_after=serialize_oc_playout_workbench_options_json(*options);
    const auto strings=[&](const std::set<std::string>& values){std::string json="[";for(const auto& value:values){if(json.size()>1)json+=',';json+=q(value);}return json+']';};
    const auto upload_context=upload?"{\"dataset\":"+q(upload->filename)+",\"bytes\":"+std::to_string(upload->content.size())+
        ",\"sha256\":"+q(footprints_workbench_sha256(upload->content))+",\"encoding\":"+q(encoded(upload->content).first?"utf8":"hex")+
        ",\"format\":"+q(format)+",\"native_kind\":"+(model_parsed?q(is_ocpn?"OcpnGraph":"OcCausalNet"):"null")+
        ",\"supplied_members\":"+strings(model_members)+",\"unchanged\":"+(model_parsed?std::string(flag(before==after)):"null")+
        ",\"before\":"+before+",\"after\":"+after+'}':"null";
    const auto context="{\"size_t_bytes\":"+std::to_string(sizeof(std::size_t))+",\"uploads\":{\"model_file\":"+upload_context+"}}";
    const auto model=upload?"{\"original_model\":"+original(upload->filename,upload->content)+",\"ocpn\":"+
        (graph?serialize_oc_playout_workbench_ocpn_json(*graph):"null")+",\"causal_net\":"+
        (causal?serialize_oc_playout_workbench_causal_net_json(*causal):"null")+'}':"null";
    const auto options_context=options?"{\"before\":"+options_before+",\"after\":"+options_after+
        ",\"unchanged\":"+flag(options_before==options_after)+'}':"null";
    response.status=failure=="null"?200:400;
    response.set_content("{\"family\":\"ORIGINAL_OBJECT_CENTRIC_PLAYOUT_WORKBENCH\",\"operation\":"+q(operation)+
        ",\"model_kind\":"+(is_ocpn?q("ocpn"):is_causal?q("causal_net"):"null")+",\"executed\":"+flag(executed)+
        ",\"error\":"+failure+",\"native_calls\":"+calls+",\"input_context\":"+context+",\"model\":"+model+
        ",\"submitted_options\":"+submitted_options+",\"supplied_options\":"+strings(option_members)+
        ",\"configured_options\":"+(options?serialize_oc_playout_workbench_options_json(*options):"null")+
        ",\"options_context\":"+options_context+",\"raw_controls\":"+raw_controls+
        ",\"result\":"+(returned?"{\"type\":"+q(result_surface?"ocpn_playout_result":"ocel")+",\"value\":"+result_value+'}':"null")+
        ",\"exports\":"+exports_json+",\"artifact_originals\":"+artifact_originals+'}',"application/json");
  }catch(const std::exception& error){
    response.status=400;
    response.set_content("{\"error\":"+minera::serialize_embedding_workbench_string_json(error.what())+'}',"application/json");
  }
}
