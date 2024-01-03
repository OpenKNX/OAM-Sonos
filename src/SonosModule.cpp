#include "SonosModule.h"


const std::string SonosModule::name()
{
    return "Sound";
}

const std::string SonosModule::version()
{
    // hides the module in the version output on the console, because the firmware version is sufficient.
    return "";
}

void SonosModule::init()
{
}

void SonosModule::loop(bool configured)
{
}

void SonosModule::setup(bool configured)
{  
}

#ifdef OPENKNX_DUALCORE
void SonosModule::loop1(bool configured)
{
}

void SonosModule::setup1(bool configured)
{
}
#endif

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
}

bool SonosModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    // if (cmd.substr(0, 5) == "play " && cmd.length() > 5)
    // {
    //     uint16_t file = std::stoi(cmd.substr(5, 10));
    //     if (file > 0)
    //     {
    //         logInfoP("Manually play the file %i", file);
    //         logIndentUp();
    //         play(file, 0, 5);
    //         logIndentDown();

    //         return true;
    //     }
    // }

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

SonosModule openknxSonosModule;