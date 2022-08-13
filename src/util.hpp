#ifndef OSMIUM_SURPLUS_UTIL_HPP
#define OSMIUM_SURPLUS_UTIL_HPP

#include <algorithm>
#include <cmath>

template <typename T>
T max3(T a, T b, T c) noexcept
{
    return std::max(a, std::max(b, c));
}

template <typename T>
T mbytes(T value) noexcept
{
    return value / (1024 * 1024);
}

double percent(double val, double all) noexcept
{
    return std::round(val * 10000 / all) / 100;
}

#endif // OSMIUM_SURPLUS_UTIL_HPP
