
#include "utils.hpp"

#include <gdalcpp.hpp>

#include <osmium/geom/haversine.hpp>
#include <osmium/geom/ogr.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/file.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/osm/undirected_segment.hpp>
#include <osmium/util/memory.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/verbose_output.hpp>
#include <osmium/visitor.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <getopt.h>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static char const *const program_name = "osp-find-way-problems";

struct options_type
{
    osmium::Timestamp before_time{osmium::end_of_time()};
    bool verbose = true;
    std::size_t max_nodes = 1800;
    double max_angle = 0.03;
    double max_segment_length = 100000.0;
};

struct stats_type
{
    uint64_t way_nodes = 0;
    uint64_t self_intersection = 0;
    uint64_t spike = 0;
    uint64_t acute_angle = 0;
    uint64_t duplicate_segment = 0;
    uint64_t no_node = 0;
    uint64_t single_node = 0;
    uint64_t same_node = 0;
    uint64_t duplicate_node = 0;
    uint64_t close_nodes = 0;
    uint64_t many_nodes = 0;
    uint64_t long_segment = 0;
};

static osmium::Location intersection(osmium::Segment const &s1,
                                     osmium::Segment const &s2)
{
    if (s1.first() == s2.first() || s1.first() == s2.second() ||
        s1.second() == s2.first() || s1.second() == s2.second()) {
        return osmium::Location{};
    }

    double const denom = ((s2.second().lat() - s2.first().lat()) *
                          (s1.second().lon() - s1.first().lon())) -
                         ((s2.second().lon() - s2.first().lon()) *
                          (s1.second().lat() - s1.first().lat()));

    if (denom != 0) {
        double const nume_a = ((s2.second().lon() - s2.first().lon()) *
                               (s1.first().lat() - s2.first().lat())) -
                              ((s2.second().lat() - s2.first().lat()) *
                               (s1.first().lon() - s2.first().lon()));

        double const nume_b = ((s1.second().lon() - s1.first().lon()) *
                               (s1.first().lat() - s2.first().lat())) -
                              ((s1.second().lat() - s1.first().lat()) *
                               (s1.first().lon() - s2.first().lon()));

        if ((denom > 0 && nume_a >= 0 && nume_a <= denom && nume_b >= 0 &&
             nume_b <= denom) ||
            (denom < 0 && nume_a <= 0 && nume_a >= denom && nume_b <= 0 &&
             nume_b >= denom)) {
            double const ua = nume_a / denom;
            double const ix =
                s1.first().lon() + ua * (s1.second().lon() - s1.first().lon());
            double const iy =
                s1.first().lat() + ua * (s1.second().lat() - s1.first().lat());
            return osmium::Location{ix, iy};
        }
    }

    return osmium::Location{};
}

static bool outside_x_range(osmium::UndirectedSegment const &s1,
                            osmium::UndirectedSegment const &s2) noexcept
{
    return s1.first().x() > s2.second().x();
}

static bool y_range_overlap(osmium::UndirectedSegment const &s1,
                            osmium::UndirectedSegment const &s2) noexcept
{
    int const tmin =
        s1.first().y() < s1.second().y() ? s1.first().y() : s1.second().y();
    int const tmax =
        s1.first().y() < s1.second().y() ? s1.second().y() : s1.first().y();
    int const omin =
        s2.first().y() < s2.second().y() ? s2.first().y() : s2.second().y();
    int const omax =
        s2.first().y() < s2.second().y() ? s2.second().y() : s2.first().y();
    return !(tmin > omax || omin > tmax);
}

static void open_writer(std::unique_ptr<osmium::io::Writer> &wptr,
                        std::string const &dir, std::string const &name)
{
    osmium::io::File file{dir + "/" + name + ".osm.pbf"};
    file.set("locations_on_ways");

    osmium::io::Header header;
    header.set("generator", program_name);

    wptr = std::make_unique<osmium::io::Writer>(file, header,
                                                osmium::io::overwrite::allow);
}

static bool all_same_nodes(osmium::WayNodeList const &wnl) noexcept
{
    osmium::object_id_type const ref = wnl[0].ref();

    for (auto const &wn : wnl) {
        if (ref != wn.ref()) {
            return false;
        }
    }

    return true;
}

static bool duplicate_nodes(osmium::WayNodeList const &wnl) noexcept
{
    osmium::object_id_type prev_ref = 0;

    for (auto const &wn : wnl) {
        if (prev_ref == wn.ref()) {
            return true;
        }
        prev_ref = wn.ref();
    }

    return false;
}

static std::vector<osmium::UndirectedSegment>
create_segment_list(osmium::WayNodeList const &wnl)
{
    assert(!wnl.empty());

    std::vector<osmium::UndirectedSegment> segments;
    segments.reserve(wnl.size() - 1);

    for (auto it1 = wnl.cbegin(), it2 = std::next(it1); it2 != wnl.cend();
         ++it1, ++it2) {
        auto const loc1 = it1->location();
        auto const loc2 = it2->location();
        if (loc1 != loc2) {
            segments.emplace_back(loc1, loc2);
        }
    }

    return segments;
}

static constexpr int const min_diff_for_close_nodes = 10;

static bool has_close_nodes(osmium::WayNodeList const &wnl)
{
    if (wnl.size() < 2) {
        return false;
    }

    osmium::Location location;

    for (auto const &wn : wnl) {
        auto const dx = std::abs(location.x() - wn.location().x());
        auto const dy = std::abs(location.y() - wn.location().y());
        if (dx < min_diff_for_close_nodes && dy < min_diff_for_close_nodes) {
            return true;
        }
        location = wn.location();
    }

    return false;
}

class CheckHandler : public HandlerWithDB
{

    options_type m_options;
    stats_type m_stats;

    gdalcpp::Layer m_layer_way_one_node;
    gdalcpp::Layer m_layer_way_duplicate_nodes;
    gdalcpp::Layer m_layer_way_intersection_points;
    gdalcpp::Layer m_layer_way_intersection_lines;
    gdalcpp::Layer m_layer_way_spike_points;
    gdalcpp::Layer m_layer_way_spike_lines;
    gdalcpp::Layer m_layer_way_acute_angle_points;
    gdalcpp::Layer m_layer_way_acute_angle_lines;
    gdalcpp::Layer m_layer_way_duplicate_segments;
    gdalcpp::Layer m_layer_way_many_nodes;
    gdalcpp::Layer m_layer_way_long_segments;

    std::unique_ptr<osmium::io::Writer> m_writer_self_intersection;
    std::unique_ptr<osmium::io::Writer> m_writer_spike;
    std::unique_ptr<osmium::io::Writer> m_writer_acute_angle;
    std::unique_ptr<osmium::io::Writer> m_writer_duplicate_segment;
    std::unique_ptr<osmium::io::Writer> m_writer_no_node;
    std::unique_ptr<osmium::io::Writer> m_writer_single_node;
    std::unique_ptr<osmium::io::Writer> m_writer_same_node;
    std::unique_ptr<osmium::io::Writer> m_writer_duplicate_node;
    std::unique_ptr<osmium::io::Writer> m_writer_close_nodes;
    std::unique_ptr<osmium::io::Writer> m_writer_many_nodes;
    std::unique_ptr<osmium::io::Writer> m_writer_long_segment;

    bool detect_spikes(osmium::Way const &way)
    {
        if (way.nodes().size() < 3) {
            return false;
        }

        auto const *first = way.nodes().cbegin();
        auto const *last = way.nodes().cend();

        auto const *prev = first;
        auto const *curr = prev + 1;
        auto const *next = curr + 1;

        for (; next != last; ++prev, ++curr, ++next) {
            if (prev->location() == next->location() &&
                prev->location() != curr->location()) {
                // found spike
                auto const ts = way.timestamp().to_iso();
                {
                    gdalcpp::Feature feature{
                        m_layer_way_spike_points,
                        m_factory.create_point(curr->location())};
                    feature.set_field("way_id", static_cast<int32_t>(way.id()));
                    feature.set_field("timestamp", ts.c_str());
                    feature.set_field("closed", way.is_closed());
                    feature.add_to_layer();
                }

                if (prev != first) {
                    auto const *p = prev - 1;
                    auto const *n = next + 1;
                    while (p != first && n != last &&
                           p->location() == n->location()) {
                        prev = p;
                        next = n;
                        --p;
                        ++n;
                    }
                }

                {
                    std::unique_ptr<OGRLineString> linestring{
                        new OGRLineString};
                    for (; prev != next; ++prev) {
                        linestring->addPoint(prev->location().lon(),
                                             prev->location().lat());
                    }
                    gdalcpp::Feature feature{m_layer_way_spike_lines,
                                             std::move(linestring)};
                    feature.set_field("way_id", static_cast<int32_t>(way.id()));
                    feature.set_field("timestamp", ts.c_str());
                    feature.set_field("closed", way.is_closed());
                    feature.add_to_layer();
                }
                return true;
            }
        }

        return false;
    }

    static double calc_angle(osmium::Location const &a,
                             osmium::Location const &m,
                             osmium::Location const &b)
    {
        int64_t const dax = a.x() - m.x();
        int64_t const day = a.y() - m.y();
        int64_t const dbx = b.x() - m.x();
        int64_t const dby = b.y() - m.y();
        auto const dp = static_cast<double>(dax * dbx + day * dby);
        double const m1 = std::sqrt(static_cast<double>(dax * dax + day * day));
        double const m2 = std::sqrt(static_cast<double>(dbx * dbx + dby * dby));

        if (m1 == 0 || m2 == 0) {
            return 0;
        }

        double const cphi = dp / (m1 * m2);
        return std::acos(cphi);
    }

    bool detect_acute_angles(osmium::Way const &way)
    {
        if (way.nodes().size() < 3) {
            return false;
        }

        auto const *prev = way.nodes().cbegin();
        auto const *curr = prev + 1;
        auto const *next = curr + 1;

        bool result = false;

        for (; next != way.nodes().end(); ++prev, ++curr, ++next) {
            auto const angle = calc_angle(prev->location(), curr->location(),
                                          next->location());
            if (angle < m_options.max_angle) {
                result = true;
                auto const ts = way.timestamp().to_iso();
                {
                    gdalcpp::Feature feature{
                        m_layer_way_acute_angle_points,
                        m_factory.create_point(curr->location())};
                    feature.set_field("way_id", static_cast<int32_t>(way.id()));
                    feature.set_field("timestamp", ts.c_str());
                    feature.set_field("closed", way.is_closed());
                    feature.set_field("angle", angle);
                    feature.add_to_layer();
                }
                auto ogr_linestring = std::make_unique<OGRLineString>();
                ogr_linestring->addPoint(prev->location().lon(),
                                         prev->location().lat());
                ogr_linestring->addPoint(curr->location().lon(),
                                         curr->location().lat());
                ogr_linestring->addPoint(next->location().lon(),
                                         next->location().lat());

                gdalcpp::Feature feature{m_layer_way_acute_angle_lines,
                                         std::move(ogr_linestring)};
                feature.set_field("way_id", static_cast<int32_t>(way.id()));
                feature.set_field("timestamp", ts.c_str());
                feature.set_field("closed", way.is_closed());
                feature.set_field("angle", angle);
                feature.add_to_layer();
            }
        }

        return result;
    }

public:
    CheckHandler(std::string const &output_dirname, options_type const &options)
    : HandlerWithDB(output_dirname + "/geoms-way-problems.db"),
      m_options(options), m_layer_way_one_node(m_dataset, "way_one_node",
                                               wkbPoint, {"SPATIAL_INDEX=NO"}),
      m_layer_way_duplicate_nodes(m_dataset, "way_duplicate_nodes", wkbPoint,
                                  {"SPATIAL_INDEX=NO"}),
      m_layer_way_intersection_points(m_dataset, "way_intersection_points",
                                      wkbPoint, {"SPATIAL_INDEX=NO"}),
      m_layer_way_intersection_lines(m_dataset, "way_intersection_lines",
                                     wkbLineString, {"SPATIAL_INDEX=NO"}),
      m_layer_way_spike_points(m_dataset, "way_spike_points", wkbPoint,
                               {"SPATIAL_INDEX=NO"}),
      m_layer_way_spike_lines(m_dataset, "way_spike_lines", wkbLineString,
                              {"SPATIAL_INDEX=NO"}),
      m_layer_way_acute_angle_points(m_dataset, "way_acute_angle_points",
                                     wkbPoint, {"SPATIAL_INDEX=NO"}),
      m_layer_way_acute_angle_lines(m_dataset, "way_acute_angle_lines",
                                    wkbLineString, {"SPATIAL_INDEX=NO"}),
      m_layer_way_duplicate_segments(m_dataset, "way_duplicate_segments",
                                     wkbLineString, {"SPATIAL_INDEX=NO"}),
      m_layer_way_many_nodes(m_dataset, "way_many_nodes", wkbLineString,
                             {"SPATIAL_INDEX=NO"}),
      m_layer_way_long_segments(m_dataset, "way_long_segments", wkbLineString,
                                {"SPATIAL_INDEX=NO"})
    {

        m_layer_way_one_node.add_field("way_id", OFTInteger, 10);
        m_layer_way_one_node.add_field("timestamp", OFTString, 20);
        m_layer_way_one_node.add_field("node_id", OFTReal, 12);
        m_layer_way_one_node.add_field("num_nodes", OFTInteger, 3);

        m_layer_way_duplicate_nodes.add_field("way_id", OFTInteger, 10);
        m_layer_way_duplicate_nodes.add_field("timestamp", OFTString, 20);
        m_layer_way_duplicate_nodes.add_field("node_id", OFTReal, 12);
        m_layer_way_duplicate_nodes.add_field("closed", OFTInteger, 1);

        m_layer_way_intersection_points.add_field("way_id", OFTInteger, 10);
        m_layer_way_intersection_points.add_field("timestamp", OFTString, 20);
        m_layer_way_intersection_points.add_field("closed", OFTInteger, 1);
        m_layer_way_intersection_lines.add_field("way_id", OFTInteger, 10);
        m_layer_way_intersection_lines.add_field("timestamp", OFTString, 20);
        m_layer_way_intersection_lines.add_field("closed", OFTInteger, 1);

        m_layer_way_spike_points.add_field("way_id", OFTInteger, 10);
        m_layer_way_spike_points.add_field("timestamp", OFTString, 20);
        m_layer_way_spike_points.add_field("closed", OFTInteger, 1);
        m_layer_way_spike_lines.add_field("way_id", OFTInteger, 10);
        m_layer_way_spike_lines.add_field("timestamp", OFTString, 20);
        m_layer_way_spike_lines.add_field("closed", OFTInteger, 1);

        m_layer_way_acute_angle_points.add_field("way_id", OFTInteger, 10);
        m_layer_way_acute_angle_points.add_field("timestamp", OFTString, 20);
        m_layer_way_acute_angle_points.add_field("closed", OFTInteger, 1);
        m_layer_way_acute_angle_points.add_field("angle", OFTReal, 20);
        m_layer_way_acute_angle_lines.add_field("way_id", OFTInteger, 10);
        m_layer_way_acute_angle_lines.add_field("timestamp", OFTString, 20);
        m_layer_way_acute_angle_lines.add_field("closed", OFTInteger, 1);
        m_layer_way_acute_angle_lines.add_field("angle", OFTReal, 20);

        m_layer_way_duplicate_segments.add_field("way_id", OFTInteger, 10);
        m_layer_way_duplicate_segments.add_field("timestamp", OFTString, 20);
        m_layer_way_duplicate_segments.add_field("closed", OFTInteger, 1);

        m_layer_way_many_nodes.add_field("way_id", OFTInteger, 10);
        m_layer_way_many_nodes.add_field("timestamp", OFTString, 20);
        m_layer_way_many_nodes.add_field("num_nodes", OFTInteger, 4);
        m_layer_way_many_nodes.add_field("closed", OFTInteger, 1);

        m_layer_way_long_segments.add_field("way_id", OFTInteger, 10);
        m_layer_way_long_segments.add_field("timestamp", OFTString, 20);
        m_layer_way_long_segments.add_field("closed", OFTInteger, 1);

        open_writer(m_writer_self_intersection, output_dirname,
                    "way-self-intersection");
        open_writer(m_writer_spike, output_dirname, "way-spike");
        open_writer(m_writer_acute_angle, output_dirname, "way-acute-angle");
        open_writer(m_writer_duplicate_segment, output_dirname,
                    "way-duplicate-segment");
        open_writer(m_writer_no_node, output_dirname, "way-no-node");
        open_writer(m_writer_single_node, output_dirname, "way-single-node");
        open_writer(m_writer_same_node, output_dirname, "way-same-node");
        open_writer(m_writer_duplicate_node, output_dirname,
                    "way-duplicate-node"),
            open_writer(m_writer_close_nodes, output_dirname,
                        "way-close-nodes");
        open_writer(m_writer_many_nodes, output_dirname, "way-many-nodes");
        open_writer(m_writer_long_segment, output_dirname, "way-long-segment");
    }

    void way(osmium::Way const &way)
    {
        if (way.timestamp() >= m_options.before_time) {
            return;
        }

        if (way.nodes().empty()) {
            ++m_stats.no_node;
            (*m_writer_no_node)(way);
            return;
        }

        m_stats.way_nodes += way.nodes().size();

        auto const ts = way.timestamp().to_iso();

        if (way.nodes().size() == 1) {
            ++m_stats.single_node;
            (*m_writer_single_node)(way);
            gdalcpp::Feature feature{m_layer_way_one_node,
                                     m_factory.create_point(way.nodes()[0])};
            feature.set_field("way_id", static_cast<int32_t>(way.id()));
            feature.set_field("node_id",
                              static_cast<double>(way.nodes()[0].ref()));
            feature.set_field("num_nodes", 1);
            feature.set_field("timestamp", ts.c_str());
            feature.add_to_layer();
            return;
        }

        if (all_same_nodes(way.nodes())) {
            ++m_stats.same_node;
            (*m_writer_same_node)(way);
            gdalcpp::Feature feature{m_layer_way_one_node,
                                     m_factory.create_point(way.nodes()[0])};
            feature.set_field("way_id", static_cast<int32_t>(way.id()));
            feature.set_field("node_id",
                              static_cast<double>(way.nodes()[0].ref()));
            feature.set_field("num_nodes",
                              static_cast<int32_t>(way.nodes().size()));
            feature.set_field("timestamp", ts.c_str());
            feature.add_to_layer();
            return;
        }

        if (duplicate_nodes(way.nodes())) {
            ++m_stats.duplicate_node;
            (*m_writer_duplicate_node)(way);
            gdalcpp::Feature feature{m_layer_way_duplicate_nodes,
                                     m_factory.create_point(way.nodes()[0])};
            feature.set_field("way_id", static_cast<int32_t>(way.id()));
            feature.set_field("node_id",
                              static_cast<double>(way.nodes()[0].ref()));
            feature.set_field("timestamp", ts.c_str());
            feature.set_field("closed", way.is_closed());
            feature.add_to_layer();
        }

        auto segments = create_segment_list(way.nodes());

        for (auto const &segment : segments) {
            auto const distance = osmium::geom::haversine::distance(
                segment.first(), segment.second());
            if (distance > m_options.max_segment_length) {
                ++m_stats.long_segment;
                (*m_writer_long_segment)(way);
                gdalcpp::Feature feature{m_layer_way_long_segments,
                                         m_factory.create_linestring(way)};
                feature.set_field("way_id", static_cast<int32_t>(way.id()));
                feature.set_field("timestamp", ts.c_str());
                feature.set_field("closed", way.is_closed());
                feature.add_to_layer();
                break;
            }
        }

        if (segments.size() < 2) {
            return;
        }

        if (detect_spikes(way)) {
            ++m_stats.spike;
            (*m_writer_spike)(way);
            return;
        }

        if (detect_acute_angles(way)) {
            ++m_stats.acute_angle;
            (*m_writer_acute_angle)(way);
        }

        std::sort(segments.begin(), segments.end());

        std::vector<osmium::Location> intersections;
        for (auto it1 = segments.cbegin(); it1 != segments.cend() - 1; ++it1) {
            osmium::UndirectedSegment const &s1 = *it1;
            for (auto it2 = it1 + 1; it2 != segments.cend(); ++it2) {
                osmium::UndirectedSegment const &s2 = *it2;
                if (s1 == s2) {
                    ++m_stats.duplicate_segment;
                    (*m_writer_duplicate_segment)(way);
                    std::unique_ptr<OGRLineString> linestring{
                        new OGRLineString{}};
                    linestring->addPoint(s1.first().lon(), s1.first().lat());
                    linestring->addPoint(s1.second().lon(), s1.second().lat());
                    gdalcpp::Feature feature{m_layer_way_duplicate_segments,
                                             std::move(linestring)};
                    feature.set_field("way_id", static_cast<int32_t>(way.id()));
                    feature.set_field("timestamp", ts.c_str());
                    feature.set_field("closed", way.is_closed());
                    feature.add_to_layer();
                } else {
                    if (outside_x_range(s2, s1)) {
                        break;
                    }
                    if (y_range_overlap(s1, s2)) {
                        osmium::Location const i = intersection(s1, s2);
                        if (i) {
                            intersections.push_back(i);
                        }
                    }
                }
            }
        }
        if (!intersections.empty()) {
            ++m_stats.self_intersection;
            (*m_writer_self_intersection)(way);

            for (auto const &location : intersections) {
                gdalcpp::Feature feature{m_layer_way_intersection_points,
                                         m_factory.create_point(location)};
                feature.set_field("way_id", static_cast<int32_t>(way.id()));
                feature.set_field("timestamp", ts.c_str());
                feature.set_field("closed", way.is_closed());
                feature.add_to_layer();
            }

            {
                gdalcpp::Feature feature{m_layer_way_intersection_lines,
                                         m_factory.create_linestring(way)};
                feature.set_field("way_id", static_cast<int32_t>(way.id()));
                feature.set_field("timestamp", ts.c_str());
                feature.set_field("closed", way.is_closed());
                feature.add_to_layer();
            }
        }

        if (has_close_nodes(way.nodes())) {
            ++m_stats.close_nodes;
            (*m_writer_close_nodes)(way);
        }

        if (way.nodes().size() > m_options.max_nodes) {
            ++m_stats.many_nodes;
            (*m_writer_many_nodes)(way);
            gdalcpp::Feature feature{m_layer_way_many_nodes,
                                     m_factory.create_linestring(way)};
            feature.set_field("way_id", static_cast<int32_t>(way.id()));
            feature.set_field("timestamp", ts.c_str());
            feature.set_field("num_nodes",
                              static_cast<int32_t>(way.nodes().size()));
            feature.set_field("closed", way.is_closed());
            feature.add_to_layer();
        }
    }

    void close()
    {
        (*m_writer_self_intersection).close();
        (*m_writer_spike).close();
        (*m_writer_acute_angle).close();
        (*m_writer_duplicate_segment).close();
        (*m_writer_no_node).close();
        (*m_writer_single_node).close();
        (*m_writer_same_node).close();
        (*m_writer_duplicate_node).close();
        (*m_writer_close_nodes).close();
        (*m_writer_many_nodes).close();
        (*m_writer_long_segment).close();
    }

    [[nodiscard]] stats_type const &stats() const noexcept { return m_stats; }

}; // class CheckHandler

static void print_help()
{
    std::cout << program_name << " [OPTIONS] OSM-FILE OUTPUT-DIR\n\n"
              << "Find ways with problems.\n"
              << "\nOptions:\n"
              << "  -a, --min-age=DAYS      Only include objects at least DAYS "
                 "days old\n"
              << "  -b, --before=TIMESTAMP  Only include objects changed last "
                 "before\n"
              << "                          this time (format: "
                 "yyyy-mm-ddThh:mm:ssZ)\n"
              << "  -h, --help              This help message\n"
              << "  -m, --max-nodes=NUM     Report ways with more nodes than "
                 "this (default: 1800).\n"
              << "  -q, --quiet             Work quietly\n";
}

static options_type parse_command_line(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"age", required_argument, nullptr, 'a'},
        {"before", required_argument, nullptr, 'b'},
        {"help", no_argument, nullptr, 'h'},
        {"max-nodes", no_argument, nullptr, 'm'},
        {"quiet", no_argument, nullptr, 'q'},
        {nullptr, 0, nullptr, 0}};

    options_type options;

    while (true) {
        int const c =
            getopt_long(argc, argv, "a:b:hm:q", long_options, nullptr);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'a':
            if (options.before_time != osmium::end_of_time()) {
                std::cerr << "You can not use both -a,--age and -b,--before "
                             "together\n";
                std::exit(2);
            }
            options.before_time = build_timestamp(optarg);
            break;
        case 'b':
            if (options.before_time != osmium::end_of_time()) {
                std::cerr << "You can not use both -a,--age and -b,--before "
                             "together\n";
                std::exit(2);
            }
            options.before_time = osmium::Timestamp{optarg};
            break;
        case 'h':
            print_help();
            std::exit(0);
        case 'm':
            options.max_nodes = std::atoi(optarg);
            break;
        case 's':
            options.max_segment_length = std::atof(optarg);
            break;
        case 'q':
            options.verbose = false;
            break;
        default:
            std::exit(2);
        }
    }

    int const remaining_args = argc - optind;
    if (remaining_args != 2) {
        std::cerr << "Usage: " << program_name
                  << " [OPTIONS] OSM-FILE OUTPUT-DIR\n"
                  << "Call '" << program_name
                  << " --help' for usage information.\n";
        std::exit(2);
    }

    return options;
}

int main(int argc, char *argv[])
try {
    auto const options = parse_command_line(argc, argv);

    osmium::util::VerboseOutput vout{options.verbose};
    vout << "Starting " << program_name << "...\n";

    std::string const input_filename{argv[optind]};
    std::string const output_dirname{argv[optind + 1]};

    vout << "Command line options:\n";
    vout << "  Reading from file '" << input_filename << "'\n";
    vout << "  Writing to directory '" << output_dirname << "'\n";
    if (options.before_time == osmium::end_of_time()) {
        vout << "  Get all objects independent of change timestamp (change "
                "with --age, -a or --before, -b)\n";
    } else {
        vout << "  Get only objects last changed before: "
             << options.before_time
             << " (change with --age, -a or --before, -b)\n";
    }

    osmium::io::File file{input_filename};
    osmium::io::Reader reader{file, osmium::osm_entity_bits::way};
    if (file.format() == osmium::io::file_format::pbf &&
        !has_locations_on_ways(reader.header())) {
        std::cerr << "Input file must have locations on ways.\n";
        return 2;
    }

    LastTimestampHandler last_timestamp_handler;
    CheckHandler handler{output_dirname, options};

    vout << "Reading ways and checking for problems...\n";
    osmium::ProgressBar progress_bar{reader.file_size(), display_progress()};
    while (osmium::memory::Buffer buffer = reader.read()) {
        progress_bar.update(reader.offset());
        osmium::apply(buffer, last_timestamp_handler, handler);
    }
    progress_bar.done();

    handler.close();
    reader.close();

    vout << "Writing out stats...\n";
    auto const last_time{last_timestamp_handler.get_timestamp()};
    write_stats(
        output_dirname + "/stats-way-problems.db", last_time,
        [&](std::function<void(char const *, uint64_t)> &add) {
            add("way_nodes", handler.stats().way_nodes);
            add("way_self_intersection", handler.stats().self_intersection);
            add("way_spike", handler.stats().spike);
            add("way_acute_angle", handler.stats().acute_angle);
            add("way_duplicate_segment", handler.stats().duplicate_segment);
            add("way_no_node", handler.stats().no_node);
            add("way_single_node", handler.stats().single_node);
            add("way_same_node", handler.stats().same_node);
            add("way_duplicate_node", handler.stats().duplicate_node);
            add("way_close_nodes", handler.stats().close_nodes);
            add("way_many_nodes", handler.stats().many_nodes);
            add("way_long_segment", handler.stats().long_segment);
        });

    osmium::MemoryUsage memory_usage;
    if (memory_usage.peak() != 0) {
        vout << "Peak memory usage: " << memory_usage.peak() << " MBytes\n";
    }

    vout << "Done with " << program_name << ".\n";

    return 0;
} catch (std::exception const &e) {
    std::cerr << e.what() << '\n';
    std::exit(1);
}
