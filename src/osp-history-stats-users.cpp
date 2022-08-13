
#include "app.hpp"
#include "db.hpp"

#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/util/verbose_output.hpp>
#include <osmium/visitor.hpp>

#include <ctime>
#include <string>
#include <vector>

/* ========================================================================= */

struct UserStats
{
    std::string username;
    uint64_t nedits = 0;
    uint64_t ncreate = 0;
    osmium::Location first_location;
    std::time_t first_edit = 0;
    std::time_t last_edit = 0;

}; // struct UserStats

class StatsHandler : public osmium::handler::Handler
{
    std::vector<UserStats> m_userlist;

    UserStats &object(osmium::OSMObject const &object)
    {
        int const uid = object.uid();

        // make sure we have enough space in user vector
        if (static_cast<std::size_t>(uid) >= m_userlist.size()) {
            m_userlist.resize(uid + 1);
        }

        UserStats &us = m_userlist[uid];

        // first time we see this user
        if (us.nedits == 0) {
            us.username = object.user();
            us.first_edit = object.timestamp().seconds_since_epoch();
        }

        if (object.version() == 1) {
            us.ncreate++;
        }

        if (object.timestamp().seconds_since_epoch() > us.last_edit) {
            us.last_edit = object.timestamp().seconds_since_epoch();
        }

        us.nedits++;

        return us;
    }

public:
    void node(osmium::Node const &node)
    {
        UserStats &us = object(node);

        // record position of first edit
        if (!us.first_location) {
            us.first_location = node.location();
        }
    }

    void way(osmium::Way const &way) { object(way); }

    void relation(osmium::Relation const &relation) { object(relation); }

    void write_database(std::string const &dbname)
    {
        auto db = open_database(dbname, true);

        db.exec("CREATE TABLE users ("
                "  uid INTEGER,"
                "  username VARCHAR,"
                "  nedits INTEGER,"
                "  ncreate INTEGER,"
                "  first_edit INTEGER,"
                "  last_edit INTEGER,"
                "  lon REAL,"
                "  lat REAL);");

        Sqlite::Statement statement_insert{
            db,
            "INSERT INTO users "
            "(uid, username, nedits, ncreate, first_edit, last_edit, lon, lat)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?)"};
        db.begin_transaction();

        for (unsigned int uid = 0; uid < m_userlist.size(); ++uid) {
            UserStats const &us = m_userlist[uid];
            if (us.nedits > 0) {
                // clang-format off
                statement_insert.
                    bind_int64(uid).
                    bind_text(us.username).
                    bind_int64(us.nedits).
                    bind_int64(us.ncreate).
                    bind_int(us.first_edit).
                    bind_int(us.last_edit);

                if (us.first_location) {
                    statement_insert.bind_double(us.first_location.lon()).
                                     bind_double(us.first_location.lat());
                } else {
                    statement_insert.bind_null().
                                     bind_null();
                }
                // clang-format on

                statement_insert.execute();
            }
        }

        db.commit();
    }

}; // class StatsHandler

/* ========================================================================= */

class App : public BasicApp
{
public:
    App()
    : BasicApp("osp-history-stats-users",
               "Generate user statistics from OSM history file",
               with_output::db)
    {}

    void run()
    {
        StatsHandler handler;
        osmium::io::Reader reader{input()};

        vout() << "Processing data...\n";
        osmium::apply(reader, handler);
        reader.close();
        vout() << "Done processing.\n";

        vout() << "Writing results to database '" << output() << "'...\n";
        handler.write_database(output());
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
