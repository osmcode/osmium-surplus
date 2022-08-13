#ifndef OSMIUM_SURPLUS_DB_HPP
#define OSMIUM_SURPLUS_DB_HPP

#include <sqlite.hpp>

#include <string_view>

Sqlite::Database open_database(std::string_view name, bool create);

#endif // OSMIUM_SURPLUS_DB_HPP
