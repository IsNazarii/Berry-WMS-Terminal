#include "wms/sqlite_repository.h"

#include "wms/schema.h"

#include <cstdint>
#include <string_view>

namespace wms {
namespace {

// How one kind of record maps to its table. Each specialisation gives the
// upsert and the select, with columns in the same order as bind() / read().
template <class T>
struct Table;

template <>
struct Table<Supplier> {
    static constexpr std::string_view upsert =
        "INSERT INTO supplier (id, full_name, phone) VALUES (?1, ?2, ?3) "
        "ON CONFLICT (id) DO UPDATE SET full_name = excluded.full_name, phone = excluded.phone";
    static constexpr std::string_view selectAll = "SELECT id, full_name, phone FROM supplier ORDER BY id";
    static constexpr std::string_view deleteOne = "DELETE FROM supplier WHERE id = ?1";
    static void bindKey(sqlite::Statement& statement, const Supplier& supplier) { statement.bind(1, supplier.id()); }

    static void bind(sqlite::Statement& statement, const Supplier& supplier) {
        statement.bind(1, supplier.id()).bind(2, supplier.fullName()).bind(3, supplier.phone());
    }
    static Supplier read(const sqlite::Statement& row) {
        return {row.columnInt(0), row.columnText(1), row.columnText(2)};
    }
};

template <>
struct Table<Operator> {
    static constexpr std::string_view upsert =
        "INSERT INTO operator (id, name, pin) VALUES (?1, ?2, ?3) "
        "ON CONFLICT (id) DO UPDATE SET name = excluded.name, pin = excluded.pin";
    static constexpr std::string_view selectAll = "SELECT id, name, pin FROM operator ORDER BY id";
    static constexpr std::string_view deleteOne = "DELETE FROM operator WHERE id = ?1";
    static void bindKey(sqlite::Statement& statement, const Operator& op) { statement.bind(1, op.id()); }

    static void bind(sqlite::Statement& statement, const Operator& op) {
        statement.bind(1, op.id()).bind(2, op.name()).bind(3, op.pinForStorage());
    }
    static Operator read(const sqlite::Statement& row) {
        return {row.columnInt(0), row.columnText(1), row.columnText(2)};
    }
};

// The code is the key: the journal refers to crops by it. Upsert keeps the
// rowid, so a renamed crop stays in its place in the list.
template <>
struct Table<Crop> {
    static constexpr std::string_view upsert =
        "INSERT INTO crop (code, name, archived) VALUES (?1, ?2, ?3) "
        "ON CONFLICT (code) DO UPDATE SET name = excluded.name, archived = excluded.archived";
    static constexpr std::string_view selectAll = "SELECT code, name, archived FROM crop ORDER BY rowid";
    static constexpr std::string_view deleteOne = "DELETE FROM crop WHERE code = ?1";

    static void bind(sqlite::Statement& statement, const Crop& crop) {
        statement.bind(1, crop.code()).bind(2, crop.name()).bind(3, std::int64_t{crop.archived()});
    }
    static void bindKey(sqlite::Statement& statement, const Crop& crop) { statement.bind(1, crop.code()); }
    static Crop read(const sqlite::Statement& row) {
        return {row.columnText(0), row.columnText(1), row.columnInt(2) != 0};
    }
};

} // namespace

template <class T>
SqliteRepository<T>::SqliteRepository(sqlite::Database& db) : db_{db} {
    migrateSchema(db_);
}

template <class T>
void SqliteRepository<T>::save(const T& record) {
    sqlite::Statement upsert = db_.prepare(Table<T>::upsert);
    Table<T>::bind(upsert, record);
    upsert.step();
}

template <class T>
void SqliteRepository<T>::remove(const T& record) {
    sqlite::Statement remove = db_.prepare(Table<T>::deleteOne);
    Table<T>::bindKey(remove, record);
    remove.step();
}

template <class T>
std::vector<T> SqliteRepository<T>::all() const {
    sqlite::Statement select = db_.prepare(Table<T>::selectAll);
    std::vector<T> records;
    // The constructors check the rows again, so a broken row throws instead of loading.
    while (select.step()) records.push_back(Table<T>::read(select));
    return records;
}

template class SqliteRepository<Supplier>;
template class SqliteRepository<Operator>;
template class SqliteRepository<Crop>;

} // namespace wms
