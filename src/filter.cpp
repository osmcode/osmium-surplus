
#include "filter.hpp"

#include <osmium/util/string.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

static void strip_whitespace(std::string *string)
{
    assert(string);

    while (!string->empty() && string->back() == ' ') {
        string->pop_back();
    }

    auto const pos = string->find_first_not_of(' ');
    if (pos != std::string::npos) {
        string->erase(0, pos);
    }
}

static osmium::StringMatcher get_string_matcher(std::string string)
{
    strip_whitespace(&string);

    if (string.size() == 1 && string.front() == '*') {
        return osmium::StringMatcher::always_true{};
    }

    if (string.empty() || (string.back() != '*' && string.front() != '*')) {
        if (string.find(',') == std::string::npos) {
            return osmium::StringMatcher::equal{string};
        }
        auto sstrings = osmium::split_string(string, ',');
        for (auto &s : sstrings) {
            strip_whitespace(&s);
        }
        return osmium::StringMatcher::list{sstrings};
    }

    auto s = string;

    if (s.back() == '*' && s.front() != '*') {
        s.pop_back();
        return osmium::StringMatcher::prefix{s};
    }

    if (s.front() == '*') {
        s.erase(0, 1);
    }

    if (!s.empty() && s.back() == '*') {
        s.pop_back();
    }

    return osmium::StringMatcher::substring{s};
}

static osmium::TagMatcher get_tag_matcher(std::string const &expression)
{
    auto const op_pos = expression.find('=');
    if (op_pos == std::string::npos) {
        return osmium::TagMatcher{get_string_matcher(expression)};
    }

    auto key = expression.substr(0, op_pos);
    auto const value = expression.substr(op_pos + 1);

    bool invert = false;
    if (!key.empty() && key.back() == '!') {
        key.pop_back();
        invert = true;
    }

    return osmium::TagMatcher{get_string_matcher(key),
                              get_string_matcher(value), invert};
}

osmium::TagsFilter load_filter_patterns(std::string const &file_name)
{
    std::ifstream file{file_name};
    if (!file.is_open()) {
        throw std::runtime_error{"Could not open file '" + file_name + "'"};
    }

    osmium::TagsFilter filter{false};

    for (std::string line; std::getline(file, line);) {
        auto const pos = line.find_first_of('#');
        if (pos != std::string::npos) {
            line.erase(pos);
        }
        if (!line.empty()) {
            if (line.back() == '\r') {
                line.resize(line.size() - 1);
            }
            filter.add_rule(true, get_tag_matcher(line));
        }
    }

    return filter;
}
