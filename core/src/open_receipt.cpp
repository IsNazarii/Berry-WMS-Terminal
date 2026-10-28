#include "wms/open_receipt.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace wms {

OpenReceipt::OpenReceipt(Uuid id, TimePoint openedAt, SupplierId supplierId, std::vector<Weighing> weighings,
                         std::optional<Uuid> sessionId)
    : id_{id}, openedAt_{openedAt}, supplierId_{supplierId}, weighings_{std::move(weighings)}, sessionId_{sessionId} {
    if (id_.isNil()) throw std::invalid_argument("Open receipt id must not be nil");
    if (supplierId_ <= 0) throw std::invalid_argument("Open receipt needs a supplier");
}

OpenReceipt OpenReceipt::open(SupplierId supplierId, std::optional<Uuid> sessionId) {
    return OpenReceipt{Uuid::generateV7(), Clock::now(), supplierId, {}, sessionId};
}

Cargo OpenReceipt::total() const noexcept {
    Cargo sum;
    for (const auto& w : weighings_) {
        if (!w.cancelled) sum += w.cargo();
    }
    return sum;
}

Cargo OpenReceipt::cargoOf(std::string_view cropCode) const noexcept {
    Cargo sum;
    for (const auto& w : weighings_) {
        if (!w.cancelled && w.cropCode == cropCode) sum += w.cargo();
    }
    return sum;
}

std::vector<std::string> OpenReceipt::crops() const {
    std::vector<std::string> codes;
    for (const auto& w : weighings_) {
        if (!w.cancelled && std::ranges::find(codes, w.cropCode) == codes.end()) codes.push_back(w.cropCode);
    }
    return codes;
}

std::size_t OpenReceipt::add(Weighing weighing) {
    if (!weighing.validate().empty()) throw std::invalid_argument("Weighing is not valid");
    weighings_.push_back(std::move(weighing));
    return weighings_.size() - 1;
}

void OpenReceipt::cancel(std::size_t index) { weighings_.at(index).cancelled = true; }

void OpenReceipt::setPrice(std::string_view cropCode, Money pricePerKg, PriceSource source) noexcept {
    for (auto& w : weighings_) {
        if (w.cancelled || w.cropCode != cropCode) continue;
        w.pricePerKg = pricePerKg;
        w.priceSource = source;
    }
}

std::vector<std::string> OpenReceipt::validateClose(Settlement settlement) const {
    return toReceipt(settlement).validate();
}

Receipt OpenReceipt::close(Settlement settlement) const {
    Receipt receipt = toReceipt(settlement);
    if (!receipt.isValid()) throw std::logic_error("Open receipt cannot be closed yet");
    return receipt;
}

Receipt OpenReceipt::toReceipt(Settlement settlement) const {
    std::vector<Weighing> lines = weighings_;
    // Weighings took the day price, but a wholesale receipt is priced outside the terminal.
    if (settlement == Settlement::HandedOver) {
        for (auto& w : lines) {
            w.pricePerKg = Money{};
            w.priceSource = PriceSource::DayPrice;
        }
    }
    return Receipt{id_, openedAt_, supplierId_, settlement, std::move(lines), SyncStatus::Pending, sessionId_};
}

} // namespace wms
