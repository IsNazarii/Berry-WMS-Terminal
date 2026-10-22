#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace wms {

// A crop the point buys. The code is what the journal stores ("raspberry");
// the name is what the operator sees. Crops change during the season, so
// they are reference data in the database, not a list in the program.
class Crop {
public:
    Crop(std::string code, std::string name);

    [[nodiscard]] const std::string& code() const noexcept { return code_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    std::string code_;
    std::string name_;
};

// A crop the operator adds on the terminal. A name already taken (ignoring
// case and spaces: "Малина" = "малина ") is refused, so one crop never splits
// into two stock rows.
[[nodiscard]] std::vector<std::string> validateNewCrop(std::string_view name, const std::vector<Crop>& existing);

// The code of a crop added on the terminal: its normalised name. Two
// terminals adding the same crop offline get the same code, so their
// records meet in one crop once synced.
[[nodiscard]] std::string cropCodeFor(std::string_view name);

} // namespace wms
