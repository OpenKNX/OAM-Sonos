#include "OpenKNX.h"
#include "Logic.h"
#include "SonosModule.h"
#include "NetworkModule.h"
#include "FileTransferModule.h"
#include "FunctionBlocksModule.h"
#ifdef USE_AUTO_CONNECT
#include <AutoConnect.h>
#include <WebServer.h>
WebServer webServer(80);
AutoConnect Portal(webServer);    
AutoConnectConfig config;
#endif

void setup()
{

    const uint8_t firmwareRevision = 0;
    openknx.init(firmwareRevision);
    openknx.addModule(0, openknxNetwork);
    openknx.addModule(1, openknxLogic);
    openknx.addModule(2, openknxFunctionBlocksModule);
    openknx.addModule(3, openknxSonosModule);
    openknx.addModule(6, openknxFileTransferModule);

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