#include "wled.h"
#include "cbuf.h"
/*
  OPC Protocol support
*/

#ifdef WLED_ENABLE_OPC

#define OPC_SERVER_PORT 7890
#define OPC_LISTEN_CHANNEL 1
#define OPC_MAX_FRAME_STORAGE 5
#define OPC_HEADER_SIZE 4

static std::vector<uint8_t> opcBuf;
static cbuf opcCircularBuf = cbuf(OPC_MAX_FRAME_STORAGE * (ledCount + OPC_HEADER_SIZE));
static bool opcParserWaitingForData = false;

void freeOPCClient(AsyncClient* client) {
  client->close(true);
  delete clientServed;
  clientServed = nullptr;
}

void handleOPCError(void* arg, AsyncClient* client, int8_t error) {
	freeOPCClient(client);
}

void handleOPCDisconnect(void* arg, AsyncClient* client) {
	freeOPCClient(client);
}

void handleOPCTimeOut(void* arg, AsyncClient* client, uint32_t time) {
	freeOPCClient(client);
}

/* void handleOPCData(void* arg, AsyncClient* client, void *data, size_t len) {
	//Will try with dynamic double buffering;
  //Can be improved by only buffering if packet is incomplete.
  Serial.printf("Packet! sz: %zu\t", len);
  Serial.printf("Timing: sc: %lu\t", millis());
  if (opcParserParsingData == 0) {  //parse header
    for (uint8_t i = opcBuf.size(); i < len && i < 4; i++) {
      opcBuf.push_back(*((uint8_t*)data + i));
    }
    if (opcBuf.size() >= 4) {
      if ((opcBuf.at(0) != OPC_LISTEN_CHANNEL && opcBuf.at(0) != 0) || opcBuf.at(1) != 0) {
        opcParserInvalidData = 1; //we still have to "receive" the full packet
        opcBuf.clear();
        opcBuf.push_back(0);
        opcBuf.push_back(0);
      } else {
        opcDataLen = opcBuf.at(2) << 8 | opcBuf.at(3);
        opcParserParsingData = 1;
        opcBuf.clear();
        for(uint16_t i = 0; i < len && i < opcDataLen; i++) opcBuf.push_back(*((uint8_t*)data + i + 4)); //skipping header here
      }
    }
  }
  if (opcParserInvalidData == 0 && opcParserParsingData == 1){
    if (opcBuf.size() >= opcDataLen) {
      Serial.printf("sl: %lu\t", millis());
      realtimeLock(realtimeTimeout, REALTIME_MODE_E131);
      if (realtimeOverride) {
        strip.show();
        opcBuf.clear();
        opcParserParsingData = 0;
        return;
      }
      uint16_t opcDataLenDiv = opcDataLen / 3;
      for (uint16_t i = 0; i < opcDataLenDiv && i < ledCount; i++) {
        setRealtimePixel(i, opcBuf.at((i*3)), opcBuf.at((i*3)+1), opcBuf.at((i*3)+2), 0);
      }
      strip.show();
      opcBuf.clear();
      opcParserParsingData = 0;
      Serial.printf("el: %lu\t", millis());
    } else {
      for (uint16_t i = opcBuf.size(); i < len && i < opcDataLen; i++) opcBuf.push_back(*((uint8_t*)data + i));
    }
  } else {
    uint16_t temp = opcBuf[0] << 8 | opcBuf[1];
    temp += len;
    if (temp >= opcDataLen) {
      opcBuf.clear();
      opcParserParsingData = 0;
    } else {
      opcBuf[0] = temp >> 8;
      opcBuf[1] = (uint8_t)(temp & 0x0F);
    }
  }
  // Serial.printf("ec: %lu\n", millis());
} */

void handleOPCData(void* arg, AsyncClient* client, void *data, size_t len) {
  if (len > opcCircularBuf.room()) return; //drop the packet if buffer full
  for (unsigned int i = 0; i < len && !opcCircularBuf.full(); i++) { 
    opcCircularBuf.write(*((uint8_t*)data + i));
  }
}

void handleOPCClient(void* arg, AsyncClient* client) {

  //add client
  if (clientServed != nullptr) freeOPCClient(client);
	clientServed = client;

  //clean and initialize buffer
  opcBuf.clear();
  opcParserParsingData = 0;
  opcParserInvalidData = 0;
  opcDataLen = 0;
	
	// register client events
	client->onData(&handleOPCData, NULL);
	client->onError(&handleOPCError, NULL);
	client->onDisconnect(&handleOPCDisconnect, NULL);
	client->onTimeout(&handleOPCTimeOut, NULL);
  
}

void discardInvalidData(uint16_t len) {
  for (uint16_t i = 0; i < len; i++) opcCircularBuf.read();
}

void parseOPCLedValues(uint8_t chan, uint8_t cmd) {
  uint16_t opcDataLenDiv = opcDataLen / 3; 
  if ((chan == 0 || chan == OPC_LISTEN_CHANNEL) && cmd == 0) {
    realtimeLock(realtimeTimeout, REALTIME_MODE_E131);
    for (uint16_t i = 0; i < opcDataLenDiv && i < ledCount; i++) {
      setRealtimePixel(i, opcCircularBuf.read(), opcCircularBuf.read(), opcCircularBuf.read(), 0);
    }
    if (opcDataLen > ledCount) discardInvalidData(opcDataLen - ledCount * 3);
  } else {
    discardInvalidData(opcDataLen);
  }
}

void handleOPCPacket() {
  static uint8_t chan;
  static uint8_t cmd;
  /* if (opcCircularBuf.available() >= ledCount && !realtimeOverride) {
    realtimeLock(realtimeTimeout, REALTIME_MODE_E131);
    uint16_t opcDataLenDiv = opcDataLen / 3;
    for (uint16_t i = 0; i < opcDataLenDiv && i < ledCount; i++) {
      setRealtimePixel(i, opcBuf.at((i*3)), opcBuf.at((i*3)+1), opcBuf.at((i*3)+2), 0);
    }
    strip.show();
  } else  if (realtimeOverride) {
    opcCircularBuf.flush();
  } */
  if (opcCircularBuf.available() >= 4 && !realtimeOverride) { //there's at least a header
    if(!opcParserWaitingForData) {
      chan = (uint8_t)opcCircularBuf.read();
      cmd = (uint8_t)opcCircularBuf.read();
      opcDataLen = (uint8_t)opcCircularBuf.read() << 8 | (uint8_t)opcCircularBuf.read();
      if (opcDataLen > opcCircularBuf.available()) {
        opcParserWaitingForData = true;
      } else {
        parseOPCLedValues(chan, cmd);
      }
    } else {
      if (opcDataLen <= opcCircularBuf.available()) {
        parseOPCLedValues(chan, cmd);
        opcParserWaitingForData = false;
      }
    }
  }
}

void initOPCServer() {
  if (opcServer != nullptr) {
    opcServer->end();
    delete opcServer;
  }
  opcServer = new AsyncServer(OPC_SERVER_PORT); 
  opcServer->onClient(&handleOPCClient, opcServer);
  opcServer->setNoDelay(true);
  opcServer->begin();
}

#else
void initOPCServer(){}
void handleOPCClient() {}
void freeOPCClient(){}
void handleOPCData() {}
void handleOPCTimeOut (){}
void handleOPCError (){}
void handleOPCDisconnect (){}

#endif
