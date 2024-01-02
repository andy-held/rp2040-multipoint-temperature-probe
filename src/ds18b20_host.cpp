#include <ds18b20_host.hpp>

#include <onewire_defs.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace
{
constexpr const uint DS18B20_FAMILY_CODE = 0x28;

constexpr const uint DS18B20_CONVERT_T_COMMAND = 0x44;
constexpr const uint DS18B20_READ_SCRATCHPAD_COMMAND = 0xbe;
constexpr const uint DS18B20_WRITE_SCRATCHPAD_COMMAND = 0x4e;
constexpr const uint DS18B20_COPY_SCRATCHPAD_COMMAND = 0x48;
constexpr const uint DS18B20_RECALL_E2_COMMAND = 0xB8;
constexpr const uint DS18B20_READ_POWER_SUPPLY_COMMAND = 0xB4;
}

ds18b20_host::ds18b20_host(const onewire &wire_in):
    wire(wire_in)
{
    auto device_ids = wire.search();

    for(auto identifier: device_ids)
    {
        if(!(identifier & DS18B20_FAMILY_CODE))
        {
            continue;
        }
        devices.push_back({identifier, 0});
        printf("device found: %llx\n", identifier);
    }
    printf("Found %zu devices\n", devices.size());
}

void ds18b20_host::request_readings() const
{
    if (!wire.reset())
    {
        printf("wire reset failed\n");
        return;
    }
    wire.transmit(ONEWIRE_SKIP_ROM_COMMAND);
    wire.transmit(DS18B20_CONVERT_T_COMMAND);
}

std::vector<ds18b20_host::reading> ds18b20_host::retrieve_readings()
{
    std::vector<reading> readings;
    for(const auto& dev: devices)
    {
        if (!wire.reset())
        {
            printf("wire reset failed\n");
            continue;
        }
        wire.transmit(ONEWIRE_MATCH_ROM_COMMAND);
        for (int i = 0; i < 8; i++)
        {
            wire.transmit(reinterpret_cast<const uint8_t*>(&dev.identifier)[i]);
        }

        wire.transmit(DS18B20_READ_SCRATCHPAD_COMMAND);
        uint8_t buf[9];
        for(int i = 0; i < 9; i++)
        {
            buf[i] = wire.receive();
        }
        if (calc_crc8(buf, 8) != buf[8])
        {
            printf("crc failed ");
            for (int i = 0; i < 9; i++)
            {
                printf("%hhu ", buf[i]);
            }
            printf("\n");
            continue;
        }
        reading read{dev.identifier, 0};
        std::memcpy(&read.temperature, buf, sizeof(uint16_t));
        readings.push_back(read);
    }
    return readings;
}
