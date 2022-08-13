
#include "utils.hpp"

#include <gdalcpp.hpp>

#include <osmium/index/id_set.hpp>
#include <osmium/index/nwr_array.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/osm/timestamp.hpp>
#include <osmium/tags/taglist.hpp>
#include <osmium/tags/tags_filter.hpp>
#include <osmium/util/memory.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/verbose_output.hpp>

#include <cstdlib>
#include <ctime>
#include <functional>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <string>

static char const *const program_name = "osp-find-orphans";

struct options_type
{
    osmium::Timestamp before_time{osmium::end_of_time()};
    bool verbose = true;
    bool untagged = true;
    bool tagged = true;
};

struct stats_type
{
    uint64_t orphan_nodes = 0;
    uint64_t orphan_ways = 0;
    uint64_t orphan_relations = 0;
};

using id_set_type = osmium::index::IdSetDense<osmium::unsigned_object_id_type>;

static osmium::nwr_array<id_set_type>
create_index_of_referenced_objects(osmium::io::File const &input_file,
                                   osmium::ProgressBar *progress_bar)
{
    osmium::nwr_array<id_set_type> index;

    osmium::io::Reader reader{input_file,
                              osmium::osm_entity_bits::way |
                                  osmium::osm_entity_bits::relation};

    while (osmium::memory::Buffer buffer = reader.read()) {
        progress_bar->update(reader.offset());

        for (auto const &object : buffer.select<osmium::OSMObject>()) {
            if (object.type() == osmium::item_type::way) {
                for (auto const &node_ref :
                     static_cast<osmium::Way const &>(object).nodes()) {
                    index(osmium::item_type::node).set(node_ref.positive_ref());
                }
            } else if (object.type() == osmium::item_type::relation) {
                for (auto const &member :
                     static_cast<osmium::Relation const &>(object).members()) {
                    index(member.type()).set(member.positive_ref());
                }
            }
        }
    }

    reader.close();

    return index;
}

class CheckHandler : public HandlerWithDB
{

    options_type m_options;
    stats_type m_stats;

    gdalcpp::Layer m_layer_orphan_nodes;
    gdalcpp::Layer m_layer_orphan_ways;

    osmium::TagsFilter m_filter{false};

    osmium::nwr_array<id_set_type> &m_index;
    osmium::nwr_array<std::unique_ptr<osmium::io::Writer>> m_writers;

public:
    CheckHandler(std::string const &output_dirname, options_type const &options,
                 osmium::nwr_array<id_set_type> *index)
    : HandlerWithDB(output_dirname + "/geoms-orphans.db"), m_options(options),
      m_layer_orphan_nodes(m_dataset, "orphan_nodes", wkbPoint,
                           {"SPATIAL_INDEX=NO"}),
      m_layer_orphan_ways(m_dataset, "orphan_ways", wkbLineString,
                          {"SPATIAL_INDEX=NO"}),
      m_index(*index)
    {
        m_layer_orphan_nodes.add_field("node_id", OFTReal, 12);
        m_layer_orphan_nodes.add_field("timestamp", OFTString, 20);

        m_layer_orphan_ways.add_field("way_id", OFTInteger, 10);
        m_layer_orphan_ways.add_field("timestamp", OFTString, 20);

        m_filter.add_rule(true, "created_by");
        m_filter.add_rule(true, "source");

        osmium::io::Header header;
        header.set("generator", program_name);

        m_writers(osmium::item_type::node) =
            std::make_unique<osmium::io::Writer>(
                output_dirname + "/n-orphans.osm.pbf", header,
                osmium::io::overwrite::allow);
        m_writers(osmium::item_type::way) =
            std::make_unique<osmium::io::Writer>(
                output_dirname + "/w-orphans.osm.pbf", header,
                osmium::io::overwrite::allow);
        m_writers(osmium::item_type::relation) =
            std::make_unique<osmium::io::Writer>(
                output_dirname + "/r-orphans.osm.pbf", header,
                osmium::io::overwrite::allow);
    }

    void node(osmium::Node const &node)
    {
        if (node.timestamp() >= m_options.before_time) {
            return;
        }

        if (m_index(osmium::item_type::node).get(node.positive_id())) {
            return;
        }

        if ((m_options.untagged && node.tags().empty()) ||
            (m_options.tagged && !node.tags().empty() &&
             osmium::tags::match_all_of(node.tags(), std::cref(m_filter)))) {
            (*m_writers(osmium::item_type::node))(node);
            ++m_stats.orphan_nodes;
            gdalcpp::Feature feature{m_layer_orphan_nodes,
                                     m_factory.create_point(node)};
            feature.set_field("node_id", static_cast<double>(node.id()));
            auto const ts = node.timestamp().to_iso();
            feature.set_field("timestamp", ts.c_str());
            feature.add_to_layer();
        }
    }

    void way(osmium::Way const &way)
    {
        if (way.timestamp() >= m_options.before_time) {
            return;
        }

        if (m_index(osmium::item_type::way).get(way.positive_id())) {
            return;
        }

        if ((m_options.untagged && way.tags().empty()) ||
            (m_options.tagged && !way.tags().empty() &&
             osmium::tags::match_all_of(way.tags(), std::cref(m_filter)))) {
            (*m_writers(osmium::item_type::way))(way);
            ++m_stats.orphan_ways;
            try {
                gdalcpp::Feature feature{m_layer_orphan_ways,
                                         m_factory.create_linestring(way)};
                feature.set_field("way_id", static_cast<double>(way.id()));
                auto const ts = way.timestamp().to_iso();
                feature.set_field("timestamp", ts.c_str());
                feature.add_to_layer();
            } catch (osmium::geometry_error const &) {
                // ignore geometry errors
            }
        }
    }

    void relation(osmium::Relation const &relation)
    {
        if (relation.timestamp() >= m_options.before_time) {
            return;
        }

        if (m_index(osmium::item_type::relation).get(relation.positive_id())) {
            return;
        }

        if ((m_options.untagged && relation.tags().empty()) ||
            (m_options.tagged && !relation.tags().empty() &&
             osmium::tags::match_all_of(relation.tags(),
                                        std::cref(m_filter)))) {
            (*m_writers(osmium::item_type::relation))(relation);
            ++m_stats.orphan_relations;
        }
    }

    void close()
    {
        m_writers(osmium::item_type::node)->close();
        m_writers(osmium::item_type::way)->close();
        m_writers(osmium::item_type::relation)->close();
    }

    [[nodiscard]] stats_type const &stats() const noexcept { return m_stats; }

}; // class CheckHandler

static void print_help()
{
    std::cout << program_name << " [OPTIONS] OSM-FILE OUTPUT-DIR\n\n"
              << "Find objects that are unreferenced and untagged (or "
                 "minimally tagged).\n"
              << "\nOptions:\n"
              << "  -a, --min-age=DAYS      Only include objects at least DAYS "
                 "days old\n"
              << "  -b, --before=TIMESTAMP  Only include objects changed last "
                 "before\n"
              << "                          this time (format: "
                 "yyyy-mm-ddThh:mm:ssZ)\n"
              << "  -h, --help              This help message\n"
              << "  -q, --quiet             Work quietly\n"
              << "  -u, --untagged-only     Untagged objects only\n"
              << "  -U, --no-untagged       No untagged objects\n";
}

static options_type parse_command_line(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"age", required_argument, nullptr, 'a'},
        {"before", required_argument, nullptr, 'b'},
        {"help", no_argument, nullptr, 'h'},
        {"quiet", no_argument, nullptr, 'q'},
        {"untagged-only", no_argument, nullptr, 'u'},
        {"no-untagged", no_argument, nullptr, 'U'},
        {nullptr, 0, nullptr, 0}};

    options_type options;

    while (true) {
        int const c =
            getopt_long(argc, argv, "a:b:hquU", long_options, nullptr);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'a':
            if (options.before_time != osmium::end_of_time()) {
                std::cerr << "You can not use both -a,--age and -b,--before "
                             "together\n";
                std::exit(2);
            }
            options.before_time = build_timestamp(optarg);
            break;
        case 'b':
            if (options.before_time != osmium::end_of_time()) {
                std::cerr << "You can not use both -a,--age and -b,--before "
                             "together\n";
                std::exit(2);
            }
            options.before_time = osmium::Timestamp{optarg};
            break;
        case 'h':
            print_help();
            std::exit(0);
        case 'q':
            options.verbose = false;
            break;
        case 'u':
            options.tagged = false;
            break;
        case 'U':
            options.untagged = false;
            break;
        default:
            std::exit(2);
        }
    }

    if (!options.tagged && !options.untagged) {
        std::cerr << "Can not use -u,--untagged-only and -U,--no-untagged "
                     "together.\n";
        std::exit(2);
    }

    int const remaining_args = argc - optind;
    if (remaining_args != 2) {
        std::cerr << "Usage: " << program_name
                  << " [OPTIONS] OSM-FILE OUTPUT-DIR\n"
                  << "Call '" << program_name
                  << " --help' for usage information.\n";
        std::exit(2);
    }

    return options;
}

int main(int argc, char *argv[])
try {
    auto const options = parse_command_line(argc, argv);

    osmium::util::VerboseOutput vout{options.verbose};
    vout << "Starting " << program_name << "...\n";

    std::string const input_filename{argv[optind]};
    std::string const output_dirname{argv[optind + 1]};

    vout << "Command line options:\n";
    vout << "  Reading from file '" << input_filename << "'\n";
    vout << "  Writing to directory '" << output_dirname << "'\n";
    if (options.before_time == osmium::end_of_time()) {
        vout << "  Get all objects independent of change timestamp (change "
                "with --age, -a or --before, -b)\n";
    } else {
        vout << "  Get only objects last changed before: "
             << options.before_time
             << " (change with --age, -a or --before, -b)\n";
    }
    vout << "  Finding untagged objects: " << (options.untagged ? "yes" : "no")
         << " (change with --untagged, -u)\n";
    vout << "  Finding tagged objects: " << (options.tagged ? "yes" : "no")
         << " (change with --no-untagged, -U)\n";

    osmium::io::File const input_file{input_filename};

    auto const file_size = osmium::util::file_size(input_filename);
    osmium::ProgressBar progress_bar{file_size * 2, display_progress()};

    vout << "First pass: Creating index of referenced objects...\n";
    auto index = create_index_of_referenced_objects(input_file, &progress_bar);
    progress_bar.file_done(file_size);

    progress_bar.remove();
    vout << "Second pass: Writing out non-referenced and untagged objects...\n";

    LastTimestampHandler last_timestamp_handler;
    CheckHandler handler{output_dirname, options, &index};

    osmium::io::Reader reader{input_file, osmium::osm_entity_bits::nwr};

    while (osmium::memory::Buffer buffer = reader.read()) {
        progress_bar.update(reader.offset());
        osmium::apply(buffer, last_timestamp_handler, handler);
    }
    progress_bar.done();

    handler.close();
    reader.close();

    vout << "Writing out stats...\n";
    auto const last_time{last_timestamp_handler.get_timestamp()};
    write_stats(output_dirname + "/stats-orphans.db", last_time,
                [&](std::function<void(char const *, uint64_t)> &add) {
                    add("orphan_nodes", handler.stats().orphan_nodes);
                    add("orphan_ways", handler.stats().orphan_ways);
                    add("orphan_relations", handler.stats().orphan_relations);
                });

    osmium::MemoryUsage memory_usage;
    if (memory_usage.peak() != 0) {
        vout << "Peak memory usage: " << memory_usage.peak() << " MBytes\n";
    }

    vout << "Done with " << program_name << ".\n";

    return 0;
} catch (std::exception const &e) {
    std::cerr << e.what() << '\n';
    std::exit(1);
}
