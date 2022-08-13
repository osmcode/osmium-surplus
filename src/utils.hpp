#ifndef OSMIUM_SURPLUS_UTILS_HPP
#define OSMIUM_SURPLUS_UTILS_HPP

#include <gdalcpp.hpp>
#include <sqlite.hpp>

#include <osmium/geom/ogr.hpp>
#include <osmium/handler.hpp>
#include <osmium/io/header.hpp>
#include <osmium/osm/timestamp.hpp>
#include <osmium/util/file.hpp>

#include <cstdlib>
#include <ctime>
#include <string>

bool display_progress() noexcept { return osmium::util::isatty(2); }

class HandlerWithDB : public osmium::handler::Handler
{

protected:
    osmium::geom::OGRFactory<> m_factory;
    gdalcpp::Dataset m_dataset;

public:
    explicit HandlerWithDB(std::string const &name)
    : m_factory(),
      m_dataset("SQLite", name, gdalcpp::SRS{m_factory.proj_string()},
                {"SPATIALITE=TRUE", "INIT_WITH_EPSG=NO"})
    {
        CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "OFF");
        m_dataset.enable_auto_transactions();
        m_dataset.exec("PRAGMA journal_mode = OFF;");
    }

}; // Class HandlerWithDB

class LastTimestampHandler : public osmium::handler::Handler
{

    osmium::Timestamp m_timestamp;

public:
    LastTimestampHandler() : m_timestamp(osmium::start_of_time()) {}

    void osm_object(osmium::OSMObject const &object) noexcept
    {
        if (object.timestamp() > m_timestamp) {
            m_timestamp = object.timestamp();
        }
    }

    osmium::Timestamp get_timestamp() const noexcept { return m_timestamp; }

}; // class LastTimestampHandler

template <typename TFunc>
void write_stats(std::string const &database_name,
                 osmium::Timestamp const &timestamp, TFunc &&func)
{
    Sqlite::Database db{database_name,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE};

    db.exec("CREATE TABLE IF NOT EXISTS stats (date TEXT, key TEXT, value "
            "INT64 DEFAULT 0);");

    Sqlite::Statement statement{
        db, "INSERT INTO stats (date, key, value) VALUES (?, ?, ?);"};

    std::string const last_time{timestamp.to_iso()};

    std::function<void(char const *, uint64_t)> add = [&](char const *name,
                                                          uint64_t value) {
        statement.bind_text(last_time)
            .bind_text(name)
            .bind_int64(value)
            .execute();
    };

    std::forward<TFunc>(func)(add);
}

inline bool has_locations_on_ways(osmium::io::Header const &header)
{
    for (auto const &option : header) {
        if (option.second == "LocationsOnWays") {
            return true;
        }
    }

    return false;
}

inline osmium::Timestamp build_timestamp(char const *ts)
{
    char *end = nullptr;
    errno = 0;
    auto const s = std::strtoll(ts, &end, 10);
    if (errno != 0 || !end || *end != '\0' || s < 0) {
        throw std::runtime_error{std::string{"Value not allowed for age: "} +
                                 ts};
    }
    return osmium::Timestamp{std::time(nullptr) - s * 60 * 60 * 24};
}

#endif // OSMIUM_SURPLUS_UTILS_HPP
