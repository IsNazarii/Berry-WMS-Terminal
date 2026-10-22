#pragma once

#include "wms/crop.h"
#include "wms/operator.h"
#include "wms/repository.h"
#include "wms/sqlite.h"
#include "wms/supplier.h"

#include <vector>

namespace wms {

// Reference data in SQLite. The SQL is the same for every kind of record
// except for the table and its columns, which come from a per-type mapping
// in the .cpp; the template exists for Supplier, Operator and Crop only
// (explicit instantiation there), so another type fails at link time, not at run time.
template <class T>
class SqliteRepository final : public Repository<T> {
public:
    // Brings the schema up to date, like the other stores; all may share one
    // database. The database must outlive the repository.
    explicit SqliteRepository(sqlite::Database& db);

    void save(const T& record) override;
    void remove(const T& record) override;
    [[nodiscard]] std::vector<T> all() const override;

private:
    sqlite::Database& db_;
};

extern template class SqliteRepository<Supplier>;
extern template class SqliteRepository<Operator>;
extern template class SqliteRepository<Crop>;

} // namespace wms
