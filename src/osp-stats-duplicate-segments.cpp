
#include <osmium/index/id_set.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/util/verbose_output.hpp>

#include <lyra.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

struct node_pair
: public std::pair<osmium::object_id_type, osmium::object_id_type>
{

    node_pair(osmium::object_id_type a, osmium::object_id_type b)
    : pair(std::min(a, b), std::max(a, b))
    {}

}; // struct node_pair

class counter
{

    std::vector<std::size_t> m_counts;

public:
    void increment(std::size_t value)
    {
        if (value >= m_counts.size()) {
            m_counts.resize(value + 1);
        }
        ++m_counts[value];
    }

    using const_iterator = std::vector<std::size_t>::const_iterator;

    [[nodiscard]] const_iterator begin() const noexcept
    {
        return m_counts.begin();
    }

    [[nodiscard]] const_iterator end() const noexcept { return m_counts.end(); }

}; // class counter

int main(int argc, char *argv[])
{
    try {
        std::string input_filename;
        std::string output_directory{"."};
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
            std::cout << cli << "\nCreate statistics on duplicate segments.\n";
            return 0;
        }

        if (input_filename.empty()) {
            std::cerr << "Missing input filename. Try '-h'.\n";
            return 1;
        }

        osmium::io::File input_file{input_filename};

        osmium::VerboseOutput vout{true};

        osmium::index::IdSetDense<osmium::unsigned_object_id_type>
            in_multiple_ways;

        vout << "Reading nodes in ways...\n";

        {
            osmium::index::IdSetDense<osmium::unsigned_object_id_type> in_way;
            osmium::io::Reader reader1{input_file,
                                       osmium::osm_entity_bits::way};
            while (auto const buffer = reader1.read()) {
                for (auto const &way : buffer.select<osmium::Way>()) {
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
                }
            }
            reader1.close();
        }

        vout << "Reading segments...\n";

        std::vector<node_pair> segments;

        osmium::io::Reader reader2{input_file, osmium::osm_entity_bits::way};
        while (auto const buffer = reader2.read()) {
            for (auto const &way : buffer.select<osmium::Way>()) {
                if (way.nodes().size() > 1) {
                    auto const *it = way.nodes().begin();
                    for (++it; it != way.nodes().end(); ++it) {
                        auto const id1 = (it - 1)->ref();
                        auto const id2 = it->ref();
                        if (in_multiple_ways.get(id1) &&
                            in_multiple_ways.get(id2)) {
                            segments.emplace_back(id1, id2);
                        }
                    }
                }
            }
        }
        reader2.close();

        vout << "Got " << segments.size() << " segments\n";

        vout << "Sorting segments...\n";
        std::sort(segments.begin(), segments.end());

        vout << "Counting segments...\n";
        std::ofstream ids{output_directory + "/ids"};
        counter counts;

        constexpr std::size_t const min_count_for_output = 10;
        for (auto it = segments.begin(); it != segments.end();) {
            auto const a = std::adjacent_find(it, segments.end());
            if (a == segments.end()) {
                break;
            }
            std::size_t count = 0;
            it = a;
            while (*a == *it) {
                ++count;
                ++it;
                if (it == segments.end()) {
                    break;
                }
            }
            counts.increment(count - 1);

            if (count >= min_count_for_output) {
                ids << count << ' ' << it->first << ' ' << it->second << '\n';
            }

            it = a + count;
        }

        int n = 0;
        for (auto cc : counts) {
            std::cout << ++n << ' ' << cc << '\n';
        }

        vout << "Done.\n";
    } catch (std::exception const &e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
