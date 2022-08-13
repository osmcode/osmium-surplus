
#include "app.hpp"

#include <osmium/util/memory.hpp>

BasicApp::BasicApp(std::string name, std::string desc, with_output out)
: CLI::App(std::move(desc), std::move(name))
{
    add_option("INPUT-FILE", m_input, "Input file")
        ->required()
        ->type_name("OSM-DATA-FILE");
    add_flag("-q,--quiet", m_quiet, "Do not output progress reports");

    if (out == with_output::file) {
        add_option("-o,--output", m_output, "Output file")
            ->required()
            ->type_name("OSM-DATA-FILE");
        m_output_type = "file";
    } else if (out == with_output::db) {
        add_option("-o,--output", m_output, "Output database")
            ->required()
            ->type_name("SQLITE-DB-FILE");
        m_output_type = "database";
    } else if (out == with_output::dir) {
        add_option("-o,--output", m_output, "Output directory")
            ->required()
            ->type_name("DIR")
            ->check(CLI::ExistingDirectory);
        m_output_type = "directory";
    }
}

void BasicApp::pre()
{
    m_vout.verbose(!m_quiet);
    vout() << "Starting " << get_name() << "...\n";
    vout() << "Params: Reading from '" << input() << "'.\n";
    if (!m_output_type.empty()) {
        vout() << "        Writing to " << m_output_type << " '" << output()
               << "'.\n";
    }
}

void BasicApp::post()
{
    osmium::MemoryUsage mem;
    vout() << "Overall memory usage: {} MByte current, {} MBytes peak\n"_format(
        mem.current(), mem.peak());

    vout() << "Done.\n";
}
