
#include "app.hpp"
#include "db.hpp"
#include "stats-basic.hpp"
#include "util.hpp"

#include <osmium/diff_handler.hpp>
#include <osmium/diff_visitor.hpp>
#include <osmium/handler.hpp>
#include <osmium/index/nwr_array.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/osm/timestamp.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/verbose_output.hpp>
#include <osmium/visitor.hpp>

#include <array>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/* ========================================================================= */

class Histogram
{
    osmium::nwr_array<std::vector<uint64_t>> m_data;
    std::string m_name;

public:
    explicit Histogram(std::string name) : m_name(std::move(name)) {}

    void incr(osmium::item_type type, std::size_t i)
    {
        auto &vec = m_data(type);
        if (vec.size() <= i) {
            vec.resize(i + 1);
        }
        ++vec[i];
    }

    void init_db(Sqlite::Database *db) const
    {
        db->exec(
            std::string{"CREATE TABLE hist_"} + m_name +
            " (ts VARCHAR, object_type CHAR, versions INTEGER, num INTEGER);");
    }

    void write_db(Sqlite::Database *db, std::string const &ts) const
    {
        auto const sql =
            "INSERT INTO hist_" + m_name +
            " (ts, object_type, versions, num) VALUES (?, ?, ?, ?)";

        Sqlite::Statement stmt{*db, sql.c_str()};

        for (auto const t : {osmium::item_type::node, osmium::item_type::way,
                             osmium::item_type::relation}) {
            std::array<char, 2> const tn = {osmium::item_type_to_char(t), '\0'};
            auto const &d = m_data(t);
            for (std::size_t i = 0; i < d.size(); ++i) {
                if (d[i] > 0) {
                    stmt.bind_text(ts);
                    stmt.bind_text(tn.data());
                    stmt.bind_int64(static_cast<int64_t>(i));
                    stmt.bind_int64(static_cast<int64_t>(d[i]));
                    stmt.execute();
                }
            }
        }
    }

}; // class Histogram

class StatsHandler : public osmium::handler::Handler, public stats_basic
{
    std::array<uint64_t, num_variables> m_variables = {0};
    Histogram m_hist_versions{"versions"};
    Histogram m_hist_members{"members"};
    Histogram m_hist_nodes{"nodes"};
    osmium::Timestamp m_max_timestamp{};

    uint64_t &v(names n) noexcept { return m_variables[n]; }

    void update_max(uint64_t value, names max) noexcept
    {
        if (value > v(max)) {
            v(max) = value;
        }
    }

    void update_common_stats(osmium::OSMObject const &object) noexcept
    {
        update_max(object.uid(), max_user_id);
        update_max(object.changeset(), max_changeset_id);
        if (object.timestamp() > m_max_timestamp) {
            m_max_timestamp = object.timestamp();
        }
        m_hist_versions.incr(object.type(), object.version());
    }

    void write_variables(Sqlite::Database *db, std::string const &ts)
    {
        Sqlite::Statement stmt{*db, insert("stats", "?").c_str()};

        stmt.bind_text(ts);
        for (std::size_t i = 0; i < num_variables; ++i) {
            stmt.bind_int64(static_cast<int64_t>(m_variables[i]));
        }
        stmt.execute();
    }

public:
    void node(osmium::Node const &node)
    {
        update_common_stats(node);
        ++v(nodes);
        v(sum_node_version) += node.version();

        update_max(node.id(), max_node_id);
        update_max(node.version(), max_node_version);

        std::size_t const num_tags = node.tags().size();
        if (num_tags == 0) {
            ++v(nodes_without_tags);
        }
        v(node_tags) += num_tags;
        update_max(num_tags, max_tags_on_node);
    }

    void way(osmium::Way const &way)
    {
        update_common_stats(way);
        ++v(ways);
        v(sum_way_version) += way.version();

        if (!way.nodes().empty() && way.is_closed()) {
            ++v(closed_ways);
        }

        update_max(way.id(), max_way_id);
        update_max(way.version(), max_way_version);

        std::size_t const num_tags = way.tags().size();
        v(way_tags) += num_tags;
        update_max(num_tags, max_tags_on_way);

        std::size_t const num_nodes = way.nodes().size();
        v(way_nodes) += num_nodes;
        update_max(num_nodes, max_nodes_on_way);

        m_hist_nodes.incr(osmium::item_type::node, way.nodes().size());
    }

    void relation(osmium::Relation const &relation)
    {
        update_common_stats(relation);
        ++v(relations);
        v(sum_relation_version) += relation.version();

        update_max(relation.id(), max_relation_id);
        update_max(relation.version(), max_relation_version);

        std::size_t const num_tags = relation.tags().size();
        v(relation_tags) += num_tags;
        update_max(num_tags, max_tags_on_relation);

        std::size_t const num_members = relation.members().size();
        v(relation_members) += num_members;
        update_max(num_members, max_members_on_relation);
        m_hist_members.incr(osmium::item_type::relation, num_members);

        for (auto const &member : relation.members()) {
            switch (member.type()) {
            case osmium::item_type::node:
                ++v(relation_members_nodes);
                break;
            case osmium::item_type::way:
                ++v(relation_members_ways);
                break;
            case osmium::item_type::relation:
                ++v(relation_members_relations);
                break;
            default:
                break;
            }
        }
    }

    void calculate_derived_stats() noexcept
    {
        v(objects) = v(nodes) + v(ways) + v(relations);
        v(max_tags) = max3(v(max_tags_on_node), v(max_tags_on_way),
                           v(max_tags_on_relation));
        v(max_version) = max3(v(max_node_version), v(max_way_version),
                              v(max_relation_version));
        v(sum_version) =
            v(sum_node_version) + v(sum_way_version) + v(sum_relation_version);
    }

    void write_database(std::string const &dbname)
    {
        auto db = open_database(dbname, true); // XXX TODO optional
        db.exec(create_table("stats"));
        m_hist_versions.init_db(&db);
        m_hist_nodes.init_db(&db);
        m_hist_members.init_db(&db);

        auto const ts = m_max_timestamp.to_iso();

        db.begin_transaction();
        write_variables(&db, ts);
        m_hist_versions.write_db(&db, ts);
        m_hist_nodes.write_db(&db, ts);
        m_hist_members.write_db(&db, ts);
        db.commit();
    }

}; // class StatsHandler

class FilterHandler : public osmium::diff_handler::DiffHandler
{
    StatsHandler *m_handler;
    osmium::Timestamp m_timestamp;

public:
    FilterHandler(StatsHandler *handler, osmium::Timestamp timestamp) noexcept
    : m_handler(handler), m_timestamp(timestamp)
    {}

    void node(osmium::DiffNode const &diff_node) const
    {
        if (diff_node.is_visible_at(m_timestamp)) {
            m_handler->node(diff_node.curr());
        }
    }

    void way(osmium::DiffWay const &diff_way) const
    {
        if (diff_way.is_visible_at(m_timestamp)) {
            m_handler->way(diff_way.curr());
        }
    }

    void relation(osmium::DiffRelation const &diff_relation) const
    {
        if (diff_relation.is_visible_at(m_timestamp)) {
            m_handler->relation(diff_relation.curr());
        }
    }

}; // class FilterHandler

/* ========================================================================= */

class App : public BasicApp
{
    osmium::Timestamp m_timestamp{};

public:
    App()
    : BasicApp("osp-stats-basic",
               "Generate basic statistics from OSM data or history file",
               with_output::db)
    {
        add_option("-t,--timestamp", m_timestamp,
                   "Timestamp for stats from history file (ISO format)")
            ->type_name("TIMESTAMP");
    }

    void run()
    {
        if (m_timestamp) {
            vout() << "        Timestamp: " << m_timestamp.to_iso() << "\n";
        }
        StatsHandler handler;

        vout() << "Opening input file...\n";
        osmium::io::Reader reader{input(), osmium::osm_entity_bits::object};

        if (m_timestamp.valid()) {
            if (!reader.header().has_multiple_object_versions()) {
                reader.close();
                throw std::runtime_error{
                    "Input file doesn't have multiple object versions. "
                    "Use without --timestamp/-t ?"};
            }
            vout() << "...this is an OSM file with history.\n";
            vout() << "Processing data...\n";
            FilterHandler filter_handler{&handler, m_timestamp};
            osmium::apply_diff(reader, filter_handler);
        } else {
            if (reader.header().has_multiple_object_versions()) {
                reader.close();
                throw std::runtime_error{
                    "Input file has multiple object versions. Use with "
                    "--timestamp/-t ?"};
            }
            vout() << "...this is an OSM file without history.\n";
            vout() << "Processing data...\n";
            osmium::ProgressBar progress_bar{reader.file_size(),
                                             vout().verbose()};
            while (auto buffer = reader.read()) {
                progress_bar.update(reader.offset());
                osmium::apply(buffer, handler);
            }
            progress_bar.done();
        }

        reader.close();
        vout() << "Done processing.\n";

        handler.calculate_derived_stats();
        vout() << "Writing results to database '" << output() << "'...\n";
        handler.write_database(output());
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
