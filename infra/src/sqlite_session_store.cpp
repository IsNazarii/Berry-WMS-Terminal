#include "wms/sqlite_session_store.h"

#include "rows.h"
#include "wms/schema.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wms {

using namespace rows;

namespace {

// ISO "2026-10-11": readable, and sorts like the dates themselves.
std::string dateText(std::chrono::year_month_day date) {
    return std::format("{:04}-{:02}-{:02}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
                       static_cast<unsigned>(date.day()));
}

std::chrono::year_month_day parseDate(const std::string& text) {
    // The schema's CHECK guarantees the shape; std::stoi still guards the digits.
    if (text.size() != 10) throw std::runtime_error("Malformed date in database: " + text);
    return std::chrono::year{std::stoi(text.substr(0, 4))} /
           std::chrono::month{static_cast<unsigned>(std::stoi(text.substr(5, 2)))} /
           std::chrono::day{static_cast<unsigned>(std::stoi(text.substr(8, 2)))};
}

} // namespace

SqliteSessionStore::SqliteSessionStore(sqlite::Database& db) : db_{db} { migrateSchema(db_); }

void SqliteSessionStore::save(const WorkDay& day) {
    const std::string id = day.id().toString();
    sqlite::ScopedTransaction transaction{db_};

    // Date and opening never change; only closing (or an amendment's effects) does.
    db_.prepare("INSERT INTO session (id, date, opened_at_ns, opened_by, closed_at_ns, closed_by) "
                "VALUES (?1, ?2, ?3, ?4, ?5, ?6) "
                "ON CONFLICT (id) DO UPDATE SET closed_at_ns = excluded.closed_at_ns, closed_by = excluded.closed_by")
        .bind(1, id)
        .bind(2, dateText(day.date()))
        .bind(3, toNanoseconds(day.openedAt()))
        .bind(4, day.openedBy())
        .bind(5, toNanoseconds(day.closedAt()))
        .bind(6, day.closedBy())
        .step();

    // The lists hold tens of rows: writing them anew is simpler and safer than
    // working out what changed. Lines of open receipts go by ON DELETE CASCADE.
    for (const char* table : {"price_change", "truck", "amendment", "open_receipt"}) {
        db_.prepare(std::string{"DELETE FROM "} + table + " WHERE session_id = ?1").bind(1, id).step();
    }

    sqlite::Statement price = db_.prepare(
        "INSERT INTO price_change (session_id, seq, crop_code, price_per_kg_kop, at_ns) VALUES (?1, ?2, ?3, ?4, ?5)");
    const auto& history = day.prices().history();
    for (std::size_t i = 0; i < history.size(); ++i) {
        price.bind(1, id)
            .bind(2, static_cast<std::int64_t>(i))
            .bind(3, history[i].cropCode)
            .bind(4, history[i].pricePerKg.raw())
            .bind(5, toNanoseconds(history[i].at))
            .step();
        price.reset();
    }

    sqlite::Statement truck = db_.prepare(
        "INSERT INTO truck (session_id, seq, id, plate, make, note, limit_boxes, limit_g, status) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)");
    const auto& trucks = day.trucks();
    for (std::size_t i = 0; i < trucks.size(); ++i) {
        const Truck& t = trucks[i];
        truck.bind(1, id)
            .bind(2, static_cast<std::int64_t>(i))
            .bind(3, t.id().toString())
            .bind(4, t.plate())
            .bind(5, t.details().make)
            .bind(6, t.details().note)
            .bind(7, std::int64_t{t.capacity().boxes})
            .bind(8, t.capacity().weight.raw())
            .bind(9, toText(kTruckStatuses, t.status()))
            .step();
        truck.reset();
    }

    sqlite::Statement amendment = db_.prepare(
        "INSERT INTO amendment (session_id, seq, by_operator, at_ns, reason, change) VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    const auto& amendments = day.amendments();
    for (std::size_t i = 0; i < amendments.size(); ++i) {
        const AmendmentRecord& a = amendments[i];
        amendment.bind(1, id)
            .bind(2, static_cast<std::int64_t>(i))
            .bind(3, a.amendment.by)
            .bind(4, toNanoseconds(a.amendment.at))
            .bind(5, a.amendment.reason)
            .bind(6, a.change)
            .step();
        amendment.reset();
    }

    sqlite::Statement receipt = db_.prepare(
        "INSERT INTO open_receipt (id, session_id, seq, opened_at_ns, supplier_id) VALUES (?1, ?2, ?3, ?4, ?5)");
    sqlite::Statement line = db_.prepare(
        "INSERT INTO open_weighing (receipt_id, line, crop_code, gross_g, box_count, tare_per_box_g, pallet_tare_g, "
        "price_per_kg_kop, price_source, weight_source, cancelled) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)");
    const auto& receipts = day.openReceipts();
    for (std::size_t i = 0; i < receipts.size(); ++i) {
        const OpenReceipt& r = receipts[i];
        const std::string receiptId = r.id().toString();
        receipt.bind(1, receiptId)
            .bind(2, id)
            .bind(3, static_cast<std::int64_t>(i))
            .bind(4, toNanoseconds(r.openedAt()))
            .bind(5, r.supplierId())
            .step();
        receipt.reset();
        for (std::size_t n = 0; n < r.weighings().size(); ++n) {
            line.bind(1, receiptId).bind(2, static_cast<std::int64_t>(n));
            bindWeighing(line, 3, r.weighings()[n]);
            line.step();
            line.reset();
        }
    }

    transaction.commit();
}

std::optional<WorkDay> SqliteSessionStore::latest() const {
    sqlite::Statement session =
        db_.prepare("SELECT id, date, opened_at_ns, opened_by, closed_at_ns, closed_by FROM session "
                    "ORDER BY opened_at_ns DESC, rowid DESC LIMIT 1");
    if (!session.step()) return std::nullopt;

    const std::string id = session.columnText(0);
    WorkDayRecord record{
        .id = uuidColumn(session, 0),
        .date = parseDate(session.columnText(1)),
        .openedAt = fromNanoseconds(session.columnInt(2)),
        .openedBy = session.columnInt(3),
        .closedAt = optionalTime(session, 4),
        .closedBy = session.isNull(5) ? std::nullopt : std::optional{session.columnInt(5)},
        // Filled from their own tables below.
        .prices = {},
        .trucks = {},
        .amendments = {},
        .openReceipts = {},
    };

    sqlite::Statement prices = db_.prepare(
        "SELECT crop_code, price_per_kg_kop, at_ns FROM price_change WHERE session_id = ?1 ORDER BY seq");
    prices.bind(1, id);
    while (prices.step()) {
        record.prices.set(prices.columnText(0), kopecks(prices.columnInt(1)), fromNanoseconds(prices.columnInt(2)));
    }

    sqlite::Statement trucks = db_.prepare(
        "SELECT id, plate, make, note, limit_boxes, limit_g, status FROM truck WHERE session_id = ?1 ORDER BY seq");
    trucks.bind(1, id);
    while (trucks.step()) {
        record.trucks.emplace_back(
            uuidColumn(trucks, 0), trucks.columnText(1),
            Cargo{.boxes = static_cast<int>(trucks.columnInt(4)), .weight = grams(trucks.columnInt(5))},
            TruckDetails{.make = trucks.columnText(2), .note = trucks.columnText(3)},
            fromText(kTruckStatuses, trucks.columnText(6)));
    }

    sqlite::Statement amendments = db_.prepare(
        "SELECT by_operator, at_ns, reason, change FROM amendment WHERE session_id = ?1 ORDER BY seq");
    amendments.bind(1, id);
    while (amendments.step()) {
        record.amendments.push_back({
            .amendment = {.by = amendments.columnInt(0),
                          .at = fromNanoseconds(amendments.columnInt(1)),
                          .reason = amendments.columnText(2)},
            .change = amendments.columnText(3),
        });
    }

    sqlite::Statement receipts = db_.prepare(
        "SELECT id, opened_at_ns, supplier_id FROM open_receipt WHERE session_id = ?1 ORDER BY seq");
    sqlite::Statement lines = db_.prepare(
        "SELECT crop_code, gross_g, box_count, tare_per_box_g, pallet_tare_g, price_per_kg_kop, price_source, "
        "weight_source, cancelled FROM open_weighing WHERE receipt_id = ?1 ORDER BY line");
    receipts.bind(1, id);
    while (receipts.step()) {
        std::vector<Weighing> weighings;
        lines.bind(1, receipts.columnText(0));
        while (lines.step()) weighings.push_back(readWeighing(lines, 0));
        lines.reset();
        record.openReceipts.emplace_back(uuidColumn(receipts, 0), fromNanoseconds(receipts.columnInt(1)),
                                         receipts.columnInt(2), std::move(weighings));
    }

    return WorkDay::restore(std::move(record));
}

} // namespace wms
