#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "content.hpp"

namespace murim_packs {

/// The id of the pack the game's builds load as their base.
constexpr const char* BASE_PACK = "wuxia";

/// The largest document a pack may hold, in bytes: well above the base
/// pack's largest, so a broken file cannot exhaust memory.
constexpr std::size_t DOCUMENT_LIMIT = std::size_t{4} << 20U;

/// Parses `bytes` as a JSON document into the core's value tree. A number
/// with a fraction or an exponent becomes a value the core refuses; a
/// document not well formed is refused as unreadable, one past
/// DOCUMENT_LIMIT as too large, and one holding a null or an integer past 64
/// bits as of the wrong type.
[[nodiscard]] murim::ContentRead parse_document(const std::string& bytes);

/// A pack source reading pack directories under a root: `<root>/<pack>/
/// pack.json` and `<root>/<pack>/data/*.json`. The sweep, the bench and the
/// core's tests read the base pack through it.
class DirectoryPacks : public murim::PackSource {
 public:
  explicit DirectoryPacks(std::filesystem::path root);

  [[nodiscard]] murim::ContentRead read(const std::string& pack,
                                        const std::string& path) const override;

  [[nodiscard]] std::vector<std::string> data_documents(
      const std::string& pack) const override;

 private:
  std::filesystem::path root_;
};

/// A pack source of documents handed in as bytes, as a host that cannot name
/// its files by path reads them, such as Godot inside an export.
class BytePacks : public murim::PackSource {
 public:
  /// Adds the document `bytes` at `path` of pack `pack`, `pack.json` or
  /// `data/` and a kind's name with `.json`.
  void add(const std::string& pack, const std::string& path, std::string bytes);

  [[nodiscard]] murim::ContentRead read(const std::string& pack,
                                        const std::string& path) const override;

  [[nodiscard]] std::vector<std::string> data_documents(
      const std::string& pack) const override;

 private:
  struct Document {
    std::string pack;
    std::string path;
    std::string bytes;
  };
  std::vector<Document> documents_;
};

/// Loads the base pack, BASE_PACK, from pack directories under `root`, and
/// returns its tables. Throws std::runtime_error naming the first error when
/// it is refused.
[[nodiscard]] murim::RegionData load_base(const std::filesystem::path& root);

}  // namespace murim_packs
