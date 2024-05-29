#include "lora_gateway.h"
#include "lora_peer.h"
#include "configuration.h"

struct_message incomingReadings;
struct_pairing pairingDataGateway;
file_meta_message file_meta_gateway;

void OnDataRecvGateway(const uint8_t *incomingData, int len) { 
  JsonDocument root;
  String payload;
  uint8_t type = incomingData[0];       // first message byte is the type of message 
  switch (type) {
    case DATA :                         // the message is data type
      memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
      root["mac"] = incomingReadings.mac;      // create a JSON document with received data and send it by event to the web page
      root["temperature"] = incomingReadings.temp;
      root["humidity"] = incomingReadings.hum;
      root["readingId"] = String(incomingReadings.readingId);
      serializeJson(root, payload);
      oled_print(payload.c_str());
      serializeJson(root, Serial);
      Serial.println();
      break;
    
    case PAIRING:                            // the message is a pairing request 
      memcpy(&pairingDataGateway, incomingData, sizeof(pairingDataGateway));

      Serial.print("\nPairing request from: ");
      printMacAddress(pairingDataGateway.mac);
      Serial.println();

      /* OLED for Dev */
      oled_print(pairingDataGateway.msgType);
      oled_print("Pairing request from: ");
      oled_print(pairingDataGateway.mac[0]);

      if (pairingDataGateway.msgType == PAIRING) { 
        if(pairingDataGateway.pairingKey == PAIRING_KEY){
          Serial.println("Correct PAIRING_KEY");
          oled_print("send response");
          Serial.println("Sending packet...");
          LoRa.beginPacket();
          LoRa.write((uint8_t *) &pairingDataGateway, sizeof(pairingDataGateway));
          LoRa.endPacket();
          addPeerGateway(pairingDataGateway.mac, pairingDataGateway.deviceName);
          LoRa.receive();
        }
        else{
          Serial.println("Wrong PAIRING_KEY");
        }
      }  

      break; 

    case FILE_META:
      memcpy(&file_meta_gateway, incomingData, sizeof(file_meta_gateway));
      // file_meta_gateway.mac

  }
}

void lora_gateway_init() {

  LoRa.onReceive(onReceive);
  LoRa.onTxDone(onTxDone);
  LoRa_rxMode();

  loadPeersFromSD();

  xTaskCreate(taskReceive, "Data Handler", 10000, (void*)OnDataRecvGateway, 1, NULL);

}