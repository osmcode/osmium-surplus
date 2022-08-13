#ifndef OSMIUM_SURPLUS_STATS_BASIC_HPP
#define OSMIUM_SURPLUS_STATS_BASIC_HPP

#include "format.hpp"

#include <array>
#include <cstddef>
#include <iterator>
#include <string>

class stats_basic
{
protected:
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage,bugprone-macro-parentheses)
#define VAR(x) +1
    constexpr static std::size_t const num_variables = 0
#include "stats-vars.hpp"
        ;
#undef VAR

    std::array<char const *, num_variables> name_strings = {
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define VAR(x) #x,
#include "stats-vars.hpp"
#undef VAR
    };

    enum names
    {
    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define VAR(x) x,
#include "stats-vars.hpp"
#undef VAR
    };

    std::string create_table(std::string const &name)
    {
        std::string sql = fmt::format("CREATE TABLE {} (ts VARCHAR", name);
        for (auto const *column : name_strings) {
            fmt::format_to(std::back_inserter(sql), ", {} INT64", column);
        }
        sql.push_back(')');
        return sql;
    }

    std::string insert(std::string const &name, std::string const &date)
    {
        std::string sql = fmt::format("INSERT INTO {} (ts", name);
        std::string params;

        for (auto const *column : name_strings) {
            sql += ", ";
            sql += column;
            params += ", ?";
        }
        fmt::format_to(std::back_inserter(sql), ") VALUES ({}{})", date,
                       params);

        return sql;
    }

}; // class stats_basic

#endif // OSMIUM_SURPLUS_STATS_BASIC_HPP
