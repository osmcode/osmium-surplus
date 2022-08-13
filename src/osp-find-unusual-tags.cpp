
#include "utils.hpp"

#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/file.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/util/memory.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/verbose_output.hpp>
#include <osmium/visitor.hpp>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <getopt.h>
#include <iostream>
#include <string>

static char const *const program_name = "osp-find-unusual-tags";

struct options_type
{
    osmium::Timestamp before_time{osmium::end_of_time()};
    bool verbose = true;
};

struct stats_type
{
    uint64_t nodes = 0;
    uint64_t ways = 0;
    uint64_t relations = 0;
    uint64_t nwr_key_empty = 0;
    uint64_t nwr_key_short = 0;
    uint64_t nwr_key_long = 0;
    uint64_t nwr_key_role = 0;
    uint64_t nwr_key_bad_chars = 0;
    uint64_t nwr_key_unusual_chars = 0;
    uint64_t nwr_value_empty = 0;
    uint64_t nwr_value_whitespace = 0;
    uint64_t n_tag_type_multipolygon = 0;
    uint64_t w_tag_type_multipolygon = 0;
    uint64_t n_tag_type_boundary = 0;
    uint64_t w_tag_type_boundary = 0;
    uint64_t n_tag_natural_coastline = 0;
    uint64_t r_tag_natural_coastline = 0;
    uint64_t r_tag_boundary_multipolygon = 0;
};

static char const *const bad_characters = "=/&<>;'\"?%#@\\,";
static char const *const usual_characters =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_:";

class CheckHandler : public osmium::handler::Handler
{

    options_type m_options;
    stats_type m_stats;

    osmium::io::Writer m_writer_nwr_key_empty;
    osmium::io::Writer m_writer_nwr_key_short;
    osmium::io::Writer m_writer_nwr_key_long;
    osmium::io::Writer m_writer_nwr_key_role;
    osmium::io::Writer m_writer_nwr_key_bad_chars;
    osmium::io::Writer m_writer_nwr_key_unusual_chars;

    osmium::io::Writer m_writer_nwr_value_empty;
    osmium::io::Writer m_writer_nwr_value_whitespace;

    osmium::io::Writer m_writer_nw_tag_type_multipolygon;
    osmium::io::Writer m_writer_nw_tag_type_boundary;

    osmium::io::Writer m_writer_nr_tag_natural_coastline;

    osmium::io::Writer m_writer_r_tag_boundary_multipolygon;

public:
    CheckHandler(std::string const &directory, options_type const &options,
                 osmium::io::Header const &header)
    : m_options(options),
      m_writer_nwr_key_empty(directory + "/nwr-key-empty.osm.pbf", header,
                             osmium::io::overwrite::allow),
      m_writer_nwr_key_short(directory + "/nwr-key-short.osm.pbf", header,
                             osmium::io::overwrite::allow),
      m_writer_nwr_key_long(directory + "/nwr-key-long.osm.pbf", header,
                            osmium::io::overwrite::allow),
      m_writer_nwr_key_role(directory + "/nwr-key-role.osm.pbf", header,
                            osmium::io::overwrite::allow),
      m_writer_nwr_key_bad_chars(directory + "/nwr-key-bad-chars.osm.pbf",
                                 header, osmium::io::overwrite::allow),
      m_writer_nwr_key_unusual_chars(directory +
                                         "/nwr-key-unusual-chars.osm.pbf",
                                     header, osmium::io::overwrite::allow),
      m_writer_nwr_value_empty(directory + "/nwr-value-empty.osm.pbf", header,
                               osmium::io::overwrite::allow),
      m_writer_nwr_value_whitespace(directory + "/nwr-value-whitespace.osm.pbf",
                                    header, osmium::io::overwrite::allow),
      m_writer_nw_tag_type_multipolygon(directory +
                                            "/nw-tag-type-multipolygon.osm.pbf",
                                        header, osmium::io::overwrite::allow),
      m_writer_nw_tag_type_boundary(directory + "/nw-tag-type-boundary.osm.pbf",
                                    header, osmium::io::overwrite::allow),
      m_writer_nr_tag_natural_coastline(directory +
                                            "/nr-tag-natural-coastline.osm.pbf",
                                        header, osmium::io::overwrite::allow),
      m_writer_r_tag_boundary_multipolygon(
          directory + "/r-tag-boundary-multipolygon.osm.pbf", header,
          osmium::io::overwrite::allow)
    {}

    void osm_object(osmium::OSMObject const &object)
    {
        if (object.timestamp() >= m_options.before_time) {
            return;
        }

        for (auto const &tag : object.tags()) {
            auto const key_len = std::strlen(tag.key());
            if (key_len == 0) {
                ++m_stats.nwr_key_empty;
                m_writer_nwr_key_empty(object);
            } else if (key_len == 1) {
                ++m_stats.nwr_key_short;
                m_writer_nwr_key_short(object);
            } else if (key_len > 80) {
                ++m_stats.nwr_key_long;
                m_writer_nwr_key_long(object);
            } else if (!std::strcmp(tag.key(), "role")) {
                ++m_stats.nwr_key_role;
                m_writer_nwr_key_role(object);
            }

            auto const key_len_bad_chars =
                std::strcspn(tag.key(), bad_characters);
            if (key_len != key_len_bad_chars) {
                ++m_stats.nwr_key_bad_chars;
                m_writer_nwr_key_bad_chars(object);
            } else {
                auto const key_len_common_chars =
                    std::strspn(tag.key(), usual_characters);
                if (key_len != key_len_common_chars) {
                    ++m_stats.nwr_key_unusual_chars;
                    m_writer_nwr_key_unusual_chars(object);
                }
            }

            if (tag.value()[0] == '\0') {
                ++m_stats.nwr_value_empty;
                m_writer_nwr_value_empty(object);
                continue;
            }

            auto const value_len = std::strlen(tag.value());
            if (isspace(tag.value()[0]) ||
                isspace(tag.value()[value_len - 1])) {
                ++m_stats.nwr_value_whitespace;
                m_writer_nwr_value_whitespace(object);
            }
        }
    }

    void node(osmium::Node const &node)
    {
        if (node.timestamp() >= m_options.before_time) {
            return;
        }

        ++m_stats.nodes;

        char const *type = node.tags().get_value_by_key("type");
        if (type) {
            if (!std::strcmp(type, "multipolygon")) {
                ++m_stats.n_tag_type_multipolygon;
                m_writer_nw_tag_type_multipolygon(node);
            }
            if (!std::strcmp(type, "boundary")) {
                ++m_stats.n_tag_type_boundary;
                m_writer_nw_tag_type_boundary(node);
            }
        }

        char const *natural = node.tags().get_value_by_key("natural");
        if (natural && !std::strcmp(natural, "coastline")) {
            ++m_stats.n_tag_natural_coastline;
            m_writer_nr_tag_natural_coastline(node);
        }
    }

    void way(osmium::Way const &way)
    {
        if (way.timestamp() >= m_options.before_time) {
            return;
        }

        ++m_stats.ways;

        char const *type = way.tags().get_value_by_key("type");
        if (type) {
            if (!std::strcmp(type, "multipolygon")) {
                ++m_stats.w_tag_type_multipolygon;
                m_writer_nw_tag_type_multipolygon(way);
            }
            if (!std::strcmp(type, "boundary")) {
                ++m_stats.w_tag_type_boundary;
                m_writer_nw_tag_type_boundary(way);
            }
        }
    }

    void relation(osmium::Relation const &relation)
    {
        if (relation.timestamp() >= m_options.before_time) {
            return;
        }

        ++m_stats.relations;

        char const *natural = relation.tags().get_value_by_key("natural");
        if (natural && !std::strcmp(natural, "coastline")) {
            ++m_stats.r_tag_natural_coastline;
            m_writer_nr_tag_natural_coastline(relation);
        }

        char const *type = relation.tags().get_value_by_key("type");
        if (type && !std::strcmp(type, "multipolygon")) {
            char const *boundary = relation.tags().get_value_by_key("boundary");
            if (boundary && !std::strcmp(boundary, "administrative")) {
                ++m_stats.r_tag_boundary_multipolygon;
                m_writer_r_tag_boundary_multipolygon(relation);
            }
        }
    }

    void close()
    {
        m_writer_nwr_key_empty.close();
        m_writer_nwr_key_short.close();
        m_writer_nwr_key_long.close();
        m_writer_nwr_key_role.close();
        m_writer_nwr_key_bad_chars.close();
        m_writer_nwr_key_unusual_chars.close();

        m_writer_nwr_value_empty.close();
        m_writer_nwr_value_whitespace.close();

        m_writer_nw_tag_type_multipolygon.close();
        m_writer_nw_tag_type_boundary.close();

        m_writer_nr_tag_natural_coastline.close();

        m_writer_r_tag_boundary_multipolygon.close();
    }

    stats_type const &stats() const noexcept { return m_stats; }

}; // class CheckHandler

static void print_help()
{
    std::cout << program_name << " [OPTIONS] OSM-FILE OUTPUT-DIR\n\n"
              << "Find objects with unusual tags.\n"
              << "\nOptions:\n"
              << "  -a, --min-age=DAYS      Only include objects at least DAYS "
                 "days old\n"
              << "  -b, --before=TIMESTAMP  Only include objects changed last "
                 "before\n"
              << "                          this time (format: "
                 "yyyy-mm-ddThh:mm:ssZ)\n"
              << "  -h, --help              This help message\n"
              << "  -q, --quiet             Work quietly\n";
}

static options_type parse_command_line(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"age", required_argument, nullptr, 'a'},
        {"before", required_argument, nullptr, 'b'},
        {"help", no_argument, nullptr, 'h'},
        {"quiet", no_argument, nullptr, 'q'},
        {nullptr, 0, nullptr, 0}};

    options_type options;

    while (true) {
        int const c = getopt_long(argc, argv, "a:b:hq", long_options, nullptr);
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
        default:
            std::exit(2);
        }
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

    osmium::io::Reader reader{input_filename, osmium::osm_entity_bits::nwr};

    osmium::io::Header header;
    header.set("generator", program_name);

    LastTimestampHandler last_timestamp_handler;
    CheckHandler handler{output_dirname, options, header};

    vout << "Reading data and checking tags...\n";
    osmium::ProgressBar progress_bar{reader.file_size(), display_progress()};
    while (osmium::memory::Buffer buffer = reader.read()) {
        progress_bar.update(reader.offset());
        osmium::apply(buffer, last_timestamp_handler, handler);
    }
    progress_bar.done();

    handler.close();
    reader.close();

    vout << "Writing out stats...\n";
    auto const last_time{last_timestamp_handler.get_timestamp()};
    write_stats(
        output_dirname + "/stats-unusual-tags.db", last_time,
        [&](std::function<void(char const *, uint64_t)> &add) {
            add("nodes", handler.stats().nodes);
            add("ways", handler.stats().ways);
            add("relations", handler.stats().relations);
            add("nwr_key_empty", handler.stats().nwr_key_empty);
            add("nwr_key_short", handler.stats().nwr_key_short);
            add("nwr_key_long", handler.stats().nwr_key_long);
            add("nwr_key_role", handler.stats().nwr_key_role);
            add("nwr_key_bad_chars", handler.stats().nwr_key_bad_chars);
            add("nwr_key_unusual_chars", handler.stats().nwr_key_unusual_chars);
            add("nwr_value_empty", handler.stats().nwr_value_empty);
            add("nwr_value_whitespace", handler.stats().nwr_value_whitespace);
            add("n_tag_type_multipolygon",
                handler.stats().n_tag_type_multipolygon);
            add("w_tag_type_multipolygon",
                handler.stats().w_tag_type_multipolygon);
            add("n_tag_type_boundary", handler.stats().n_tag_type_boundary);
            add("w_tag_type_boundary", handler.stats().w_tag_type_boundary);
            add("n_tag_natural_coastline",
                handler.stats().n_tag_natural_coastline);
            add("r_tag_natural_coastline",
                handler.stats().r_tag_natural_coastline);
            add("r_tag_boundary_multipolygon",
                handler.stats().r_tag_boundary_multipolygon);
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
