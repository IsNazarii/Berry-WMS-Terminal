#pragma once

#include "wms/cargo.h"
#include "wms/receipt.h"
#include "wms/supplier.h"
#include "wms/transaction.h"
#include "wms/uuid.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace wms {

// A receipt still being weighed. One operator keeps several open at once: two
// big suppliers unloading in turns and a retail one with a couple of boxes in
// between. Unlike Receipt it changes, so it is not a journal record; closing
// turns it into an immutable Receipt.
class OpenReceipt {
public:
    // The id is final from the start: pallets may go into a truck before the
    // receipt is closed, and their allocations point at this id.
    // Throws std::invalid_argument for a nil id or no supplier.
    OpenReceipt(Uuid id, TimePoint openedAt, SupplierId supplierId, std::vector<Weighing> weighings);

    [[nodiscard]] static OpenReceipt open(SupplierId supplierId);

    [[nodiscard]] const Uuid& id() const noexcept { return id_; }
    [[nodiscard]] TimePoint openedAt() const noexcept { return openedAt_; }
    [[nodiscard]] SupplierId supplierId() const noexcept { return supplierId_; }
    [[nodiscard]] const std::vector<Weighing>& weighings() const noexcept { return weighings_; }
    // Without cancelled lines.
    [[nodiscard]] Cargo total() const noexcept;
    [[nodiscard]] Cargo cargoOf(std::string_view cropCode) const noexcept;

    // Returns the index of the new line. Throws std::invalid_argument when
    // Weighing::validate() is not empty: the UI shows those errors first.
    std::size_t add(Weighing weighing);
    // Throws std::out_of_range for an index outside the receipt.
    void cancel(std::size_t index);

    // Operator-facing: why it cannot be closed with this settlement yet.
    [[nodiscard]] std::vector<std::string> validateClose(Settlement settlement) const;
    // Same id and time as when it was opened: the receipt is that visit.
    // Throws std::logic_error when validateClose() is not empty.
    [[nodiscard]] Receipt close(Settlement settlement) const;

private:
    [[nodiscard]] Receipt toReceipt(Settlement settlement) const;

    Uuid id_;
    TimePoint openedAt_;
    SupplierId supplierId_;
    std::vector<Weighing> weighings_;
};

} // namespace wms
