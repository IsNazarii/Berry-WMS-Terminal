#pragma once

// How core values become columns and back; shared by the SQLite stores.
// Internal to infra: the stores' headers do not include it.

#include "wms/payment.h"
#include "wms/receipt.h"
#include "wms/sqlite.h"
#include "wms/transaction.h"
#include "wms/truck.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace wms::rows {

using namespace std::string_view_literals;

// The text each enum value is stored as; must match the CHECKs in the schema.
inline constexpr std::array kSettlements{std::pair{Settlement::OnSite, "on_site"sv},
                                         std::pair{Settlement::HandedOver, "handed_over"sv}};
inline constexpr std::array kPriceSources{std::pair{PriceSource::DayPrice, "day_price"sv},
                                          std::pair{PriceSource::Manual, "manual"sv}};
inline constexpr std::array kWeightSources{std::pair{WeightSource::Scale, "scale"sv},
                                           std::pair{WeightSource::Manual, "manual"sv}};
inline constexpr std::array kPaymentMethods{std::pair{PaymentMethod::Cash, "cash"sv},
                                            std::pair{PaymentMethod::BankTransfer, "bank_transfer"sv}};
inline constexpr std::array kSyncStatuses{std::pair{SyncStatus::Pending, "pending"sv},
                                          std::pair{SyncStatus::Synced, "synced"sv}};
inline constexpr std::array kTruckStatuses{std::pair{TruckStatus::Loading, "loading"sv},
                                           std::pair{TruckStatus::Departed, "departed"sv},
                                           std::pair{TruckStatus::CarriedOver, "carried_over"sv}};

template <class E, std::size_t N>
std::string_view toText(const std::array<std::pair<E, std::string_view>, N>& names, E value) {
    for (const auto& [v, text] : names) {
        if (v == value) return text;
    }
    throw std::logic_error("Enum value has no stored name");
}

template <class E, std::size_t N>
E fromText(const std::array<std::pair<E, std::string_view>, N>& names, const std::string& text) {
    for (const auto& [value, t] : names) {
        if (t == text) return value;
    }
    throw std::runtime_error("Unknown value in database: " + text);
}

// Nanoseconds since the Unix epoch: exact for the system clock on Linux and Windows alike.
std::int64_t toNanoseconds(TimePoint at);
TimePoint fromNanoseconds(std::int64_t ns);
std::optional<std::int64_t> toNanoseconds(std::optional<TimePoint> at);
std::optional<TimePoint> optionalTime(const sqlite::Statement& row, int column);

// Throws std::runtime_error for text that is not a UUID.
Uuid uuidColumn(const sqlite::Statement& row, int column);

// A weighing takes 9 consecutive parameters / columns, starting at `first`,
// in the order of the weighing tables: crop_code, gross_g, box_count,
// tare_per_box_g, pallet_tare_g, price_per_kg_kop, price_source,
// weight_source, cancelled.
inline constexpr int kWeighingColumns = 9;
void bindWeighing(sqlite::Statement& statement, int first, const Weighing& weighing);
Weighing readWeighing(const sqlite::Statement& row, int first);

// For INSERT ... ON CONFLICT DO NOTHING RETURNING: a returned row means the
// record was new, no row means the id was already there.
bool inserted(sqlite::Statement& insert);

} // namespace wms::rows
