#include <subscription.hpp>

namespace mqtt::detail
{
template<>
std::tuple<const void*, uint32_t> get_data_view<const std::string&>(const std::string& data)
{
    return {static_cast<const void*>(data.data()), data.length()};
}

template<>
std::tuple<const void*, uint32_t> get_data_view<std::string_view>(std::string_view data)
{
    return {static_cast<const void*>(data.data()), data.length()};
}
}
