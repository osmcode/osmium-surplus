
#include "app.hpp"

#include <osmium/builder/attr.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/io/file.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/util/string.hpp>

#include <cctype>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

static bool char_is_good(char c) noexcept
{
    //    return c == 0x0a || c == 0x0d || (c > 0x1f && c != 0x7f);
    if (c == 0x0a || c == 0x0d) {
        return true;
    }
    return !std::iscntrl(c);
}

static bool check_string(char const *s) noexcept
{
    while (*s) {
        if (!char_is_good(*s)) {
            return false;
        }
        ++s;
    }
    return true;
}

static std::string cleanup_string(char const *s)
{
    std::string out;

    while (*s) {
        if (char_is_good(*s)) {
            out.append(1, *s);
        }
        ++s;
    }

    return out;
}

void fix(osmium::OSMObject const &object, osmium::memory::Buffer *buffer)
{
    std::vector<std::pair<std::string, std::string>> tags;
    for (osmium::Tag const &tag : object.tags()) {
        tags.emplace_back(cleanup_string(tag.key()),
                          cleanup_string(tag.value()));
    }

    // NOLINTNEXTLINE(google-build-using-namespace)
    using namespace osmium::builder::attr;

    switch (object.type()) {
    case osmium::item_type::node:
        // clang-format off
        osmium::builder::add_node(*buffer,
            _id(object.id()),
            _version(object.version()),
            _timestamp(object.timestamp()),
            _cid(object.changeset()),
            _uid(object.uid()),
            _user(cleanup_string(object.user())),
            _location(static_cast<osmium::Node const &>(object).location()),
            _tags(tags)
        );
        // clang-format on
        break;
    case osmium::item_type::way:
        // clang-format off
        osmium::builder::add_way(*buffer,
            _id(object.id()),
            _version(object.version()),
            _timestamp(object.timestamp()),
            _cid(object.changeset()),
            _uid(object.uid()),
            _user(cleanup_string(object.user())),
            _tags(tags),
            _nodes(static_cast<osmium::Way const &>(object).nodes())
        );
        // clang-format on
        break;
    case osmium::item_type::relation: {
        std::vector<member_type_string> members;

        for (osmium::RelationMember const &member :
             static_cast<osmium::Relation const &>(object).members()) {
            members.emplace_back(member.type(), member.ref(),
                                 cleanup_string(member.role()));
        }

        // clang-format off
        osmium::builder::add_relation(*buffer,
            _id(object.id()),
            _version(object.version()),
            _timestamp(object.timestamp()),
            _cid(object.changeset()),
            _uid(object.uid()),
            _user(cleanup_string(object.user())),
            _tags(tags),
            _members(members)
        );
        // clang-format on
    } break;
    default:
        break;
    }
}

void print_error(osmium::OSMObject const &object, char const *where)
{
    std::cerr << "Error in " << osmium::item_type_to_char(object.type())
              << object.id() << " on " << where << "\n";
}

class App : public BasicApp
{
    std::string m_error_file;

    std::size_t m_errors_on_users = 0;
    std::size_t m_errors_on_roles = 0;
    std::size_t m_errors_on_keys = 0;
    std::size_t m_errors_on_values = 0;

    bool okay(osmium::OSMObject const &object, osmium::memory::Buffer *buffer)
    {
        bool okay = true;

        if (!check_string(object.user())) {
            ++m_errors_on_users;
            print_error(object, "user");
            okay = false;
        }

        for (osmium::Tag const &tag : object.tags()) {
            if (!check_string(tag.key())) {
                print_error(object, "key");
                ++m_errors_on_keys;
                okay = false;
            }
            if (!check_string(tag.value())) {
                print_error(object, "value");
                ++m_errors_on_values;
                okay = false;
            }
        }

        if (object.type() == osmium::item_type::relation) {
            for (osmium::RelationMember const &member :
                 static_cast<osmium::Relation const &>(object).members()) {
                if (!check_string(member.role())) {
                    ++m_errors_on_roles;
                    print_error(object, "role");
                    okay = false;
                }
            }
        }

        if (!okay) {
            fix(object, buffer);
        }

        return okay;
    }

public:
    App()
    : BasicApp("osp-find-and-fix-control-characters",
               "Find and fix control characters in OSM files",
               with_output::file)
    {
        add_option("-e,--error-file", m_error_file, "Error file")
            ->type_name("OSM-FILE")
            ->required();
    }
    void run()
    {
        vout() << "        Writing errors to: '" << m_error_file << "'.\n";
        osmium::io::Reader reader{input(), osmium::osm_entity_bits::nwr};
        osmium::io::Header header = reader.header();
        osmium::io::Writer writer_error{m_error_file,
                                        osmium::io::overwrite::allow};

        std::unique_ptr<osmium::io::Writer> writer_data;

        if (!output()
                 .empty()) { // XXX because output() is required this will always be true
            writer_data = std::make_unique<osmium::io::Writer>(
                output(), header, osmium::io::overwrite::allow);
        }

        osmium::memory::Buffer out_buffer{1024};

        vout() << "Processing data...\n";
        while (osmium::memory::Buffer buffer = reader.read()) {
            for (auto const &object : buffer.select<osmium::OSMObject>()) {
                if (okay(object, &out_buffer)) {
                    if (writer_data) {
                        (*writer_data)(object);
                    }
                } else {
                    writer_error(object);
                    if (writer_data) {
                        (*writer_data)(out_buffer.get<osmium::OSMObject>(0));
                    }
                    out_buffer.clear();
                }
            }
        }

        if (writer_data) {
            writer_data->close();
        }
        writer_error.close();
        reader.close();

        vout() << "Errors:\n"
               << "  users : " << m_errors_on_users << "\n"
               << "  keys  : " << m_errors_on_keys << "\n"
               << "  values: " << m_errors_on_values << "\n"
               << "  roles : " << m_errors_on_roles << "\n";
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
