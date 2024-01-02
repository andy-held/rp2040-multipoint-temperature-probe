#pragma once

#include <onewire.hpp>

#include <cstdint>
#include <vector>

class ds18b20_host
{
  public:
    struct reading
    {
        uint64_t identifier;
        uint16_t temperature;
    };

    ds18b20_host(const onewire &wire);

    void request_readings() const;
    std::vector<reading> retrieve_readings();

  private:
    struct device
    {
        uint64_t identifier;
        uint8_t crc_fails;
    };
    const onewire &wire;
    std::vector<device> devices;
};
