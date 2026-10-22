#include "wms/schema.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace wms {

namespace {

// Each step is applied once and never edited afterwards: terminals in the
// field already have it. Changes go into a new step at the end.
//
// Conventions: ids are UUID text (readable in any SQLite browser); time is
// nanoseconds since the Unix epoch, so createdAt comes back exactly; money is
// in kopecks (_kop), weight in grams (_g); enums are spelled out as text, so
// reordering an enum in C++ cannot change what old rows mean. STRICT tables
// reject a value of the wrong type instead of quietly storing it.
constexpr std::array<std::string_view, 8> kMigrations{
    R"sql(
    CREATE TABLE receipt (
        id            TEXT PRIMARY KEY,
        created_at_ns INTEGER NOT NULL,
        supplier_id   INTEGER NOT NULL,
        settlement    TEXT NOT NULL CHECK (settlement IN ('on_site', 'handed_over')),
        sync_status   TEXT NOT NULL CHECK (sync_status IN ('pending', 'synced'))
    ) STRICT;

    -- Lines keep their position: allocations point at a weighing by index.
    CREATE TABLE weighing (
        receipt_id       TEXT NOT NULL REFERENCES receipt (id),
        line             INTEGER NOT NULL,
        crop_code        TEXT NOT NULL,
        gross_g          INTEGER NOT NULL,
        box_count        INTEGER NOT NULL,
        tare_per_box_g   INTEGER NOT NULL,
        pallet_tare_g    INTEGER NOT NULL,
        price_per_kg_kop INTEGER NOT NULL,
        price_source     TEXT NOT NULL CHECK (price_source IN ('day_price', 'manual')),
        weight_source    TEXT NOT NULL CHECK (weight_source IN ('scale', 'manual')),
        cancelled        INTEGER NOT NULL CHECK (cancelled IN (0, 1)),
        PRIMARY KEY (receipt_id, line)
    ) STRICT;

    CREATE TABLE payment (
        id            TEXT PRIMARY KEY,
        created_at_ns INTEGER NOT NULL,
        supplier_id   INTEGER NOT NULL,
        amount_kop    INTEGER NOT NULL,
        method        TEXT NOT NULL CHECK (method IN ('cash', 'bank_transfer')),
        sync_status   TEXT NOT NULL CHECK (sync_status IN ('pending', 'synced'))
    ) STRICT;

    -- No foreign key to receipt: pallets of a receipt still open go into a
    -- truck before the receipt itself is written. truck_id NULL = warehouse.
    CREATE TABLE allocation (
        id             TEXT PRIMARY KEY,
        created_at_ns  INTEGER NOT NULL,
        receipt_id     TEXT NOT NULL,
        truck_id       TEXT,
        crop_code      TEXT NOT NULL,
        boxes          INTEGER NOT NULL,
        weight_g       INTEGER NOT NULL,
        weighing_index INTEGER,
        sync_status    TEXT NOT NULL CHECK (sync_status IN ('pending', 'synced'))
    ) STRICT;

    CREATE TABLE stock_shipment (
        id            TEXT PRIMARY KEY,
        created_at_ns INTEGER NOT NULL,
        crop_code     TEXT NOT NULL,
        boxes         INTEGER NOT NULL,
        weight_g      INTEGER NOT NULL,
        truck_id      TEXT NOT NULL,
        sync_status   TEXT NOT NULL CHECK (sync_status IN ('pending', 'synced'))
    ) STRICT;
    )sql",

    // Sessions, open and archived. Unlike the journal these rows are
    // rewritten as the session changes; `seq` keeps the order of each list.
    R"sql(
    CREATE TABLE session (
        id           TEXT PRIMARY KEY,
        date         TEXT NOT NULL CHECK (date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),
        opened_at_ns INTEGER NOT NULL,
        opened_by    INTEGER NOT NULL,
        closed_at_ns INTEGER,
        closed_by    INTEGER,
        CHECK ((closed_at_ns IS NULL) = (closed_by IS NULL))
    ) STRICT;

    CREATE TABLE price_change (
        session_id       TEXT NOT NULL REFERENCES session (id),
        seq              INTEGER NOT NULL,
        crop_code        TEXT NOT NULL,
        price_per_kg_kop INTEGER NOT NULL,
        at_ns            INTEGER NOT NULL,
        PRIMARY KEY (session_id, seq)
    ) STRICT;

    -- A carried-over truck is in two sessions under the same id.
    CREATE TABLE truck (
        session_id  TEXT NOT NULL REFERENCES session (id),
        seq         INTEGER NOT NULL,
        id          TEXT NOT NULL,
        plate       TEXT NOT NULL,
        make        TEXT NOT NULL,
        note        TEXT NOT NULL,
        limit_boxes INTEGER NOT NULL,
        limit_g     INTEGER NOT NULL,
        status      TEXT NOT NULL CHECK (status IN ('loading', 'departed', 'carried_over')),
        PRIMARY KEY (session_id, id)
    ) STRICT;

    CREATE TABLE amendment (
        session_id TEXT NOT NULL REFERENCES session (id),
        seq        INTEGER NOT NULL,
        by_operator INTEGER NOT NULL,
        at_ns      INTEGER NOT NULL,
        reason     TEXT NOT NULL,
        change     TEXT NOT NULL,
        PRIMARY KEY (session_id, seq)
    ) STRICT;

    CREATE TABLE open_receipt (
        id           TEXT PRIMARY KEY,
        session_id   TEXT NOT NULL REFERENCES session (id),
        seq          INTEGER NOT NULL,
        opened_at_ns INTEGER NOT NULL,
        supplier_id  INTEGER NOT NULL
    ) STRICT;

    -- Same columns as weighing: a closed receipt's lines move there as they are.
    CREATE TABLE open_weighing (
        receipt_id       TEXT NOT NULL REFERENCES open_receipt (id) ON DELETE CASCADE,
        line             INTEGER NOT NULL,
        crop_code        TEXT NOT NULL,
        gross_g          INTEGER NOT NULL,
        box_count        INTEGER NOT NULL,
        tare_per_box_g   INTEGER NOT NULL,
        pallet_tare_g    INTEGER NOT NULL,
        price_per_kg_kop INTEGER NOT NULL,
        price_source     TEXT NOT NULL CHECK (price_source IN ('day_price', 'manual')),
        weight_source    TEXT NOT NULL CHECK (weight_source IN ('scale', 'manual')),
        cancelled        INTEGER NOT NULL CHECK (cancelled IN (0, 1)),
        PRIMARY KEY (receipt_id, line)
    ) STRICT;
    )sql",

    // Reference data the server owns; ids are the server's numbers, so no
    // AUTOINCREMENT. The PIN is plain text on purpose (see Operator).
    R"sql(
    CREATE TABLE supplier (
        id        INTEGER PRIMARY KEY CHECK (id > 0),
        full_name TEXT NOT NULL CHECK (full_name <> ''),
        phone     TEXT NOT NULL
    ) STRICT;

    CREATE TABLE operator (
        id   INTEGER PRIMARY KEY CHECK (id > 0),
        name TEXT NOT NULL CHECK (name <> ''),
        pin  TEXT NOT NULL CHECK (length(pin) BETWEEN 4 AND 6)
    ) STRICT;
    )sql",

    // Payments name the operator who paid. SQLite cannot add a NOT NULL
    // column without a default, and a default would quietly fill in anyone
    // a future INSERT forgets, so the table is rebuilt instead. Rows from
    // before this step exist only in alpha test databases, where operator 1
    // seeded them; they are put down to operator 1.
    R"sql(
    CREATE TABLE payment_new (
        id            TEXT PRIMARY KEY,
        created_at_ns INTEGER NOT NULL,
        supplier_id   INTEGER NOT NULL,
        amount_kop    INTEGER NOT NULL,
        method        TEXT NOT NULL CHECK (method IN ('cash', 'bank_transfer')),
        paid_by       INTEGER NOT NULL CHECK (paid_by > 0),
        sync_status   TEXT NOT NULL CHECK (sync_status IN ('pending', 'synced'))
    ) STRICT;
    INSERT INTO payment_new (id, created_at_ns, supplier_id, amount_kop, method, paid_by, sync_status)
        SELECT id, created_at_ns, supplier_id, amount_kop, method, 1, sync_status FROM payment ORDER BY rowid;
    DROP TABLE payment;
    ALTER TABLE payment_new RENAME TO payment;
    )sql",

    // Crops the operator adds during the season. Not WITHOUT ROWID: the
    // rowid keeps the order they were added in.
    R"sql(
    CREATE TABLE crop (
        code TEXT PRIMARY KEY CHECK (code <> ''),
        name TEXT NOT NULL CHECK (name <> '')
    ) STRICT;
    )sql",

    // A crop with records is archived instead of deleted. Unlike paid_by,
    // the default is the right value for every row, past and future.
    R"sql(
    ALTER TABLE crop ADD COLUMN archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1));
    )sql",

    // A truck is limited in boxes only. Rebuilt rather than ALTER ... DROP
    // COLUMN to add the CHECK. Weight-only trucks exist only in alpha test
    // databases; they get a rough 2 kg per box, which the operator sees in
    // the session table.
    R"sql(
    CREATE TABLE truck_new (
        session_id  TEXT NOT NULL REFERENCES session (id),
        seq         INTEGER NOT NULL,
        id          TEXT NOT NULL,
        plate       TEXT NOT NULL,
        make        TEXT NOT NULL,
        note        TEXT NOT NULL,
        limit_boxes INTEGER NOT NULL CHECK (limit_boxes > 0),
        status      TEXT NOT NULL CHECK (status IN ('loading', 'departed', 'carried_over')),
        PRIMARY KEY (session_id, id)
    ) STRICT;
    INSERT INTO truck_new (session_id, seq, id, plate, make, note, limit_boxes, status)
        SELECT session_id, seq, id, plate, make, note,
               CASE WHEN limit_boxes > 0 THEN limit_boxes ELSE max(1, limit_g / 2000) END, status
        FROM truck;
    DROP TABLE truck;
    ALTER TABLE truck_new RENAME TO truck;
    )sql",

    // No foreign key to allocation, as allocation has none to receipt: sync
    // may bring a reversal before the allocation it takes back.
    R"sql(
    CREATE TABLE allocation_reversal (
        id            TEXT PRIMARY KEY,
        created_at_ns INTEGER NOT NULL,
        allocation_id TEXT NOT NULL,
        sync_status   TEXT NOT NULL CHECK (sync_status IN ('pending', 'synced'))
    ) STRICT;
    )sql",
};

std::int64_t userVersion(sqlite::Database& db) {
    sqlite::Statement statement = db.prepare("PRAGMA user_version");
    statement.step();
    return statement.columnInt(0);
}

} // namespace

void migrateSchema(sqlite::Database& db) {
    const auto latest = static_cast<std::int64_t>(kMigrations.size());
    const std::int64_t version = userVersion(db);
    if (version > latest) {
        throw std::runtime_error("Database schema version " + std::to_string(version) +
                                 " is newer than this program knows (" + std::to_string(latest) + ")");
    }
    for (std::int64_t step = version; step < latest; ++step) {
        // The step and its version number commit together, so a crash between
        // them cannot make the next start apply the step twice.
        sqlite::ScopedTransaction transaction{db};
        db.execute(kMigrations[static_cast<std::size_t>(step)]);
        db.execute("PRAGMA user_version = " + std::to_string(step + 1));
        transaction.commit();
    }
}

} // namespace wms
