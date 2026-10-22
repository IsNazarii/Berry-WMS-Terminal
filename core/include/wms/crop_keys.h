#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace wms {

// Which crops are keys on the receiving screen; the rest wait in a menu.
// Keys keep their places, so the operator's hand finds a crop where it was:
// a crop used from the menu takes the place of the key used longest ago.
// Longest ago rather than least often: a crop whose season has ended leaves
// the keys at once, not after the next one has caught up on weighings.
class CropKeys {
public:
    // `keys` and `recent` as saved by an earlier run; throws
    // std::invalid_argument for zero keys.
    explicit CropKeys(std::size_t size, std::vector<std::string> keys = {}, std::vector<std::string> recent = {});

    // Drops crops gone from the catalogue and fills free places with the
    // first crops of it that are not keys yet.
    void fill(const std::vector<std::string>& catalogue);
    // The crop was weighed, priced or picked from the menu.
    void use(std::string_view code);

    // Codes in screen order.
    [[nodiscard]] const std::vector<std::string>& keys() const noexcept { return keys_; }
    // Codes used so far, most recent first.
    [[nodiscard]] const std::vector<std::string>& recent() const noexcept { return recent_; }

private:
    std::size_t size_;
    std::vector<std::string> keys_;
    std::vector<std::string> recent_;
};

} // namespace wms
