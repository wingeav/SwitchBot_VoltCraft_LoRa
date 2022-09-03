//Selected Board: DOIT ESP32 DEVKIT V1
#include "Arduino.h"
#include <NimBLEDevice.h>

#define WIFI_LoRa_32_V2 true
#include "heltec.h"
#define BAND 868E6  //you can set band here directly,e.g. 868E6,915E6
//#define BAND    433E6  //you can set band here directly,e.g. 868E6,915E6

//Switchbot
NimBLEUUID bmeSwitchServiceUUID("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
NimBLEUUID bmeSwitchNotifyCharacteristicsUUID("cba20003-224d-11e6-9fb8-0002a5d5c51b");
NimBLEUUID bmeSwitchWriteCharacteristicsUUID("cba20002-224d-11e6-9fb8-0002a5d5c51b");
//Voltcraft
NimBLEUUID bmeVoltServiceUUID("FFF0");
NimBLEUUID bmeVoltNotifyCharacteristicsUUID("FFF4");
NimBLEUUID bmeVoltWriteCharacteristicsUUID("FFF3");

// Test im Büro
//std::string addressVoltcraft = "a3:00:00:00:50:51";
//std::string addressList[2] = { "d1:E7:4D:02:89:E4", "D9:BB:5B:BA:C8:24" };

// Prod im Keller
std::string addressVoltcraft = "a3:00:00:00:4f:52";
std::string addressList[2] = {"d2:a2:68:c9:33:35","fc:c8:2d:59:6b:fb" };


long intervalTempMeasure = 293;  // 5 Minutes
long intervalPowerMeasure = 34;    // 30 Seconds
long intervalPowerDaily = 3600;    // 1 Hour
//long intervalPowerDaily = 60;  // 1 Hour
long secondsTempMeasure;
long secondsPowerMeasure;
long secondsPowerDaily;
int bleConnectRetries = 2;  // number of BLE Connect retries


class ClientCallbacks : public NimBLEClientCallbacks {
  //  None of these are required as they will be handled by the library with defaults.
  //                       Remove as you see fit for your needs
  void onConnect(NimBLEClient* pClient) {
    Serial.println("ONCONNECT - Connected");
    // After connection we should change the parameters if we don't need fast response times.
    // These settings are 150ms interval, 0 latency, 450ms timout.
    // Timeout should be a multiple of the interval, minimum is 100ms.
    // I find a multiple of 3-5 * the interval works best for quick response/reconnect.
    // Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
    //pClient->updateConnParams(120,120,0,60);
    pClient->updateConnParams(60, 60, 0, 60);
  };

  void onDisconnect(NimBLEClient* pClient) {
    Serial.print(pClient->getPeerAddress().toString().c_str());
    Serial.println(" Disconnected - Starting scan");
  };

  // Called when the peripheral requests a change to the connection parameters.
  // Return true to accept and apply them or false to reject and keep
  //  the currently used parameters. Default will return true.
  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
    if (params->itvl_min < 24) {  // 1.25ms units
      return false;
    } else if (params->itvl_max > 40) {  // 1.25ms units
      return false;
    } else if (params->latency > 2) {  // Number of intervals allowed to skip
      return false;
    } else if (params->supervision_timeout > 100) {  // 10ms units
      return false;
    }

    return true;
  };
};


void InitBLE() {
  NimBLEDevice::init("");
  NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setMTU(160);  // MTU auf 160 begrenzen, für bisherige Abfrage nicht unbedingt notwendig

#ifdef ESP_PLATFORM
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // +9db
#else
  NimBLEDevice::setPower(9);  // +9db
#endif
}



void NotifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                    uint8_t* pData, size_t length, bool isNotify) {

  if (pData[1] == 6 && pData[2] == 0x17 && pData[3] == 0x0) {
    Serial.println("AUTH DAILY NOTIFY");
  } else {
    Serial.println("Clear Display");
    Heltec.display->init();
    Heltec.display->clear();
    Heltec.display->display();
    Heltec.display->flipScreenVertically();
    Heltec.display->setFont(ArialMT_Plain_16);
    Heltec.display->clear();
    Heltec.display->display();
    Heltec.display->drawString(0, 0, "Send LoRa");
    Heltec.display->display();
    delay(100);

    Serial.print("Callback address: ");
    Serial.println(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str());
    //Serial.print("pData[1] = ");
    //Serial.println(pData[1],HEX);
    Serial.print("pData Size = ");
    Serial.println(length);

    LoRa.beginPacket();
    LoRa.setTxPower(20, RF_PACONFIG_PASELECT_PABOOST);
    LoRa.print("address=");
    LoRa.print(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str());
    LoRa.endPacket();

    LoRa.beginPacket();
    LoRa.print("size=");
    LoRa.print(length);
    LoRa.endPacket();

    for (int i = 0; i < length; i = i + 10) {  // sende die LoRa Daten im 10er Block - jedes Byte einzeln dauert zu lange, alles zusammen geht unterwegs verloren
      int l; 
      LoRa.beginPacket();
      LoRa.setTxPower(20, RF_PACONFIG_PASELECT_PABOOST);
      if (i + 10 < length) {
        l = i + 10;
      } else {
        l = length;
      }
      for (int a = i; a < l; a++) { // send 10 bytes
        Serial.println(pData[a], HEX);
        LoRa.write(pData[a]);  // write data as byte instead of char
      }
      LoRa.endPacket();
      delay(1);  // mache nach jedem Block eine kurze Pause, sonst kommt es zu einer Exception - 1ms reicht bereits
    }
    LoRa.beginPacket();
    LoRa.print("&END");
    LoRa.endPacket();
  
    Serial.println("Clear Display");
    Heltec.display->init();
    Heltec.display->clear();
    Heltec.display->display();
  }
}


bool ConnectDevice(NimBLEAddress address, uint8_t mode) {  // mode: 0 = Switchbot; 1 = Voltract Measure; 2 = Voltcraft Day
  NimBLEClient* pClient = nullptr;
  NimBLEUUID bmeServiceUUID;
  NimBLEUUID bmeNotifyCharacteristicsUUID;
  NimBLEUUID bmeWriteCharacteristicsUUID;

  switch (mode) {
    case 0:
      Serial.println("mode Switchbot");
      bmeServiceUUID = bmeSwitchServiceUUID;
      bmeNotifyCharacteristicsUUID = bmeSwitchNotifyCharacteristicsUUID;
      bmeWriteCharacteristicsUUID = bmeSwitchWriteCharacteristicsUUID;
      break;
    case 1:
      Serial.println("Voltcraft Measure");
      //no break
    case 2:
      Serial.println("Voltcraft Day");
      bmeServiceUUID = bmeVoltServiceUUID;
      bmeNotifyCharacteristicsUUID = bmeVoltNotifyCharacteristicsUUID;
      bmeWriteCharacteristicsUUID = bmeVoltWriteCharacteristicsUUID;
      break;
    default:
      Serial.println("Wrong mode");
      return false;
  }

  if (NimBLEDevice::getClientListSize()) {
    Serial.println(" - RE CONNECT ...");
    pClient = NimBLEDevice::getClientByPeerAddress(address);
    Serial.println(" - RE CONNECT  2 ...");
    if (pClient) {
      Serial.println(" - CLIENT VORHANDEN -- CONNECTING");
      if (!pClient->connect(address, false)) {
        Serial.println("Reconnect 2 failed");
        //delay(40000);
      } else {
        Serial.println("Reconnected client");
      }
    } else {
      Serial.println("no Client in Reconnect");
      //pClient = NimBLEDevice::getDisconnectedClient();
    }
  }

  if (!pClient) {
    static ClientCallbacks clientCB;
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&clientCB, false);
    // Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
    // These settings are safe for 3 clients to connect reliably, can go faster if you have less
    // connections. Timeout should be a multiple of the interval, minimum is 100ms.
    // Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
    pClient->setConnectionParams(12, 12, 0, 51);
    // Set how long we are willing to wait for the connection to complete (seconds), default is 30.
    pClient->setConnectTimeout(10);
    Serial.println(" - Connecting device...");
    if (pClient->connect(address)) {
      Serial.println(" - Connected to server");
    } else {
      NimBLEDevice::deleteClient(pClient);
      Serial.println(" - Failed to connect, deleted client");
      return false;
    }
  }

  if (!pClient->isConnected()) {
    Serial.println(" - not connected");
    return false;
  }


  Serial.println("Hole SERVICE");
  NimBLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService) {  // make sure it's not null
    Serial.println("Service found");
    NimBLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(bmeNotifyCharacteristicsUUID);
    Serial.println("have Characteristic ?");
    if (pRemoteCharacteristic) {  // make sure it's not null
      Serial.println("Characteristic not null");
      //  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
      //  Unsubscribe parameter defaults are: response=false.
      if (pRemoteCharacteristic->canNotify()) {
        Serial.println("CAN NOTIFY = OK");
        bool subscribeSucess = false;

        Serial.println("SUBSCRIBE");
        subscribeSucess = pRemoteCharacteristic->subscribe(true, NotifyCallback);
        Serial.println("SUBSCRIBE successful?");
        if (!subscribeSucess) {
          // Disconnect if subscribe failed
          Serial.print("Error on subscribe");
          pClient->disconnect();
          return false;
        } else {
          Serial.print("SUBSCRIBED SUCCESSFULLY ");
        }
      } else {
        Serial.print("no CanNotify ");
        return false;
      }
    } else {
      Serial.println("no Characteristic == NULL");
    }


    Serial.println("get Write Characteristic");
    pRemoteCharacteristic = pRemoteService->getCharacteristic(bmeWriteCharacteristicsUUID);
    if (pRemoteCharacteristic) {  // make sure it's not null
      Serial.println("get Write Characteristic successful");
      if (pRemoteCharacteristic->canWriteNoResponse()) {
        Serial.println("CanWrite is True");
        byte reqData[9];
        int lengthData;
        switch (mode) {
          case 0:
            Serial.println("set Swichbot Bytes");
            reqData[0] = 0x57;  // 0x57 -> fixed value
            reqData[1] = 0x0f;  // Bit 0 bis 3 = 0x0F -> "Expand Command"
            reqData[2] = 0x31;  // 0x31 get temperature/humidty
            lengthData = 3;
            break;
          case 1:
            Serial.println("set Voltcraft Measure Bytes");
            reqData[0] = 0x0f;  // fixed value
            reqData[1] = 0x05;  // length of payload starting on next byte incl. checksum
            reqData[2] = 0x04;  // get measurement
            reqData[3] = 0x00;  // fixed value
            reqData[4] = 0x00;  // fixed value
            reqData[5] = 0x00;  // fixed value
            reqData[6] = 0x05;  // checksum, stating after length byte and ending byte before  (simple addition of all bytes + 1)
            reqData[7] = 0xff;  // fixed value
            reqData[8] = 0xff;  // fixed value
            lengthData = 9;
            break;
          case 2:
            Serial.println("set Voltcraft Day Bytes");
            reqData[0] = 0x0f;  // fixed value
            reqData[1] = 0x05;  // length of payload starting on next byte incl. checksum
            reqData[2] = 0x0a;  // get day data --> 0x0a = 24h; 0x0b = 30 days; 0x0c = last 12 months
            reqData[3] = 0x00;  // fixed value
            reqData[4] = 0x00;  // fixed value
            reqData[5] = 0x00;  // fixed value
            reqData[6] = 0x0b;  // checksum, stating after length byte and ending byte before  (simple addition of all bytes + 1)
            reqData[7] = 0xff;  // fixed value
            reqData[8] = 0xff;  // fixed value
            lengthData = 9;
            break;
        }

        if (pRemoteCharacteristic->writeValue(reqData, lengthData, false)) {
          Serial.print("Wrote new value to: ");
          Serial.println(pRemoteCharacteristic->getUUID().toString().c_str());
        } else {
          // Disconnect if write failed
          Serial.println("Error on writeValue - Disconnect");
          pClient->disconnect();
          return false;
        }
      } else {
        Serial.println("no CanWrite");
      }
    }
  } else {
    Serial.println("Remoteservice not found.");
    return false;
  }
  return true;
}


void setup() {
  Serial.begin(115200);

  Serial.println("Init BLE");
  InitBLE();

  Serial.println("Init LoRa");
  Heltec.begin(true /*DisplayEnable Disable*/, true /*Heltec.LoRa Enable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, BAND /*long BAND*/);
  Heltec.display->clear();
  Heltec.display->display();
  Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_10);

  secondsTempMeasure = intervalTempMeasure;
  secondsPowerMeasure = intervalPowerMeasure;
  secondsPowerDaily = intervalPowerDaily;
}

void loop() {

  secondsTempMeasure += 1;
  secondsPowerMeasure += 1;
  secondsPowerDaily += 1;

  if (secondsTempMeasure >= intervalTempMeasure) {
    for (auto address : addressList) {
      bool result = false;
      int counter = 0;
      while (!result && counter <= bleConnectRetries) {
        result = ConnectDevice(*new NimBLEAddress(address, BLE_ADDR_RANDOM), 0);
        if (!result) {
          Serial.print("ERROR connect to address:");
          Serial.println(address.c_str());
          delay(50);
        }
        counter++;
      }
    }
    secondsTempMeasure = 0;
  }

  if (secondsPowerMeasure >= intervalPowerMeasure) {
    bool result = false;
    int counter = 0;
    while (!result && counter <= bleConnectRetries) {
      result = ConnectDevice(*new NimBLEAddress(addressVoltcraft, BLE_ADDR_PUBLIC), 1);
      if (!result) {
        Serial.println("ERROR connect Measure");
        delay(50);
      }
      counter++;
    }
    secondsPowerMeasure = 0;
  }

  if (secondsPowerDaily >= intervalPowerDaily) {
    Serial.print("BSeconds: ");
    Serial.println(secondsPowerDaily);

    bool result = false;
    int counter = 0;
    while (!result && counter <= bleConnectRetries) {
      result = ConnectDevice(*new NimBLEAddress(addressVoltcraft, BLE_ADDR_PUBLIC), 2);
      if (!result) {
        Serial.println("ERROR connect Day");
        delay(50);
      }
      counter++;
    }
    secondsPowerDaily = 0;
  }
  delay(1000);
}