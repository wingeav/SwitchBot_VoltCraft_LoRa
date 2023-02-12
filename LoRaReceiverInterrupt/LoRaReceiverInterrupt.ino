// WiFi LoRa 32(V2)
/* 
  Uses interrup method check the new incoming messages, and print via serial
  in 115200 baud rate.

  The default interrupt pin in SX1276/8(DIO0) connected to ESP32's GPIO26
 
  by Aaron.Lee from HelTec AutoMation, ChengDu, China
  成都惠利特自动化科技有限公司
  www.heltec.cn
  
  this project also realess in GitHub:
  https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series
*/
#include "heltec.h"
#include "WiFiPassword.h"
#include <WiFiClientSecure.h>
#include "ca_cert.h"
#include <Ticker.h>
#include <ArduinoJson.h>

#define BAND 868E6  //you can set band here directly,e.g. 868E6,915E6
//#define BAND    433E6  //you can set band here directly,e.g. 868E6,915E6


bool beginn = false;
int dataSize = 0;
int bytesRead = 0;
bool newData=false;
const char* server = "myurl.de";
bool doSendData = false;
uint8_t c_buffer = 0;
char output[2500];

struct DATASET {
  uint8_t address[6];
  uint8_t data[61];
  size_t length;
};

DATASET a_dataset[30];
uint8_t c_dataset=0;

void sendData() {//(const char getRequest[]) {
    doSendData=false;
    Serial.println(output);

  Serial.println("Check WIFI");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(WiFi.status());
    if (WiFi.status() == 4) {
        Serial.println("WiFi connecting Error -- retry");
        WiFi.begin(ssid, pass);
    }
  }



    WiFiClientSecure client;
    client.setCACert(root_ca);
    if (!client.connect("myurl.de", 443))
      Serial.println("Connection failed!");
    else {
      Serial.println("Connected to JSON server!");
    }
    
    client.println("POST /json_measure.php HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Content-Type: application/json");
    client.println("Content-Length: 2500");
    client.println("");
    client.println(output);
    client.println("");
    if (client.connected()) {
      Serial.println("CLIENT successfully connected");
    } else {
      Serial.println("ERROR : CLIENT not connected");
    }

    client.stop();
}


std::string convert_address(uint8_t *address) { // converts the address from bytes to string - address is sent in reverse
  char b_address[18]; 
  snprintf(b_address, sizeof(b_address), "%02x:%02x:%02x:%02x:%02x:%02x",
                                     address[5], address[4], address[3],
                                     address[2], address[1], address[0]);
  return std::string(b_address);
}

void processData() {// (uint8_t pData[60], String pAddress) {
  StaticJsonDocument<2500> doc;
  doc["password"] = "Ilagde21Designer5revivalHemd";
  JsonArray m_array = doc.createNestedArray("Measure");
  JsonArray d_array = doc.createNestedArray("Day");
  JsonArray s_array = doc.createNestedArray("Switchbot");

    // Add values in the document
    //
    // Add an array.
    //

  Serial.println("IN PROCESS DATA 0");
  c_buffer = c_dataset;
  for (int ds = 0; ds < c_dataset; ds++) {
      if (a_dataset[ds].data[1] == 0x0f && a_dataset[ds].data[2] == 0x04) { // Measure Voltcraft
        Serial.println("process Measure");

        uint16_t checksum = 0;
        for (int i=0;i < 6; i++) { // checksum includes address
          checksum += a_dataset[ds].address[i];
        }

        for (int i=0;i < a_dataset[ds].length-1; i++) { // -1 because last byte is the checksum
          checksum += a_dataset[ds].data[i];
        }
        checksum = (checksum & 0xFF); // if the checksum size is larger than 1 byte, it is truncated to 1 byte (bitwise AND)
        if (checksum != a_dataset[ds].data[a_dataset[ds].length-1]) {
          Serial.print("checksums different - calculated: ");
          Serial.print(checksum,HEX);
          Serial.print(" in data: ");
          Serial.println(a_dataset[ds].data[a_dataset[ds].length-1],HEX);
          return;
        } else {
          Serial.print("checksums OK - calculated: ");
          Serial.print(checksum,HEX);
          Serial.print(" in data: ");
          Serial.println(a_dataset[ds].data[a_dataset[ds].length-1],HEX);
        }

        // Notification handle = 0x0014 value: 0f 0f 04 00 01 00 88 50 dc 00 d6 32 01 00 00 00 00 67 2a
        //                                     |  |  |     |  |        |  |     |        |           |  
        //                                     |  |  |     |  |        |  |     |        |           + checksum byte starting after length-byte, ending w/ byte before --> (sum of bytes + 1)
        //                                     |  |  |     |  |        |  |     |        + total consumption, 4 bytes (hardware versions >= 3 there is a value)
        //                                     |  |  |     |  |        |  |     + frequency (Hz)
        //                                     |  |  |     |  |        |  + Ampere/1000 (A), 2 bytes
        //                                     |  |  |     |  |        + Voltage (V)
        //                                     |  |  |     |  |
        //                                     |  |  |     |  + Watt/1000, 3 bytes
        //                                     |  |  |     + Power, 0 = off, 1 = on
        //                                     |  |  + Capture measurement response 0x0400
        //                                     |  + Length of payload starting w/ next byte.
        //                                      + static start sequence for message, 0x0f
        uint8_t poweron = a_dataset[ds].data[4];
        float watt = float((a_dataset[ds].data[5] * 256 * 256) + (a_dataset[ds].data[6] * 256) + (a_dataset[ds].data[7])) / 1000; // "move" byte 5 by 16 bit = 65536, move byte 6 by 8 bit = 256, no move byte 7 .... This sumarized makes the individual bytes into a total number -- divided by 1000 gives the watts
        uint16_t volt = a_dataset[ds].data[8];
        float ampere = float((a_dataset[ds].data[9] * 256) + (a_dataset[ds].data[10])) / 1000; // byte 9 um 8 bit "verschieben" ) = 256, Byte 10 nicht verschieben.... Damit werden die einzelnen Bytes zu einer Gesamtzahl -- dividiert durch 1000 ergibt die Ampere (das ganze muss vor der division als float definiert werden, sonst wird die Summe als int gewertet)
        uint8_t frequency = a_dataset[ds].data[11]; // Frequenz scheint nur das erste Byte zu enthalten ohne verschieben
        float consumption = float((a_dataset[ds].data[14] * 256 * 256 *256) + (a_dataset[ds].data[15] * 256 * 256) + (a_dataset[ds].data[16] * 256)  + a_dataset[ds].data[17]) / 1000;

        const char urlVolt[] = "/strommessung.php?passwort=geheim";
        JsonObject o_measure = m_array.createNestedObject();
        o_measure["address"] = convert_address(a_dataset[ds].address);
        o_measure["poweron"] = poweron;
        o_measure["watt"] = watt;
        o_measure["voltage"] = volt;
        o_measure["ampere"] = ampere;
        o_measure["frequency"] = frequency;
        o_measure["consumption"] = consumption;

        doSendData=true;

        Serial.print("ADDRESS: ");
        Serial.println(convert_address(a_dataset[ds].address).c_str());

        Serial.print("POWERON: ");
        Serial.println(poweron);   

        Serial.print("Watt: ");
        printf("%.4lf\n",watt);

        Serial.print("volt: ");
        Serial.println(volt);

        Serial.print("ampere: ");
        Serial.println(ampere);

        Serial.print("frequency: ");
        Serial.println(frequency);

        Serial.print("consumption: ");
        printf("%.4lf\n",consumption);
      } else if (a_dataset[ds].data[1] == 0x33 && a_dataset[ds].data[2] == 0x0a) { // Day Voltcraft
          Serial.println("process Day");

          Serial.print("address: ");
          Serial.println(convert_address(a_dataset[ds].address).c_str());
                    
          // Notification handle = 0x002e value: 0f 33 0a 00 00 0e 00 0e 00 0e 00 0e 00 0c 00 09 00 08 00 0b
          //                                     |  |  |     |     |     |     |     |     |     |     + Current hour - 16, 2 bytes for Wh
          //                                     |  |  |     |     |     |     |     |     |     + Current hour - 17, 2 bytes for Wh
          //                                     |  |  |     |     |     |     |     |     + Current hour - 18, 2 bytes for Wh
          //                                     |  |  |     |     |     |     |     + Current hour - 19, 2 bytes for Wh
          //                                     |  |  |     |     |     |     + Current hour - 20, 2 bytes for Wh
          //                                     |  |  |     |     |     + Current hour - 21, 2 bytes for Wh
          //                                     |  |  |     |     + Current hout - 22, 3 bytes for Wh
          //                                     |  |  |     + Current hour - 23, 2 bytes for Wh
          //                                     |  |  + 0x0a00, Request data for day request
          //                                     |  + Length of payload starting w/ next byte incl. checksum
          //                                     + static start sequence for message, 0x0f

          // Notification handle = 0x002e value: 00 0e 00 0e 00 11 00 0f 00 10 00 0f 00 0d 00 0e 00 0e 00 0e
          // Notification handle = 0x002e value: 00 0e 00 0e 00 0e 00 0e 00 0d 00 00 42 ff ff
          //                                                             |     |     |  + static end sequence of message, 0xffff
          //                                                             |     |     + checksum byte starting after length-byte, ending w/ byte before
          //                                                             |     + Current hour, 2 bytes for Wh
          //                                                             + Current hour - 2 , 2 bytes for Wh    Serial.println("CALLBACK Daily");
      
          uint16_t checksum = 0;
          for (int i=0;i < 6; i++) { // checksum includes address
            checksum += a_dataset[ds].address[i];
          }

        for (int i=0;i < a_dataset[ds].length-1; i++) { // -1 because last byte is the checksum
            checksum += a_dataset[ds].data[i];
            Serial.print(a_dataset[ds].data[i],HEX);
            Serial.print(" ");
          }
          Serial.print("CHECKSUM: ");
          Serial.print(a_dataset[ds].data[a_dataset[ds].length-1],HEX);

          Serial.println("");
          checksum = (checksum & 0xFF); // if the checksum size is larger than 1 byte, it is truncated to 1 byte (bitwise AND)
          if (checksum != a_dataset[ds].data[a_dataset[ds].length-1]) {
            Serial.print("checksums different - calculated: ");
            Serial.print(checksum,HEX);
            Serial.print(" in data: ");
            Serial.println(a_dataset[ds].data[a_dataset[ds].length-1],HEX);
            return;
          } else {
            Serial.print("checksums OK - calculated: ");
            Serial.print(checksum,HEX);
            Serial.print(" in data: ");
            Serial.println(a_dataset[ds].data[a_dataset[ds].length-1],HEX);
          }

         
          JsonObject o_day = d_array.createNestedObject();
          o_day["address"] = convert_address(a_dataset[ds].address);

          int byte = 4;
          for (int d = -23; d <= 0; d++) 
          {
            float kwh = 0;
            o_day[String(d)] = float((a_dataset[ds].data[byte] * 256) + (a_dataset[ds].data[byte+1])) / 1000;
            byte += 2;
            Serial.println("Serialize");
            serializeJson(doc, Serial);
            Serial.println("");
          }
          
          const char urlVolt[] = "/strommessung.php?passwort=geheim";
          char temp [10];

          doSendData=true;

      } else if (a_dataset[ds].data[0] == 0x01 && a_dataset[ds].data[1] >= 0x00 && a_dataset[ds].data[1] <= 0x09) { // Switchbot Meter (decimal part of temperature value - 0 to 9)
          Serial.println("process Switchbot");

          //Byte 0: Status - 1 = OK
          //Byte 1: decimal Temperature - 0 to 9
          //Byte 2: Bits 1-7 - Temperature in Celsius
          //        Bit  8   - Positive/Negative: 0 = Negative, 1 = Positive
          //Byte 3: Humidity
          
          uint16_t checksum = 0;
          for (int i=0;i < 6; i++) { // checksum includes address
            checksum += a_dataset[ds].address[i];
          }

          for (int i=0;i < a_dataset[ds].length-1; i++) { // -1 because last byte is the checksum
            checksum += a_dataset[ds].data[i];
          }
          checksum = (checksum & 0xFF); // if the checksum size is larger than 1 byte, it is truncated to 1 byte (bitwise AND)
          if (checksum != a_dataset[ds].data[a_dataset[ds].length-1]) {
            Serial.print("checksums different - calculated: ");
            Serial.print(checksum,HEX);
            Serial.print(" in data: ");
            Serial.println(a_dataset[ds].data[a_dataset[ds].length-1],HEX);
            return;
          } else {
            Serial.print("checksums OK - calculated: ");
            Serial.print(checksum,HEX);
            Serial.print(" in data: ");
            Serial.println(a_dataset[ds].data[a_dataset[ds].length-1],HEX);
          }

          float temperature = (a_dataset[ds].data[2] & 0x7f) + (float(a_dataset[ds].data[1]) / 10); // bitwise AND of byte 2 with 01111111. with this the positive/negative bit is set to 0 (cleared); 
          if ((a_dataset[ds].data[2] >> 7 == 0)) { temperature = temperature * -1; }  // shift the bits right by 7 so that only the positive/negative bit remains and check if it is 0 (negative).

          uint8_t humidity = a_dataset[ds].data[3];

          JsonObject o_switchbot = s_array.createNestedObject();
          o_switchbot["address"] = convert_address(a_dataset[ds].address);
          o_switchbot["temperature"] = temperature;
          o_switchbot["humidity"] = humidity;

          char tempURL[] = "/tempmessung.php?passwort=geheim";
          doSendData=true;

          Serial.print("address: ");
          Serial.println(convert_address(a_dataset[ds].address).c_str());
          Serial.print("Temperature: ");
          Serial.println(temperature);
          Serial.print("Humidity: ");
          Serial.println(humidity);

      } else {
          // only für debug
          Serial.print("a_dataset[ds].data1 ");
          Serial.println(a_dataset[ds].data[1],HEX);
          Serial.print("a_dataset[ds].data2 ");
          Serial.println(a_dataset[ds].data[2],HEX);
          Serial.print("a_dataset[ds].data3 ");
          Serial.println(a_dataset[ds].data[3],HEX);
      }
  }
  serializeJson(doc, output);
}

void setup() {
 
 //WIFI Kit series V1 not support Vext control
  Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Disable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, BAND /*long BAND*/);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, pass);
  
  Serial.print("connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(WiFi.status());
    if (WiFi.status() == 4) {
        Serial.println("WiFi connecting Error -- retry");
        WiFi.begin(ssid, pass);
    }
  }
  Heltec.display->init();
  Heltec.display->clear();
  Heltec.display->display();
  //Heltec.display->flipScreenVertically();
  Heltec.display->setFont(ArialMT_Plain_16);
  Heltec.display->clear();
  Heltec.display->display();
  Heltec.display->drawString(0, 0, "WLAN connected");
  Heltec.display->display();

  

  // register the receive callback
  LoRa.onReceive(onReceive);

  // put the radio into receive mode
  LoRa.receive();
}

void loop() {
  // do nothing
  if (newData) {
      Heltec.display->clear();
      Heltec.display->display();
      Heltec.display->drawString(0, 0, "Process Data");
      Heltec.display->display();
      Serial.println("NEW DATA 1");

    processData();// (data, address);
      Serial.println("NEW DATA 2");
    newData = false;
    c_dataset = 0;
  }
  if (doSendData) {
    sendData();//(buffer);
    Heltec.display->clear();
    Heltec.display->display();
  }
  delay(10);
}

void onReceive(int packetSize)
{
  uint8_t byteArray[32] = {};

  // received a packet
  //Serial.print("Received packets: ");
  //Serial.println(packetSize);
  // read packet
  Serial.println("bytearray beginn");
  for (int i = 0; i < packetSize; i++)
  {
    byteArray[i] = LoRa.read();
    Serial.print(byteArray[i],HEX);
    Serial.print(" ");
  }
  Serial.println("");
  Serial.println("bytearray ende");
  


  String receivedString = (char*)byteArray;
  //Serial.println(receivedString.c_str());
  if (receivedString.indexOf("address=") == 0) {
      beginn = true;
      uint8_t c_address = 0;
      for (int x = 8; x < packetSize; x++ ) { // address is sent in reverse. 
        a_dataset[c_dataset].address[c_address] = byteArray[x];
        c_address++;
      }
      Serial.print("ADDRESS FOUND: ");
      
      Serial.println(convert_address(a_dataset[c_dataset].address).c_str());
  } else if (beginn == true && receivedString.indexOf("size=") == 0) { // "size" String found after "address" string
      dataSize = receivedString.substring(5).toInt();
      a_dataset[c_dataset].length = dataSize;
      //Serial.print("SIZE FOUND: ");
      //Serial.println(dataSize);
  } else if (receivedString.indexOf("&DS_END") == 0) { // end of dataset
      //Serial.println("END FOUND - BEGIN = FALSE");
      beginn = false;
      bytesRead = 0;
      Serial.println("DATASET END");
      c_dataset++;
  } else if (receivedString.indexOf("&G_END") == 0) { // global end -- all dataset have been sent
      Serial.println("GLOBAL END");
      newData = true; 
  } else if (beginn == true) {
      //Serial.println("FOUND DATA?");
      for (int i = 0; i < packetSize; i++) {
          if (bytesRead > dataSize) {
            Serial.print("TOO MUCH DATA ! bytes Read: ");
            Serial.print(bytesRead);
            Serial.print(" of ");
            Serial.println(dataSize);
            Serial.print ("extra byte: ");
            Serial.println(byteArray[i],HEX);
            continue;
            //return; // don't save the extra data
          } 
          a_dataset[c_dataset].data[bytesRead] = byteArray[i];
          //Serial.print(byteArray[i],HEX);
          //Serial.print(" ");
          bytesRead++;
      }
      //Serial.println("");
  }

  //Serial.print("CHARS: ");
  //Serial.println(receivedString.c_str());




/*  // +1 for the NULL terminator
  char str[sizeof(byteArray) + 1];
  // Copy contents
  memcpy(str, byteArray, sizeof(byteArray));
  // Append NULL terminator
  str[packetSize] = '\0';*/
  // print RSSI of packet
  Serial.print("RSSI: ");
  Serial.println(LoRa.packetRssi());
}
