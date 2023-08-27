
#include "app.hpp"

#include <osmium/io/any_compression.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/input_iterator.hpp>
#include <osmium/osm/changeset.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/string.hpp>

#include <unordered_set>
#include <utility>
#include <vector>

/* ========================================================================= */

class App : public BasicApp
{
    std::string m_changeset_input;
    std::string m_changeset_error;

public:
    App()
    : BasicApp("osp-changeset-check-timestamps",
               "Check timestamps in changesets", with_output::file)
    {
        add_option("-c,--changeset", m_changeset_input, "Changeset file")
            ->type_name("CHANGESET-FILE")
            ->required();
        add_option("-e,--changeset-error", m_changeset_error,
                   "Changeset error file")
            ->type_name("CHANGESET-FILE")
            ->required();
    }

    std::vector<std::pair<osmium::Timestamp, osmium::Timestamp>>
    read_changset_times(osmium::io::File const &changeset_input_file)
    {
        std::vector<std::pair<osmium::Timestamp, osmium::Timestamp>> ranges;

        osmium::io::Reader reader{changeset_input_file,
                                  osmium::osm_entity_bits::changeset};
        osmium::ProgressBar progress_bar{reader.file_size(), verbose()};

        auto const input_range =
            osmium::io::make_input_iterator_range<osmium::Changeset>(reader);

        std::size_t n = 0;
        for (osmium::Changeset const &changeset : input_range) {
            if (changeset.id() >= ranges.size()) {
                ranges.resize(changeset.id() + 100);
            }
            ranges[changeset.id()] =
                std::make_pair(changeset.created_at(), changeset.closed_at());

            ++n;
            if (n > 1000) {
                n = 0;
                progress_bar.update(reader.offset());
            }
        }

        progress_bar.done();
        reader.close();

        return ranges;
    }

    std::unordered_set<osmium::changeset_id_type> read_osm_data(
        osmium::io::File const &data_input_file,
        osmium::io::File const &data_error_file,
        std::vector<std::pair<osmium::Timestamp, osmium::Timestamp>> const
            &ranges)
    {
        std::unordered_set<osmium::changeset_id_type> changesets;

        osmium::io::Reader reader{data_input_file,
                                  osmium::osm_entity_bits::nwr};
        osmium::io::Writer writer{data_error_file,
                                  osmium::io::overwrite::allow};

        osmium::ProgressBar progress_bar{reader.file_size(), verbose()};
        while (osmium::memory::Buffer buffer = reader.read()) {
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                progress_bar.update(reader.offset());
                if (object.changeset() < ranges.size()) {
                    auto const &r = ranges[object.changeset()];
                    if (!(r.first <= object.timestamp() &&
                          object.timestamp() <= r.second)) {
                        writer(object);
                        changesets.insert(object.changeset());
                    }
                } else {
                    vout() << "Changeset id " << object.changeset()
                           << " not in changeset file. Ignoring it.\n";
                }
            }
        }
        progress_bar.done();

        writer.close();
        reader.close();

        return changesets;
    }

    void write_errors(
        osmium::io::File const &changeset_input_file,
        osmium::io::File const &changeset_error_file,
        std::unordered_set<osmium::changeset_id_type> const &changesets)
    {
        osmium::io::Reader reader{changeset_input_file,
                                  osmium::osm_entity_bits::changeset};
        osmium::io::Writer writer{changeset_error_file,
                                  osmium::io::overwrite::allow};

        osmium::ProgressBar progress_bar{reader.file_size(), verbose()};
        while (osmium::memory::Buffer buffer = reader.read()) {
            progress_bar.update(reader.offset());
            for (auto const &changeset : buffer.select<osmium::Changeset>()) {
                if (changesets.count(changeset.id())) {
                    writer(changeset);
                }
            }
        }
        progress_bar.done();

        writer.close();
        reader.close();
    }

    void run()
    {
        vout() << "        Reading changesets from '" << m_changeset_input
               << "'.\n";
        vout() << "        Writing changeset errors to '" << m_changeset_error
               << "'.\n";

        osmium::io::File const changeset_input_file{m_changeset_input};
        osmium::io::File const data_input_file{input()};
        osmium::io::File const changeset_error_file{m_changeset_error};
        osmium::io::File const data_error_file{output()};

        vout() << "Reading changesets...\n";
        auto ranges = read_changset_times(changeset_input_file);

        vout() << "Reading OSM data...\n";
        auto const changesets =
            read_osm_data(data_input_file, data_error_file, ranges);
        vout() << "Found " << changesets.size() << " changesets with errors\n";

        ranges.clear();

        vout() << "Reading changesets again and writing out errors...\n";
        write_errors(changeset_input_file, changeset_error_file, changesets);
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
