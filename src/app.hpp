#ifndef OSMIUM_SURPLUS_APP_HPP
#define OSMIUM_SURPLUS_APP_HPP

#include "format.hpp"

#include <osmium/util/verbose_output.hpp>

#include <CLI11.hpp>

#include <string>

enum class with_output
{
    none,
    file,
    db,
    dir
};

class BasicApp : public CLI::App
{
    osmium::VerboseOutput m_vout{true};
    std::string m_input;
    std::string m_output;
    std::string m_output_type;
    bool m_quiet = false;

public:
    BasicApp(std::string name, std::string desc, with_output out);

    std::string const &input() const noexcept { return m_input; }

    std::string const &output() const noexcept { return m_output; }

    osmium::VerboseOutput &vout() noexcept { return m_vout; }

    bool verbose() const noexcept { return !m_quiet; }

    void pre();
    void post();

}; // class BasicApp

template <typename TApp>
int run_app(int argc, char *argv[]) noexcept
{
    try {
        TApp app;

        try {
            app.parse(argc, argv);
            app.pre();
            app.run();
            app.post();
        } catch (CLI::ParseError const &e) {
            return app.exit(e) > 0 ? 2 : 0;
        } catch (std::exception const &e) {
            fmt::print(stderr, "Error: {}\n"_format(e.what()));
            return 1;
        }
    } catch (...) {
        return 3;
    }

    return 0;
}

#endif // OSMIUM_SURPLUS_APP_HPP
