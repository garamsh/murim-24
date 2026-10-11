#pragma once

#include <string>
#include <vector>

namespace murim {

/// A name as it is shown: in Korean, and romanised for English.
struct NameEntry {
  std::string korean;
  std::string romanised;
};

/// The names people bear: a surname for each family, drawn by the seed when
/// the world begins, and given names for each sex, drawn at birth.
struct Names {
  std::vector<NameEntry> surnames;
  std::vector<NameEntry> female;
  std::vector<NameEntry> male;
};

/// A person's full name: its family's surname and its given name.
struct Name {
  NameEntry surname;
  NameEntry given;
};

}  // namespace murim
