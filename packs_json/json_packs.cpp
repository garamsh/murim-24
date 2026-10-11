#include "json_packs.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "content.hpp"

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

DirectoryPacks::DirectoryPacks(std::filesystem::path root)
    : root_(std::move(root)) {}

ContentRead DirectoryPacks::read(const std::string& pack,
                                 const std::string& path) const {
  const std::filesystem::path file = root_ / pack / path;
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(file, error);
  if (error) {
    return refused(ContentRefusal::UNREADABLE);
  }
  if (size > DOCUMENT_LIMIT) {
    return refused(ContentRefusal::TOO_LARGE);
  }
  std::ifstream input(file, std::ios::binary);
  if (!input) {
    return refused(ContentRefusal::UNREADABLE);
  }
  std::ostringstream bytes;
  bytes << input.rdbuf();
  return parse_document(bytes.str());
}

std::vector<std::string> DirectoryPacks::data_documents(
    const std::string& pack) const {
  std::vector<std::string> paths;
  std::error_code error;
  const std::filesystem::path data = root_ / pack / "data";
  for (const auto& entry : std::filesystem::directory_iterator(data, error)) {
    if (entry.is_regular_file(error)) {
      paths.push_back("data/" + entry.path().filename().string());
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

void BytePacks::add(const std::string& pack, const std::string& path,
                    std::string bytes) {
  documents_.erase(std::remove_if(documents_.begin(), documents_.end(),
                                  [&](const Document& document) {
                                    return document.pack == pack &&
                                           document.path == path;
                                  }),
                   documents_.end());
  documents_.push_back(Document{pack, path, std::move(bytes)});
}

ContentRead BytePacks::read(const std::string& pack,
                            const std::string& path) const {
  const auto found = std::find_if(
      documents_.begin(), documents_.end(), [&](const Document& document) {
        return document.pack == pack && document.path == path;
      });
  return found == documents_.end() ? refused(ContentRefusal::UNREADABLE)
                                   : parse_document(found->bytes);
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

std::vector<std::string> BytePacks::data_documents(
    const std::string& pack) const {
  std::vector<std::string> paths;
  const std::string prefix = "data/";
  for (const Document& document : documents_) {
    if (document.pack == pack &&
        document.path.compare(0, prefix.size(), prefix) == 0) {
      paths.push_back(document.path);
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

}  // namespace murim_packs
