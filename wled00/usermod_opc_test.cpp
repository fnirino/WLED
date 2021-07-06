#include "wled.h"
/*
 * This v1 usermod file allows you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in const.h)
 * If you just need 8 bytes, use 2551-2559 (you do not need to increase EEPSIZE)
 * 
 * Consider the v2 usermod API if you need a more advanced feature set!
 */

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

#define SERVER_PORT 7890
#define OPC_LISTEN_CHANNEL 1

typedef struct {
  uint8_t parsingData:1;
  uint8_t invalidCmd:1;
  uint8_t invalidData:1;
  uint8_t headerIdx:4;
  uint8_t channel;
  uint8_t cmd;
  uint16_t dataIdx;
  uint16_t dataLen;
} parserStruct;

static AsyncClient* clientServed = nullptr;
static AsyncServer* opcServer = nullptr;
static parserStruct pS;
static std::vector<uint8_t> buf;

void freeClient(AsyncClient* client) {
  client->close(true);
  delete clientServed;
  clientServed = nullptr;
}

void handleError(void* arg, AsyncClient* client, int8_t error) {
	Serial.printf("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
	freeClient(client);
}

void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
	//Will try with dynamic double buffering;
  //Serial.printf("\nOPC Received, len: %u\n", len);
  if (pS.parsingData == 0) {
    //Serial.printf("\nBufsize before for: %d\n", buf.size());
    for (uint8_t i = buf.size(); i < len && i < 4; i++) {
      buf.push_back(*((uint8_t*)data + i));
    }
    //Serial.printf("\nBufsize after for: %d\n", buf.size());
    if (buf.size() >= 4) {
      
      if ((buf.at(0) != OPC_LISTEN_CHANNEL && buf.at(0) != 0) || buf.at(1) != 0) {
        pS.invalidData = 1; //we still have to "receive" the full packet
        buf.clear();
        buf.push_back(0);
        buf.push_back(0);
      } else {
        pS.dataLen = buf.at(2) << 8 | buf.at(3);
        pS.parsingData = 1;
        //Serial.printf("\nPacket Header: Channel: %u, Cmd: %u, Ps_H: %u, Ps_L: %u, dataLEn: %u, actual azync len: %u\n", buf[0], buf[1], buf[2], buf[3], pS.dataLen, len);
        buf.clear();
        for(uint16_t i = 0; i < len && i < pS.dataLen; i++) buf.push_back(*((uint8_t*)data + i + 4)); //skipping header here
      }
    }
  }
  if (pS.invalidData == 0 && pS.parsingData == 1){
    if (buf.size() >= pS.dataLen) {
      realtimeLock(realtimeTimeout, REALTIME_MODE_E131);
      if (realtimeOverride) {
        strip.show();
        buf.clear();
        pS.parsingData = 0;
        return;
      }
      //Serial.println("setting segments");
      //Serial.printf("\nData buffered: %u, LED amount: %u, Data len: %u\n", buf.size(), ledCount, pS.dataLen);
      for (uint16_t i = 0; i < pS.dataLen / 3 && i < ledCount; i++) {
        setRealtimePixel(i, buf.at((i*3)), buf.at((i*3)+1), buf.at((i*3)+2), 0);
        //Serial.printf("\nPixel: %u, r: %u, g: %u, b:%u", i, buf.at(i*3), buf.at((i*3)+1), buf.at((i*3)+2));
      }
      strip.show();
      buf.clear();
      pS.parsingData = 0;
    } else {
      for (uint16_t i = buf.size(); i < len && i < pS.dataLen; i++) buf.push_back(*((uint8_t*)data + i));
    }
  } else {
    uint16_t temp = buf[0] << 8 | buf[1];
    temp += len;
    if (temp >= pS.dataLen) {
      buf.clear();
      pS.parsingData = 0;
    } else {
      buf[0] = temp >> 8;
      buf[1] = (uint8_t)(temp & 0x0F);
    }
  }
}

void handleDisconnect(void* arg, AsyncClient* client) {
	//Serial.printf("\n client %s disconnected \n", client->remoteIP().toString().c_str());
	freeClient(client);
}

void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
	//Serial.printf("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
	freeClient(client);
}


/* server events */
void handleNewClient(void* arg, AsyncClient* client) {
	//Serial.printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());

	// add to list
  if (clientServed != nullptr) freeClient(client);
	clientServed = client;

  //clean state
  buf.clear();
  pS.parsingData = 0;
  pS.invalidData = 0;
  pS.dataLen = 0;
	
	// register events
	client->onData(&handleData, NULL);
	client->onError(&handleError, NULL);
	client->onDisconnect(&handleDisconnect, NULL);
	client->onTimeout(&handleTimeOut, NULL);
}

//gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup()
{
  
}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected()
{
  if (opcServer != nullptr) delete opcServer;
  opcServer = new AsyncServer(SERVER_PORT); 
  opcServer->onClient(&handleNewClient, opcServer);
	opcServer->begin();
}

//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop()
{
  
}

