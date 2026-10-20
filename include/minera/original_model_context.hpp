#pragma once
#include "minera/llm_context.hpp"
#include "minera/declare.hpp"
#include "minera/log_skeleton.hpp"
#include "minera/temporal_profile.hpp"
namespace minera {
// Direct original-model descriptions, without discovery, normalization, a
// character budget or an output byte cap. Text is an authoritative byte string.
// Ordered dictionary view used by direct supplied-model descriptions. This
// does not change the mined DeclareModel or its legacy deterministic formatter.
struct OrderedDeclareFamily {std::string key;std::vector<DeclareConstraint> value;};
struct OrderedDeclareModel {std::vector<OrderedDeclareFamily> rules;};
[[nodiscard]] LlmContextResult abstract_declare(const OrderedDeclareModel&,bool include_header=true);
[[nodiscard]] LlmContextResult abstract_declare(const DeclareModel&,bool include_header=true);
[[nodiscard]] LlmContextResult abstract_log_skeleton(const LogSkeletonModel&,bool include_header=true);
[[nodiscard]] LlmContextResult abstract_temporal_profile(const TemporalProfile&,bool include_header=true);
[[nodiscard]] LlmContextResult abstract_petri_net(const PetriNet&,const TokenMarking& initial,const TokenMarking& final,bool response_header=true);
// Document metadata is retained and read literally where formatting uses it.
// Checked access reports malformed read metadata as an exception, not UB.
[[nodiscard]] LlmContextResult abstract_petri_net(const PnmlDocument&,bool response_header=true);
} // namespace minera
