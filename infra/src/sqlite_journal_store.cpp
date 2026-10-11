#include "wms/sqlite_journal_store.h"

#include "rows.h"
#include "wms/schema.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace wms {

using namespace rows;

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
        line.bind(1, receipt.id().toString()).bind(2, static_cast<std::int64_t>(i));
        bindWeighing(line, 3, weighings[i]);
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
        while (lines.step()) weighings.push_back(readWeighing(lines, 0));
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
