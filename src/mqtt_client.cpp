#include "lwip/err.h"
#include <mqtt_client.hpp>

#include <lwip/dns.h>
#include <lwip/apps/mqtt.h>

#include <stdexcept>

namespace
{
struct MQTT_Connection_Status
{
    int status = -1;

    explicit operator std::string_view() const
    {
        switch(status)
        {
        case MQTT_CONNECT_ACCEPTED:
            return "Connected";
        case MQTT_CONNECT_REFUSED_PROTOCOL_VERSION:
            return "Refused protocol version";
        case MQTT_CONNECT_REFUSED_IDENTIFIER:
            return "Refused identifier";
        case MQTT_CONNECT_REFUSED_SERVER:
            return "Refused server";
        case MQTT_CONNECT_REFUSED_USERNAME_PASS:
            return "Refused username/password";
        case MQTT_CONNECT_REFUSED_NOT_AUTHORIZED_:
            return "Refused not authorized";
        case MQTT_CONNECT_DISCONNECTED:
            return "Disconnected";
        case MQTT_CONNECT_TIMEOUT:
            return "Timeout";
        default:
            return "Unkown MQTT connection status";
        }
    }
};

struct MQTT_Request_Status
{
    err_t error = 0;
    bool satisfied = false;
};

void request_cb(void *callback_arg, err_t err)
{
    auto& status = *static_cast<MQTT_Request_Status*>(callback_arg);
    status.satisfied = true;
    status.error = err;
}

struct DNS_Query_Status
{
    ip_addr_t ip;
    bool resolved = false;
    bool failed = false;
};


ip_addr_t run_dns_lookup(const char* hostname)
{
    printf("Running DNS query for %s.\n", hostname);

    auto dns_gethostbyname_cb = [](const char* /*name*/, const ip_addr_t *ipaddr, void *callback_arg)
    {
        auto& query_status = *static_cast<DNS_Query_Status*>(callback_arg);
        query_status.resolved = true;
        if(ipaddr)
        {
            query_status.ip = *ipaddr;
        }
        else
        {
            query_status.failed = true;
        }
    };

    DNS_Query_Status query_status;
    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(hostname, &query_status.ip, dns_gethostbyname_cb, &query_status);
    cyw43_arch_lwip_end();

    if (err == ERR_INPROGRESS)
    {
        while (!query_status.resolved) // wait until the callback was called
        {
            sleep_ms(5);
        }
    }

    if (err == ERR_ARG || query_status.failed)
    {
        throw std::runtime_error("DNS lookup failed.");
    }

    printf("DNS query finished with resolved addr of %s.\n", ip4addr_ntoa(&query_status.ip));
    return query_status.ip;
}

void internal_sub(const char* topic, mqtt_client_t* lwip_mqtt_client)
{
    constexpr const u8_t qos = 1; /* 0 1 or 2, see MQTT specification */
    MQTT_Request_Status status;
    cyw43_arch_lwip_begin();
    auto err = mqtt_subscribe(lwip_mqtt_client, topic, qos, request_cb, &status);
    cyw43_arch_lwip_end();
    if (err != ERR_OK)
    {
        throw std::runtime_error("MQTT calling subscribe returned error: " + std::to_string(err));
    }

    while(!status.satisfied)
    {
        sleep_ms(5);
    }
    if(status.error != ERR_OK)
    {
        throw std::runtime_error("MQTT subscribe failed: " + std::to_string(status.error));
    }
}

void internal_unsub(const char* topic, mqtt_client_t* lwip_mqtt_client)
{
    MQTT_Request_Status status;
    cyw43_arch_lwip_begin();
    auto err = mqtt_unsubscribe(lwip_mqtt_client, topic, request_cb, &status);
    cyw43_arch_lwip_end();
    if (err != ERR_OK)
    {
        throw std::runtime_error("MQTT calling subscribe returned error: " + std::to_string(err));
    }

    while(!status.satisfied)
    {
        sleep_ms(5);
    }
}
}

void init_wifi(uint32_t country)
{
    if (cyw43_arch_init_with_country(country))
    {
        throw std::runtime_error("Could not init to Wifi");
    }
    cyw43_arch_enable_sta_mode();
}

void connect_wifi(const char *ssid, const char *pass, uint32_t auth, const uint32_t timeout)
{
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, auth, timeout))
    {
        throw std::runtime_error("Wifi connection timed out");
    }
}

namespace mqtt
{
client::client(const char* client_id, ip_addr_t remote_addr_in, const uint32_t port, const char* user, const char* pass):
    remote_addr(remote_addr_in)
{
    auto incoming_publish_cb = [](void *arg, const char *topic, u32_t tot_len)
    {
        auto thiz = static_cast<client*>(arg);
        if(thiz->active_callback) // there is an unfinished receive callback
        {
            printf("ERROR: incoming MQTT callback while another callback is still active\n");
            thiz->active_callback->reset();
            thiz->active_callback = nullptr;
        }
        if (tot_len == 0)
        {
            // callbacks that do not have data are still called
            return;
        }
        auto iter = thiz->active_callbacks.find(std::string(topic));
        if(iter == thiz->active_callbacks.end())
        {
            printf("ERROR: incoming MQTT publish for topic \"%s\" this client is not subscribed to\n", topic);
            return;
        }
        thiz->active_callback = iter->second.get();
        thiz->active_callback->check_size(tot_len);
    };

    auto incoming_data_cb = [](void *arg, const u8_t *data, u16_t len, u8_t flags)
    {
        auto thiz = static_cast<client*>(arg);
        if(!thiz->active_callback) // there is no active callback
        {
            if(len != 0 || !(flags & MQTT_DATA_FLAG_LAST))
            {
                printf("ERROR: incoming MQTT data while no callback is active\n");
            }
            return;
        }
        thiz->active_callback->receive(data, len);
        if(flags & MQTT_DATA_FLAG_LAST)
        {
            thiz->active_callback->resolve();
            thiz->active_callback = nullptr;
        }
    };

    lwip_mqtt_client = mqtt_client_new();

    mqtt_set_inpub_callback(lwip_mqtt_client, incoming_publish_cb, incoming_data_cb, static_cast<void*>(this));

    struct mqtt_connect_client_info_t ci;

    memset(&ci, 0, sizeof(ci));

    ci.client_id = client_id;
    ci.client_user = user;
    ci.client_pass = pass;
    ci.keep_alive = 100;
    ci.will_topic = NULL;

    auto connection_cb = [](mqtt_client_t* /*client*/, void* arg, mqtt_connection_status_t status)
    {
        MQTT_Connection_Status* connection_status = reinterpret_cast<MQTT_Connection_Status*>(arg);
        connection_status->status = status;
    };

    err_t err;
    MQTT_Connection_Status connection_status;
    cyw43_arch_lwip_begin();
    err = mqtt_client_connect(lwip_mqtt_client, &remote_addr, port, connection_cb, &connection_status, &ci);
    cyw43_arch_lwip_end();

    if (err != ERR_OK)
    {
        throw std::runtime_error(std::string("mqtt_connect returned ") + std::to_string(err));
    }

    while(!mqtt_client_is_connected(lwip_mqtt_client))
    {
        if(connection_status.status > 0)
        {
            throw std::runtime_error(std::string("MQTT connection failed: ") + std::string(std::string_view(connection_status)));
        }
        sleep_ms(5);
    }

    printf("MQTT connected.\n");
}

client::client(const char* client_id, const char* hostname, const uint32_t port, const char* user, const char* pass):
    client(
        client_id,
        run_dns_lookup(hostname),
        port,
        user,
        pass
    )
{
}

client::~client()
{
    cyw43_arch_lwip_begin();
    mqtt_disconnect(lwip_mqtt_client);
    cyw43_arch_lwip_end();

    mqtt_client_free(lwip_mqtt_client);
}

void client::publish(const char* topic, const void *data, uint32_t data_len)
{
    while (active_callback)
    {
        sleep_ms(5);
    }

    for (auto const& [callback_topic, callback]: active_callbacks)
    {
        internal_unsub(callback_topic.c_str(), lwip_mqtt_client);
    }
    constexpr const u8_t qos = 1; /* 0 1 or 2, see MQTT specification */
    constexpr const u8_t retain = 0;
    MQTT_Request_Status status;
    cyw43_arch_lwip_begin();
    auto err = mqtt_publish(lwip_mqtt_client, topic, data, data_len, qos, retain, request_cb, &status);
    cyw43_arch_lwip_end();
    if (err != ERR_OK)
    {
        printf("MQTT calling publish returned error: %d\n", err);
    }

    while(!status.satisfied)
    {
        sleep_ms(5);
    }
    if(status.error != ERR_OK)
    {
        printf("MQTT publish failed: %d\n", status.error);
    }
    for (auto const&  [callback_topic, callback]: active_callbacks)
    {
        internal_sub(callback_topic.c_str(), lwip_mqtt_client);
    }
}

void client::unsubscribe(const char* topic)
{
    // TODO: what if the callback is active?
    auto number_of_removed = active_callbacks.erase(std::string(topic));
    if(number_of_removed == 0)
    {
        printf("WARNING: Unsubscribed a MQTT topic that was not subscribed.\n");
        return;
    }
    internal_unsub(topic, lwip_mqtt_client);
}

void client::subscribe_detail(const char* topic, std::unique_ptr<detail::AbstractSubscriptionCallbackWrapper> callback)
{
    auto iter = active_callbacks.find(std::string(topic));
    if(iter != active_callbacks.end())
    {
        printf("WARNING: Requested subscribing to a MQTT topic that was already subscribed.\n");
        return;
    }
    internal_sub(topic, lwip_mqtt_client);
    active_callbacks.emplace(std::make_pair(topic, std::move(callback)));
}
}
