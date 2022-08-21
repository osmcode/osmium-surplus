
#include "app.hpp"

#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/util/verbose_output.hpp>
#include <osmium/visitor.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

/* ========================================================================= */

class StatHandler : public osmium::handler::Handler
{
    static constexpr int const distance_fields = 32;

    std::array<int64_t, distance_fields> m_distance{0};

    uint64_t m_way_count = 0;
    uint64_t m_way_nodes_count = 0;

public:
    void way(osmium::Way const &way)
    {
        auto const &nodes = way.nodes();
        if (nodes.size() > 1) {
            ++m_way_count;
            m_way_nodes_count += nodes.size();

            std::adjacent_find(nodes.cbegin(), nodes.cend(),
                               [&](auto const &a, auto const &b) {
                                   int64_t diff = b.ref() - a.ref();

                                   if (diff == 0) {
                                       ++m_distance[0];
                                   } else {
                                       int offset = 0;
                                       if (diff < 0) {
                                           diff = -diff;
                                           offset = 16;
                                       }
                                       if (diff <= 9) {
                                           ++m_distance[offset + diff];
                                       } else if (diff <= 127) {
                                           ++m_distance[offset + 10];
                                       } else if (diff <= 255) {
                                           ++m_distance[offset + 11];
                                       } else if (diff <= 65636) {
                                           ++m_distance[offset + 12];
                                       } else {
                                           ++m_distance[offset + 13];
                                       }
                                   }

                                   return false;
                               });
        }
    }

    void out(int i) const
    {
        auto const basis =
            static_cast<double>(m_way_nodes_count - m_way_count) / 100.0;

        fmt::print( ": {:10d} (~ {:5.2f}%)\n", m_distance[i],
                                                      m_distance[i] / basis);
    }

    void output_stats() const
    {
        std::cout << "number of ways with >1 node: " << m_way_count
                  << " with together " << m_way_nodes_count << " nodes\n";

        std::cout << "         diff <= -65536";
        out(16 + 13);
        std::cout << "-65536 < diff <=   -255";
        out(16 + 12);
        std::cout << "  -255 < diff <=   -127";
        out(16 + 11);
        std::cout << "  -127 < diff <      -9";
        out(16 + 10);

        for (int i = 9; i > 0; i--) {
            std::cout << "         diff ==     -" << i;
            out(16 + i);
        }

        std::cout << "         diff ==      0";
        out(0);

        for (int i = 1; i < 9; ++i) {
            std::cout << "         diff ==      " << i;
            out(i);
        }

        std::cout << "     9 < diff <=    127";
        out(10);
        std::cout << "   127 < diff <=    255";
        out(11);
        std::cout << "   255 < diff <=  65536";
        out(12);
        std::cout << " 65536 < diff          ";
        out(13);
    }

}; // class StatHandler

/* ========================================================================= */

class App : public BasicApp
{
public:
    App()
    : BasicApp("osp-stats-way-node-refs-delta",
               "Calculate stats for delta encoding of way nodes",
               with_output::none)
    {}

    void run()
    {
        StatHandler handler;
        osmium::io::Reader reader{input(), osmium::osm_entity_bits::way};

        vout() << "Processing data...\n";
        osmium::apply(reader, handler);
        reader.close();
        vout() << "Done processing.\n";

        handler.output_stats();
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
