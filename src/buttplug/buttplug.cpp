#include "buttplug.h"
#include "buttplug_internal.hpp"

#include <chrono>
#include <thread>

#include <nlohmann/json.hpp>

#include <Geode/Geode.hpp>

using json = nlohmann::json;

namespace gd_buttplug {

static char const* const SERVER_URL =
    "ws://127.0.0.1:12345";

static uint32_t const PROTOCOL_MAJOR = 4;
static uint32_t const PROTOCOL_MINOR = 0;

Client& instance() {
    static Client client;
    return client;
}

Client::Client()
    : m_started(false)
    , m_connected(false)
    , m_next_message_id(1) {
}

Client::~Client() {
    stop();
}

void Client::start() {
    bool expected = false;

    if (!m_started.compare_exchange_strong(expected, true))
        return;

    configureSocket();
    m_socket.start();
}

void Client::stop() {
    bool expected = true;

    if (!m_started.compare_exchange_strong(expected, false))
        return;

    m_connected.store(false);

    m_socket.stop();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_vibrators.clear();
}

bool Client::isConnected() const {
    return m_connected.load();
}

uint32_t Client::vibratorCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    return static_cast<uint32_t>(m_vibrators.size());
}

void Client::configureSocket() {
    m_socket.setUrl(SERVER_URL);
    m_socket.setPingInterval(30);

    m_socket.setOnMessageCallback(
        [this](
            std::unique_ptr<ix::WebSocketMessage> const& message
        ) {
            this->onMessage(message);
        }
    );
}

void Client::onMessage(
    std::unique_ptr<ix::WebSocketMessage> const& message
) {
    if (!message)
        return;

    switch (message->type) {
        case ix::WebSocketMessageType::Open:
            handleOpen();
            break;

        case ix::WebSocketMessageType::Message:
            handleMessage(message->str);
            break;

        case ix::WebSocketMessageType::Close:
            m_connected.store(false);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_vibrators.clear();
            }

            geode::log::debug(
                "Buttplug connection closed."
            );

            break;

        case ix::WebSocketMessageType::Error:
            m_connected.store(false);

            geode::log::warn(
                "Buttplug WebSocket error: {}",
                message->errorInfo.reason
            );

            break;

        default:
            break;
    }
}

void Client::handleOpen() {
    m_connected.store(true);

    geode::log::info(
        "Connected to Buttplug/Intiface."
    );

    sendRequestServerInfo();
}

void Client::sendRequestServerInfo() {
    json message = json::array();

    json request;

    request["Id"] = m_next_message_id++;
    request["ClientName"] = "Geometry Dash Geode";
    request["ProtocolVersionMajor"] = PROTOCOL_MAJOR;
    request["ProtocolVersionMinor"] = PROTOCOL_MINOR;

    message.push_back({
        {"RequestServerInfo", request}
    });

    sendJson(message.dump());
}

void Client::requestDeviceList() {
    json message = json::array();

    json request;

    request["Id"] = m_next_message_id++;

    message.push_back({
        {"RequestDeviceList", request}
    });

    sendJson(message.dump());
}

void Client::handleMessage(
    std::string const& message
) {
    try {
        json root = json::parse(message);

        if (!root.is_array())
            return;

        for (json const& packet : root) {
            if (!packet.is_object())
                continue;

            if (packet.contains("ServerInfo")) {
                geode::log::info(
                    "Buttplug server handshake successful."
                );

                requestDeviceList();
                continue;
            }

            if (packet.contains("DeviceList")) {
                parseDeviceList(message);
                continue;
            }

            if (packet.contains("Error")) {
                geode::log::warn(
                    "Buttplug returned an Error message: {}",
                    packet["Error"].dump()
                );
            }
        }
    }
    catch (json::exception const& e) {
        geode::log::warn(
            "Failed to parse Buttplug message: {}",
            e.what()
        );
    }
}

void Client::parseDeviceList(
    std::string const& message
) {
    try {
        json root = json::parse(message);

        if (!root.is_array())
            return;

        std::vector<Vibrator> new_vibrators;

        for (json const& packet : root) {
            if (!packet.contains("DeviceList"))
                continue;

            json const& list = packet["DeviceList"];

            if (!list.contains("Devices"))
                continue;

            json const& devices = list["Devices"];

            if (!devices.is_object())
                continue;

            for (
                auto it = devices.begin();
                it != devices.end();
                ++it
            ) {
                json const& device = it.value();

                if (!device.contains("DeviceIndex"))
                    continue;

                uint32_t device_index =
                    device["DeviceIndex"].get<uint32_t>();

                if (!device.contains("DeviceFeatures"))
                    continue;

                json const& features =
                    device["DeviceFeatures"];

                if (!features.is_object())
                    continue;

                for (
                    auto feature_it = features.begin();
                    feature_it != features.end();
                    ++feature_it
                ) {
                    json const& feature =
                        feature_it.value();

                    if (!feature.contains("FeatureIndex"))
                        continue;

                    if (!feature.contains("Output"))
                        continue;

                    json const& output =
                        feature["Output"];

                    if (!output.contains("Vibrate"))
                        continue;

                    json const& vibrate =
                        output["Vibrate"];

                    if (!vibrate.contains("Value"))
                        continue;

                    json const& range =
                        vibrate["Value"];

                    if (!range.is_array() ||
                        range.size() != 2) {
                        continue;
                    }

                    Vibrator vibrator;

                    vibrator.device_index =
                        device_index;

                    vibrator.feature_index =
                        feature["FeatureIndex"]
                            .get<uint32_t>();

                    vibrator.minimum =
                        range[0].get<int32_t>();

                    vibrator.maximum =
                        range[1].get<int32_t>();

                    if (vibrator.maximum <
                        vibrator.minimum) {
                        continue;
                    }

                    new_vibrators.push_back(vibrator);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_vibrators.swap(new_vibrators);
        }

        geode::log::info(
            "Buttplug: found {} vibrator feature(s).",
            vibratorCount()
        );
    }
    catch (json::exception const& e) {
        geode::log::warn(
            "Failed to parse Buttplug DeviceList: {}",
            e.what()
        );
    }
}

int32_t Client::scaleIntensity(
    uint8_t intensity,
    int32_t minimum,
    int32_t maximum
) {
    if (maximum <= minimum)
        return minimum;

    int64_t range =
        static_cast<int64_t>(maximum) -
        static_cast<int64_t>(minimum);

    int64_t value =
        static_cast<int64_t>(minimum) +
        (range * intensity) / 100;

    if (value < minimum)
        value = minimum;

    if (value > maximum)
        value = maximum;

    return static_cast<int32_t>(value);
}

void Client::setIntensity(uint8_t intensity) {
    if (!m_connected.load()) {
        return;
    }

    if (intensity > 100) {
        intensity = 100;
    }

    std::vector<Vibrator> vibrators;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        vibrators = m_vibrators;
    }

    for (Vibrator const& vibrator : vibrators) {
        int32_t value = scaleIntensity(
            intensity,
            vibrator.minimum,
            vibrator.maximum
        );

        sendVibrate(
            vibrator.device_index,
            vibrator.feature_index,
            value
        );
    }
}

void Client::vibrate(
    uint32_t duration_ms,
    uint8_t intensity
) {
    std::vector<Vibrator> vibrators;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        vibrators = m_vibrators;
    }

    for (Vibrator const& vibrator : vibrators) {
        int32_t value = scaleIntensity(
            intensity,
            vibrator.minimum,
            vibrator.maximum
        );

        sendVibrate(
            vibrator.device_index,
            vibrator.feature_index,
            value
        );
    }

    std::thread(
        [this, duration_ms]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(duration_ms)
            );

            this->stopDevices();
        }
    ).detach();
}

void Client::sendVibrate(
    uint32_t device_index,
    uint32_t feature_index,
    int32_t value
) {
    json command = json::array();

    json output;

    output["Id"] = m_next_message_id++;
    output["DeviceIndex"] = device_index;
    output["FeatureIndex"] = feature_index;

    output["Command"] = {
        {
            "Vibrate",
            {
                {"Value", value}
            }
        }
    };

    command.push_back({
        {"OutputCmd", output}
    });

    sendJson(command.dump());
}

void Client::stopDevices() {
    json command = json::array();

    json stop;

    stop["Id"] = m_next_message_id++;

    command.push_back({
        {"StopCmd", stop}
    });

    sendJson(command.dump());
}

void Client::sendJson(
    std::string const& message
) {
    if (!m_connected.load())
        return;

    m_socket.send(message);
}

} // namespace gd_buttplug


extern "C" {

bp_result bp_init(void) {
    gd_buttplug::instance().start();

    return BP_OK;
}

void bp_shutdown(void) {
    gd_buttplug::instance().stop();
}

int32_t bp_is_connected(void) {
    return gd_buttplug::instance().isConnected()
        ? 1
        : 0;
}

uint32_t bp_vibrator_count(void) {
    return gd_buttplug::instance().vibratorCount();
}

bp_result bp_vibrate(
    uint32_t duration_ms,
    uint8_t intensity
) {
    if (duration_ms == 0 || intensity > 100)
        return BP_INVALID_ARGUMENT;

    if (!gd_buttplug::instance().isConnected())
        return BP_NOT_CONNECTED;

    if (gd_buttplug::instance().vibratorCount() == 0)
        return BP_NO_DEVICES;

    gd_buttplug::instance().vibrate(
        duration_ms,
        intensity
    );

    return BP_OK;
}

bp_result bp_stop(void) {
    if (!gd_buttplug::instance().isConnected())
        return BP_NOT_CONNECTED;

    gd_buttplug::instance().stopDevices();

    return BP_OK;
}

bp_result bp_set_intensity(uint8_t intensity) {
    if (intensity > 100) {
        return BP_INVALID_ARGUMENT;
    }

    if (!gd_buttplug::instance().isConnected()) {
        return BP_NOT_CONNECTED;
    }

    if (gd_buttplug::instance().vibratorCount() == 0) {
        return BP_NO_DEVICES;
    }

    gd_buttplug::instance().setIntensity(
        intensity
    );

    return BP_OK;
}

}
