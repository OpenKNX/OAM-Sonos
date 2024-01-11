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
SonosChannel* firstChannel = nullptr;

OpenKNX::Channel* SonosModule::createChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */)
{
    if (_channelIndex > 0)
        return nullptr;

    auto channel = new SonosChannel(*_sonosApi);
    if (firstChannel == nullptr)
        firstChannel = channel;
    return channel;
} 


void SonosModule::setup()
{  
     _sonosApi = new SonosApi();

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
    uint16_t asap = ko.asap();
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
    if (firstChannel == nullptr)
        return false;
    if (cmd == "test")
    {
        firstChannel->test();
    
       return true;
    }

    return false;
}

void SonosModule::showHelp()
{
//    openknx.console.printHelpLine("play XXX", "Manually play the file");
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