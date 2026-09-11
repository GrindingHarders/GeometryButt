#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include "buttplug/buttplug.h"

#include <algorithm>
#include <cstdint>

using namespace geode::prelude;

static bool isEnabled() {
    return Mod::get()->getSettingValue<bool>("enabled");
}

static bool settingBool(char const* name) {
    return Mod::get()->getSettingValue<bool>(name);
}

static uint8_t getIntensity() {
    int64_t value =
        Mod::get()->getSettingValue<int64_t>("intensity");

    if (value < 0) {
        value = 0;
    }

    if (value > 100) {
        value = 100;
    }

    return static_cast<uint8_t>(value);
}

static uint32_t getDuration() {
    int64_t value =
        Mod::get()->getSettingValue<int64_t>("duration");

    if (value < 1) {
        value = 1;
    }

    if (value > 5000) {
        value = 5000;
    }

    return static_cast<uint32_t>(value);
}

static void vibrate() {
    if (!isEnabled()) {
        return;
    }

    bp_vibrate(getDuration(), getIntensity());
}

static void vibrateIntensity(uint8_t intensity, uint32_t duration) {
    if (!isEnabled()) {
        return;
    }

    if (intensity == 0) {
        return;
    }

    bp_vibrate(duration, intensity);
}

$on_mod(Loaded) {
    log::info("Geometry Butt (GD Buttplug.io Mod) loaded.");

    bp_result result = bp_init();

    if (result != BP_OK) {
        log::warn("Failed to initialize Buttplug client: {}", static_cast<int>(result));
    }
}

class $modify(ButtplugPlayLayer, PlayLayer) {

    struct Fields {

        bool m_attemptActive = false;

        bool m_playerHasDied = false;

        int m_lastProgress = -1;

        float m_progressTimer = 0.0f;
    };


    void startGame() {

        PlayLayer::startGame();

        m_fields->m_attemptActive = true;
        m_fields->m_playerHasDied = false;

        m_fields->m_lastProgress = -1;
        m_fields->m_progressTimer = 0.0f;

        if (isEnabled() && settingBool("on-level-start")) {
            vibrate();
        }
    }

    void resetLevel() {

        bool wasActive =
            m_fields->m_attemptActive;

        m_fields->m_attemptActive = false;
        m_fields->m_playerHasDied = false;

        m_fields->m_lastProgress = -1;
        m_fields->m_progressTimer = 0.0f;

        PlayLayer::resetLevel();

        if (wasActive && isEnabled() && settingBool("on-level-start")) {
            vibrate();
        }

        m_fields->m_attemptActive = true;
    }


    /*
     * vibrating on death
     */
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        bool isLevelPlayer = player == m_player1 || player == m_player2;
        
        PlayLayer::destroyPlayer(player, obj);
        
        if (!isLevelPlayer) {
            return;
        }
        
        if (!player || !player->m_isDead) {
            return;
        }
        
        if (isEnabled() && settingBool("on-death")) {
            vibrate();
        }
    }


    /*
     * vibrate on level completion
     */
    void levelComplete() {

        if (isEnabled() && settingBool("on-level-complete")) {
            vibrate();
        }

        m_fields->m_attemptActive = false;

        PlayLayer::levelComplete();
    }


    /*
     * vibrate on new best (i think on death will also do this automatically)
     */
    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {

        PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);

        if (isEnabled() && settingBool("on-new-best")) {
            vibrate();
        }
    }


    /*
     * progressive rumble
     * this is currently the one (aside from maybe on jump)
     * i am most disappointed on being unable to interupt the
     * current vibration on but eh its not like anyone will
     * seriously use this anyway its fine for now i guess
     */
    void updateProgressbar(void) {
        PlayLayer::updateProgressbar();

        if (!isEnabled()) {
            return;
        }

        if (!settingBool("progressive-rumble")) {
            return;
        }

        if (!m_fields->m_attemptActive) {
            return;
        }

        if (m_isPaused) {
            return;
        }

        int progress = getCurrentPercentInt();

        if (progress < 0) {
            progress = 0;
        }

        if (progress > 100) {
            progress = 100;
        }

        /*
         * updateProgressbar() can be called multiple times for
         * the same percentage, so only react when the percentage
         * actually changes.
         */
        if (progress == m_fields->m_lastProgress) {
            return;
        }

        m_fields->m_lastProgress = progress;


        int64_t minimum = Mod::get()->getSettingValue<int64_t>("progressive-min");

        int64_t maximum = Mod::get()->getSettingValue<int64_t>("progressive-max");


        
        if (minimum < 0) {
            minimum = 0;
        }

        if (minimum > 100) {
            minimum = 100;
        }

        if (maximum < 0) {
            maximum = 0;
        }

        if (maximum > 100) {
            maximum = 100;
        }

        if (maximum < minimum) {
            maximum = minimum;
        }


        int64_t intensity = minimum + ((maximum - minimum) * progress) / 100;

        log::debug("Progressive rumble: {}% -> intensity {}", progress, intensity);


        /*
         * Do not send zero-intensity vibrations.
         */
        if (intensity <= 0) {
            return;
        }


        bp_vibrate(120, static_cast<uint8_t>(intensity));
}


};


/*
 * do when jump
 * i want to eventually change it to keep vibrating while holding
 * (would make slow wave super fun ;)) but yeah im kinda too stupid
 * to know how and im not fucking asking an ai so this is all for now
 */

class $modify(ButtplugPlayerObject, PlayerObject) {

    bool pushButton(PlayerButton button) {

        bool result = PlayerObject::pushButton(button);

        if (button != PlayerButton::Jump) {
            return result;
        }

        if (!isEnabled()) {
            return result;
        }

        if (!settingBool("on-jump")) {
            return result;
        }

        if (!isPlayer1() && !isPlayer2()) {
            return result;
        }

        PlayLayer* playLayer = PlayLayer::get();

        if (!playLayer) {
            return result;
        }

        if (!playLayer->isGameplayActive()) {
            return result;
        }

        vibrate();

        return result;
    }
};
