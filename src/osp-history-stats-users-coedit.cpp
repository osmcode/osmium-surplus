
#include "app.hpp"
#include "db.hpp"

#include <osmium/diff_handler.hpp>
#include <osmium/diff_visitor.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/util/verbose_output.hpp>

#include <ctime>
#include <fstream>
#include <map>
#include <string>

/* ========================================================================= */

class StatsHandler : public osmium::diff_handler::DiffHandler
{
    std::map<std::pair<osmium::user_id_type, osmium::user_id_type>, uint32_t> m_userpairs;

    void handle_object(osmium::DiffObject const &object)
    {
        auto uids = std::make_pair(object.prev().uid(), object.curr().uid());

        if (uids.first == uids.second || uids.first == 0 || uids.second == 0) {
            return;
        }

        if (uids.first > uids.second) {
            std::swap(uids.first, uids.second);
        }

        ++m_userpairs[uids];
    }

public:
    void node(osmium::DiffNode const &node)
    {
        if (!node.first()) {
            handle_object(node);
        }
    }

    void way(osmium::DiffWay const &way) {
        if (!way.first()) {
            handle_object(way);
        }
    }

    void relation(osmium::DiffRelation const &relation) {
        if (!relation.first()) {
            handle_object(relation);
        }
    }

    void write_graph(std::string const &filename)
    {
        std::ofstream out{filename};
        out << "graph osmcoedit {\n";
        for (auto const &p : m_userpairs) {
            out << "  U" << p.first.first << " -- U" << p.first.second << " [weight=" << p.second << "]\n";
        }
        out << "}\n";
    }

}; // class StatsHandler

/* ========================================================================= */

class App : public BasicApp
{
public:
    App()
    : BasicApp("osp-history-stats-users",
               "Generate user statistics from OSM history file",
               with_output::dir)
    {}

    void run()
    {
        StatsHandler handler;
        osmium::io::Reader reader{input()};

        vout() << "Processing data...\n";
        osmium::apply_diff(reader, handler);
        reader.close();

        handler.write_graph(output() + "/osmcoedit.dot");

        vout() << "Done processing.\n";
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
