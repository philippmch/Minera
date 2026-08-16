#pragma once

#include <string>

#include "minera/log_util.hpp"

namespace minera {

[[nodiscard]] std::string log_train_test_json(const LogTrainTestSplit& result);

[[nodiscard]] std::string log_prefix_json(
    const EventLog& log, LogUtilityVariant variant,
    const std::vector<std::vector<std::size_t>>& change_indexes = {});

[[nodiscard]] std::string log_until_activity_json(
    const LogUntilActivityResult& result);

[[nodiscard]] std::string log_activity_targets_json(
    const LogActivityTargetResult& result);

[[nodiscard]] std::string log_feature_selection_json(
    const LogFeatureSelectionResult& result);

}  // namespace minera
