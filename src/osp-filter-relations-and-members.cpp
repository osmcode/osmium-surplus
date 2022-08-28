
#include "app.hpp"
#include "util.hpp"

#include <osmium/index/id_set.hpp>
#include <osmium/index/nwr_array.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/util/file.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/verbose_output.hpp>

#include <array>
#include <cstdlib>
#include <exception>
#include <string>

/* ========================================================================= */

constexpr static std::array<osmium::item_type, 3> const nwr{
    osmium::item_type::node, osmium::item_type::way,
    osmium::item_type::relation};

using idset_type = osmium::index::IdSetDense<osmium::unsigned_object_id_type>;

static void read_relations(osmium::io::File const &input_file,
                           osmium::nwr_array<idset_type> *ids)
{
    osmium::io::Reader reader{input_file, osmium::osm_entity_bits::relation};

    while (auto const buffer = reader.read()) {
        for (auto const &relation : buffer.select<osmium::Relation>()) {
            for (auto const &member : relation.members()) {
                (*ids)(member.type()).set(member.positive_ref());
            }
        }
    }

    reader.close();
}

static void read_ways(osmium::io::File const &input_file,
                      osmium::nwr_array<idset_type> *ids)
{
    osmium::io::Reader reader{input_file, osmium::osm_entity_bits::way};

    while (auto const buffer = reader.read()) {
        for (auto const &way : buffer.select<osmium::Way>()) {
            if (ids->ways().get(way.positive_id())) {
                for (auto const &nr : way.nodes()) {
                    ids->nodes().set(nr.positive_ref());
                }
            }
        }
    }

    reader.close();
}

/* ========================================================================= */

class App : public BasicApp
{
public:
    App()
    : BasicApp("osp-filter-relations-and-members",
               "Filter relations and their members from OSM file",
               with_output::file)
    {}

    void run()
    {
        osmium::io::File const input_file{input()};

        osmium::nwr_array<idset_type> ids;

        vout() << "Reading relations...\n";
        read_relations(input_file, &ids);

        vout() << "Reading ways...\n";
        read_ways(input_file, &ids);

        vout() << "Copying data...\n";
        osmium::io::Reader reader{input_file};
        osmium::io::Writer writer{output(), osmium::io::overwrite::allow};

        osmium::nwr_array<std::size_t> counts_in{};
        osmium::nwr_array<std::size_t> counts_out{};

        std::size_t const file_size = osmium::file_size(input());
        osmium::ProgressBar progress_bar{file_size, vout().verbose()};
        while (auto const buffer = reader.read()) {
            progress_bar.update(reader.offset());
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                ++counts_in(object.type());
                if (object.type() == osmium::item_type::relation ||
                    ids(object.type()).get(object.positive_id())) {
                    ++counts_out(object.type());
                    writer(object);
                    ids(object.type()).unset(object.positive_id());
                }
            }
        }

        writer.close();
        reader.close();
        progress_bar.done();
        vout() << "Done processing.\n";

        for (auto t : nwr) {
            vout() << fmt::format("Copied {}/{} ({}%) {}s.\n", counts_out(t),
                                  counts_in(t),
                                  percent(counts_out(t), counts_in(t)),
                                  osmium::item_type_to_name(t));
        }
        for (auto t : nwr) {
            if (ids(t).empty()) {
                vout() << fmt::format("All {}s found.\n",
                                      osmium::item_type_to_name(t));
            } else {
                vout() << fmt::format("Missing {} {}s.\n", ids(t).size(),
                                      osmium::item_type_to_name(t));
            }
        }
        for (auto t : nwr) {
            vout() << fmt::format("Memory used for {} id indexes: {} MBytes.\n",
                                  osmium::item_type_to_name(t),
                                  mbytes(ids(t).used_memory()));
        }
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
