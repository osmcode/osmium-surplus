
#include "outputs.hpp"
#include "utils.hpp"

#include <osmium/index/id_set.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/file.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/tags/tags_filter.hpp>
#include <osmium/util/memory.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/verbose_output.hpp>
#include <osmium/visitor.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

static char const *const program_name = "osp-find-relation-problems";
static std::size_t const min_members_of_large_relations = 1000;

struct options_type
{
    osmium::Timestamp before_time{osmium::end_of_time()};
    bool verbose = true;
};

struct stats_type
{
    uint64_t relations = 0;
    uint64_t relation_members = 0;
};

struct MPFilter : public osmium::TagsFilter
{

    MPFilter() : osmium::TagsFilter(true)
    {
        add_rule(false, "type");
        add_rule(false, "created_by");
        add_rule(false, "source");
        add_rule(false, "note");
    }

}; // struct MPFilter

class CheckHandler : public osmium::handler::Handler
{

    Outputs &m_outputs;
    options_type m_options;
    stats_type m_stats;
    MPFilter m_mp_filter;

    static std::vector<osmium::unsigned_object_id_type>
    find_duplicate_ways(osmium::Relation const &relation)
    {
        std::vector<osmium::unsigned_object_id_type> duplicate_ids;

        std::vector<osmium::unsigned_object_id_type> way_ids;
        way_ids.reserve(relation.members().size());
        for (auto const &member : relation.members()) {
            if (member.type() == osmium::item_type::way) {
                way_ids.push_back(member.positive_ref());
            }
        }
        std::sort(way_ids.begin(), way_ids.end());

        auto it = way_ids.begin();
        while (it != way_ids.end()) {
            it = std::adjacent_find(it, way_ids.end());
            if (it != way_ids.end()) {
                duplicate_ids.push_back(*it);
                ++it;
            }
        }

        return duplicate_ids;
    }

    void multipolygon_relation(osmium::Relation const &relation)
    {
        if (relation.members().empty()) {
            return;
        }

        std::uint64_t node_member = 0;
        std::uint64_t relation_member = 0;
        std::uint64_t unknown_role = 0;
        std::uint64_t empty_role = 0;

        for (auto const &member : relation.members()) {
            switch (member.type()) {
            case osmium::item_type::node:
                ++node_member;
                break;
            case osmium::item_type::way:
                if (member.role()[0] == '\0') {
                    ++empty_role;
                } else if ((std::strcmp(member.role(), "inner") != 0) &&
                           (std::strcmp(member.role(), "outer") != 0)) {
                    ++unknown_role;
                }
                break;
            case osmium::item_type::relation:
                ++relation_member;
                break;
            default:
                break;
            }
        }

        if (node_member != 0U) {
            m_outputs["multipolygon_node_member"].add(relation, node_member);
        }

        if (relation_member != 0U) {
            m_outputs["multipolygon_relation_member"].add(relation,
                                                          relation_member);
        }

        if (unknown_role != 0U) {
            m_outputs["multipolygon_unknown_role"].add(relation, unknown_role);
        }

        if (empty_role != 0U) {
            m_outputs["multipolygon_empty_role"].add(relation, empty_role);
        }

        if (relation.members().size() == 1 &&
            relation.members().cbegin()->type() == osmium::item_type::way) {
            m_outputs["multipolygon_single_way"].add(relation);
        }

        auto const duplicates = find_duplicate_ways(relation);
        if (!duplicates.empty()) {
            m_outputs["multipolygon_duplicate_way"].add(relation, 1,
                                                        duplicates);
        }

        if (relation.tags().size() == 1 ||
            std::none_of(relation.tags().cbegin(), relation.tags().cend(),
                         std::cref(m_mp_filter))) {
            m_outputs["multipolygon_old_style"].add(relation);
            return;
        }

        char const *area = relation.tags().get_value_by_key("area");
        if (area) {
            m_outputs["multipolygon_area_tag"].add(relation);
        }

        char const *boundary = relation.tags().get_value_by_key("boundary");
        if (boundary) {
            if (!std::strcmp(boundary, "administrative")) {
                m_outputs["multipolygon_boundary_administrative_tag"].add(
                    relation);
            } else {
                m_outputs["multipolygon_boundary_other_tag"].add(relation);
            }
        }
    }

    void boundary_relation(const osmium::Relation &relation)
    {
        if (relation.members().empty()) {
            return;
        }

        uint64_t empty_role = 0;
        for (const auto &member : relation.members()) {
            if (member.role()[0] == '\0') {
                ++empty_role;
            }
        }
        if (empty_role != 0U) {
            m_outputs["boundary_empty_role"].add(relation, empty_role);
        }

        const auto duplicates = find_duplicate_ways(relation);
        if (!duplicates.empty()) {
            m_outputs["boundary_duplicate_way"].add(relation, 1, duplicates);
        }

        const char *area = relation.tags().get_value_by_key("area");
        if (area) {
            m_outputs["boundary_area_tag"].add(relation);
        }

        // is boundary:historic or historic:boundary also okay?
        const char *boundary = relation.tags().get_value_by_key("boundary");
        if (!boundary) {
            m_outputs["boundary_no_boundary_tag"].add(relation);
        }
    }

public:
    CheckHandler(Outputs *outputs, options_type const &options)
    : m_outputs(*outputs), m_options(options)
    {}

    void relation(osmium::Relation const &relation)
    {
        if (relation.timestamp() >= m_options.before_time) {
            return;
        }

        ++m_stats.relations;
        m_stats.relation_members += relation.members().size();

        if (relation.members().empty()) {
            m_outputs["relation_no_members"].add(relation);
        }

        if (relation.members().size() >= min_members_of_large_relations) {
            m_outputs["relation_large"].add(relation);
        }

        if (relation.tags().empty()) {
            m_outputs["relation_no_tag"].add(relation);
            return;
        }

        char const *type = relation.tags().get_value_by_key("type");
        if (!type) {
            m_outputs["relation_no_type_tag"].add(relation);
            return;
        }

        if (relation.tags().size() == 1) {
            m_outputs["relation_only_type_tag"].add(relation);
        }

        if (!std::strcmp(type, "multipolygon")) {
            multipolygon_relation(relation);
        } else if (!std::strcmp(type, "boundary")) {
            boundary_relation(relation);
        }
    }

    [[nodiscard]] stats_type const &stats() const noexcept { return m_stats; }

    void close()
    {
        m_outputs.for_all([](Output &output) { output.close_writer_rel(); });
    }

}; // class CheckHandler

static void print_help()
{
    std::cout << program_name << " [OPTIONS] OSM-FILE OUTPUT-DIR\n\n"
              << "Find relations with problems.\n"
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

static void write_data_files(std::string const &input_filename,
                             Outputs *outputs)
{
    osmium::io::Reader reader{input_filename};
    osmium::ProgressBar progress_bar{reader.file_size(), display_progress()};

    while (osmium::memory::Buffer buffer = reader.read()) {
        progress_bar.update(reader.offset());
        for (auto const &object : buffer.select<osmium::OSMObject>()) {
            outputs->for_all(
                [&](Output &output) { output.write_to_all(object); });
        }
    }

    progress_bar.done();
    reader.close();

    outputs->for_all([](Output &output) { output.close_writer_all(); });
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

    osmium::io::File file{input_filename};
    osmium::io::Reader reader{file, osmium::osm_entity_bits::relation};
    if (file.format() == osmium::io::file_format::pbf &&
        !has_locations_on_ways(reader.header())) {
        std::cerr << "Input file must have locations on ways.\n";
        return 2;
    }

    osmium::io::Header header;
    header.set("generator", program_name);

    Outputs outputs{output_dirname, "geoms-relation-problems", header};
    outputs.add_output("relation_no_members", false, false);
    outputs.add_output("relation_no_tag");
    outputs.add_output("relation_only_type_tag");
    outputs.add_output("relation_no_type_tag");
    outputs.add_output("relation_large");
    outputs.add_output("multipolygon_node_member", true, false);
    outputs.add_output("multipolygon_relation_member", false, false);
    outputs.add_output("multipolygon_unknown_role", false, true);
    outputs.add_output("multipolygon_empty_role", false, true);
    outputs.add_output("multipolygon_area_tag", false, true);
    outputs.add_output("multipolygon_boundary_administrative_tag", false, true);
    outputs.add_output("multipolygon_boundary_other_tag", false, true);
    outputs.add_output("multipolygon_old_style", false, false);
    outputs.add_output("multipolygon_single_way", false, true);
    outputs.add_output("multipolygon_duplicate_way", false, true);
    outputs.add_output("boundary_empty_role", false, true);
    outputs.add_output("boundary_duplicate_way", false, true);
    outputs.add_output("boundary_area_tag", false, true);
    outputs.add_output("boundary_no_boundary_tag", false, true);

    LastTimestampHandler last_timestamp_handler;
    CheckHandler handler{&outputs, options};

    vout << "Reading relations and checking for problems...\n";
    osmium::ProgressBar progress_bar{reader.file_size(), display_progress()};
    while (osmium::memory::Buffer buffer = reader.read()) {
        progress_bar.update(reader.offset());
        osmium::apply(buffer, last_timestamp_handler, handler);
    }
    progress_bar.done();
    reader.close();

    outputs.for_all([&](Output &output) { output.prepare(); });

    vout << "Writing out data files...\n";
    write_data_files(input_filename, &outputs);

    vout << "Writing out stats...\n";
    auto const last_time{last_timestamp_handler.get_timestamp()};
    write_stats(output_dirname + "/stats-relation-problems.db", last_time,
                [&](std::function<void(char const *, uint64_t)> &add_stat) {
                    add_stat("relation_count", handler.stats().relations);
                    add_stat("relation_member_count",
                             handler.stats().relation_members);
                    outputs.for_all([&](Output &output) {
                        add_stat(output.name(), output.counter());
                    });
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
