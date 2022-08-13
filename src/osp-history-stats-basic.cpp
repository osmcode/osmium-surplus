
#include "app.hpp"
#include "date-convert.hpp"
#include "db.hpp"
#include "stats-basic.hpp"
#include "util.hpp"

#include <osmium/diff_handler.hpp>
#include <osmium/diff_visitor.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/util/verbose_output.hpp>

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

/* ========================================================================= */

class StatsHandler : public osmium::diff_handler::DiffHandler,
                     public stats_basic
{
    std::vector<std::array<int64_t, num_variables>> m_stats{{0}};

    std::string m_db_file_name;

    DateConvert m_dc;

    unsigned int m_start = 0;
    unsigned int m_end = 0;

    int64_t &v(std::size_t time, names n) noexcept { return m_stats[time][n]; }

    void update_count(names name, int64_t incr) noexcept
    {
        v(m_start, name) += incr;
        v(m_end, name) -= incr;
    }

    void update_max(int64_t value, names max) noexcept
    {
        if (value > v(m_start, max)) {
            v(m_start, max) = value;
        }
    }

    void init_start_end(osmium::DiffObject const &diff_object)
    {
        m_start = m_dc(diff_object.start_time().seconds_since_epoch());
        m_end = m_dc(diff_object.end_time().seconds_since_epoch());

        if (m_end > m_stats.size()) {
            m_stats.resize(m_end + 1);
        }
        if (m_start > m_stats.size()) {
            m_stats.resize(m_start + 1);
        }
    }

    void update_common_stats(osmium::OSMObject const &object) noexcept
    {
        update_max(object.uid(), max_user_id);
        update_max(object.changeset(), max_changeset_id);
    }

    void calculate_derived_stats(std::size_t time) noexcept
    {
        v(time, objects) = v(time, nodes) + v(time, ways) + v(time, relations);
        v(time, max_tags) =
            max3(v(time, max_tags_on_node), v(time, max_tags_on_way),
                 v(time, max_tags_on_relation));
        v(time, max_version) =
            max3(v(time, max_node_version), v(time, max_way_version),
                 v(time, max_relation_version));
    }

public:
    StatsHandler(std::time_t last_time, std::string dbfile)
    : m_db_file_name(std::move(dbfile)), m_dc(last_time)
    {}

    void node(osmium::DiffNode const &diff_node)
    {
        osmium::Node const &node = diff_node.curr();
        if (!node.visible()) {
            return;
        }

        init_start_end(diff_node);
        update_common_stats(node);

        update_count(nodes, 1);

        update_max(node.id(), max_node_id);
        update_max(node.version(), max_node_version);

        std::size_t const num_tags = node.tags().size();
        if (num_tags == 0) {
            update_count(nodes_without_tags, 1);
        }
        update_count(node_tags, num_tags);
        update_max(num_tags, max_tags_on_node);
    }

    void way(osmium::DiffWay const &diff_way)
    {
        osmium::Way const &way = diff_way.curr();
        if (!way.visible()) {
            return;
        }

        init_start_end(diff_way);
        update_common_stats(way);

        update_count(ways, 1);

        if (!way.nodes().empty() && way.is_closed()) {
            update_count(closed_ways, 1);
        }

        update_max(way.id(), max_way_id);
        update_max(way.version(), max_way_version);

        std::size_t const num_tags = way.tags().size();
        update_count(way_tags, num_tags);
        update_max(num_tags, max_tags_on_way);

        int64_t const nodes_count = way.nodes().size();
        update_max(nodes_count, max_nodes_on_way);
        update_count(way_nodes, nodes_count);
    }

    void relation(osmium::DiffRelation const &diff_relation)
    {
        osmium::Relation const &relation = diff_relation.curr();
        if (!relation.visible()) {
            return;
        }

        init_start_end(diff_relation);
        update_common_stats(relation);

        update_count(relations, 1);

        update_max(relation.id(), max_relation_id);
        update_max(relation.version(), max_relation_version);

        std::size_t const num_tags = relation.tags().size();
        update_count(relation_tags, num_tags);
        update_max(num_tags, max_tags_on_relation);

        int64_t const num_members = relation.members().size();
        update_count(relation_members, num_members);
        update_max(num_members, max_members_on_relation);

        for (auto const &member : relation.members()) {
            switch (member.type()) {
            case osmium::item_type::node:
                update_count(relation_members_nodes, 1);
                break;
            case osmium::item_type::way:
                update_count(relation_members_ways, 1);
                break;
            case osmium::item_type::relation:
                update_count(relation_members_relations, 1);
                break;
            default:
                throw std::runtime_error("unknown relation member type");
                break;
            }
        }
    }

    void write_database()
    {
        auto db = open_database(m_db_file_name, true);

        db.exec(create_table("diffs"));
        db.exec(create_table("stats"));

        Sqlite::Statement statement_insert_into_diffs{
            db, insert("diffs", "date('now', ?)").c_str()};
        Sqlite::Statement statement_insert_into_stats{
            db, insert("stats", "date('now', ?)").c_str()};
        db.begin_transaction();

        std::array<int64_t, num_variables> counters{0};
        for (auto time = m_stats.size() - 1; time > 0; --time) {
            std::string const dm = "-{} days"_format(time);

            calculate_derived_stats(time);

            statement_insert_into_diffs.bind_text(dm);
            statement_insert_into_stats.bind_text(dm);

            for (std::size_t i = 0; i < num_variables; ++i) {
                statement_insert_into_diffs.bind_int64(m_stats[time][i]);
                counters[i] += m_stats[time][i];
                statement_insert_into_stats.bind_int64(counters[i]);
            }

            statement_insert_into_diffs.execute();
            statement_insert_into_stats.execute();
        }

        db.commit();
    }

}; // class StatsHandler

/* ========================================================================= */

class App : public BasicApp
{
public:
    App()
    : BasicApp("osp-history-stats-basic",
               "Generate basic statistics from OSM history file",
               with_output::db)
    {}

    void run()
    {
        StatsHandler handler{std::time(nullptr), output()};
        osmium::io::Reader reader{input()};

        vout() << "Processing data...\n";
        osmium::apply_diff(reader, handler);
        reader.close();
        vout() << "Done processing.\n";

        vout() << "Writing results to database '" << output() << "'...\n";
        handler.write_database();
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
