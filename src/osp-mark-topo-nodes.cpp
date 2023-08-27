
#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/index/id_set.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>

#include <lyra.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

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
            std::cout
                << cli
                << "\nAdd tags to nodes in multiple ways and relations.\n";
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

        osmium::io::Reader reader1{input_file,
                                   osmium::osm_entity_bits::way |
                                       osmium::osm_entity_bits::relation};
        while (auto const buffer = reader1.read()) {
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                if (object.type() == osmium::item_type::way) {
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

        constexpr std::size_t const initial_buffer_size = 1024;
        osmium::memory::Buffer outbuffer{initial_buffer_size};
        osmium::io::Writer writer{output_directory +
                                  "/with-marked-topo-nodes.osm.pbf"};

        osmium::io::Reader reader2{input_file};
        while (auto const buffer = reader2.read()) {
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                if (object.type() == osmium::item_type::node) {
                    bool const in_mw =
                        in_multiple_ways.get(object.positive_id());
                    bool const in_rel = in_relation.get(object.positive_id());
                    if (!object.tags().empty() || (!in_mw && !in_rel)) {
                        writer(object);
                    } else {
                        {
                            osmium::builder::NodeBuilder builder{outbuffer};
                            builder.set_location(
                                static_cast<osmium::Node const &>(object)
                                    .location());
                            builder.set_id(object.id());
                            builder.set_version(object.version());
                            builder.set_timestamp(object.timestamp());
                            builder.set_changeset(object.changeset());
                            builder.set_uid(object.uid());
                            builder.set_user(object.user());
                            if (in_mw) {
                                builder.add_tags(
                                    {std::make_pair("_in", "ways")});
                            } else if (in_rel) {
                                builder.add_tags(
                                    {std::make_pair("_in", "rel")});
                            }
                        }
                        outbuffer.commit();
                        writer(*outbuffer.cbegin());
                        outbuffer.clear();
                    }
                } else {
                    writer(object);
                }
            }
        }

        writer.close();
        reader2.close();
    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
