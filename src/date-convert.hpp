#ifndef OSMIUM_SURPLUS_DATE_CONVERT_HPP
#define OSMIUM_SURPLUS_DATE_CONVERT_HPP

#include <osmium/osm/timestamp.hpp>

#include <ctime>

class DateConvert
{
    static int const seconds_in_a_day = 60 * 60 * 24;

    std::time_t m_today;

public:
    DateConvert(std::time_t today = std::time(nullptr)) noexcept
    : m_today(today / seconds_in_a_day * seconds_in_a_day)
    {}

    unsigned int operator()(std::time_t t) noexcept
    {
        if (t == osmium::end_of_time()) {
            return 0;
        }
        return (m_today - t) / seconds_in_a_day;
    }

}; // class DateConvert

#endif // OSMIUM_SURPLUS_DATE_CONVERT_HPP
