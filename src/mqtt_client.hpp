#pragma once

#include <map>
#include <memory>
#include <string>
#include <tuple>

#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>

#include <subscription.hpp>

typedef struct mqtt_client_s mqtt_client_t;

void init_wifi(uint32_t country);

void connect_wifi(const char *ssid, const char *pass, uint32_t auth = CYW43_AUTH_WPA2_AES_PSK, const uint32_t timeout = 10000);

namespace mqtt
{
struct client
{
    client(const char* client_id, ip_addr_t remote_addr, const uint32_t port = 1883, const char* user = nullptr, const char* pass = nullptr);

    client(const char* client_id, const char* hostname, const uint32_t port = 1883, const char* user = nullptr, const char* pass = nullptr);

    ~client();

    void publish(const char* topic, const void* data, uint32_t data_len);

    template<typename T>
    void publish(const char* topic, T data)
    {
        auto [ptr, len] = detail::get_data_view<T>(data);
        publish(topic, ptr, len);
    }

    template<typename T>
    void subscribe(const char* topic, void (*callback)(T))
    {
        subscribe_detail(topic, std::make_unique<detail::SubscriptionCallbackWrapper<T>>(callback));
    }

    void unsubscribe(const char* topic);

    void subscribe_detail(const char* topic, std::unique_ptr<detail::AbstractSubscriptionCallbackWrapper> callback);

    ip_addr_t remote_addr;
    mqtt_client_t* lwip_mqtt_client;
    std::map<std::string, std::unique_ptr<detail::AbstractSubscriptionCallbackWrapper>> active_callbacks;
    detail::AbstractSubscriptionCallbackWrapper* active_callback = nullptr;
};

}
