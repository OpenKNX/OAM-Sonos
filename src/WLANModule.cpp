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
void WLANModule::showHelp()
{
    openknx.console.printHelpLine("wlan", "Show IP address if connected");
}

bool WLANModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    if (cmd == "wlan")
    {
        if (!wlanConfigured)
        {
            Serial.println();
            Serial.println("WiFi configuration missing");
            return true;
        }
        auto state = WiFi.status();
        Serial.println();
        Serial.print("State: ");
        switch(state)
        {
            case WL_NO_SHIELD:
                Serial.println("no shield");
                break;
            case WL_IDLE_STATUS:
                Serial.println("idle");
                break;
            case WL_NO_SSID_AVAIL:
                Serial.println("no ssid available");
                break;
            case WL_SCAN_COMPLETED:
                Serial.println("scan complete");
                break;
            case WL_CONNECTED:
                Serial.println("connected");
                break;
            case WL_CONNECT_FAILED:
                Serial.println("connection failed");
                break;
            case WL_CONNECTION_LOST:
                Serial.println("connection lost");
                break;
            case WL_DISCONNECTED:
                Serial.println("disconnected");
                break;
            default:
                Serial.println("unknown");
                break;
        }
        if (usingSavedConfiguration)
            Serial.print("SSID (KNX config missing, using fallback from flash): ");
        else
            Serial.print("SSID: ");
        Serial.println(WiFi.SSID());
        if (state == WL_CONNECTED)
        {
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
            Serial.print("Gateway-IP: ");
            Serial.println(WiFi.gatewayIP());
        }
        return true;
    }
    return false;
}

bool WLANModule::connected()
{
    return WiFi.status() == WL_CONNECTED;
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
                wlanConfigured = true;
                usingSavedConfiguration = true;
                WiFi.mode(WIFI_STA);
                WiFi.begin(ssid, password);
            }
        }
    }
    else
    {
        KoWLAN_WLANState.value(false, DPT_Switch);
        if (strlen((const char *)ParamWLAN_WifiSSID) > 0)
        {
            wlanConfigured = true;
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

void WLANModule::loop()
{
#if ARDUINO_ARCH_ESP32 
    bool isConnected = connected();   
    if ((bool)KoWLAN_WLANState.value(DPT_Switch) != isConnected)
    {
        KoWLAN_WLANState.value(isConnected, DPT_Switch);
    }
#endif
}


WLANModule openknxWLANModule;