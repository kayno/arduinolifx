/*
 LIFX bulb emulator by Kayne Richens (kayno@kayno.net)
 
 Emulates a LIFX bulb. Connect an RGB LED (or LED strip via drivers) 
 to redPin, greenPin and bluePin as you normally would on an 
 ethernet-ready Arduino and control it from the LIFX app!
 
 Notes:
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
#include <EEPROM.h>   

#include "lifx.h"
#include "RGBMoodLifx.h"
#include "color.h"

// set to 1 to output debug messages (including packet dumps) to serial (38400 baud)
const boolean DEBUG = 0;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 
  0xDE, 0xAD, 0xDE, 0xAD, 0xDE, 0xAD };
byte site_mac[] = { 
  0x4c, 0x49, 0x46, 0x58, 0x56, 0x32 }; // spells out "LIFXV2" - version 2 of the app changes the site address to this...

// pins for the RGB LED:
const int redPin = 3;
const int greenPin = 5;
const int bluePin = 6;

// label (name) for this bulb
char bulbLabel[LifxBulbLabelLength] = "Arduino Bulb";

// tags for this bulb
char bulbTags[LifxBulbTagsLength] = {
  0,0,0,0,0,0,0,0};
char bulbTagLabels[LifxBulbTagLabelsLength] = "";

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
  Serial.println(F("LIFX bulb emulator for Arduino starting up..."));

  // start the Ethernet - using DHCP so keep trying until we get an address
  while(Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to get DHCP address, trying again..."));
    delay(1000);
  }

  Serial.print(F("IP address for this bulb: "));
  Serial.println(Ethernet.localIP());

  // set up a UDP and TCP port ready for incoming
  Udp.begin(LifxPort);
  TcpServer.begin();

  // set up the LED pins
  pinMode(redPin, OUTPUT); 
  pinMode(greenPin, OUTPUT); 
  pinMode(bluePin, OUTPUT); 

  LIFXBulb.setFadingSteps(20);
  LIFXBulb.setFadingSpeed(20);
  
  // read in settings from EEPROM (if they exist) for bulb label and tags
  if(EEPROM.read(EEPROM_CONFIG_START) == EEPROM_CONFIG[0]
    && EEPROM.read(EEPROM_CONFIG_START+1) == EEPROM_CONFIG[1]
    && EEPROM.read(EEPROM_CONFIG_START+2) == EEPROM_CONFIG[2]) {
      if(DEBUG) {
        Serial.println(F("Config exists in EEPROM, reading..."));
        Serial.print(F("Bulb label: "));
      }
  
      for(int i = 0; i < LifxBulbLabelLength; i++) {
        bulbLabel[i] = EEPROM.read(EEPROM_BULB_LABEL_START+i);
        
        if(DEBUG) {
          Serial.print(bulbLabel[i]);
        }
      }
      
      if(DEBUG) {
        Serial.println();
        Serial.print(F("Bulb tags: "));
      }
      
      for(int i = 0; i < LifxBulbTagsLength; i++) {
        bulbTags[i] = EEPROM.read(EEPROM_BULB_TAGS_START+i);
        
        if(DEBUG) {
          Serial.print(bulbTags[i]);
        }
      }
      
      if(DEBUG) {
        Serial.println();
        Serial.print(F("Bulb tag labels: "));
      }
      
      for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
        bulbTagLabels[i] = EEPROM.read(EEPROM_BULB_TAG_LABELS_START+i);
        
        if(DEBUG) {
          Serial.print(bulbTagLabels[i]);
        }
      }
      
      if(DEBUG) {
        Serial.println();
        Serial.println(F("Done reading EEPROM config."));
      }
  } else {
    // first time sketch has been run, set defaults into EEPROM
    if(DEBUG) {
      Serial.println(F("Config does not exist in EEPROM, writing..."));
    }
    
    EEPROM.write(EEPROM_CONFIG_START, EEPROM_CONFIG[0]);
    EEPROM.write(EEPROM_CONFIG_START+1, EEPROM_CONFIG[1]);
    EEPROM.write(EEPROM_CONFIG_START+2, EEPROM_CONFIG[2]);
    
    for(int i = 0; i < LifxBulbLabelLength; i++) {
       EEPROM.write(EEPROM_BULB_LABEL_START+i, bulbLabel[i]);
    }
    
    for(int i = 0; i < LifxBulbTagsLength; i++) {
      EEPROM.write(EEPROM_BULB_TAGS_START+i, bulbTags[i]);
    }
    
    for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
      EEPROM.write(EEPROM_BULB_TAG_LABELS_START+i, bulbTagLabels[i]);
    }
      
    if(DEBUG) {
      Serial.println(F("Done writing EEPROM config."));
    }
  }
  
  if(DEBUG) {
    Serial.println(F("EEPROM dump:"));
    for(int i = 0; i < 256; i++) {
      Serial.print(EEPROM.read(i));
      Serial.print(SPACE);
    }
    Serial.println();
  }

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
      Serial.print(F("-TCP "));
      for(int i = 0; i < LifxPacketSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(SPACE);
      }

      for(int i = LifxPacketSize; i < packetSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(SPACE);
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
      Serial.print(F("-UDP "));
      for(int i = 0; i < LifxPacketSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(SPACE);
      }

      for(int i = LifxPacketSize; i < packetSize; i++) {
        Serial.print(PacketBuffer[i], HEX);
        Serial.print(SPACE);
      }
      Serial.println();
    }

    // push the data into the LifxPacket structure
    LifxPacket request;
    processRequest(PacketBuffer, sizeof(PacketBuffer), request);

    //respond to the request
    handleRequest(request);

  }

  Ethernet.maintain();

  //delay(10);
}

void processRequest(byte *packetBuffer, int packetSize, LifxPacket &request) {

  request.size        = packetBuffer[0] + (packetBuffer[1] << 8); //little endian
  request.protocol    = packetBuffer[2] + (packetBuffer[3] << 8); //little endian
  request.reserved1   = packetBuffer[4] + packetBuffer[5] + packetBuffer[6] + packetBuffer[7];

  byte bulbAddress[] = { 
    packetBuffer[8], packetBuffer[9], packetBuffer[10], packetBuffer[11], packetBuffer[12], packetBuffer[13]       
  };
  memcpy(request.bulbAddress, bulbAddress, 6);

  request.reserved2   = packetBuffer[14] + packetBuffer[15];

  byte site[] = { 
    packetBuffer[16], packetBuffer[17], packetBuffer[18], packetBuffer[19], packetBuffer[20], packetBuffer[21]       
  };
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
    Serial.print(F("  Received packet type "));
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
        //tags
        lowByte(bulbTags[0]),
        lowByte(bulbTags[1]),
        lowByte(bulbTags[2]),
        lowByte(bulbTags[3]),
        lowByte(bulbTags[4]),
        lowByte(bulbTags[5]),
        lowByte(bulbTags[6]),
        lowByte(bulbTags[7])
        };

      memcpy(response.data, StateData, sizeof(StateData));
      response.data_size = sizeof(StateData);
      sendPacket(response);
    } 
    break;


  case SET_POWER_STATE:
  case GET_POWER_STATE: 
    {
      // set if we are setting
      if(request.packet_type == SET_POWER_STATE) {
        power_status = word(request.data[1], request.data[0]);
        setLight();
      }

      // respond to both get and set commands
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
      // set if we are setting
      if(request.packet_type == SET_BULB_LABEL) {
        for(int i = 0; i < LifxBulbLabelLength; i++) {
          if(bulbLabel[i] != request.data[i]) {
            bulbLabel[i] = request.data[i];
            EEPROM.write(EEPROM_BULB_LABEL_START+i, request.data[i]);
          }
        }
      }

      // respond to both get and set commands
      response.packet_type = BULB_LABEL;
      response.protocol = LifxProtocol_AllBulbsResponse;
      memcpy(response.data, bulbLabel, sizeof(bulbLabel));
      response.data_size = sizeof(bulbLabel);
      sendPacket(response);
    } 
    break;


  case SET_BULB_TAGS: 
  case GET_BULB_TAGS: 
    {
      // set if we are setting
      if(request.packet_type == SET_BULB_TAGS) {
        for(int i = 0; i < LifxBulbTagsLength; i++) {
          if(bulbTags[i] != request.data[i]) {
            bulbTags[i] = lowByte(request.data[i]);
            EEPROM.write(EEPROM_BULB_TAGS_START+i, request.data[i]);
          }
        }
      }

      // respond to both get and set commands
      response.packet_type = BULB_TAGS;
      response.protocol = LifxProtocol_AllBulbsResponse;
      memcpy(response.data, bulbTags, sizeof(bulbTags));
      response.data_size = sizeof(bulbTags);
      sendPacket(response);
    } 
    break;


  case SET_BULB_TAG_LABELS: 
  case GET_BULB_TAG_LABELS: 
    {
      // set if we are setting
      if(request.packet_type == SET_BULB_TAG_LABELS) {
        for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
          if(bulbTagLabels[i] != request.data[i]) {
            bulbTagLabels[i] = request.data[i];
            EEPROM.write(EEPROM_BULB_TAG_LABELS_START+i, request.data[i]);
          }
        }
      }

      // respond to both get and set commands
      response.packet_type = BULB_TAG_LABELS;
      response.protocol = LifxProtocol_AllBulbsResponse;
      memcpy(response.data, bulbTagLabels, sizeof(bulbTagLabels));
      response.data_size = sizeof(bulbTagLabels);
      sendPacket(response);
    } 
    break;


  case GET_VERSION_STATE: 
    {
      // respond to get command
      response.packet_type = VERSION_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte VersionData[] = { 
        lowByte(LifxBulbVendor),
        highByte(LifxBulbVendor),
        0x00,
        0x00,
        lowByte(LifxBulbProduct),
        highByte(LifxBulbProduct),
        0x00,
        0x00,
        lowByte(LifxBulbVersion),
        highByte(LifxBulbVersion),
        0x00,
        0x00
        };

      memcpy(response.data, VersionData, sizeof(VersionData));
      response.data_size = sizeof(VersionData);
      sendPacket(response);
      
      /*
      // respond again to get command (real bulbs respond twice, slightly diff data (see below)
      response.packet_type = VERSION_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte VersionData2[] = { 
        lowByte(LifxVersionVendor), //vendor stays the same
        highByte(LifxVersionVendor),
        0x00,
        0x00,
        lowByte(LifxVersionProduct*2), //product is 2, rather than 1
        highByte(LifxVersionProduct*2),
        0x00,
        0x00,
        0x00, //version is 0, rather than 1
        0x00,
        0x00,
        0x00
        };

      memcpy(response.data, VersionData2, sizeof(VersionData2));
      response.data_size = sizeof(VersionData2);
      sendPacket(response);
      */
    } 
    break;


  case GET_MESH_FIRMWARE_STATE: 
    {
      // respond to get command
      response.packet_type = MESH_FIRMWARE_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      // timestamp data comes from observed packet from a LIFX v1.5 bulb
      byte MeshVersionData[] = { 
        0x00, 0x2e, 0xc3, 0x8b, 0xef, 0x30, 0x86, 0x13, //build timestamp
        0xe0, 0x25, 0x76, 0x45, 0x69, 0x81, 0x8b, 0x13, //install timestamp
        lowByte(LifxFirmwareVersionMinor),
        highByte(LifxFirmwareVersionMinor),
        lowByte(LifxFirmwareVersionMajor),
        highByte(LifxFirmwareVersionMajor)
        };

      memcpy(response.data, MeshVersionData, sizeof(MeshVersionData));
      response.data_size = sizeof(MeshVersionData);
      sendPacket(response);
    } 
    break;


  case GET_WIFI_FIRMWARE_STATE: 
    {
      // respond to get command
      response.packet_type = WIFI_FIRMWARE_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      // timestamp data comes from observed packet from a LIFX v1.5 bulb
      byte WifiVersionData[] = { 
        0x00, 0xc8, 0x5e, 0x31, 0x99, 0x51, 0x86, 0x13, //build timestamp
        0xc0, 0x0c, 0x07, 0x00, 0x48, 0x46, 0xd9, 0x43, //install timestamp
        lowByte(LifxFirmwareVersionMinor),
        highByte(LifxFirmwareVersionMinor),
        lowByte(LifxFirmwareVersionMajor),
        highByte(LifxFirmwareVersionMajor)
        };

      memcpy(response.data, WifiVersionData, sizeof(WifiVersionData));
      response.data_size = sizeof(WifiVersionData);
      sendPacket(response);
    } 
    break;


  default: 
    {
      if(DEBUG) {
        Serial.println(F("  Unknown packet type, ignoring"));
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
    Serial.print(F("+UDP "));
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

  // bulbAddress mac address
  for(int i = 0; i < sizeof(mac); i++) {
    Udp.write(lowByte(mac[i]));
  }

  // reserved2
  Udp.write(lowByte(0x00));
  Udp.write(lowByte(0x00));

  // site mac address
  for(int i = 0; i < sizeof(site_mac); i++) {
    Udp.write(lowByte(site_mac[i]));
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
    Serial.print(F("+TCP "));
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

  // bulbAddress mac address
  for(int i = 0; i < sizeof(mac); i++) {
    TCPBuffer[byteCount++] = lowByte(mac[i]);
  }

  // reserved2
  TCPBuffer[byteCount++] = lowByte(0x00);
  TCPBuffer[byteCount++] = lowByte(0x00);

  // site mac address
  for(int i = 0; i < sizeof(site_mac); i++) {
    TCPBuffer[byteCount++] = lowByte(site_mac[i]);
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
  Serial.print(SPACE);
  Serial.print(highByte(LifxPacketSize + pkt.data_size), HEX);
  Serial.print(SPACE);

  // protocol
  Serial.print(lowByte(pkt.protocol), HEX);
  Serial.print(SPACE);
  Serial.print(highByte(pkt.protocol), HEX);
  Serial.print(SPACE);

  // reserved1
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  // bulbAddress mac address
  for(int i = 0; i < sizeof(mac); i++) {
    Serial.print(lowByte(mac[i]), HEX);
    Serial.print(SPACE);
  }

  // reserved2
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  // site mac address
  for(int i = 0; i < sizeof(site_mac); i++) {
    Serial.print(lowByte(site_mac[i]), HEX);
    Serial.print(SPACE);
  }

  // reserved3
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  // timestamp
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  //packet type
  Serial.print(lowByte(pkt.packet_type), HEX);
  Serial.print(SPACE);
  Serial.print(highByte(pkt.packet_type), HEX);
  Serial.print(SPACE);

  // reserved4
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  //data
  for(int i = 0; i < pkt.data_size; i++) {
    Serial.print(pkt.data[i], HEX);
    Serial.print(SPACE);
  }
}

void setLight() {
  if(DEBUG) {
    Serial.print(F("Set light - "));
    Serial.print(F("hue: "));
    Serial.print(hue);
    Serial.print(F(", sat: "));
    Serial.print(sat);
    Serial.print(F(", bri: "));
    Serial.print(bri);
    Serial.print(F(", kel: "));
    Serial.print(kel);
    Serial.print(F(", power: "));
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
    LIFXBulb.fadeHSB(0, 0, 0);
  }
}







