#include "OpenKNX.h"
#include "Logic.h"
#include "SonosModule.h"
#include "OTAUpdateModule.h"
#ifdef ARDUINO_ARCH_ESP32
#include "WLANModule.h"
#else
#include "NetworkModule.h"
#endif
#ifdef ARDUINO_ARCH_RP2040
#include "UsbExchangeModule.h"
#include "FileTransferModule.h"
#endif
#ifdef USE_AUTO_CONNECT
#include <AutoConnect.h>
#include <WebServer.h>
WebServer webServer(80);
AutoConnect Portal(webServer);    
AutoConnectConfig config;
#endif

void setup()
{
    const uint8_t firmwareRevision = 1;
    openknx.init(firmwareRevision);
#ifdef ARDUINO_ARCH_ESP32    
    openknx.addModule(1, openknxWLANModule);
#else
    openknx.addModule(1, openknxNetwork);
#endif    
    openknx.addModule(2, openknxLogic);
    openknx.addModule(3, openknxSonosModule);
    openknx.addModule(4, openknxOTAUpdateModule);
#ifdef ARDUINO_ARCH_RP2040
    openknx.addModule(5, openknxUsbExchangeModule);
    openknx.addModule(6, openknxFileTransferModule);
#endif
    openknx.setup();
#ifdef USE_AUTO_CONNECT
    config.apid ="OpenKNX";
    config.password = "12345678";
    Portal.config(config);
    Portal.begin();
#endif

}

void loop()
{
#ifdef USE_AUTO_CONNECT
    Portal.handleClient();
#endif
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