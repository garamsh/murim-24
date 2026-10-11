#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "region.hpp"

namespace murim {

/// One value of a pack's content as the core reads it (ADR 0049): a boolean,
/// an integer, a number with a fraction or an exponent, which the core
/// refuses, a string, a list or a map. A list's items, or a map's values
/// under `keys`, are other nodes of its tree, named by their index in it.
/// `pack` is the index, in the load order, of the pack that last set it.
struct ContentNode {
  enum class Kind : std::uint8_t {
    BOOLEAN,
    INTEGER,
    FRACTION,
    STRING,
    LIST,
    MAP,
  };
  Kind kind = Kind::MAP;
  bool boolean = false;
  std::int64_t integer = 0;
  std::string text;
  std::vector<std::size_t> children;
  std::vector<std::string> keys;
  std::size_t pack = 0;
};

/// A document's values: the nodes of one tree, its root the first.
struct ContentTree {
  std::vector<ContentNode> nodes;
};

/// Returns the node of `tree` the map at `node` holds under `key`, if it
/// holds one.
[[nodiscard]] std::optional<std::size_t> member(const ContentTree& tree,
                                                std::size_t node,
                                                const std::string& key);

/// Why content is refused.
enum class ContentRefusal : std::uint8_t {
  /// A document is not well formed, or a document or pack is missing.
  UNREADABLE,
  /// A document is larger than a pack may hold.
  TOO_LARGE,
  /// A field the kind does not have, or a document of a kind the core does
  /// not know.
  UNKNOWN_FIELD,
  /// A field of the wrong type, or a document or entry that is not a map.
  WRONG_TYPE,
  /// A number with a fraction or an exponent: content numbers are integers.
  FRACTION,
  /// A required field is missing.
  MISSING_FIELD,
  /// A number outside its limits, or a word that is not one of its choices.
  OUT_OF_RANGE,
  /// An id that is not lowercase ASCII letters, digits and underscores.
  BAD_ID,
  /// Two entries of one kind with the same id in one pack.
  DUPLICATE_ID,
  /// A reference to an id no entry of its kind has.
  UNRESOLVED_ID,
  /// No base pack, or more than one, or a base that requires a pack.
  BASE,
  /// Packs that require or load after one another in a cycle.
  CYCLE,
  /// A pack required is missing, refused or of too low a version.
  MISSING_REQUIREMENT,
  /// The merged content fails a check of the content as a whole, such as a
  /// template's buildings overlapping.
  INCONSISTENT,
};

/// One error found in loading content: the pack, the document and the
/// entry it lies in, where they apply, the path of the field within the
/// entry, and why.
struct ContentError {
  std::string pack;
  std::string document;
  std::string entry;
  std::string path;
  ContentRefusal reason = ContentRefusal::UNREADABLE;
};

/// A document as a pack source serves it: its tree, or why it could not be
/// read.
struct ContentRead {
  std::optional<ContentTree> tree;
  ContentRefusal refusal = ContentRefusal::UNREADABLE;
};

/// The port through which the core receives content packs (ADR 0049). A pack
/// is a manifest, `pack.json`, and documents under `data/`, one per kind of
/// content, each a list of entries. The source serves them as the core's
/// value trees; how they are stored and parsed is the adapter's.
class PackSource {
 public:
  PackSource() = default;
  PackSource(const PackSource&) = default;
  PackSource(PackSource&&) = default;
  PackSource& operator=(const PackSource&) = default;
  PackSource& operator=(PackSource&&) = default;
  virtual ~PackSource() = default;

  /// Returns the document at `path` within pack `pack`, such as `pack.json`
  /// or `data/factions.json`.
  [[nodiscard]] virtual ContentRead read(const std::string& pack,
                                         const std::string& path) const = 0;

  /// Returns the paths of pack `pack`'s data documents, each `data/` and a
  /// kind's name with `.json`; none for a pack the source lacks.
  [[nodiscard]] virtual std::vector<std::string> data_documents(
      const std::string& pack) const = 0;
};

/// The content a set of packs gives: the tables a region is generated from,
/// unless the base pack or the merged set is refused; every error found, in
/// the order found; the packs refused, each with every pack requiring it;
/// and the packs loaded, in the order merged, the base first.
struct LoadedContent {
  std::optional<RegionData> data;
  std::vector<ContentError> errors;
  std::vector<std::string> refused;
  std::vector<std::string> order;
};

/// Loads the base pack `base` and the mods `mods`, in that preferred order,
/// from `source`: orders them so each loads after the packs it requires and
/// the packs it names to load after, checks every document against its
/// kind, refuses a pack with any error with every pack requiring it, merges
/// the rest field by field, the later winning, and builds the tables from the
/// merge. Ids become indices in the order they first appear.
[[nodiscard]] LoadedContent load_content(const PackSource& source,
                                         const std::string& base,
                                         const std::vector<std::string>& mods);

}  // namespace murim
