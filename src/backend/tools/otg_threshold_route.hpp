#pragma once
// Opt-in sibling for the exact relation map. Ordinary OCEL routes retain their
// existing parser, algorithms and defaults.
void handle_exact_otg_conformance(const httplib::Request& request,
                                  httplib::Response& response) {
 try {
  const auto present=[&](std::string_view name){const std::string k(name);return request.has_param(k)||request.has_file(k);};
  const auto lookup=[&](std::string_view name){const std::string k(name);return request.has_file(k)?form_field(request,k):request.has_param(k)?request.get_param_value(k):std::string{};};
  static const std::set<std::string> allowed={"variant","alpha","beta","gamma","delta","etot_theta","otg_theta","otg_theta_interaction","otg_theta_descendants","otg_theta_inheritance","otg_theta_cobirth","otg_theta_codeath","ocdfg_theta_activity","ocdfg_theta_flow","real","real_graph_kind","ocdfg_activity_attribute","ocdfg_object_type_attribute","otg_theta_by_relation_json","log","real_graph","normative"};
  for(const auto& [k,v]:request.params){(void)v;if(!allowed.contains(k))throw std::invalid_argument("unknown OCEL conformance parameter: "+k);}
  for(const auto& [k,v]:request.files){(void)v;if(!allowed.contains(k))throw std::invalid_argument("unknown OCEL conformance field: "+k);}
  for(const auto& k:allowed){if(request.params.count(k)+request.files.count(k)>1)throw std::invalid_argument("duplicate query/multipart OCEL conformance control: "+k);}
  for(const auto* k:{"otg_theta_interaction","otg_theta_descendants","otg_theta_inheritance","otg_theta_cobirth","otg_theta_codeath"})if(present(k))throw std::invalid_argument("otg_theta_by_relation_json cannot combine with "+std::string(k));
  const auto real_mode=present("real")?lower_ascii(lookup("real")):std::string("ocel");
  if(real_mode!="ocel"&&real_mode!="graph")throw std::invalid_argument("real expects ocel or graph");
  const auto requested=present("real_graph_kind")?lower_ascii(lookup("real_graph_kind")):std::string{};
  const auto variant=present("variant")?lookup("variant"):(real_mode=="graph"&&!requested.empty()?requested:std::string("ocdfg"));
  if(variant!="otg")throw std::invalid_argument("otg_theta_by_relation_json requires variant otg");
  if(real_mode=="ocel"&&present("real_graph_kind"))throw std::invalid_argument("real_graph_kind requires real=graph");
  if(real_mode=="graph"&&!requested.empty()&&requested!="otg")throw std::invalid_argument("real_graph_kind must match the conformance variant");
  if(!request.is_multipart_form_data()||!request.has_file(real_mode=="graph"?"real_graph":"log")||!request.has_file("normative"))throw std::invalid_argument("OCEL conformance requires multipart fields log or real_graph and normative");
  const auto real_body=form_field(request,real_mode=="graph"?"real_graph":"log"),normative_body=form_field(request,"normative"),threshold_json=lookup("otg_theta_by_relation_json");
  if(real_body.empty()||normative_body.empty())throw std::invalid_argument("OCEL conformance requires nonempty log or real_graph and normative");
  const auto number=[&](std::string_view name,double fallback){if(!present(name))return fallback;const auto value=lookup(name);char* end=nullptr;const double parsed=std::strtod(value.c_str(),&end);if(end==value.c_str()||end!=value.c_str()+value.size())throw std::invalid_argument(std::string(name)+" expects a number");return parsed;};
  minera::OtgConformanceOptions options;options.alpha=number("alpha",options.alpha);options.beta=number("beta",options.beta);options.gamma=number("gamma",options.gamma);options.theta=number("otg_theta",options.theta);options.theta_by_relation=minera::parse_otg_theta_by_relation_json(threshold_json);
  const auto real=real_mode=="graph"?minera::parse_exact_otg_graph_json(real_body):minera::discover_otg_classic(minera::parse_ocel_json(real_body));
  std::optional<minera::OtgGraph> normative_graph;
  try{normative_graph=minera::parse_exact_otg_graph_json(normative_body);}catch(const std::exception&){/* Existing OCEL normative fallback. */}
  const auto normative=normative_graph?*normative_graph:minera::discover_otg_classic(minera::parse_ocel_json(normative_body));
  const auto result=minera::compare_otg(real,normative,options);
  response.set_content(minera::serialize_exact_otg_conformance_json(result,real,normative,options,real_body,normative_body,threshold_json),"application/json");
 }catch(const std::exception& e){response.status=400;response.set_content(error_json(e.what()),"application/json");}
}
