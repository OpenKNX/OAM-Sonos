#include "SonosModule.h"
#include "SonosChannel.h"

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

    auto channel = new SonosChannel(*this, _channelIndex, _sonosApi);
    if (firstChannel == nullptr)
        firstChannel = channel;
    return channel;
}

void SonosModule::setup()
{
    
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
    if (cmd == "son debug")
    {
        _sonosApi.setDebugSerial(&Serial);
        return true;
    }
    if (cmd == "son nodebug")
    {
        _sonosApi.setDebugSerial(nullptr);
        return true;
    }
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
    openknx.console.printHelpLine("son<CC> treb", "Get trebl of channel CC. i.e. son01 treb");
    openknx.console.printHelpLine("son<CC> treb <X>", "Set treble (-10/10) of channel CC. i.e. son01 treb 0");
    openknx.console.printHelpLine("son<CC> bass", "Get bass of channel CC. i.e. son01 bass");
    openknx.console.printHelpLine("son<CC> bass <X>", "Set bass (-10/10) of channel CC. i.e. son01 bass 0");
    openknx.console.printHelpLine("son<CC> rvol <XXX>", "Set relative volume of channel CC. i.e. son01 rvol -1");
    openknx.console.printHelpLine("son<CC> ldn", "Show loudness state of channel CC. i.e. son01 ldn");
    openknx.console.printHelpLine("son<CC> ldn <X>", "Set loudness state of channel CC. i.e. son01 ldn 1");
    openknx.console.printHelpLine("son<CC> mute", "Show mute state of channel CC. i.e. son01 mute");
    openknx.console.printHelpLine("son<CC> mute <X>", "Set mute state of channel CC. i.e. son01 mute 1");
    openknx.console.printHelpLine("son<CC> gvol", "Show group volume of channel CC. i.e. son01 gvol");
    openknx.console.printHelpLine("son<CC> rgvol <XXX>", "Set relative group volume of channel CC. i.e. son01 rgvol -1");
    openknx.console.printHelpLine("son<CC> gvol <XXX>", "Set group volume of channel CC. i.e. son01 gvol 17");
    openknx.console.printHelpLine("son<CC> gmute", "Show group mute state of channel CC. i.e. son01 gmute");
    openknx.console.printHelpLine("son<CC> gmute <X>", "Set group mute state of channel CC. i.e. son01 gmute 1");
    openknx.console.printHelpLine("son<CC> track", "Show the track info of channel CC. i.e. son01 track");
    openknx.console.printHelpLine("son<CC> state", "Show the play state of channel CC. i.e. son01 state");
    openknx.console.printHelpLine("son<CC> src <S>", "Start playing of the source number S of channel CC. i.e. son01 src 1");
    openknx.console.printHelpLine("son<CC> play", "Start playing of the current media of channel CC. i.e. son01 play");
    openknx.console.printHelpLine("son<CC> pause", "Pause playing of channel CC. i.e. son01 pause");
    openknx.console.printHelpLine("son<CC> stop", "Stop playing of channel CC. i.e. son01 stop");
    openknx.console.printHelpLine("son<CC> next", "Channel CC go to next song. i.e. son01 next");
    openknx.console.printHelpLine("son<CC> prev", "Channel CC go to previous song. i.e. son01 prev");
    openknx.console.printHelpLine("son<CC> findc", "Find the coordinator of the Channel CC. i.e. son01 findc");
    openknx.console.printHelpLine("son<CC> findnpc", "Find next playing coordinator of the Channel CC. i.e. son01 findnpc");
    openknx.console.printHelpLine("son<CC> joinnext", "Join the Channel CC to the next playing group. i.e. son01 joinnext");
    openknx.console.printHelpLine("son<CC> dele <TT>", "The Channel CC delegates the coordination to channel TT. i.e. son01 dele 02");
    openknx.console.printHelpLine("son<CC> join <TT>", "The Channel CC join group of channel TT. i.e. son01 join 02");
    openknx.console.printHelpLine("son<CC> noti <N>", "The Channel CC plays the notification N. i.e. son01 noti 1");
    openknx.console.printHelpLine("son<CC> led", "Show led state of channel CC. i.e. son01 led");
    openknx.console.printHelpLine("son<CC> led <X>", "Set led state of channel CC. i.e. son01 led 1");
    openknx.console.printHelpLine("son<CC> pl <XXXXX>", "Start playing playlist named XX on channel CC. i.e. son01 pl best");
    openknx.console.printHelpLine("son debug", "Debug sonos api");
    openknx.console.printHelpLine("son nodebug", "Disable sonos api debug messages");
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

void SonosModule::loop()
{
    _sonosApi.loop();
    bool connected = WiFi.status() == WL_CONNECTED;
    if (!connected)
    {
        return;
    }
    if (!_channelSetupCalled)
    {
        logInfoP("Wifi connected. IP: %s", WiFi.localIP().toString());
  
        ChannelOwnerModule::setup();
        _channelSetupCalled = true;
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