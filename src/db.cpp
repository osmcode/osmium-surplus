
#include "db.hpp"

#include <filesystem>

Sqlite::Database open_database(std::string_view name, bool create)
{
    if (create) {
        std::filesystem::remove(name);
    }

    // NOLINTNEXTLINE(hicpp-signed-bitwise)
    auto const flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

    return Sqlite::Database{name.data(), flags};
}
