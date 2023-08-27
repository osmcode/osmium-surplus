
#include <osmium/io/any_input.hpp>
#include <osmium/util/memory_mapping.hpp>

#include <lyra.hpp>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <fcntl.h>
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
    constexpr std::size_t const max_nodes_in_ways = 50;
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
            std::cout << cli << "\nCreate statistics for way nodes index.\n";
            return 0;
        }

        if (input_filename.empty()) {
            std::cerr << "Missing input filename. Try '-h'.\n";
            return 1;
        }

        auto const start = std::time(nullptr);

        osmium::TypedMemoryMapping<uint32_t> ways{10ULL * 1024ULL * 1024ULL * 1024ULL};

        std::vector<std::pair<osmium::unsigned_object_id_type, osmium::unsigned_object_id_type>> extras;
        extras.reserve(1024ULL * 1024ULL * 1024ULL);

        osmium::unsigned_object_id_type largest_node_id = 0;

        std::cerr << ((std::time(nullptr) - start) / 60) << " mins: Reading data...\n";
        osmium::io::Reader reader{input_filename, osmium::osm_entity_bits::way};
        while (auto const buffer = reader.read()) {
            for (auto const &way : buffer.select<osmium::Way>()) {
                for (auto const &nr : way.nodes()) {
                    auto const id = nr.positive_ref();
                    if (id > largest_node_id) {
                        largest_node_id = id;
                        if (id > ways.size()) {
                            ways.resize(id + 10000);
                        }
                    }
                    uint32_t *const ptr = ways.begin() + id;
                    if (*ptr == 0) {
                        *ptr = static_cast<uint32_t>(way.positive_id());
                    } else {
                        extras.emplace_back(id, way.positive_id());
                    }
                }
            }
        }
        reader.close();

        std::cerr << ((std::time(nullptr) - start) / 60) << " mins: Reading done.\n";

        ways.resize(largest_node_id + 1);

        std::cerr << ((std::time(nullptr) - start) / 60) << " mins: Counting empty slots...\n";
        std::size_t count_empty_slots = 0;
        for (auto const way_id : ways) {
            if (way_id == 0) {
                ++count_empty_slots;
            }
        }

        ways.unmap();

        std::cerr << ((std::time(nullptr) - start) / 60) << " mins: Sorting extras...\n";
        std::sort(extras.begin(), extras.end());

        osmium::unsigned_object_id_type last_node_id = 0;
        std::size_t count = 0;
        std::vector<std::size_t> counts;
        counts.resize(100);
        std::vector<std::pair<std::size_t, osmium::unsigned_object_id_type>> nodes_in_many_ways;
        for (auto const &p : extras) {
            if (last_node_id == p.first) {
                ++count;
            } else {
                if (count > max_nodes_in_ways) {
                    nodes_in_many_ways.emplace_back(count, p.first);
                }
                if (count > 99) {
                    count = 99;
                }
                ++counts[count];
                count = 1;
                last_node_id = p.first;
            }
        }

        std::cerr << ((std::time(nullptr) - start) / 60) << " mins: Done.\n\n";

        std::cout << "Largest node id: " << largest_node_id << "\n";
        std::cout << "Extra vector size: " << extras.size() << " entries\n";
        std::cout << "Empty node slots: " << count_empty_slots << percent(count_empty_slots, largest_node_id, "all slots") << "\n";

        std::cout << "Number of ways per node -> Count:\n";
        std::cout << "  1: " << (largest_node_id - count_empty_slots) << "\n";
        for (std::size_t i = 1; i < 99; ++i) {
            if (counts[i] > 0) {
                std::cout << "  " << (i + 1) << ": " << counts[i] << "\n";
            }
        }
        std::cout << "  100 or more: " << counts[99] << "\n";

        std::cout << "\nFirst 10 nodes that are in more than " << max_nodes_in_ways << " ways:\n";
        std::cout << "(Showing node id and the number of ways the node is in.)\n";
        std::sort(nodes_in_many_ways.begin(), nodes_in_many_ways.end());
        std::reverse(nodes_in_many_ways.begin(), nodes_in_many_ways.end());
        for (std::size_t i = 0; i < std::min(static_cast<std::size_t>(10), nodes_in_many_ways.size()); ++i) {
            std::cout << nodes_in_many_ways[i].second << " (" << nodes_in_many_ways[i].first << ")\n";
        }
    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
