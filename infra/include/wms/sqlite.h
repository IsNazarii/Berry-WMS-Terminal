#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

// Thin RAII layer over the sqlite3 C API: handles are closed by destructors and
// every error code becomes an exception, so the stores above it never check codes.
// Only integers, text and NULL are bound: money and weight are integers, and
// leaving out floating point keeps it from sneaking into the schema.
namespace wms::sqlite {

class Error : public std::runtime_error {
public:
    Error(int code, const std::string& message) : std::runtime_error{message}, code_{code} {}

    // The primary sqlite3 result code, e.g. SQLITE_CONSTRAINT.
    [[nodiscard]] int code() const noexcept { return code_; }

private:
    int code_;
};

// A compiled SQL statement. Parameters are numbered from 1, columns from 0,
// as in sqlite3 itself.
class Statement {
public:
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    ~Statement();

    Statement& bind(int index, std::int64_t value);
    Statement& bind(int index, std::string_view text);
    Statement& bind(int index, std::nullopt_t);
    Statement& bind(int index, std::optional<std::int64_t> value);

    // Runs to the next row: true when a row is ready to be read, false when done.
    // Throws Error on a failure, e.g. a violated constraint.
    bool step();
    // Ready to run again; bound values are cleared.
    void reset();

    [[nodiscard]] bool isNull(int column) const;
    [[nodiscard]] std::int64_t columnInt(int column) const;
    [[nodiscard]] std::string columnText(int column) const;

private:
    friend class Database;
    Statement(sqlite3* db, std::string_view sql);

    sqlite3* db_;
    sqlite3_stmt* statement_ = nullptr;
};

// One connection. A file database is switched to write-ahead logging, so a
// crash mid-write never corrupts it; foreign keys are always enforced.
class Database {
public:
    // ":memory:" opens a private in-memory database (for tests).
    // Throws Error when the file cannot be opened or created.
    explicit Database(const std::string& path);
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    ~Database();

    // Runs one or more statements separated by ';', without parameters.
    void execute(std::string_view sql);
    [[nodiscard]] Statement prepare(std::string_view sql);
    [[nodiscard]] std::int64_t lastInsertRowId() const noexcept;

private:
    sqlite3* db_ = nullptr;
};

// BEGIN in the constructor; ROLLBACK in the destructor unless commit() was
// called, so an exception halfway through a write leaves nothing behind.
class ScopedTransaction {
public:
    explicit ScopedTransaction(Database& db);
    ScopedTransaction(const ScopedTransaction&) = delete;
    ScopedTransaction& operator=(const ScopedTransaction&) = delete;
    ~ScopedTransaction();

    void commit();

private:
    Database& db_;
    bool done_ = false;
};

} // namespace wms::sqlite
