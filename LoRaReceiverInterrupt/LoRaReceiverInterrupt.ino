//Selected Board: WiFi LoRa 32(V2)
#include "heltec.h"
#include "WiFiPassword.h"
#include <WiFiClientSecure.h>
#include "ca_cert.h"

#define BAND 868E6  //you can set band here directly,e.g. 868E6,915E6
//#define BAND    433E6  //you can set band here directly,e.g. 868E6,915E6

bool beginn = false;
int dataSize = 0;
int bytesRead = 0;
uint8_t data[60] = {};
String address;
bool newData=false;
const char* server = "wingeav.de";
bool doSendData = false;
char buffer[350];

void sendData(const char getRequest[]) {
    doSendData=false;
    WiFiClientSecure client;
    client.setCACert(root_ca);
    //Serial.println("\nStarting connection to server...");
    if (!client.connect("wingeav.de", 443))
      Serial.println("Connection failed!");
    else {
      Serial.println("Connected to server!");
    }
      // Make a HTTP request:
      client.println(getRequest);
      client.print("Host: ");
      client.println(server);
      client.println("Connection: close");
      client.println();
      
      Serial.println("Data sent");

      /*while (client.connected()) {
        String line = client.readStringUntil('\n');
        //Serial.println(line);
        if (line == "\r") {
          Serial.println("headers received");
          break;
        }
      }
      // if there are incoming bytes available
      // from the server, read them and print them:
      while (client.available()) {
        char c = client.read();
        Serial.write(c);
      }
      Serial.println("");
      Serial.println("");*/
      client.stop();
}



void processData (uint8_t pData[60], String pAddress) {
    Serial.println("IN PROCESS DATA");
    if (pData[1] == 0x0f && pData[2] == 0x04) { // Measure Voltcraft
      Serial.println("process Measure");

      // Notification handle = 0x0014 value: 0f 0f 04 00 01 00 88 50 dc 00 d6 32 01 00 00 00 00 67 2a
      //                                     |  |  |     |  |        |  |     |        |           |  
      //                                     |  |  |     |  |        |  |     |        |           + checksum byte starting with length-byte
      //                                     |  |  |     |  |        |  |     |        + total consumption, 4 bytes (hardware versions >= 3 there is a value)
      //                                     |  |  |     |  |        |  |     + frequency (Hz)
      //                                     |  |  |     |  |        |  + Ampere/1000 (A), 2 bytes
      //                                     |  |  |     |  |        + Voltage (V)
      //                                     |  |  |     |  |
      //                                     |  |  |     |  + Watt/1000, 3 bytes
      //                                     |  |  |     + Power, 0 = off, 1 = on
      //                                     |  |  + Capture measurement response 0x0400
      //                                     |  + Length of payload starting w/ next byte.
      //                                     + static start sequence for message, 0x0f
      uint8_t poweron = pData[4];
      float watt = float((pData[5] * 256 * 256) + (pData[6] * 256) + (pData[7])) / 1000; // "move" byte 5 by 16 bit = 65536, move byte 6 by 8 bit = 256, no move byte 7 .... This sumarized makes the individual bytes into a total number -- divided by 1000 gives the watts
      uint16_t volt = pData[8];
      float ampere = float((pData[9] * 256) + (pData[10])) / 1000; // byte 9 um 8 bit "verschieben" ) = 256, Byte 10 nich verschieben.... Damit werden die einzelnen Bytes zu einer Gesamtzahl -- dividiert durch 1000 ergibt die Ampere (das ganze muss vor der division als float definiert werden, sonst wird die Summe als int gewertet)
      uint8_t frequency = pData[11]; // Frequenz scheint nur das erste Byte zu enthalten ohne verschieben
      float consumption = float((pData[14] * 256 * 256 *256) + (pData[15] * 256 * 256) + (pData[16] * 256)  + pData[17]) / 1000;

      const char urlVolt[] = "/strommessung.php?passwort=Mai80buketlikenessgelobtReim";
      //generate buffer for HTTP GET Request
      sprintf (buffer, "GET %s&sensor=%s&mode=%s&poweron=%d&watt=%5.4f&voltage=%d&ampere=%2.2f&frequency=%d&consumption=%5.4f HTTP/1.1",urlVolt, pAddress.c_str(), "measure", poweron, watt, volt, ampere, frequency, consumption);
      doSendData=true;
      
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
  } else if (pData[1] == 0x33 && pData[2] == 0x0a) { // Day Voltcraft
      Serial.println("process Day");
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
  
      int byte = 4;
      float kwh[24] = {0};
      for (int d = 0; d < 24; d++)
      {
        kwh[d] = float((pData[byte] * 256) + (pData[byte+1])) / 1000;
        byte += 2;
      }
      
      const char urlVolt[] = "/strommessung.php?passwort=Mai80buketlikenessgelobtReim";
      char temp [10];
      //generate buffer for HTTP GET Request: /strommessung.php?passwort=Mai80buketlikenessgelobtReim&sensor=xx:xx:xx:xx:xx:xx&mode=daily&array=xx!xx!xx...!xx"; 
      sprintf (buffer, "GET %s&sensor=%s&mode=%s&array=",urlVolt,pAddress.c_str(), "daily");
      for (int d = 0; d < 24; d++)
      {
        if (d > 0) {strcat (buffer,"!");} // separate data by "!"
        sprintf (temp,"%5.4f",kwh[d]);
        strcat (buffer,temp);
      }
      strcat (buffer," HTTP/1.1");
      doSendData=true;


      Serial.print("address: ");
      Serial.println(pAddress.c_str());
      int count = 23;
      for (int d = 0; d < 24; d++)
      {
        Serial.print("-" );
        Serial.print(count );
        Serial.print("h: " );
        printf("%.4lf\n",kwh[d]);
        count--;
      }  
  } else if (pData[0] == 0x01 && pData[1] >= 0x00 && pData[1] <= 0x09) { // Switchbot Meter (decimal part of temperature value - 0 to 9)
      Serial.println("process Switchbot");

      //Byte 0: Status - 1 = OK
      //Byte 1: decimal Temperature - 0 to 9
      //Byte 2: Bits 1-7 - Temperature in Celsius
      //        Bit  8   - Positive/Negative: 0 = Negative, 1 = Positive
      //Byte 3: Humidity
      
      float temperature = (pData[2] & 0x7f) + (float(pData[1]) / 10); // bitwise AND of byte 2 with 01111111. with this the positive/negative bit is set to 0 (cleared); 
      if ((pData[2] >> 7 == 0)) { temperature = temperature * -1; }  // shift the bits right by 7 so that only the positive/negative bit remains and check if it is 0 (negative).

      uint8_t humidity = pData[3];


      char tempURL[] = "/tempmessung.php?passwort=Mai80buketlikenessgelobtReim";
      sprintf (buffer, "GET %s&sensor=%s&temperatur=%3.2f&feuchte=%d HTTP/1.1",tempURL,pAddress.c_str(), temperature,humidity);
      doSendData=true;

      Serial.print("address: ");
      Serial.println(pAddress.c_str());
      Serial.print("Temperature: ");
      Serial.println(temperature);
      Serial.print("Humidity: ");
      Serial.println(humidity);

  } else {
      // only für debug
      Serial.print("pData1 ");
      Serial.println(pData[1],HEX);
      Serial.print("pData2 ");
      Serial.println(pData[2],HEX);
      Serial.print("pData3 ");
      Serial.println(pData[3],HEX);
  }
}

void setup() {
 
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
  Heltec.display->flipScreenVertically();
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
  if (newData) { // received complete dataset via LoRa?
      Heltec.display->clear();
      Heltec.display->display();
      Heltec.display->drawString(0, 0, "Process Data");
      Heltec.display->display();

    processData (data, address);
    newData = false;
  }
  if (doSendData) { // dataset ready for sending to webserver?
    sendData(buffer);
    Heltec.display->clear();
    Heltec.display->display();
  }
  delay(1);
}

void onReceive(int packetSize) //LoRa Callback function 
{
  uint8_t byteArray[32] = {};
  // received a packet
  //Serial.print("Received packets: ");
  //Serial.println(packetSize);
  // read packet
  for (int i = 0; i < packetSize; i++)
  {
    byteArray[i] = LoRa.read();
  }

  String receivedString = (char*)byteArray;
  if (receivedString.indexOf("address=") == 0) { // received a string with "address="? this is the beginning of the data
      beginn = true;
      address = receivedString.substring(8);
      //Serial.print("ADDRESS FOUND: ");
      //Serial.println(address.c_str());
  } else if (beginn == true && receivedString.indexOf("size=") == 0) {
      dataSize = receivedString.substring(5).toInt();
      //Serial.print("SIZE FOUND: ");
      //Serial.println(dataSize);
  } else if (receivedString.indexOf("&END") == 0) { // received "&END" string? All data has been received
      //Serial.println("END FOUND - BEGIN = FALSE");
      beginn = false;
      newData = true;     
      bytesRead = 0;
  } else if (beginn == true) { // bytes of Switchbot/Voltcraft measures
      //Serial.println("FOUND DATA?");
      for (int i = 0; i < packetSize; i++) {
        if (byteArray[i] != 59) {
          if (bytesRead > dataSize) { // read more bytes than expected? something went wrong... but still go ahead
            Serial.print("TOO MUCH DATA ! bytes Read: ");
            Serial.print(bytesRead);
            Serial.print(" of ");
            Serial.println(dataSize);
          }
          data[bytesRead] = byteArray[i];
          bytesRead++;
        }
      }
  }
  //Serial.print("CHARS: ");
  //Serial.println(receivedString.c_str());

  // print RSSI of packet
  Serial.print("RSSI: ");
  Serial.println(LoRa.packetRssi());
}
