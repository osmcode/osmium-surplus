
#include <osmium/index/id_set.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>

#include <lyra.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

static std::string percent(std::uint64_t fraction, std::uint64_t all,
                           char const *text) noexcept
{
    auto const p = fraction * 100 / all;
    return " (" + std::to_string(p) + "% of " + text + ")";
}

int main(int argc, char *argv[])
{
    try {
        std::string input_filename;
        std::string output_directory;
        bool help = false;

        // clang-format off
        auto const cli
            = lyra::opt(output_directory, "DIR")
                ["-o"]["--output-dir"]
                ("output directory")
            | lyra::help(help)
            | lyra::arg(input_filename, "FILENAME")
                ("input file");
        // clang-format on

        auto const result = cli.parse(lyra::args(argc, argv));
        if (!result) {
            std::cerr << "Error in command line: " << result.message() << '\n';
            return 1;
        }

        if (help) {
            std::cout << cli << "\nCreate statistics on way nodes.\n";
            return 0;
        }

        if (input_filename.empty()) {
            std::cerr << "Missing input filename. Try '-h'.\n";
            return 1;
        }

        osmium::index::IdSetDense<osmium::unsigned_object_id_type> in_way;
        osmium::index::IdSetDense<osmium::unsigned_object_id_type>
            in_multiple_ways;
        osmium::index::IdSetDense<osmium::unsigned_object_id_type> in_relation;

        osmium::io::File const input_file{input_filename};

        std::uint64_t count_ways = 0;
        std::uint64_t count_relations = 0;

        osmium::io::Reader reader1{input_file,
                                   osmium::osm_entity_bits::way |
                                       osmium::osm_entity_bits::relation};
        while (auto const buffer = reader1.read()) {
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                if (object.type() == osmium::item_type::way) {
                    ++count_ways;
                    auto const &way = static_cast<osmium::Way const &>(object);
                    if (way.nodes().empty()) {
                        continue;
                    }
                    auto const *it = way.nodes().begin();
                    if (way.is_closed()) {
                        ++it;
                    }
                    for (; it != way.nodes().end(); ++it) {
                        if (in_way.get(it->positive_ref())) {
                            in_multiple_ways.set(it->positive_ref());
                        } else {
                            in_way.set(it->positive_ref());
                        }
                    }
                } else {
                    ++count_relations;
                    for (auto const &member :
                         static_cast<osmium::Relation const &>(object)
                             .members()) {
                        if (member.type() == osmium::item_type::node) {
                            in_relation.set(member.positive_ref());
                        }
                    }
                }
            }
        }
        reader1.close();

        std::uint64_t count_nodes = 0;
        std::uint64_t count_nodes_with_tags = 0;
        std::uint64_t count_nodes_in_way = 0;
        std::uint64_t count_nodes_with_tags_in_way = 0;
        std::uint64_t count_nodes_in_multiple_ways = 0;
        std::uint64_t count_nodes_in_relation = 0;

        std::unique_ptr<osmium::io::Writer> writer_nodes_with_tags_in_way;
        if (!output_directory.empty()) {
            writer_nodes_with_tags_in_way =
                std::make_unique<osmium::io::Writer>(
                    output_directory + "/nodes_with_tags_in_way.osm.pbf");
        }

        osmium::io::Reader reader2{input_file, osmium::osm_entity_bits::node};
        while (auto const buffer = reader2.read()) {
            for (auto const &node : buffer.select<osmium::Node>()) {
                ++count_nodes;
                if (!node.tags().empty()) {
                    ++count_nodes_with_tags;
                }
                if (in_way.get(node.positive_id())) {
                    ++count_nodes_in_way;
                    if (!node.tags().empty()) {
                        ++count_nodes_with_tags_in_way;
                        if (writer_nodes_with_tags_in_way) {
                            (*writer_nodes_with_tags_in_way)(node);
                        }
                    }
                    if (in_multiple_ways.get(node.positive_id())) {
                        ++count_nodes_in_multiple_ways;
                    }
                }
                if (in_relation.get(node.positive_id())) {
                    ++count_nodes_in_relation;
                }
            }
        }

        if (writer_nodes_with_tags_in_way) {
            writer_nodes_with_tags_in_way->close();
        }
        reader2.close();

        std::cout
            << "nodes: " << count_nodes << "\nways: " << count_ways
            << "\nrelations: " << count_relations
            << "\nnodes with tags: " << count_nodes_with_tags
            << percent(count_nodes_with_tags, count_nodes, "all nodes")
            << "\nnodes in way: " << count_nodes_in_way
            << percent(count_nodes_in_way, count_nodes, "all nodes")
            << "\nnodes with tags in way: " << count_nodes_with_tags_in_way
            << percent(count_nodes_with_tags_in_way, count_nodes, "all nodes")
            << percent(count_nodes_with_tags_in_way, count_nodes_with_tags,
                       "tagged nodes")
            << "\nnodes in multiple ways: " << count_nodes_in_multiple_ways
            << percent(count_nodes_in_multiple_ways, count_nodes, "all nodes")
            << "\nnodes in relation: " << count_nodes_in_relation
            << percent(count_nodes_in_relation, count_nodes, "all nodes")
            << '\n';

    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
