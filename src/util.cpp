#include <util.hpp>

#include <cassert>
#include <climits>

void print_string_view(const std::string_view str)
{
    assert(str.length() <= INT_MAX);
    printf(
        "%.*s\n",
        static_cast<int>(str.length()),
        str.data());
}
