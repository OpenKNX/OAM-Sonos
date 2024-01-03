#include "FileTransferModule.h"
#include "Logic.h"
#include "OpenKNX.h"
#include "SonosModule.h"
//#include "UsbExchangeModule.h"

void setup()
{
    const uint8_t firmwareRevision = 1;
    openknx.init(firmwareRevision);
    openknx.addModule(1, openknxLogic);
    openknx.addModule(2, openknxSonosModule);
 //   openknx.addModule(3, openknxUsbExchangeModule);
    openknx.addModule(4, openknxFileTransferModule);
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