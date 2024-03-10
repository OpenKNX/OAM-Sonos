#include "WLANModule.h"
#include <Preferences.h>

WLANModule::WLANModule()
{

}

const std::string WLANModule::name()
{
    return "WLAN.Module";
}

const std::string WLANModule::version()
{
    // hides the module in the version output on the console, because the firmware version is sufficient.
    return "";
}

void WLANModule::setup(bool configured)
{
#if ARDUINO_ARCH_ESP32     
    Preferences preferences;
    if (!configured)
    {
        if (preferences.begin("WLAN", true))
        {
            auto ssid = preferences.getString("SSID");
            auto password = preferences.getString("PWD");
            if (ssid.length() > 0)
            {
                WiFi.mode(WIFI_STA);
                WiFi.begin(ssid, password);
            }
        }
    }
    else
    {
        if (strlen((const char *)ParamWLAN_WifiSSID) > 0)
        {
            WiFi.mode(WIFI_STA);
            WiFi.begin((const char *)ParamWLAN_WifiSSID, (const char *)ParamWLAN_WifiPassword);
            bool needSave = true;
            if (preferences.begin("WLAN", true))
            {
                auto ssid = preferences.getString("SSID");
                auto password = preferences.getString("PWD");
                if (ssid == (const char *)ParamWLAN_WifiSSID && password == (const char *)ParamWLAN_WifiPassword)
                    needSave = false;
                preferences.end();
            }
            if (needSave)
            {
                preferences.begin("WLAN", false);
                preferences.putString("SSID", (const char *)ParamWLAN_WifiSSID);
                preferences.putString("PWD", (const char *)ParamWLAN_WifiPassword);
            }
        }
    }

#endif
    // Do not call baseclass, baseclass will be called after first WiFi connection
}

WLANModule openknxWLANModule;