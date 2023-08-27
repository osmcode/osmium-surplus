
#include "filter.hpp"

#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/tags/tags_filter.hpp>
#include <osmium/visitor.hpp>

#include <lyra.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <utility>

class RewriteHandler : public osmium::handler::Handler
{

    osmium::memory::Buffer *m_buffer;
    osmium::TagsFilter const &m_filter;

    template <typename T>
    void copy_attributes(T &builder, osmium::OSMObject const &object)
    {
        builder.set_id(object.id())
            .set_version(object.version())
            .set_changeset(object.changeset())
            .set_timestamp(object.timestamp())
            .set_uid(object.uid())
            .set_user(object.user());
    }

    void copy_tags(osmium::builder::Builder *parent,
                   osmium::TagList const &tags)
    {
        osmium::builder::TagListBuilder builder{*parent};

        for (auto const &tag : tags) {
            if (!m_filter(tag)) {
                builder.add_tag(tag);
            }
        }
    }

public:
    explicit RewriteHandler(osmium::memory::Buffer *buffer,
                            osmium::TagsFilter const &filter)
    : m_buffer(buffer), m_filter(filter)
    {
        assert(buffer);
    }

    void node(osmium::Node const &node)
    {
        {
            osmium::builder::NodeBuilder builder{*m_buffer};
            copy_attributes(builder, node);
            builder.set_location(node.location());
            copy_tags(&builder, node.tags());
        }
        m_buffer->commit();
    }

    void way(osmium::Way const &way)
    {
        {
            osmium::builder::WayBuilder builder{*m_buffer};
            copy_attributes(builder, way);
            copy_tags(&builder, way.tags());
            builder.add_item(way.nodes());
        }
        m_buffer->commit();
    }

    void relation(osmium::Relation const &relation)
    {
        {
            osmium::builder::RelationBuilder builder{*m_buffer};
            copy_attributes(builder, relation);
            copy_tags(&builder, relation.tags());
            builder.add_item(relation.members());
        }
        m_buffer->commit();
    }

}; // class RewriteHandler

int main(int argc, char *argv[])
{
    try {
        std::string input_filename;
        std::string output_filename;
        std::string filter_filename;
        bool help = false;

        // clang-format off
        auto const cli
            = lyra::opt(output_filename, "OUTPUT-FILE")
                ["-o"]["--output"]
                ("output file")
            | lyra::opt(filter_filename, "FILTER-FILE")
                ["-e"]["--expressions"]
                ("filter expressions file")
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
                << "\nRemove tags matching any expressions in filter file.\n";
            return 0;
        }

        if (input_filename.empty()) {
            std::cerr << "Missing input filename. Try '-h'.\n";
            return 1;
        }

        if (output_filename.empty()) {
            std::cerr << "Missing output filename. Try '-h'.\n";
            return 1;
        }

        if (filter_filename.empty()) {
            std::cerr << "Missing filter filename. Try '-h'.\n";
            return 1;
        }

        auto const filter = load_filter_patterns(filter_filename);

        osmium::io::File const input_file{input_filename};
        osmium::io::File const output_file{output_filename};

        osmium::io::Reader reader{input_file};
        osmium::io::Writer writer{output_file, osmium::io::overwrite::allow};

        while (auto const buffer = reader.read()) {
            osmium::memory::Buffer output_buffer{
                buffer.committed(), osmium::memory::Buffer::auto_grow::yes};
            RewriteHandler handler{&output_buffer, filter};
            osmium::apply(buffer, handler);
            writer(std::move(output_buffer));
        }

        writer.close();
        reader.close();

    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
