
#include "app.hpp"
#include "db.hpp"
#include "util.hpp"

#include <osmium/index/nwr_array.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/util/file.hpp>
#include <osmium/util/memory.hpp>
#include <osmium/util/progress_bar.hpp>
#include <osmium/util/verbose_output.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <map>
#include <string>
#include <utility>
#include <vector>

/* ========================================================================= */

static constexpr const std::array<osmium::item_type, 3> nwr{
    osmium::item_type::node, osmium::item_type::way,
    osmium::item_type::relation};

constexpr uint64_t const shift = 16;
constexpr uint64_t const mask = (1ULL << shift) - 1;

static constexpr uint64_t get_id(uint64_t value) noexcept
{
    return value >> shift;
}

static constexpr uint64_t get_type(uint64_t value) noexcept
{
    return value & mask;
}

static constexpr uint64_t combine(uint64_t id, uint64_t type) noexcept
{
    assert(type < mask);
    return (id << shift) | type;
}

static void sort_unique(std::vector<uint64_t> *vec)
{
    std::sort(vec->begin(), vec->end());
    auto const last = std::unique(vec->begin(), vec->end());
    vec->erase(last, vec->end());
}

class TypeMap
{
    // map from type string to index used in m_types
    std::map<std::string, std::size_t> m_map;

    // contains pairs of "type name" and counter
    std::vector<std::pair<std::string, uint32_t>> m_types;

public:
    TypeMap() { m_types.emplace_back(); }

    static std::string generate_filename(std::string name)
    {
        if (name.empty()) {
            name = "UNKNOWN";
        } else {
            for (auto &c : name) {
                if (!std::isalnum(c) && c != '_' && c != '-' && c != ':') {
                    c = '@';
                }
            }
        }
        return name;
    }

    std::size_t add(char const *rel_type)
    {
        if (!rel_type) {
            ++m_types[0].second;
            return 0;
        }

        std::string type = rel_type;
        auto [it, inserted] = m_map.try_emplace(type, m_types.size());
        if (inserted) {
            m_types.emplace_back(type, 0);
        }
        ++m_types[it->second].second;
        return it->second;
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_types.size(); }

    void insert_into_db(Sqlite::Database *db)
    {
        db->exec("CREATE TABLE types ("
                 "  id INT,"
                 "  type VARCHAR,"
                 "  filename VARCHAR,"
                 "  count INT);");

        Sqlite::Statement insert{
            *db, "INSERT INTO types "
                 "(id, type, filename, count) VALUES (?, ?, ?, ?);"};

        db->begin_transaction();
        for (std::size_t i = 0; i < size(); ++i) {
            insert.bind_int64(i);
            insert.bind_text(m_types[i].first);
            auto const fn = generate_filename(m_types[i].first);
            insert.bind_text(fn);
            insert.bind_int64(m_types[i].second);
            insert.execute();
        }
        db->commit();
    }

    [[nodiscard]] auto begin() const noexcept { return m_types.cbegin(); }

    [[nodiscard]] auto end() const noexcept { return m_types.cend(); }

}; // class TypeMap

static osmium::nwr_array<std::vector<uint64_t>>
read_relations(osmium::io::File const &input_file, TypeMap *types)
{
    osmium::nwr_array<std::vector<uint64_t>> ids;

    osmium::io::Reader reader{input_file, osmium::osm_entity_bits::relation};
    while (auto const buffer = reader.read()) {
        for (auto const &relation : buffer.select<osmium::Relation>()) {
            std::size_t const n = types->add(relation.tags()["type"]);
            ids.relations().push_back(combine(relation.positive_id(), n));
            for (auto const &member : relation.members()) {
                ids(member.type()).push_back(combine(member.positive_ref(), n));
            }
        }
    }
    reader.close();

    for (auto t : nwr) {
        sort_unique(&ids(t));
    }

    return ids;
}

static void read_ways(osmium::io::File const &input_file,
                      osmium::nwr_array<std::vector<uint64_t>> *ids)
{
    auto it = ids->ways().cbegin();
    osmium::io::Reader reader{input_file, osmium::osm_entity_bits::way};
    while (auto const buffer = reader.read()) {
        for (auto const &way : buffer.select<osmium::Way>()) {
            while (get_id(*it) < way.positive_id()) {
                ++it;
                if (it == ids->ways().cend()) {
                    return;
                }
            }
            while (get_id(*it) == way.positive_id()) {
                auto const n = get_type(*it);
                for (auto const &nr : way.nodes()) {
                    ids->nodes().push_back(combine(nr.positive_ref(), n));
                }
                ++it;
                if (it == ids->ways().cend()) {
                    return;
                }
            }
        }
    }
}

static void copy_data(osmium::io::File const &input_file,
                      std::vector<std::unique_ptr<osmium::io::Writer>> &writers,
                      osmium::nwr_array<std::vector<uint64_t>> const &ids,
                      bool verbose)
{
    osmium::io::Reader reader{input_file, osmium::io::buffers_type::single};
    osmium::ProgressBar progress_bar{reader.file_size(), verbose};

    osmium::nwr_array<std::vector<uint64_t>::const_iterator> its;
    its.nodes() = ids.nodes().cbegin();
    its.ways() = ids.ways().cbegin();
    its.relations() = ids.relations().cbegin();

    while (auto const buffer = reader.read()) {
        progress_bar.update(reader.offset());
        for (auto const &object : buffer.select<osmium::OSMObject>()) {
            auto const t = object.type();
            if (its(t) != ids(t).cend()) {
                while (get_id(*its(t)) < object.positive_id()) {
                    ++its(t);
                    if (its(t) == ids(t).cend()) {
                        goto next_buffer;
                    }
                }
                while (get_id(*its(t)) == object.positive_id()) {
                    auto const n = get_type(*its(t));
                    assert(n < writers.size());
                    (*writers[n])(object);
                    ++its(t);
                    if (its(t) == ids(t).cend()) {
                        goto next_buffer;
                    }
                }
            }
        }
    next_buffer:;
    }

    reader.close();
    progress_bar.done();
}

/* ========================================================================= */

class App : public BasicApp
{
public:
    App()
    : BasicApp("osp-analyze-relations-types",
               "Split input file based on relation types", with_output::dir)
    {}

    void run()
    {
        osmium::io::File const input_file{input()};

        auto db = open_database(output() + "/relation-types.db", true);

        TypeMap types;
        vout() << "Reading relations...\n";
        auto ids = read_relations(input_file, &types);
        vout() << fmt::format( "Found {:z} different type tags.\n", types.size());
        types.insert_into_db(&db);

        vout() << "Reading ways...\n";
        read_ways(input_file, &ids);
        sort_unique(&ids.nodes());

        vout() << fmt::format( "Data to copy: {} nodes, {} ways, {} relations.\n",
            ids.nodes().size(), ids.ways().size(), ids.relations().size());

        std::vector<std::unique_ptr<osmium::io::Writer>> writers;
        writers.reserve(types.size());
        for (auto const &type : types) {
            writers.emplace_back(std::make_unique<osmium::io::Writer>(
                fmt::format("{}/{}.osm.pbf",output(),
                                       TypeMap::generate_filename(type.first)),
                osmium::io::overwrite::allow));
        }

#if 0
        for (auto t : nwr) {
            for (auto v : ids(t)) {
                fmt::print("{}{} {}\n", osmium::item_type_to_char(t), get_id(v),
                           get_type(v));
            }
        }
#endif

        vout() << "Copying data...\n";
        copy_data(input_file, writers, ids, vout().verbose());
        for (auto &writer : writers) {
            writer->close();
        }
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
