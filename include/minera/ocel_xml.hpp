#pragma once

#include <filesystem>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

#include "minera/ocel.hpp"

namespace minera {

enum class OcelXmlFormat { kClassic, kStandard };

struct OcelXmlImportOptions {
  // Unknown XML fields are ignored by default. Set to reject unsupported
  // constructs (for example list-valued attributes) instead.
  bool reject_unsupported = false;
  std::size_t max_bytes = kOcelMaxDocumentBytes;
};

struct OcelXmlExportOptions {
  OcelXmlFormat format = OcelXmlFormat::kClassic;
  bool allow_lossy = false;
  bool include_declaration = true;
};

[[nodiscard]] Ocel parse_ocel_xml(std::string_view xml,
                                  const OcelXmlImportOptions& options = {});
[[nodiscard]] Ocel import_ocel_xml(std::istream& input,
                                   const OcelXmlImportOptions& options = {});
[[nodiscard]] Ocel import_ocel_xml(const std::filesystem::path& path,
                                   const OcelXmlImportOptions& options = {});

void write_ocel_xml(std::ostream& out, const Ocel& log,
                    const OcelXmlExportOptions& options = {});
[[nodiscard]] std::string serialize_ocel_xml(
    const Ocel& log, const OcelXmlExportOptions& options = {});
void export_ocel_xml(const Ocel& log, const std::filesystem::path& path,
                     const OcelXmlExportOptions& options = {});

}  // namespace minera
