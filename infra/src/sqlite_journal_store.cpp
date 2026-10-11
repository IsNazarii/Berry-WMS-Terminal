#include "wms/sqlite_journal_store.h"

#include "wms/schema.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wms {

namespace {

using namespace std::string_view_literals;

// The text each enum value is stored as; must match the CHECKs in the schema.
constexpr std::array kSettlements{std::pair{Settlement::OnSite, "on_site"sv},
                                  std::pair{Settlement::HandedOver, "handed_over"sv}};
constexpr std::array kPriceSources{std::pair{PriceSource::DayPrice, "day_price"sv},
                                   std::pair{PriceSource::Manual, "manual"sv}};
constexpr std::array kWeightSources{std::pair{WeightSource::Scale, "scale"sv},
                                    std::pair{WeightSource::Manual, "manual"sv}};
constexpr std::array kPaymentMethods{std::pair{PaymentMethod::Cash, "cash"sv},
                                     std::pair{PaymentMethod::BankTransfer, "bank_transfer"sv}};
constexpr std::array kSyncStatuses{std::pair{SyncStatus::Pending, "pending"sv},
                                   std::pair{SyncStatus::Synced, "synced"sv}};

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

std::int64_t toNanoseconds(TimePoint at) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(at.time_since_epoch()).count();
}

TimePoint fromNanoseconds(std::int64_t ns) {
    return TimePoint{std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds{ns})};
}

Uuid uuidColumn(const sqlite::Statement& row, int column) {
    const std::string text = row.columnText(column);
    const std::optional<Uuid> id = Uuid::parse(text);
    if (!id) throw std::runtime_error("Malformed UUID in database: " + text);
    return *id;
}

// For INSERT ... ON CONFLICT DO NOTHING RETURNING: a returned row means the
// record was new, no row means the id was already there.
bool inserted(sqlite::Statement& insert) {
    const bool row = insert.step();
    while (insert.step()) {
    }
    return row;
}

} // namespace

SqliteJournalStore::SqliteJournalStore(sqlite::Database& db) : db_{db} { migrateSchema(db_); }

bool SqliteJournalStore::save(const Receipt& receipt) {
    sqlite::ScopedTransaction transaction{db_};
    sqlite::Statement insert = db_.prepare(
        "INSERT INTO receipt (id, created_at_ns, supplier_id, settlement, sync_status) VALUES (?1, ?2, ?3, ?4, ?5) "
        "ON CONFLICT (id) DO NOTHING RETURNING id");
    insert.bind(1, receipt.id().toString())
        .bind(2, toNanoseconds(receipt.createdAt()))
        .bind(3, receipt.supplierId())
        .bind(4, toText(kSettlements, receipt.settlement()))
        .bind(5, toText(kSyncStatuses, receipt.syncStatus()));
    // Nothing written yet; the transaction's destructor rolls back the empty BEGIN.
    if (!inserted(insert)) return false;

    sqlite::Statement line = db_.prepare(
        "INSERT INTO weighing (receipt_id, line, crop_code, gross_g, box_count, tare_per_box_g, pallet_tare_g, "
        "price_per_kg_kop, price_source, weight_source, cancelled) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)");
    const std::vector<Weighing>& weighings = receipt.weighings();
    for (std::size_t i = 0; i < weighings.size(); ++i) {
        const Weighing& w = weighings[i];
        line.bind(1, receipt.id().toString())
            .bind(2, static_cast<std::int64_t>(i))
            .bind(3, w.cropCode)
            .bind(4, w.gross.raw())
            .bind(5, std::int64_t{w.boxCount})
            .bind(6, w.tarePerBox.raw())
            .bind(7, w.palletTare.raw())
            .bind(8, w.pricePerKg.raw())
            .bind(9, toText(kPriceSources, w.priceSource))
            .bind(10, toText(kWeightSources, w.weightSource))
            .bind(11, std::int64_t{w.cancelled ? 1 : 0});
        line.step();
        line.reset();
    }
    transaction.commit();
    return true;
}

bool SqliteJournalStore::save(const Payment& payment) {
    sqlite::Statement insert = db_.prepare(
        "INSERT INTO payment (id, created_at_ns, supplier_id, amount_kop, method, sync_status) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6) ON CONFLICT (id) DO NOTHING RETURNING id");
    insert.bind(1, payment.id().toString())
        .bind(2, toNanoseconds(payment.createdAt()))
        .bind(3, payment.supplierId())
        .bind(4, payment.amount().raw())
        .bind(5, toText(kPaymentMethods, payment.method()))
        .bind(6, toText(kSyncStatuses, payment.syncStatus()));
    return inserted(insert);
}

bool SqliteJournalStore::save(const Allocation& allocation) {
    sqlite::Statement insert = db_.prepare(
        "INSERT INTO allocation (id, created_at_ns, receipt_id, truck_id, crop_code, boxes, weight_g, "
        "weighing_index, sync_status) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9) "
        "ON CONFLICT (id) DO NOTHING RETURNING id");
    insert.bind(1, allocation.id().toString())
        .bind(2, toNanoseconds(allocation.createdAt()))
        .bind(3, allocation.receiptId().toString())
        .bind(5, allocation.cropCode())
        .bind(6, std::int64_t{allocation.cargo().boxes})
        .bind(7, allocation.cargo().weight.raw())
        .bind(9, toText(kSyncStatuses, allocation.syncStatus()));
    if (const auto* truck = std::get_if<TruckId>(&allocation.destination())) {
        insert.bind(4, truck->toString());
    } else {
        insert.bind(4, std::nullopt);
    }
    const auto index = allocation.weighingIndex();
    insert.bind(8, index ? std::optional{static_cast<std::int64_t>(*index)} : std::nullopt);
    return inserted(insert);
}

bool SqliteJournalStore::save(const StockShipment& shipment) {
    sqlite::Statement insert = db_.prepare(
        "INSERT INTO stock_shipment (id, created_at_ns, crop_code, boxes, weight_g, truck_id, sync_status) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7) ON CONFLICT (id) DO NOTHING RETURNING id");
    insert.bind(1, shipment.id().toString())
        .bind(2, toNanoseconds(shipment.createdAt()))
        .bind(3, shipment.cropCode())
        .bind(4, std::int64_t{shipment.cargo().boxes})
        .bind(5, shipment.cargo().weight.raw())
        .bind(6, shipment.truckId().toString())
        .bind(7, toText(kSyncStatuses, shipment.syncStatus()));
    return inserted(insert);
}

Journal SqliteJournalStore::load() const {
    Journal journal;

    // rowid breaks ties between records created in the same instant.
    sqlite::Statement receipts = db_.prepare(
        "SELECT id, created_at_ns, supplier_id, settlement, sync_status FROM receipt ORDER BY created_at_ns, rowid");
    sqlite::Statement lines = db_.prepare(
        "SELECT crop_code, gross_g, box_count, tare_per_box_g, pallet_tare_g, price_per_kg_kop, price_source, "
        "weight_source, cancelled FROM weighing WHERE receipt_id = ?1 ORDER BY line");
    while (receipts.step()) {
        std::vector<Weighing> weighings;
        lines.bind(1, receipts.columnText(0));
        while (lines.step()) {
            weighings.push_back({
                .cropCode = lines.columnText(0),
                .gross = grams(lines.columnInt(1)),
                .boxCount = static_cast<int>(lines.columnInt(2)),
                .tarePerBox = grams(lines.columnInt(3)),
                .palletTare = grams(lines.columnInt(4)),
                .pricePerKg = kopecks(lines.columnInt(5)),
                .priceSource = fromText(kPriceSources, lines.columnText(6)),
                .weightSource = fromText(kWeightSources, lines.columnText(7)),
                .cancelled = lines.columnInt(8) != 0,
            });
        }
        lines.reset();
        journal.add(Receipt{uuidColumn(receipts, 0), fromNanoseconds(receipts.columnInt(1)), receipts.columnInt(2),
                            fromText(kSettlements, receipts.columnText(3)), std::move(weighings),
                            fromText(kSyncStatuses, receipts.columnText(4))});
    }

    sqlite::Statement payments = db_.prepare(
        "SELECT id, created_at_ns, supplier_id, amount_kop, method, sync_status FROM payment "
        "ORDER BY created_at_ns, rowid");
    while (payments.step()) {
        journal.add(Payment{uuidColumn(payments, 0), fromNanoseconds(payments.columnInt(1)), payments.columnInt(2),
                            kopecks(payments.columnInt(3)), fromText(kPaymentMethods, payments.columnText(4)),
                            fromText(kSyncStatuses, payments.columnText(5))});
    }

    sqlite::Statement allocations = db_.prepare(
        "SELECT id, created_at_ns, receipt_id, truck_id, crop_code, boxes, weight_g, weighing_index, sync_status "
        "FROM allocation ORDER BY created_at_ns, rowid");
    while (allocations.step()) {
        const Destination destination =
            allocations.isNull(3) ? Destination{ToWarehouse{}} : Destination{uuidColumn(allocations, 3)};
        const std::optional<std::size_t> index =
            allocations.isNull(7) ? std::nullopt
                                  : std::optional{static_cast<std::size_t>(allocations.columnInt(7))};
        journal.add(Allocation{uuidColumn(allocations, 0), fromNanoseconds(allocations.columnInt(1)),
                               uuidColumn(allocations, 2), destination, allocations.columnText(4),
                               {.boxes = static_cast<int>(allocations.columnInt(5)),
                                .weight = grams(allocations.columnInt(6))},
                               index, fromText(kSyncStatuses, allocations.columnText(8))});
    }

    sqlite::Statement shipments = db_.prepare(
        "SELECT id, created_at_ns, crop_code, boxes, weight_g, truck_id, sync_status FROM stock_shipment "
        "ORDER BY created_at_ns, rowid");
    while (shipments.step()) {
        journal.add(StockShipment{uuidColumn(shipments, 0), fromNanoseconds(shipments.columnInt(1)),
                                  shipments.columnText(2),
                                  {.boxes = static_cast<int>(shipments.columnInt(3)),
                                   .weight = grams(shipments.columnInt(4))},
                                  uuidColumn(shipments, 5), fromText(kSyncStatuses, shipments.columnText(6))});
    }
    return journal;
}

} // namespace wms
