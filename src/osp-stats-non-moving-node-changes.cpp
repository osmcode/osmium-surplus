
#include <osmium/diff_handler.hpp>
#include <osmium/diff_visitor.hpp>
#include <osmium/io/any_input.hpp>

#include <lyra.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

class StatsHandler : public osmium::diff_handler::DiffHandler {

    std::uint64_t count_node_changes = 0;
    std::uint64_t count_node_changes_same_location = 0;
    std::uint64_t count_node_changes_tagged = 0;
    std::uint64_t count_node_changes_same_location_tagged = 0;

public:
    void node(const osmium::DiffNode& dnode) {
        if (dnode.first()) {
            return;
        }

        bool const is_tagged = !dnode.prev().tags().empty() || !dnode.curr().tags().empty();

        ++count_node_changes;
        if (is_tagged) {
            ++count_node_changes_tagged;
        }

        if (dnode.prev().location() == dnode.curr().location()) {
            ++count_node_changes_same_location;
            if (is_tagged) {
                ++count_node_changes_same_location_tagged;
            }
        }
    }

    void print_result() {
        std::cout << "node changes:              " << count_node_changes << '\n';
        std::cout << " w/same location:          " << count_node_changes_same_location << '\n';
        std::cout << "node changes (tagged):     " << count_node_changes_tagged << '\n';
        std::cout << " w/same location (tagged): " << count_node_changes_same_location_tagged << '\n';
    }

}; // class StatsHandler

int main(int argc, char *argv[])
{
    try {
        std::string input_filename;
        bool help = false;

        // clang-format off
        auto const cli
            = lyra::help(help)
            | lyra::arg(input_filename, "FILENAME")
                ("input file");
        // clang-format on

        auto const result = cli.parse(lyra::args(argc, argv));
        if (!result) {
            std::cerr << "Error in command line: " << result.message() << '\n';
            return 1;
        }

        if (help) {
            std::cout << cli << "\nCreate statistics on node changes.\n";
            return 0;
        }

        if (input_filename.empty()) {
            std::cerr << "Missing input filename. Try '-h'.\n";
            return 1;
        }

        StatsHandler statshandler{};

        osmium::io::Reader reader{input_filename, osmium::osm_entity_bits::node};
        osmium::apply_diff(reader, statshandler);
        reader.close();

        statshandler.print_result();

    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
