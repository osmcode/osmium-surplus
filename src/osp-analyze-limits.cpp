
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/util/verbose_output.hpp>

#include <lyra.hpp>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

static std::vector<std::size_t> hist_keys;
static std::vector<std::size_t> hist_values;
static std::vector<std::size_t> hist_roles;
static std::vector<std::size_t> hist_way_nodes;
static std::vector<std::size_t> hist_members;

void increment(std::vector<std::size_t> *hist, std::size_t len)
{
    if (hist->size() <= len) {
        hist->resize(len + 1);
    }
    ++(*hist)[len];
}

void output_hist(std::string const &dir, char const *name,
                 std::vector<std::size_t> const &hist)
{
    std::ofstream out{dir + "/hist-" + name + ".csv"};
    for (std::size_t len = 0; len < hist.size(); ++len) {
        out << len << ',' << hist[len] << '\n';
    }
}

std::tuple<std::size_t, std::size_t, std::size_t, std::size_t, bool>
check_limits(osmium::OSMObject const &object)
{
    std::size_t max_len_keys = 0;
    std::size_t max_len_values = 0;
    std::size_t max_len_roles = 0;
    std::size_t tags_bytes = 0;
    bool empty_key_or_role = false;

    for (auto const &tag : object.tags()) {
        auto const len_key = std::strlen(tag.key());
        auto const len_value = std::strlen(tag.value());

        increment(&hist_keys, len_key);
        increment(&hist_values, len_value);

        tags_bytes += len_key;
        tags_bytes += len_value;

        if (len_key > max_len_keys) {
            max_len_keys = len_key;
        }
        if (len_value > max_len_values) {
            max_len_values = len_value;
        }
        if (len_key == 0 || len_value == 0) {
            empty_key_or_role = true;
        }
    }

    if (object.type() == osmium::item_type::way) {
        increment(&hist_way_nodes,
                  static_cast<osmium::Way const &>(object).nodes().size());
    } else if (object.type() == osmium::item_type::relation) {
        auto const &members =
            static_cast<osmium::Relation const &>(object).members();
        increment(&hist_members, members.size());
        for (auto const &member : members) {
            auto const len = std::strlen(member.role());

            increment(&hist_roles, len);

            if (len > max_len_roles) {
                max_len_roles = len;
            }
        }
    }

    return {max_len_keys, max_len_values, max_len_roles, tags_bytes,
            empty_key_or_role};
}

int main(int argc, char *argv[])
{
    try {
        std::string input_filename;
        std::string output_directory{"."};
        std::size_t max_key_length = 63;
        std::size_t max_value_length = 200;
        std::size_t max_role_length = 63;
        std::size_t max_tags_count = 50;
        std::size_t max_tags_bytes = 1024;
        bool help = false;

        // clang-format off
        auto const cli
            = lyra::opt(output_directory, "DIR")
                ["-o"]["--output-dir"]
                ("output directory (default: cwd)")
            | lyra::opt(max_key_length, "LENGTH")
                ["-k"]["--max-key-length"]
                ("max key length (default: 63)")
            | lyra::opt(max_value_length, "LENGTH")
                ["-v"]["--max-value-length"]
                ("max value length (default: 200)")
            | lyra::opt(max_role_length, "LENGTH")
                ["-r"]["--max-role-length"]
                ("max role length (default: 63)")
            | lyra::opt(max_tags_count, "COUNT")
                ["-t"]["--max-tags-count"]
                ("max tags count (default: 50)")
            | lyra::opt(max_tags_bytes, "BYTES")
                ["-b"]["--max-tags-bytes"]
                ("max tags bytes (default: 1024)")
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
            std::cout << cli
                      << "\nExtract objects with unusual number of 'things'.\n";
            return 0;
        }

        if (input_filename.empty()) {
            std::cerr << "Missing input filename. Try '-h'.\n";
            return 1;
        }

        std::vector<std::size_t> hist_tags_count;
        std::vector<std::size_t> hist_tags_bytes;

        osmium::io::File input_file{input_filename};

        osmium::VerboseOutput vout{true};

        osmium::io::Reader reader{input_file};
        osmium::io::Writer writer_key_length{output_directory +
                                                 "/key-length.osm.pbf",
                                             osmium::io::overwrite::allow};
        osmium::io::Writer writer_value_length{output_directory +
                                                   "/value-length.osm.pbf",
                                               osmium::io::overwrite::allow};
        osmium::io::Writer writer_role_length{output_directory +
                                                  "/role-length.osm.pbf",
                                              osmium::io::overwrite::allow};
        osmium::io::Writer writer_empty{output_directory +
                                            "/empty-key-or-value.osm.pbf",
                                        osmium::io::overwrite::allow};
        osmium::io::Writer writer_tags_count{output_directory +
                                                 "/tags-count.osm.pbf",
                                             osmium::io::overwrite::allow};
        osmium::io::Writer writer_tags_bytes{output_directory +
                                                 "/tags-bytes.osm.pbf",
                                             osmium::io::overwrite::allow};

        while (auto const buffer = reader.read()) {
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                increment(&hist_tags_count, object.tags().size());
                if (object.tags().size() > max_tags_count) {
                    writer_tags_count(object);
                }
                auto [lk, lv, lr, tags_bytes, empty] = check_limits(object);
                increment(&hist_tags_bytes, tags_bytes);
                if (lk > max_key_length) {
                    writer_key_length(object);
                }
                if (lv > max_value_length) {
                    writer_value_length(object);
                }
                if (lr > max_role_length) {
                    writer_role_length(object);
                }
                if (tags_bytes > max_tags_bytes) {
                    writer_tags_bytes(object);
                }
                if (empty) {
                    writer_empty(object);
                }
            }
        }

        writer_tags_bytes.close();
        writer_tags_count.close();
        writer_empty.close();
        writer_role_length.close();
        writer_value_length.close();
        writer_key_length.close();

        reader.close();

        output_hist(output_directory, "key-lengths", hist_keys);
        output_hist(output_directory, "value-lengths", hist_values);
        output_hist(output_directory, "role-lengths", hist_roles);
        output_hist(output_directory, "tags-count", hist_tags_count);
        output_hist(output_directory, "tags-bytes", hist_tags_bytes);
        output_hist(output_directory, "way-nodes-count", hist_way_nodes);
        output_hist(output_directory, "members-count", hist_members);

        vout << "Done.\n";
    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
