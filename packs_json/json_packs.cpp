#include "json_packs.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "content.hpp"
#include "region.hpp"

namespace murim_packs {
namespace {

using murim::ContentNode;
using murim::ContentRead;
using murim::ContentRefusal;
using murim::ContentTree;
using Kind = ContentNode::Kind;
using Json = nlohmann::json;

// Sets node `node` from the scalar `json`, returning why it cannot be, if
// it cannot.
std::optional<ContentRefusal> set_scalar(const Json& json, ContentNode& node) {
  switch (json.type()) {
    case Json::value_t::boolean:
      node.kind = Kind::BOOLEAN;
      node.boolean = json.get<bool>();
      return std::nullopt;
    case Json::value_t::number_integer:
      node.kind = Kind::INTEGER;
      node.integer = json.get<std::int64_t>();
      return std::nullopt;
    case Json::value_t::number_unsigned: {
      const auto whole = json.get<std::uint64_t>();
      if (whole > static_cast<std::uint64_t>(
                      std::numeric_limits<std::int64_t>::max())) {
        return ContentRefusal::WRONG_TYPE;
      }
      node.kind = Kind::INTEGER;
      node.integer = static_cast<std::int64_t>(whole);
      return std::nullopt;
    }
    case Json::value_t::number_float:
      node.kind = Kind::FRACTION;
      return std::nullopt;
    case Json::value_t::string:
      node.kind = Kind::STRING;
      node.text = json.get<std::string>();
      return std::nullopt;
    default:
      return ContentRefusal::WRONG_TYPE;
  }
}

// Converts `root` into a tree, node by node from an explicit stack, returning
// why it cannot be, if it cannot.
std::optional<ContentRefusal> convert(const Json& root, ContentTree& tree) {
  tree.nodes.emplace_back();
  std::vector<std::pair<const Json*, std::size_t>> waiting{{&root, 0}};
  while (!waiting.empty()) {
    const auto [json, index] = waiting.back();
    waiting.pop_back();
    ContentNode made;
    if (json->is_array() || json->is_object()) {
      made.kind = json->is_array() ? Kind::LIST : Kind::MAP;
      for (const auto& [key, item] : json->items()) {
        const std::size_t child = tree.nodes.size();
        tree.nodes.emplace_back();
        made.children.push_back(child);
        if (json->is_object()) {
          made.keys.push_back(key);
        }
        waiting.emplace_back(&item, child);
      }
    } else if (const auto refused = set_scalar(*json, made)) {
      return refused;
    }
    tree.nodes.at(index) = std::move(made);
  }
  return std::nullopt;
}

constexpr std::string_view MANIFEST_FILE = "pack.json";
constexpr std::string_view DATA_DIRECTORY = "data";
// A map's documents are its pack's `map/` files, named with the directory.
constexpr std::string_view MAP_DIRECTORY = "map";
constexpr std::string_view MAP_PREFIX = "map/";
constexpr std::string_view EXTENSION = ".json";
constexpr std::size_t READ_CHUNK = std::size_t{64} << 10U;

// The document a file under `data/` named `name` holds: its name without
// `.json`, or, for a file of another type, its whole name, which names no
// kind.
std::string document_of(const std::string& name) {
  const bool json = name.size() > EXTENSION.size() &&
                    name.compare(name.size() - EXTENSION.size(),
                                 EXTENSION.size(), EXTENSION) == 0;
  return json ? name.substr(0, name.size() - EXTENSION.size()) : name;
}

ContentRead refused(ContentRefusal reason) {
  return ContentRead{std::nullopt, reason};
}

}  // namespace

ContentRead parse_document(const std::string& bytes) {
  if (bytes.size() > DOCUMENT_LIMIT) {
    return refused(ContentRefusal::TOO_LARGE);
  }
  const Json json = Json::parse(bytes, nullptr, /*allow_exceptions=*/false);
  if (json.is_discarded()) {
    return refused(ContentRefusal::UNREADABLE);
  }
  ContentTree tree;
  if (const auto refusal = convert(json, tree)) {
    return refused(*refusal);
  }
  return ContentRead{std::move(tree), ContentRefusal::UNREADABLE};
}

std::string file_of(const std::string& document) {
  if (document == murim::MANIFEST) {
    return std::string{MANIFEST_FILE};
  }
  const bool mapped = document.compare(0, MAP_PREFIX.size(), MAP_PREFIX) == 0;
  return (mapped ? document : std::string{DATA_DIRECTORY} + "/" + document) +
         std::string{EXTENSION};
}

DirectoryPacks::DirectoryPacks(std::filesystem::path root)
    : root_(std::move(root)) {}

ContentRead DirectoryPacks::read(const std::string& pack,
                                 const std::string& document) const {
  std::ifstream input(root_ / pack / file_of(document), std::ios::binary);
  if (!input) {
    return refused(ContentRefusal::UNREADABLE);
  }
  // Reads no more than one byte past the limit, which parse_document
  // refuses, so a huge file is never held whole.
  std::string bytes;
  std::array<char, READ_CHUNK> chunk{};
  while (input && bytes.size() <= DOCUMENT_LIMIT) {
    input.read(chunk.data(), chunk.size());
    bytes.append(chunk.data(), static_cast<std::size_t>(input.gcount()));
  }
  if (input.bad()) {
    return refused(ContentRefusal::UNREADABLE);
  }
  return parse_document(bytes);
}

std::vector<std::string> DirectoryPacks::documents(
    const std::string& pack) const {
  std::vector<std::string> names;
  std::error_code error;
  for (const auto& [directory, prefix] :
       {std::pair{DATA_DIRECTORY, std::string_view{}},
        std::pair{MAP_DIRECTORY, MAP_PREFIX}}) {
    for (const auto& entry :
         std::filesystem::directory_iterator(root_ / pack / directory, error)) {
      if (entry.is_regular_file(error)) {
        names.push_back(std::string{prefix} +
                        document_of(entry.path().filename().string()));
      }
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

void BytePacks::add(const std::string& pack, const std::string& path,
                    std::string bytes) {
  files_[{pack, path}] = std::move(bytes);
}

ContentRead BytePacks::read(const std::string& pack,
                            const std::string& document) const {
  const auto found = files_.find({pack, file_of(document)});
  return found == files_.end() ? refused(ContentRefusal::UNREADABLE)
                               : parse_document(found->second);
}

std::vector<std::string> BytePacks::documents(const std::string& pack) const {
  std::vector<std::string> names;
  const std::string prefix = std::string{DATA_DIRECTORY} + "/";
  for (const auto& [key, bytes] : files_) {
    const auto& [owner, path] = key;
    if (owner != pack) {
      continue;
    }
    if (path.compare(0, prefix.size(), prefix) == 0) {
      names.push_back(document_of(path.substr(prefix.size())));
    } else if (path.compare(0, MAP_PREFIX.size(), MAP_PREFIX) == 0) {
      names.push_back(document_of(path));
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

murim::RegionData load_base(const std::filesystem::path& root) {
  const DirectoryPacks source{root};
  murim::LoadedContent loaded = murim::load_content(source, BASE_PACK, {});
  if (!loaded.data) {
    const murim::ContentError first =
        loaded.errors.empty() ? murim::ContentError{} : loaded.errors.front();
    throw std::runtime_error("the base pack is refused: " + first.pack + " " +
                             first.document + " " + first.entry + " " +
                             first.path);
  }
  return std::move(*loaded.data);
}

}  // namespace murim_packs
