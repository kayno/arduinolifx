/*
 LIFX bulb emulator by Kayne Richens (kayno@kayno.net)
 
 Emulates a LIFX bulb. Connect an RGB LED (or LED strip via drivers) 
 to redPin, greenPin and bluePin as you normally would on an 
 ethernet-ready Arduino and control it from the LIFX app!
 
 Notes:
 - Currently you cannot control an Arduino bulb and real LIFX bulbs at 
 the same time with the Android LIFX app. Untested with the iOS LIFX 
 app.
 - Only one client (e.g. app) can connect to the bulb at once
 
 Set the following variables below to suit your Arduino and network 
 environment:
 - mac (unique mac address for your arduino)
 - redPin (PWM pin for RED)
 - greenPin  (PWM pin for GREEN)
 - bluePin  (PWM pin for BLUE)
 
 Made possible by the work of magicmonkey: 
 https://github.com/magicmonkey/lifxjs/ - you can use this to control 
 your arduino bulb as well as real LIFX bulbs at the same time!
 
 And also the RGBMood library by Harold Waterkeyn, which was modified
 slightly to support powering down the LED
*/


#include <SPI.h>   
#include <Ethernet.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>   

#include "lifx.h"
#include "RGBMoodLifx.h"
#include "color.h"

// set to 1 to output debug messages (including packet dumps) to serial (38400 baud)
const boolean DEBUG = 1;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0xDE, 0xAD, 0xDE, 0xAD, 0xDE, 0xAD };

// pins for the RGB LED:
const int redPin = 3;
const int greenPin = 5;
const int bluePin = 6;

// label (name) for this bulb
char bulbLabel[LifxBulbLabelLength] = "Arduino Bulb";

// initial bulb values - warm white!
long power_status = 65535;
long hue = 0;
long sat = 0;
long bri = 65535;
long kel = 2000;
long dim = 0;

// Ethernet instances, for UDP broadcasting, and TCP server and client
EthernetUDP Udp;
EthernetServer TcpServer = EthernetServer(LifxPort);
EthernetClient client;
  
RGBMoodLifx LIFXBulb(redPin, greenPin, bluePin);

void setup() {

  Serial.begin(38400);
  Serial.println("LIFX bulb emulator for Arduino starting up...");

  // start the Ethernet - using DHCP so keep trying until we get an address
  while(Ethernet.begin(mac) == 0) {
    Serial.println("Failed to get DHCP address, trying again...");
    delay(1000);
  }

  Serial.print("IP address for this bulb: ");
  Serial.println(Ethernet.localIP());

  // set up a UDP and TCP port ready for incoming
  Udp.begin(LifxPort);
  TcpServer.begin();

  // set up the LED pins
  pinMode(redPin, OUTPUT); 
  pinMode(greenPin, OUTPUT); 
  pinMode(bluePin, OUTPUT); 
  
  LIFXBulb.setFadingSteps(20);
  LIFXBulb.setFadingSpeed(10);

  // set the bulb based on the initial colors
  setLight();
}

void loop() {
  LIFXBulb.tick();

  // buffers for receiving and sending data
  byte PacketBuffer[128]; //buffer to hold incoming packet,

  client = TcpServer.available();
  if (client == true) {
    // read incoming data
    int packetSize = 0;
    while (client.available()) {
      byte b = client.read();
      PacketBuffer[packetSize] = b;
      packetSize++;
    }

    if(DEBUG) {
      Serial.print("-TCP ");
      for(int i = 0; i < LifxPacketSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(" ");
      }

      for(int i = LifxPacketSize; i < packetSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }

    // push the data into the LifxPacket structure
    LifxPacket request;
    processRequest(PacketBuffer, packetSize, request);

    //respond to the request
    handleRequest(request);
  }

  // if there's UDP data available, read a packet
  int packetSize = Udp.parsePacket();
  if(packetSize) {
    Udp.read(PacketBuffer, 128);

    if(DEBUG) {
      Serial.print("-UDP ");
      for(int i = 0; i < LifxPacketSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(" ");
      }

      for(int i = LifxPacketSize; i < packetSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }

    // push the data into the LifxPacket structure
    LifxPacket request;
    processRequest(PacketBuffer, sizeof(PacketBuffer), request);

    //respond to the request
    handleRequest(request);

  }

  //delay(10);
}

void processRequest(byte *packetBuffer, int packetSize, LifxPacket &request) {

  request.size        = packetBuffer[0] + (packetBuffer[1] << 8); //little endian
  request.protocol    = packetBuffer[2] + (packetBuffer[3] << 8); //little endian
  request.reserved1   = packetBuffer[4] + packetBuffer[5] + packetBuffer[6] + packetBuffer[7];

  byte bulbAddress[] = { 
    packetBuffer[8], packetBuffer[9], packetBuffer[10], packetBuffer[11], packetBuffer[12], packetBuffer[13]     };
  memcpy(request.bulbAddress, bulbAddress, 6);

  request.reserved2   = packetBuffer[14] + packetBuffer[15];

  byte site[] = { 
    packetBuffer[16], packetBuffer[17], packetBuffer[18], packetBuffer[19], packetBuffer[20], packetBuffer[21]     };
  memcpy(request.site, site, 6);

  request.reserved3   = packetBuffer[22] + packetBuffer[23];
  request.timestamp   = packetBuffer[24] + packetBuffer[25] + packetBuffer[26] + packetBuffer[27] + 
    packetBuffer[28] + packetBuffer[29] + packetBuffer[30] + packetBuffer[31];
  request.packet_type = packetBuffer[32] + (packetBuffer[33] << 8); //little endian
  request.reserved4   = packetBuffer[34] + packetBuffer[35];

  int i;
  for(i = LifxPacketSize; i < packetSize; i++) {
    request.data[i-LifxPacketSize] = packetBuffer[i];
  }

  request.data_size = i;
}

void handleRequest(LifxPacket &request) {
  if(DEBUG) {
    Serial.print("  Received packet type ");
    Serial.println(request.packet_type, HEX);
  }

  LifxPacket response;
  switch(request.packet_type) {

  case GET_PAN_GATEWAY: 
    {
      // we are a gateway, so respond to this

      // respond with the UDP port
      response.packet_type = PAN_GATEWAY;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte UDPdata[] = { 
        SERVICE_UDP, //UDP
        lowByte(LifxPort), 
        highByte(LifxPort), 
        0x00, 
        0x00 
      };

      memcpy(response.data, UDPdata, sizeof(UDPdata));
      response.data_size = sizeof(UDPdata);
      sendPacket(response);

      // respond with the TCP port details
      response.packet_type = PAN_GATEWAY;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte TCPdata[] = { 
        SERVICE_TCP, //TCP
        lowByte(LifxPort), 
        highByte(LifxPort), 
        0x00, 
        0x00 
      };

      memcpy(response.data, TCPdata, sizeof(TCPdata)); 
      response.data_size = sizeof(TCPdata);
      sendPacket(response);

    } 
    break;


  case SET_LIGHT_STATE: 
    {
      // set the light colors
      hue = word(request.data[2], request.data[1]);
      sat = word(request.data[4], request.data[3]);
      bri = word(request.data[6], request.data[5]);
      kel = word(request.data[8], request.data[7]);

      setLight();
    } 
    break;


  case GET_LIGHT_STATE: 
    {
      // send the light's state
      response.packet_type = LIGHT_STATUS;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte StateData[] = { 
        lowByte(hue),  //hue
        highByte(hue), //hue
        lowByte(sat),  //sat
        highByte(sat), //sat
        lowByte(bri),  //bri
        highByte(bri), //bri
        lowByte(kel),  //kel
        highByte(kel), //kel
        lowByte(dim),  //dim
        highByte(dim), //dim
        lowByte(power_status),  //power status
        highByte(power_status), //power status
        // label
        lowByte(bulbLabel[0]),
        lowByte(bulbLabel[1]),
        lowByte(bulbLabel[2]),
        lowByte(bulbLabel[3]),
        lowByte(bulbLabel[4]),
        lowByte(bulbLabel[5]),
        lowByte(bulbLabel[6]),
        lowByte(bulbLabel[7]),
        lowByte(bulbLabel[8]),
        lowByte(bulbLabel[9]),
        lowByte(bulbLabel[10]),
        lowByte(bulbLabel[11]),
        lowByte(bulbLabel[12]),
        lowByte(bulbLabel[13]),
        lowByte(bulbLabel[14]),
        lowByte(bulbLabel[15]),
        lowByte(bulbLabel[16]),
        lowByte(bulbLabel[17]),
        lowByte(bulbLabel[18]),
        lowByte(bulbLabel[19]),
        lowByte(bulbLabel[20]),
        lowByte(bulbLabel[21]),
        lowByte(bulbLabel[22]),
        lowByte(bulbLabel[23]),
        lowByte(bulbLabel[24]),
        lowByte(bulbLabel[25]),
        lowByte(bulbLabel[26]),
        lowByte(bulbLabel[27]),
        lowByte(bulbLabel[28]),
        lowByte(bulbLabel[29]),
        lowByte(bulbLabel[30]),
        lowByte(bulbLabel[31]),
        0x00, //tags
        0x00, //tags
        0x00, //tags
        0x00, //tags
        0x00, //tags
        0x00, //tags
        0x00, //tags
        0x00  //tags
      };

      memcpy(response.data, StateData, sizeof(StateData));
      response.data_size = sizeof(StateData);
      sendPacket(response);
    } 
    break;


  case SET_POWER_STATE:
  case GET_POWER_STATE: 
    {
      if(request.packet_type == SET_POWER_STATE) {
        power_status = word(request.data[1], request.data[0]);
        setLight();
      }

      response.packet_type = POWER_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte PowerData[] = { 
        lowByte(power_status),
        highByte(power_status)
        };

      memcpy(response.data, PowerData, sizeof(PowerData));
      response.data_size = sizeof(PowerData);
      sendPacket(response);
    } 
    break;


  case SET_BULB_LABEL: 
  case GET_BULB_LABEL: 
    {
      if(request.packet_type == SET_BULB_LABEL) {
        for(int i = 0; i < LifxBulbLabelLength; i++) {
          bulbLabel[i] = request.data[i];
        }
      }

      response.packet_type = BULB_LABEL;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte BulbData[] = { 
        lowByte(bulbLabel[0]),
        lowByte(bulbLabel[1]),
        lowByte(bulbLabel[2]),
        lowByte(bulbLabel[3]),
        lowByte(bulbLabel[4]),
        lowByte(bulbLabel[5]),
        lowByte(bulbLabel[6]),
        lowByte(bulbLabel[7]),
        lowByte(bulbLabel[8]),
        lowByte(bulbLabel[9]),
        lowByte(bulbLabel[10]),
        lowByte(bulbLabel[11]),
        lowByte(bulbLabel[12]),
        lowByte(bulbLabel[13]),
        lowByte(bulbLabel[14]),
        lowByte(bulbLabel[15]),
        lowByte(bulbLabel[16]),
        lowByte(bulbLabel[17]),
        lowByte(bulbLabel[18]),
        lowByte(bulbLabel[19]),
        lowByte(bulbLabel[20]),
        lowByte(bulbLabel[21]),
        lowByte(bulbLabel[22]),
        lowByte(bulbLabel[23]),
        lowByte(bulbLabel[24]),
        lowByte(bulbLabel[25]),
        lowByte(bulbLabel[26]),
        lowByte(bulbLabel[27]),
        lowByte(bulbLabel[28]),
        lowByte(bulbLabel[29]),
        lowByte(bulbLabel[30]),
        lowByte(bulbLabel[31])
        };

      memcpy(response.data, BulbData, sizeof(BulbData));
      response.data_size = sizeof(BulbData);
      sendPacket(response);
    } 
    break;


  default: 
    {
      if(DEBUG) {
        Serial.println("  Unknown packet type, ignoring");
      }
    } 
    break;
  }
}

void sendPacket(LifxPacket &pkt) {
  sendUDPPacket(pkt);

  if(client.connected()) {
    sendTCPPacket(pkt);
  }
}

unsigned int sendUDPPacket(LifxPacket &pkt) { 
  // broadcast packet on local subnet
  IPAddress remote_addr(Udp.remoteIP());
  IPAddress broadcast_addr(remote_addr[0], remote_addr[1], remote_addr[2], 255);

  if(DEBUG) {
    Serial.print("+UDP ");
    printLifxPacket(pkt);
    Serial.println();
  }

  Udp.beginPacket(broadcast_addr, Udp.remotePort());

  // size
  Udp.write(lowByte(LifxPacketSize + pkt.data_size));
  Udp.write(highByte(LifxPacketSize + pkt.data_size));

  // protocol
  Udp.write(lowByte(pkt.protocol));
  Udp.write(highByte(pkt.protocol));

  // reserved1
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));

  // buldAddress mac address
  for(int i = 0; i < sizeof(mac); i++) {
    Udp.write(lowByte(mac[i]));
  }

  // reserved2
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));

  // site mac address
  for(int i = 0; i < sizeof(mac); i++) {
    Udp.write(lowByte(mac[i]));
  }

  // reserved3
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));

  // timestamp
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));

  //packet type
  Udp.write(lowByte(pkt.packet_type));
  Udp.write(highByte(pkt.packet_type));

  // reserved4
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));

  //data
  for(int i = 0; i < pkt.data_size; i++) {
    Udp.write(lowByte(pkt.data[i]));
  }

  Udp.endPacket();

  return LifxPacketSize + pkt.data_size;
}

unsigned int sendTCPPacket(LifxPacket &pkt) { 

  if(DEBUG) {
    Serial.print("+TCP ");
    printLifxPacket(pkt);
    Serial.println();
  }

  byte TCPBuffer[128]; //buffer to hold outgoing packet,
  int byteCount = 0;

  // size
  TCPBuffer[byteCount++] = lowByte(LifxPacketSize + pkt.data_size);
  TCPBuffer[byteCount++] = highByte(LifxPacketSize + pkt.data_size);

  // protocol
  TCPBuffer[byteCount++] = lowByte(pkt.protocol);
  TCPBuffer[byteCount++] = highByte(pkt.protocol);

  // reserved1
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);

  // buldAddress mac address
  for(int i = 0; i < sizeof(mac); i++) {
    TCPBuffer[byteCount++] = lowByte(mac[i]);
  }

  // reserved2
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);

  // site mac address
  for(int i = 0; i < sizeof(mac); i++) {
    TCPBuffer[byteCount++] = lowByte(mac[i]);
  }

  // reserved3
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);

  // timestamp
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);

  //packet type
  TCPBuffer[byteCount++] = lowByte(pkt.packet_type);
  TCPBuffer[byteCount++] = highByte(pkt.packet_type);

  // reserved4
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);

  //data
  for(int i = 0; i < pkt.data_size; i++) {
    TCPBuffer[byteCount++] = lowByte(pkt.data[i]);
  }

  client.write(TCPBuffer, byteCount);

  return LifxPacketSize + pkt.data_size;
}

// print out a LifxPacket data structure as a series of hex bytes - used for DEBUG
void printLifxPacket(LifxPacket &pkt) {
  // size
  Serial.print(lowByte(LifxPacketSize + pkt.data_size), HEX);
  Serial.print(" ");
  Serial.print(highByte(LifxPacketSize + pkt.data_size), HEX);
  Serial.print(" ");

  // protocol
  Serial.print(lowByte(pkt.protocol), HEX);
  Serial.print(" ");
  Serial.print(highByte(pkt.protocol), HEX);
  Serial.print(" ");

  // reserved1
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");

  // buldAddress mac address
  for(int i = 0; i < sizeof(mac); i++) {
    Serial.print(lowByte(mac[i]), HEX);
    Serial.print(" ");
  }

  // reserved2
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");

  // site mac address
  for(int i = 0; i < sizeof(mac); i++) {
    Serial.print(lowByte(mac[i]), HEX);
    Serial.print(" ");
  }

  // reserved3
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");

  // timestamp
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");

  //packet type
  Serial.print(lowByte(pkt.packet_type), HEX);
  Serial.print(" ");
  Serial.print(highByte(pkt.packet_type), HEX);
  Serial.print(" ");

  // reserved4
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");
  Serial.print(lowByte(0x00), HEX);
  Serial.print(" ");

  //data
  for(int i = 0; i < pkt.data_size; i++) {
    Serial.print(pkt.data[i], HEX);
    Serial.print(" ");
  }
}

void setLight() {
  if(DEBUG) {
    Serial.print("Set light - ");
    Serial.print("hue: ");
    Serial.print(hue);
    Serial.print(", sat: ");
    Serial.print(sat);
    Serial.print(", bri: ");
    Serial.print(bri);
    Serial.print(", kel: ");
    Serial.print(kel);
    Serial.print(", power: ");
    Serial.print(power_status);
    Serial.println(power_status ? " (on)" : "(off)");
  }

  if(power_status) {
    int this_hue = map(hue, 0, 65535, 0, 359);
    int this_sat = map(sat, 0, 65535, 0, 255);
    int this_bri = map(bri, 0, 65535, 0, 255);
     
    // if we are setting a "white" colour (kelvin temp)
    if(kel > 0 && this_sat < 1) {
      // convert kelvin to RGB
      rgb kelvin_rgb;
      kelvin_rgb = kelvinToRGB(kel);
      
      // convert the RGB into HSV
      hsv kelvin_hsv;
      kelvin_hsv = rgb2hsv(kelvin_rgb);
      
      // set the new values ready to go to the bulb (brightness does not change, just hue and saturation)
      this_hue = kelvin_hsv.h;
      this_sat = map(kelvin_hsv.s*1000, 0, 1000, 0, 255); //multiply the sat by 1000 so we can map the percentage value returned by rgb2hsv
    }

    LIFXBulb.fadeHSB(this_hue, this_sat, this_bri);
  } 
  else {
    LIFXBulb.setRGB(0, 0, 0);
  }
}






