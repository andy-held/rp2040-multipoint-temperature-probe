#include <onewire.hpp>
#include <ds18b20_host.hpp>
#include <mqtt_client.hpp>

#include <pico/binary_info.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

#include <array>
#include <bitset>
#include <stdio.h>
#include <string_view>

constexpr const char* wifi_ssid = "";
constexpr const char* wifi_password = "";
constexpr const char* mqtt_hostname = "";
constexpr const uint32_t mqtt_port = 1883;
constexpr const char* mqtt_user = "";
constexpr const char* mqtt_pass = "";
constexpr const char* mqtt_client_id = "picoW";
constexpr const std::string_view topic_prefix = "temperature_";

int main()
{
    bi_decl(bi_program_description("This is a multi-point temperature probe"));

    stdio_init_all();
    printf("Start multi-point temperature probe\n");

    init_wifi(wifi_ssid, wifi_password, CYW43_COUNTRY_GERMANY);

    auto client = mqtt_client(mqtt_hostname, mqtt_port, mqtt_client_id, mqtt_user, mqtt_pass);

    std::array<onewire, 2> wires
    {
        onewire(15, 14),
        onewire(17, 16)
    };

    std::array<ds18b20_host, 2> hosts
    {
        ds18b20_host(wires[0]),
        ds18b20_host(wires[1])
    };

    std::array<char, topic_prefix.size() + 17> topic_str_buf;
    std::copy(topic_prefix.begin(), topic_prefix.end(), topic_str_buf.data());
    std::array<char, 9> temp_str_buf;
    while(true)
    {
        for(const auto& host: hosts)
        {
            host.request_readings();
        }

        sleep_ms(760); /* 12bit: max. 750 ms */

        for(auto& host: hosts)
        {
            const auto readings = host.retrieve_readings();
            for(const auto reading: readings)
            {
                sprintf(topic_str_buf.data() + topic_prefix.size(), "%llx", reading.identifier);
                auto temp = reading.temperature * 0.0625f;
                auto temp_str_char_count = sprintf(temp_str_buf.data(), "%6.2f", temp);
                client.publish(topic_str_buf.data(), temp_str_buf.data(), temp_str_char_count);
                printf("%s : %s\n", topic_str_buf.data(), temp_str_buf.data());
            }
        }

        sleep_ms(58000);
    }
}
