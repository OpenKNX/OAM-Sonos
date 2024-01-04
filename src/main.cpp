#include "OpenKNX.h"
#include "Logic.h"
#include "SonosModule.h"
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
#ifdef ARDUINO_ARCH_RP2040
    //openknx.addModule(3, openknxUsbExchangeModule);
    openknx.addModule(4, openknxFileTransferModule);
#endif
    openknx.setup();
}

void loop()
{
    openknx.loop();
}

#ifdef OPENKNX_DUALCORE
void setup1()
{
    openknx.setup1();
}

void loop1()
{
    openknx.loop1();
}
#endif