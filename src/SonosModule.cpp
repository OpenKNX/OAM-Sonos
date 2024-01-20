#include "SonosModule.h"
#include "SonosChannel.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

SonosModule::SonosModule()
    : ChannelOwnerModule(SON_ChannelCount)
{
}

const std::string SonosModule::name()
{
    return "Sonos.Module";
}

const std::string SonosModule::version()
{
    // hides the module in the version output on the console, because the firmware version is sufficient.
    return "";
}
SonosChannel *firstChannel = nullptr;

OpenKNX::Channel *SonosModule::createChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */)
{
    if (ParamSON_CHSonosChannelUsage <= 1)
        return nullptr;

    auto sonosApi = new SonosApi();
    auto channel = new SonosChannel(_channelIndex, *sonosApi);
    sonosApi->init(_webServer, channel->speakerIP(), _channelIndex);
    if (firstChannel == nullptr)
        firstChannel = channel;
    return channel;
}

void SonosModule::setup()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin((const char *)ParamNET_WifiSSID, (const char *)ParamNET_WifiPassword);
    // Do not call baseclass, baseclass will be called after first WiFi connection
}

void SonosModule::setup1()
{
    // Do not call baseclass, baseclass will be called after first WiFi connection
}

void SonosModule::processBeforeRestart()
{
}
void SonosModule::processBeforeTablesUnload()
{
}

void SonosModule::processInputKo(GroupObject &ko)
{
    // we have to check first, if external KO are used
    auto asap = ko.asap();
    switch (asap)
    {
            // case XXX:
            //     processInputKoDayNight(ko);
            //     break;

        default:
            break;
    }
    ChannelOwnerModule::processInputKo(ko);
}

bool SonosModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    if (cmd.rfind("son", 0) == 0)
    {
        auto channelString = cmd.substr(3);
        auto pos = channelString.find_first_of(' ');
        if (pos != std::string::npos && pos > 0)
        {
            auto channelNumberString = channelString.substr(0, pos);
            auto channel = atoi(channelNumberString.c_str());
            auto channelCmd = channelString.substr(pos + 1);
            if (channel < 1 || channel > getNumberOfChannels())
            {
                Serial.printf("Channel not %d found\r\n", channel);
                return true;
            }
            auto sonosChannel = (SonosChannel*) getChannel(channel - 1);
            if (sonosChannel == nullptr)
            {
                Serial.printf("Channel not %d activated\r\n", channel);
                return true;
            }
            return sonosChannel->processCommand(channelCmd, diagnoseKo);
        }
    }
    return false;
}

void SonosModule::showHelp()
{
    openknx.console.printHelpLine("son<CC> uid", "Show sonos UID of channel CC. i.e. son01 uid");
    openknx.console.printHelpLine("son<CC> vol", "Show volume of channel CC. i.e. son01 vol");
    openknx.console.printHelpLine("son<CC> vol <XXX>", "Set volume of channel CC. i.e. son01 vol 17");
    openknx.console.printHelpLine("son<CC> rvol <XXX>", "Set relative volume of channel CC. i.e. son01 rvol -1");
    openknx.console.printHelpLine("son<CC> mute", "Show mute state of channel CC. i.e. son01 mute");
    openknx.console.printHelpLine("son<CC> mute <X>", "Set mute state of channel CC. i.e. son01 mute 1");
    openknx.console.printHelpLine("son<CC> gvol", "Show group volume of channel CC. i.e. son01 gvol");
    openknx.console.printHelpLine("son<CC> rgvol <XXX>", "Set relative group volume of channel CC. i.e. son01 rgvol -1");
    openknx.console.printHelpLine("son<CC> gvol <XXX>", "Set group volume of channel CC. i.e. son01 gvol 17");
    openknx.console.printHelpLine("son<CC> gmute", "Show group mute state of channel CC. i.e. son01 gmute");
    openknx.console.printHelpLine("son<CC> gmute <X>", "Set group mute state of channel CC. i.e. son01 gmute 1");
    openknx.console.printHelpLine("son<CC> track", "Show the track info of channel CC. i.e. son01 track");
    openknx.console.printHelpLine("son<CC> state", "Show the play state of channel CC. i.e. son01 state");
    openknx.console.printHelpLine("son<CC> play", "Start playing of the current media of channel CC. i.e. son01 play");
    openknx.console.printHelpLine("son<CC> pause", "Pause playing of channel CC. i.e. son01 pause");
    openknx.console.printHelpLine("son<CC> next", "Channel CC go to next song. i.e. son01 next");
    openknx.console.printHelpLine("son<CC> prev", "Channel CC go to previous song. i.e. son01 prev");
}

void SonosModule::showInformations()
{
}

void SonosModule::savePower()
{
}

bool SonosModule::restorePower()
{
    return true;
}

AsyncWebServer *webServer;
void SonosModule::loop()
{

    bool connected = WiFi.status() == WL_CONNECTED;
    if (!connected)
    {
        return;
    }
    if (!_channelSetupCalled)
    {
        logInfoP("Wifi connected. IP: %s", WiFi.localIP().toString());
        _webServer = new AsyncWebServer(80);
  
        ChannelOwnerModule::setup();
        _channelSetupCalled = true;

        _webServer->begin();
    }
    // if (webServer != nullptr)
    //     webServer->handleClient();
    if (!_channelSetup1Called)
    {
        return;
    }
    ChannelOwnerModule::loop();
}

void SonosModule::loop1()
{
    bool connected = WiFi.status() == WL_CONNECTED;
    if (!connected)
        return;
    if (!_channelSetup1Called)
    {
        ChannelOwnerModule::setup1();
        _channelSetup1Called = true;
    }
    ChannelOwnerModule::loop1();
}

SonosModule openknxSonosModule;