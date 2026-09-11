#ifndef GD_BUTTPLUG_INTERNAL_HPP
#define GD_BUTTPLUG_INTERNAL_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <ixwebsocket/IXWebSocket.h>

namespace gd_buttplug {

struct Vibrator {
    uint32_t device_index;
    uint32_t feature_index;
    int32_t minimum;
    int32_t maximum;
};

class Client {
public:
    Client();
    ~Client();

    Client(Client const&) = delete;
    Client& operator=(Client const&) = delete;

    void start();
    void stop();

    bool isConnected() const;
    uint32_t vibratorCount() const;

    void vibrate(uint32_t duration_ms, uint8_t intensity);

    void setIntensity(
        uint8_t intensity
    );

    void stopDevices();

private:
    void configureSocket();

    void onMessage(
        std::unique_ptr<ix::WebSocketMessage> const& message
    );

    void handleOpen();
    void handleMessage(std::string const& message);

    void parseDeviceList(std::string const& message);

    void sendRequestServerInfo();
    void requestDeviceList();

    void sendVibrate(
        uint32_t device_index,
        uint32_t feature_index,
        int32_t value
    );

    void sendJson(std::string const& message);

    static int32_t scaleIntensity(
        uint8_t intensity,
        int32_t minimum,
        int32_t maximum
    );

private:
    ix::WebSocket m_socket;

    std::atomic<bool> m_started;
    std::atomic<bool> m_connected;

    mutable std::mutex m_mutex;
    std::vector<Vibrator> m_vibrators;

    std::atomic<uint32_t> m_next_message_id;
};

Client& instance();

}

#endif
