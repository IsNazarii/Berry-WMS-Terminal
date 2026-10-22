#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace wms {

class Journal;

// A crop the point buys. The code is what the journal stores ("raspberry");
// the name is what the operator sees. Crops change during the season, so
// they are reference data in the database, not a list in the program.
// A crop with records is never deleted, only archived: hidden from the lists,
// while old receipts still show its name.
class Crop {
public:
    Crop(std::string code, std::string name, bool archived = false);

    [[nodiscard]] const std::string& code() const noexcept { return code_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] bool archived() const noexcept { return archived_; }

private:
    std::string code_;
    std::string name_;
    bool archived_;
};

// The crop the name stands for, archived ones included: the same normalised
// name, or a code equal to it. nullptr when there is none.
[[nodiscard]] const Crop* findCropNamed(std::string_view name, const std::vector<Crop>& existing);

// A crop the operator adds on the terminal. A name already taken (ignoring
// case and spaces: "Малина" = "малина ") is refused, so one crop never splits
// into two stock rows. The name of an archived crop is free: adding it brings
// that crop back (see findCropNamed).
[[nodiscard]] std::vector<std::string> validateNewCrop(std::string_view name, const std::vector<Crop>& existing);

// The code of a crop added on the terminal: its normalised name. Two
// terminals adding the same crop offline get the same code, so their
// records meet in one crop once synced.
[[nodiscard]] std::string cropCodeFor(std::string_view name);

// Operator-facing: why the crop cannot be removed (deleted or archived) now.
[[nodiscard]] std::vector<std::string> validateRemoveCrop(std::string_view code, const Journal& journal);

} // namespace wms
