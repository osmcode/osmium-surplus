#ifndef OSMIUM_SURPLUS_OUTPUTS_HPP
#define OSMIUM_SURPLUS_OUTPUTS_HPP

#include <gdalcpp.hpp>

#include <osmium/geom/ogr.hpp>
#include <osmium/index/nwr_array.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/header.hpp>

#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <utility>

class Output
{

    struct mem_rel_mapping
    {

        osmium::unsigned_object_id_type member_id;
        osmium::unsigned_object_id_type relation_id;

        mem_rel_mapping(osmium::unsigned_object_id_type mem_id,
                        osmium::unsigned_object_id_type rel_id = 0)
        : member_id(mem_id), relation_id(rel_id)
        {}

        bool operator<(mem_rel_mapping const &other) const noexcept
        {
            return std::tie(member_id, relation_id) <
                   std::tie(other.member_id, other.relation_id);
        }

    }; // struct mem_rel_mapping

    using id_map_type = std::vector<mem_rel_mapping>;

    std::string m_name;
    std::map<osmium::unsigned_object_id_type,
             std::vector<osmium::unsigned_object_id_type>>
        m_marks;
    osmium::geom::OGRFactory<> &m_factory;
    std::unique_ptr<gdalcpp::Layer> m_layer_points;
    std::unique_ptr<gdalcpp::Layer> m_layer_lines;

    osmium::io::File m_file;
    osmium::io::Writer m_writer_rel;
    osmium::io::Writer m_writer_all;

    uint64_t m_counter;

    osmium::nwr_array<id_map_type> m_id_maps;

    static std::string underscore_to_dash(std::string const &str)
    {
        std::string out;

        for (char const c : str) {
            out += (c == '_') ? '-' : c;
        }

        return out;
    }

    bool check_mark(osmium::unsigned_object_id_type rel_id,
                    osmium::unsigned_object_id_type obj_id)
    {
        if (m_marks.count(rel_id) == 0) {
            return false;
        }
        auto const &vec = m_marks[rel_id];
        auto const range = std::equal_range(vec.begin(), vec.end(), obj_id);
        return range.first != range.second;
    }

    void
    add_features_to_layers(osmium::OSMObject const &object,
                           std::pair<id_map_type::const_iterator,
                                     id_map_type::const_iterator> const &range)
    {
        const auto ts = object.timestamp().to_iso();

        for (auto it = range.first; it != range.second; ++it) {
            auto const rel_id = it->relation_id;
            if (object.type() == osmium::item_type::node && m_layer_points) {
                try {
                    gdalcpp::Feature feature{
                        *m_layer_points,
                        m_factory.create_point(
                            static_cast<osmium::Node const &>(object))};
                    feature.set_field("rel_id", static_cast<int32_t>(rel_id));
                    feature.set_field("node_id",
                                      static_cast<double>(object.id()));
                    feature.set_field("timestamp", ts.c_str());
                    feature.set_field("mark", 0);
                    feature.add_to_layer();
                } catch (osmium::geometry_error &e) {
                    std::cerr << "Geometry error writing out node "
                              << object.id() << " for relation " << rel_id
                              << ": " << e.what() << '\n';
                }
            } else if (object.type() == osmium::item_type::way &&
                       m_layer_lines) {
                try {
                    gdalcpp::Feature feature{
                        *m_layer_lines,
                        m_factory.create_linestring(
                            static_cast<osmium::Way const &>(object))};
                    feature.set_field("rel_id", static_cast<int32_t>(rel_id));
                    feature.set_field("way_id",
                                      static_cast<int32_t>(object.id()));
                    feature.set_field("timestamp", ts.c_str());
                    feature.set_field("mark",
                                      check_mark(rel_id, object.positive_id()));
                    feature.add_to_layer();
                } catch (osmium::geometry_error &e) {
                    std::cerr << "Geometry error writing out way "
                              << object.id() << " for relation " << rel_id
                              << ": " << e.what() << '\n';
                }
            }
        }
    }

    void add_members_to_index(osmium::Relation const &relation)
    {
        for (auto const &member : relation.members()) {
            m_id_maps(member.type())
                .emplace_back(member.positive_ref(), relation.positive_id());
        }
    }

public:
    Output(std::string const &name, gdalcpp::Dataset &dataset,
           osmium::geom::OGRFactory<> &factory, std::string const &directory,
           osmium::io::Header const &header, bool points, bool lines)
    : m_name(name), m_factory(factory), m_layer_points(nullptr),
      m_layer_lines(nullptr),
      m_file(directory + "/" + underscore_to_dash(name) + "-all.osm.pbf",
             "pbf,locations_on_ways=true"),
      m_writer_rel(directory + "/" + underscore_to_dash(name) + ".osm.pbf",
                   header, osmium::io::overwrite::allow),
      m_writer_all(m_file, header, osmium::io::overwrite::allow), m_counter(0),
      m_id_maps()
    {
        if (points) {
            m_layer_points.reset(new gdalcpp::Layer{
                dataset, name + "_points", wkbPoint, {"SPATIAL_INDEX=NO"}});
            m_layer_points->add_field("rel_id", OFTInteger, 10);
            m_layer_points->add_field("node_id", OFTReal, 12);
            m_layer_points->add_field("timestamp", OFTString, 20);
            m_layer_points->add_field("mark", OFTInteger, 1);
        }
        if (lines) {
            m_layer_lines.reset(new gdalcpp::Layer{
                dataset, name + "_lines", wkbLineString, {"SPATIAL_INDEX=NO"}});
            m_layer_lines->add_field("rel_id", OFTInteger, 10);
            m_layer_lines->add_field("way_id", OFTInteger, 10);
            m_layer_lines->add_field("timestamp", OFTString, 20);
            m_layer_lines->add_field("mark", OFTInteger, 1);
        }
    }

    char const *name() const noexcept { return m_name.c_str(); }

    std::int64_t counter() const noexcept { return m_counter; }

    void add(osmium::Relation const &relation, uint64_t increment = 1,
             std::vector<osmium::unsigned_object_id_type> const &marks = {})
    {
        m_counter += increment;
        m_writer_rel(relation);
        add_members_to_index(relation);
        if (!marks.empty()) {
            m_marks.emplace(relation.positive_id(), marks);
        }
    }

    using id_pair = std::pair<osmium::unsigned_object_id_type,
                              osmium::unsigned_object_id_type>;

    void write_to_all(osmium::OSMObject const &object)
    {
        auto const &map = m_id_maps(object.type());
        auto const range = std::equal_range(
            map.begin(), map.end(), mem_rel_mapping{object.positive_id()},
            [](auto const &a, auto const &b) {
                return a.member_id < b.member_id;
            });
        if (range.first != range.second) {
            m_writer_all(object);
            add_features_to_layers(object, range);
        }
    }

    void prepare()
    {
        std::sort(m_id_maps(osmium::item_type::node).begin(),
                  m_id_maps(osmium::item_type::node).end());
        std::sort(m_id_maps(osmium::item_type::way).begin(),
                  m_id_maps(osmium::item_type::way).end());
        std::sort(m_id_maps(osmium::item_type::relation).begin(),
                  m_id_maps(osmium::item_type::relation).end());
    }

    void close_writer_rel() { m_writer_rel.close(); }

    void close_writer_all() { m_writer_all.close(); }

}; // class Output

/**
 * This is a collection of Outputs using the same database.
 */
class Outputs
{

    std::map<std::string, Output> m_outputs;
    std::string m_dirname;
    osmium::io::Header m_header;
    osmium::geom::OGRFactory<> m_factory;
    gdalcpp::Dataset m_dataset;

public:
    Outputs(std::string const &dirname, std::string const &dbname,
            osmium::io::Header &header)
    : m_outputs(), m_dirname(dirname), m_header(header), m_factory(),
      m_dataset("SQLite", dirname + "/" + dbname + ".db",
                gdalcpp::SRS{m_factory.proj_string()},
                {"SPATIALITE=TRUE", "INIT_WITH_EPSG=NO"})
    {
        CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "OFF");
        m_dataset.enable_auto_transactions();
        m_dataset.exec("PRAGMA journal_mode = OFF;");
    }

    void add_output(char const *name, bool points = true, bool lines = true)
    {
        m_outputs.emplace(std::piecewise_construct, std::forward_as_tuple(name),
                          std::forward_as_tuple(name, m_dataset, m_factory,
                                                m_dirname, m_header, points,
                                                lines));
    }

    Output &operator[](char const *name) { return m_outputs.at(name); }

    template <typename TFunc>
    void for_all(TFunc &&func)
    {
        for (auto &out : m_outputs) {
            std::forward<TFunc>(func)(out.second);
        }
    }

}; // class Outputs

#endif // OSMIUM_SURPLUS_OUTPUTS_HPP
