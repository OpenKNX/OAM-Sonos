#include "OpenKNX.h"
#include "Logic.h"
#include "SonosModule.h"
#include "OTAUpdateModule.h"
#ifdef ARDUINO_ARCH_RP2040
//#include "UsbExchangeModule.h"
#include "FileTransferModule.h"
#endif

void setup()
{
    const uint8_t firmwareRevision = 1;
    openknx.init(firmwareRevision);
    openknx.addModule(1, openknxLogic);
    openknx.addModule(2, openknxSonosModule);
    openknx.addModule(3, openknxOTAUpdateModule);
#ifdef ARDUINO_ARCH_RP2040
    //openknx.addModule(4, openknxUsbExchangeModule);
    openknx.addModule(5, openknxFileTransferModule);
#endif
    openknx.setup();
}

void loop()
{
   openknx.loop();
}

void setup1()
{
    openknx.setup1();
}

void loop1()
{
    openknx.loop1();
}