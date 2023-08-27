
#include "app.hpp"
#include "util.hpp"

#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/tags/filter.hpp>
#include <osmium/util/verbose_output.hpp>
#include <osmium/visitor.hpp>

#include <cstdlib>
#include <iterator>

/* ========================================================================= */

struct StatHandler : public osmium::handler::Handler
{
    int64_t count_nodes = 0;
    int64_t count_no_tags = 0;
    int64_t count_important_tags = 0;

    osmium::tags::KeyFilter filter{true};

    StatHandler()
    {
        filter.add(false, "LINZ:source_version");
        filter.add(false, "attribution");
        filter.add(false, "converted_by");
        filter.add(false, "created_by");
        filter.add(false, "gnis:created");
        filter.add(false, "gnis:feature_id");
        filter.add(false, "odbl");
        filter.add(false, "osak:identifier");
        filter.add(false, "osak:revision");
        filter.add(false, "source");
        filter.add(false, "source:addr");
        filter.add(false, "source:date");
        filter.add(false, "source:file");
        filter.add(false, "source_ref");
        filter.add(false, "tiger:tlid");
        filter.add(false, "tiger:tzid");
        filter.add(false, "Tiger:MTFCC");
        filter.add(false, "tiger:reviewed");
        filter.add(false, "tiger:country");
        filter.add(false, "tiger:upload_uuid");
        filter.add(false, "tiger:name_base");
    }

    void node(osmium::Node const &node)
    {
        ++count_nodes;

        if (node.tags().empty()) {
            ++count_no_tags;
            return;
        }

        osmium::TagList const &tags = node.tags();
        osmium::tags::KeyFilter::iterator const fi_begin{filter, tags.begin(),
                                                         tags.end()};
        osmium::tags::KeyFilter::iterator const fi_end{filter, tags.end(),
                                                       tags.end()};

        auto const c = std::distance(fi_begin, fi_end);

        if (c > 0) {
            ++count_important_tags;
        }
    }

    void output_stats() const
    {
        auto const count_unimportant_tags = count_nodes - count_important_tags;

        fmt::print("all nodes:                        {:10d} (100.00%)\n",
                   count_nodes);
        fmt::print("nodes with important tags:        {:10d} ({:6.2f}%)\n",
                   count_important_tags,
                   percent(count_important_tags, count_nodes));
        fmt::print("nodes without tags:               {:10d} ({:6.2f}%)\n",
                   count_no_tags, percent(count_no_tags, count_nodes));
        fmt::print("nodes with only unimportant tags: {:10d} ({:6.2f}%)\n",
                   count_unimportant_tags,
                   percent(count_unimportant_tags, count_nodes));
    }

}; // class StatHandler

/* ========================================================================= */

class App : public BasicApp
{
public:
    App()
    : BasicApp("osp-stats-tags-on-nodes", "Calculate some stats on node tags",
               with_output::none)
    {}

    void run()
    {
        StatHandler handler;
        osmium::io::Reader reader{input()};

        vout() << "Processing data...\n";
        osmium::apply(reader, handler);
        reader.close();
        vout() << "Done processing.\n";
        handler.output_stats();
    }
}; // class App

int main(int argc, char *argv[]) { return run_app<App>(argc, argv); }
