
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/util/verbose_output.hpp>

#include <lyra.hpp>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

static char const *const bad_chars = "=+/&<>;'\"?%#@\\, \t\r\n\f";

bool chars_are_good(char const *str)
{
    for (; *str; ++str) {
        if (!std::isalnum(*str) && *str != ':' && *str != '_' && *str != '-') {
            return false;
        }
    }

    return true;
}

int check_chars(osmium::OSMObject const &object)
{
    bool undecided = false;

    for (auto const &tag : object.tags()) {
        if (std::strpbrk(tag.key(), bad_chars)) {
            return 2;
        }
        if (!chars_are_good(tag.key())) {
            undecided = true;
        }
    }

    if (object.type() == osmium::item_type::relation) {
        for (auto const &member :
             static_cast<osmium::Relation const &>(object).members()) {
            if (std::strpbrk(member.role(), bad_chars)) {
                return 2;
            }
            if (!chars_are_good(member.role())) {
                undecided = true;
            }
        }
    }

    return undecided ? 1 : 0;
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
            std::cout
                << cli
                << "\nFilter objects with anomalous chars in keys and roles.\n";
            return 0;
        }

        if (input_filename.empty()) {
            std::cerr << "Missing input filename. Try '-h'.\n";
            return 1;
        }

        osmium::io::File input_file{input_filename};

        osmium::VerboseOutput vout{true};

        osmium::io::Reader reader{input_file};
        osmium::io::Writer writer_bad{output_directory + "/bad-chars.osm.pbf",
                                      osmium::io::overwrite::allow};
        osmium::io::Writer writer_undecided{output_directory +
                                                "/undecided-chars.osm.pbf",
                                            osmium::io::overwrite::allow};

        while (auto const buffer = reader.read()) {
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                int level = check_chars(object);
                if (level == 1) {
                    writer_undecided(object);
                } else if (level == 2) {
                    writer_bad(object);
                }
            }
        }
        reader.close();

        writer_undecided.close();
        writer_bad.close();

        vout << "Done.\n";
    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
