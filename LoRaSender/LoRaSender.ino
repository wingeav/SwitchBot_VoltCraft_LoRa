//DOIT ESP32 DEVKIT V1
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
std::string addressList[4] = {"d2:a2:68:c9:33:35","fc:c8:2d:59:6b:fb","e4:e7:35:11:0d:e9","cd:c6:1a:a2:32:fd" };


struct SwitchB {
  size_t length;
  //std::string address;
  uint8_t address[6];
  uint8_t data[4];
};

struct VoltCM {
  size_t length;
  //std::string address;
  uint8_t address[6];
  uint8_t data[20];
};

struct VoltCD {
  size_t length;
  //std::string address;
  uint8_t address[6];
  uint8_t data[60];
};

//SwitchB switchB[3];
SwitchB switchB[16];
VoltCM voltCM[10];
VoltCD voltCD;

uint8_t c_switchB=0;
uint8_t c_voltCM=0;
uint8_t c_voltCD=0;



long intervalTempMeasure = 83;  // 2 Minutes
long intervalPowerMeasure = 34;    // 30 Seconds
long intervalPowerDaily = 3600;    // 1 Hour
long secondsTempMeasure;
long secondsPowerMeasure;
long secondsPowerDaily;
int bleConnectRetries = 2;  // number of BLE Connect retries

hw_timer_t *timer = NULL; // define Timer Variable
//uint64_t alarmInterval = (1000000 * 60 * 6); // set Timer to 6 Minutes
uint64_t alarmInterval = (1000000 * 60 * 3); // set Timer to 6 Minutes

bool doSendLoRa = false;


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

  if (doSendLoRa) {return;} // don't process BLE Data during sending LoRa Data
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
    Heltec.display->drawString(0, 0, "Get BLE Data");
    Heltec.display->display();
    delay(100);

    Serial.print("Callback address: ");
    Serial.println(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString().c_str());
    //Serial.print("pData[1] = ");
    //Serial.println(pData[1],HEX);
    Serial.print("pData Size = ");
    Serial.println(length);

    if (pData[1] == 0x0f && pData[2] == 0x04) { // Measure Voltcraft
      Serial.print("VoltCM: ");
      Serial.println(c_voltCM);

      for (int i = 0; i < length;i++) {
        voltCM[c_voltCM].data[i] = pData[i];
      }
      voltCM[c_voltCM].length = length;
      memcpy(voltCM[c_voltCM].address, pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().getNative(), 6);
      c_voltCM++;
    }

    if (pData[1] == 0x33 && pData[2] == 0x0a) { // Day Voltcraft
      Serial.print("VoltCD: ");
      Serial.println(c_voltCD);

      for (int i = 0; i < length;i++) {
        voltCD.data[i] = pData[i];
      }
      voltCD.length = length;
      memcpy(voltCD.address, pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().getNative(), 6);
      c_voltCD++;
    }

    if (pData[0] == 0x01 && pData[1] >= 0x00 && pData[1] <= 0x09) { // Switchbot Meter (decimal part of temperature value - 0 to 9)
      Serial.print("SwitchB: ");
      Serial.println(c_switchB);

      for (int i = 0; i < length;i++) {
        switchB[c_switchB].data[i] = pData[i];
      }
      switchB[c_switchB].length = length;
      memcpy(switchB[c_switchB].address, pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().getNative(), 6);
      c_switchB++;
    }
  }
  pRemoteCharacteristic->getRemoteService()->getClient()->disconnect(); // required for more than 3 BLE devices. works only with default parameter, BLE_ERR_SUCCESS doesn't seem to work.
  Heltec.display->clear();
  Heltec.display->display();

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


void IRAM_ATTR onTimer() {
  if (c_switchB > 0 && c_voltCM > 0) {
    doSendLoRa = true; 

  } else {
    Serial.println("keine Daten in den letzten 5 Minuten");
    Serial.println("Starte neu...");
    ESP.restart();
  }
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


  // IDEE: SENDE LoRa Rquest an "Reciver" und frage nach Datum + Uhrzeit

  secondsTempMeasure = intervalTempMeasure;
  secondsPowerMeasure = intervalPowerMeasure;
  secondsPowerDaily = intervalPowerDaily;

  timer = timerBegin(0, 80, true); // initialize Timer on Timer-Number 0. Set divider to 80 for the 80MHz CPU (80 000 000 Hz / 80 = 1 000 000)
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, alarmInterval, true);
  timerAlarmEnable(timer);                                              
}

void loop() {

  secondsTempMeasure += 1;
  secondsPowerMeasure += 1;
  secondsPowerDaily += 1;

  if (secondsTempMeasure >= intervalTempMeasure && doSendLoRa == false) { //doSendLoRa == false --> don't get new BLE Data during LoRa Sending
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

  if (secondsPowerMeasure >= intervalPowerMeasure && doSendLoRa == false) { //doSendLoRa == false --> don't get new BLE Data during LoRa Sending
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

  if (secondsPowerDaily >= intervalPowerDaily && doSendLoRa == false) { //doSendLoRa == false --> don't get new BLE Data during LoRa Sending
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

if (doSendLoRa) { // send LoRa Data
    Serial.println("send Data");
    Heltec.display->init();
    Heltec.display->clear();
    Heltec.display->display();
    Heltec.display->flipScreenVertically();
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->clear();
    Heltec.display->display();
    Heltec.display->drawString(0,0, "Send LoRa");
    Heltec.display->display();

    for (int b = 0; b < c_switchB; b++) {
      uint16_t checksum=0;
      Serial.print("SwitchB Address: ");
      for (int c_address = 0; c_address < 6; c_address++) {
        Serial.print(switchB[b].address[c_address],HEX);
        Serial.print("#");
      }
      Serial.println("");
      
      //LoRa.setSpreadingFactor(6);
      LoRa.setTxPower(20, RF_PACONFIG_PASELECT_PABOOST);
      LoRa.beginPacket();
      LoRa.print("address=");
      for (int c_address = 0; c_address < 6; c_address++) {
        LoRa.write(switchB[b].address[c_address]);
        checksum += switchB[b].address[c_address];
      }
      LoRa.endPacket();

      LoRa.beginPacket();
      LoRa.print("size=");
      LoRa.print(switchB[b].length+1);
      LoRa.endPacket();

      LoRa.beginPacket();
      for (int i = 0; i < switchB[b].length; i++) {  
        //Serial.print(switchB[b].data[i],HEX);  // write data as byte
        LoRa.write(switchB[b].data[i]);  // write data as byte
        checksum += switchB[b].data[i]; // calculate checksum ()
      }
      checksum = (checksum & 0xFF); // if the checksum size is larger than 1 byte, it is truncated to 1 byte (bitwise AND)
      LoRa.write(checksum);  // write cecksum as byte
      LoRa.endPacket();
      LoRa.beginPacket();
      LoRa.print("&DS_END"); // Send Dataset End marker
      LoRa.endPacket();
      Serial.print("checksum: ");
      Serial.println(checksum);
      delay(1500);  // mache kurze Pause, sonst kommt es zu einer Exception - 1ms reicht bereits
    }
    c_switchB = 0;


    for (int b = 0; b < c_voltCM; b++) {
      uint16_t checksum=0;
      LoRa.beginPacket();
      LoRa.print("address=");
      //LoRa.print(voltCM[b].address.c_str());
      for (int c_address = 0; c_address < 6; c_address++) {
        LoRa.write(voltCM[b].address[c_address]);
        checksum += voltCM[b].address[c_address];
      }
      LoRa.endPacket();

      LoRa.beginPacket();
      LoRa.print("size=");
      LoRa.print(voltCM[b].length+1);
      LoRa.endPacket();

      LoRa.beginPacket();
      for (int i = 0; i < voltCM[b].length; i++) {  
        LoRa.write(voltCM[b].data[i]);  // write data as byte
        checksum += voltCM[b].data[i];
      }
      checksum = (checksum & 0xFF); // if the checksum size is larger than 1 byte, it is truncated to 1 byte (bitwise AND)
      LoRa.write(checksum);  // write cecksum as byte
      LoRa.endPacket();
      LoRa.beginPacket();
      LoRa.print("&DS_END"); // Send Dataset End marker
      LoRa.endPacket();
      delay(1500);  // mache kurze Pause, sonst kommt es zu einer Exception - 1ms reicht bereits
    }
    c_voltCM = 0;


    if (c_voltCD > 0) {
      uint16_t checksum=0;
      LoRa.beginPacket();
      LoRa.print("address=");
      //LoRa.print(voltCD.address.c_str());
      for (int c_address = 0; c_address < 6; c_address++) {
        LoRa.write(voltCD.address[c_address]);
        checksum += voltCD.address[c_address];
      }
      LoRa.endPacket();

      LoRa.beginPacket();
      LoRa.print("size=");
      LoRa.print(voltCD.length+1);
      LoRa.endPacket();


      for (int i = 0; i < voltCD.length; i = i + 10) {  // send LoRa data in a 10 Byte Block - ervery single Byte is to slow, all byte get lost
        int l;
        LoRa.beginPacket();
        if (i + 10 < voltCD.length) {
          l = i + 10;
        } else {
          l = voltCD.length;
        }
        for (int a = i; a < l; a++) {
          //Serial.println(a);
          Serial.println(voltCD.data[a], HEX);
          LoRa.write(voltCD.data[a]);  // write data as byte
          checksum += voltCD.data[a];
        }
        LoRa.endPacket();
        delay(10);  // if we don't wait a little time, we will get an Exception 
      }
      checksum = (checksum & 0xFF); // if the checksum size is larger than 1 byte, it is truncated to 1 byte (bitwise AND)
      Serial.print ("CHECKSUM: ");
      Serial.println(checksum, HEX);
      LoRa.beginPacket();
      LoRa.write(checksum);  // write cecksum as byte
      LoRa.endPacket();
      LoRa.beginPacket();
      LoRa.print("&DS_END"); // Send Dataset End marker
      LoRa.endPacket();
      delay(1500);  
    }
    c_voltCD = 0;
    LoRa.beginPacket();
    LoRa.print("&G_END"); // Send Global End marker
    LoRa.endPacket();

    Serial.println("Clear Display");
    Heltec.display->init();
    Heltec.display->clear();
    Heltec.display->display();  
    doSendLoRa = false; 
    timerWrite (timer, 0); // reset Timer
  }  
  delay(1000);
  
}