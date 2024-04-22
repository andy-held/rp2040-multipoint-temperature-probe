#include <onewire.hpp>
#include <ds18b20_host.hpp>
#include <mqtt_client.hpp>
#include <util.hpp>

#include <pico/binary_info.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

#include <array>
#include <bitset>
#include <stdio.h>
#include <stdexcept>
#include <string_view>

constexpr const char* wifi_ssid = "";
constexpr const char* wifi_password = "";
constexpr const char* mqtt_hostname = "";
constexpr const uint32_t mqtt_port = 1883;
constexpr const char* mqtt_user = "client";
constexpr const char* mqtt_pass = "";
constexpr const char* mqtt_client_id = "test";
constexpr const std::string_view topic_prefix = "heizung/temperature/";

int main()
{
    bi_decl(bi_program_description("This is a multi-point temperature probe"));

    stdio_init_all();
    printf("Start multi-point temperature probe %s\n", mqtt_client_id);

    init_wifi(CYW43_COUNTRY_GERMANY);
    while(true)
    {
        try
        {
            connect_wifi(wifi_ssid, wifi_password);
            break;
        } catch (std::runtime_error& err)
        {
            printf("WIFI connection could not be established: %s \n", err.what());
            printf("Retrying in 10 seconds\n");
            sleep_ms(10000);
        }
    }

    auto try_creating_client = []()
    {
        while (true)
        {
            try
            {
                return mqtt::client(mqtt_client_id, mqtt_hostname, mqtt_port, mqtt_user, mqtt_pass);
            } catch (std::runtime_error& err)
            {
                printf("MQTT connection could not be established %s \n", err.what());
                printf("Retrying in 10 seconds\n");
                sleep_ms(10000);
            }
        }
    };

    auto client = try_creating_client();

    auto received_mqtt_fun = [](const std::string& message)
    {
        printf("Received %s for topic 1\n", message.c_str());
    };
    client.subscribe<const std::string&>("test/topic1", received_mqtt_fun);

    auto received_mqtt_fun2 = [](const std::string& message)
    {
        printf("Received %s for topic 2\n", message.c_str());
    };
    client.subscribe<const std::string&>("test/topic2", received_mqtt_fun2);

    constexpr std::string_view msg0 = "testing testing testing";
    constexpr std::string_view msg1 = "schmesting schmesting";
    int i = 0;
    while(true)
    {
        std::string_view msg;
        if(i%2 == 0)
        {
            msg = msg0;
        }
        else
        {
            msg = msg1;
        }
        sleep_ms(10000);
        if(i%4 < 2)
        {
            client.publish<std::string_view>("test/topic1", msg);
        }
        else
        {
            client.publish<std::string_view>("test/topic2", msg);
        }
        print_string_view(msg);
        i++;
    }

    // std::array<onewire, 2> wires
    // {
    //     onewire(15, 14),
    //     onewire(17, 16)
    // };

    // std::array<ds18b20_host, 2> hosts
    // {
    //     ds18b20_host(wires[0]),
    //     ds18b20_host(wires[1])
    // };

    // std::array<char, topic_prefix.size() + 17> topic_str_buf;
    // std::copy(topic_prefix.begin(), topic_prefix.end(), topic_str_buf.data());
    // std::array<char, 9> temp_str_buf;
    // while(true)
    // {
    //     for(const auto& host: hosts)
    //     {
    //         host.request_readings();
    //     }

    //     sleep_ms(760); /* 12bit: max. 750 ms */

    //     for(auto& host: hosts)
    //     {
    //         const auto readings = host.retrieve_readings();
    //         for(const auto reading: readings)
    //         {
    //             sprintf(topic_str_buf.data() + topic_prefix.size(), "%llx", reading.identifier);
    //             auto temp = reading.temperature * 0.0625f;
    //             auto temp_str_char_count = sprintf(temp_str_buf.data(), "%6.2f", temp);
    //             client.publish(topic_str_buf.data(), temp_str_buf.data(), temp_str_char_count);
    //             printf("%s : %s\n", topic_str_buf.data(), temp_str_buf.data());
    //         }
    //     }

    //     sleep_ms(58000);
    // }
}
