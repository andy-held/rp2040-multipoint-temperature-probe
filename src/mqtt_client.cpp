#include <mqtt_client.hpp>

#include <lwip/dns.h>

#include <stdexcept>

template<>
std::tuple<const void*, uint32_t> get_data_view<std::string>(const std::string& data)
{
    return {static_cast<const void*>(data.data()), sizeof(data.length())};
}

void init_wifi(const char *ssid, const char *pass, uint32_t country, uint32_t auth, const uint32_t timeout)
{
    if (cyw43_arch_init_with_country(country))
    {
        throw std::runtime_error("Could not init to Wifi");
    }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, auth, timeout))
    {
        throw std::runtime_error("Wifi connection timed out");
    }
}

ip_addr_t run_dns_lookup(const char* hostname)
{
    printf("Running DNS query for %s.\n", hostname);

    auto dns_gethostbyname_cb = [](const char* /*name*/, const ip_addr_t *ipaddr, void *callback_arg)
    {
        auto& ip = *static_cast<ip_addr_t*>(callback_arg);
        printf("DNS query finished with resolved addr of %s.\n", ip4addr_ntoa(ipaddr));
        ip = *ipaddr;
    };

    ip_addr_t ip;
    ip.addr = 0;
    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(hostname, &ip, dns_gethostbyname_cb, &ip);
    cyw43_arch_lwip_end();

    if (err == ERR_ARG)
    {
        std::runtime_error("DNS lookup failed.");
    }

    if (err == ERR_INPROGRESS)
    {
        while (ip.addr == 0) // wait until the callback was called
        {
            sleep_ms(5);
        }
    }
    printf("Resolved ip %lu err code %d\n", (unsigned long)ip.addr, err);
    return ip;
}

mqtt_client::mqtt_client(const char* hostname, const uint32_t port, const char* user, const char* pass)
{
    remote_addr = run_dns_lookup(hostname);

    lwip_mqtt_client = mqtt_client_new();
    struct mqtt_connect_client_info_t ci;
    err_t err;

    memset(&ci, 0, sizeof(ci));

    ci.client_id = "PicoW";
    ci.client_user = user;
    ci.client_pass = pass;
    ci.keep_alive = 0;
    ci.will_topic = NULL;

    auto connection_cb = [](mqtt_client_t* /*client*/, void* /*arg*/, mqtt_connection_status_t status)
    {
        if (status != 0)
        {
            printf("Error during connection: err %d.\n", status);
        } else
        {
            printf("MQTT connected.\n");
        }
    };

    err = mqtt_client_connect(lwip_mqtt_client, &remote_addr, port, connection_cb, nullptr, &ci);

    if (err != ERR_OK)
    {
        std::runtime_error(std::string("mqtt_connect return ") + std::to_string(err) + "\n");
    }

    while(!mqtt_client_is_connected(lwip_mqtt_client))
    {
        sleep_ms(5);
    }
}

void mqtt_client::publish(const char* topic, const void *data, uint32_t data_len)
{
    auto pub_request_cb = [](void *callback_arg, err_t err)
    {
        auto client = *static_cast<mqtt_client *>(callback_arg);
        client.receiving = 0;
        client.received++;
        if(err != ERR_OK)
        {
            printf("pub_request_cb: err %d\n", err);
        }
    };
    constexpr const u8_t qos = 2; /* 0 1 or 2, see MQTT specification */
    constexpr const u8_t retain = 0;
    cyw43_arch_lwip_begin();
    receiving = 1;
    auto err = mqtt_publish(lwip_mqtt_client, topic, data, data_len, qos, retain, pub_request_cb, this);
    cyw43_arch_lwip_end();
    if (err != ERR_OK)
    {
        printf("Publish err: %d\n", err);
    }
}
