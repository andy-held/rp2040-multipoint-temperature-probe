#pragma once

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <tuple>

namespace mqtt::detail
{
template<typename T>
std::tuple<const void*, uint32_t> get_data_view(T data)
{
    return {static_cast<const void*>(&data), sizeof(T)};
}

template<>
std::tuple<const void*, uint32_t> get_data_view<const std::string&>(const std::string& data);

template<>
std::tuple<const void*, uint32_t> get_data_view<std::string_view>(std::string_view data);

struct AbstractSubscriptionCallbackWrapper
{
    virtual ~AbstractSubscriptionCallbackWrapper() = default;
    virtual void check_size(uint32_t size) = 0;
    virtual void receive(const uint8_t* bytes, size_t size) = 0;
    virtual void resolve() = 0;
    virtual void reset() = 0;
};

template<typename T>
struct SubscriptionCallbackWrapper: AbstractSubscriptionCallbackWrapper
{
    SubscriptionCallbackWrapper(void (*callback_in)(T)):
        callback(callback_in)
    {}

    ~SubscriptionCallbackWrapper() override = default;

    void check_size(uint32_t size) override
    {
        if (size != sizeof(T))
        {
            printf("ERROR: incoming MQTT data does not match subscribed size.\n");
        }
    }

    void reveice(const uint8_t* bytes, size_t size) override
    {
        if(bytes_received + size <= sizeof(T))
        {
            std::memcpy(&data[bytes_received], bytes, size);
            bytes_received += size;
        }
    }

    void resolve() override
    {
        assert(bytes_received == sizeof(T));
        T t;
        std::memcpy(&t, data.data(), sizeof(T)); // to prevent alignment problems
        callback(t);
        reset();
    }

    void reset() override
    {
        data = {};
        bytes_received = 0;
    }

    void (*callback)(T);
    std::array<uint8_t, sizeof(T)> data; // if this were just T this might cause alignment problems on ARM
    size_t bytes_received = 0;
};

template<>
struct SubscriptionCallbackWrapper<const std::string&>: AbstractSubscriptionCallbackWrapper
{
    SubscriptionCallbackWrapper(void (*callback_in)(const std::string&)):
        callback(callback_in)
    {}

    ~SubscriptionCallbackWrapper() override = default;

    void check_size(uint32_t /*size*/) override
    {
    }

    void receive(const uint8_t* bytes, size_t size) override
    {
        stream.write((const char*)bytes, size);
    }

    void resolve() override
    {
        stream.put('\0');
        callback(stream.str());
        reset();
    }

    void reset() override
    {
        stream.clear();
        stream.str("");
    }

    void (*callback)(const std::string&);
    std::ostringstream stream;
};
}
