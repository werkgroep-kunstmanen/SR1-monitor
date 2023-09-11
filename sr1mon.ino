/********************************************************************
 * RCSId: $Id$
 *
 * SR1 (Ayecka) monitor, using WiFi 
 * Project: Eumetcast receiver
 * Author: Rob Alblas
 *    werkgroep Kunstmanen (working group Satellites)
 *    www.kunstmanen.net
 *
 * History: 
 * $Log$
 *
 * Description:
 * SR1 (Ayecka) monitor, using WiFi.
 * Shows SNR, modcod and link marge.
 * Hardware: 8266 and I2C Oled display.
 * Basic code originates from:
 *   https://github.com/shortbloke/Arduino_SNMP_Manager
 *
 * Make sure IPAddress equals the management port of the Ayecka SR1
 *   (defauklt 192.168.178.99)
 ********************************************************************/
#if defined(ESP8266)
#include <ESP8266WiFi.h> // ESP8266 Core WiFi Library         
#else
#include <WiFi.h>        // ESP32 Core WiFi Library    
#endif

#define OLED
#ifdef OLED
  #include "U8g2lib.h"
  U8G2_SH1106_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0);
  #define FONTW 8
  #define FONTH 15
  #define OFFSX 0
  #define OFFSY 15

  #define LENOBUF 16
  void dprintf(int x,int y,char *si)
  {
    char s[LENOBUF];
    int yp=y;
    int i;
    strcpy(s,si);
    for (i=strlen(s); i<LENOBUF; i++) s[i]=' ';
    x=x*FONTW+OFFSX;
    y=y*FONTH+OFFSY;
    u8g2.setBufferCurrTileRow(0);
    u8g2.drawStr(x,OFFSY,s);
    u8g2.setBufferCurrTileRow(yp*2);
    u8g2.sendBuffer();
  }
#endif

#include <WiFiUdp.h>

//#define DEBUG // for debugging SNMP
// sr1 seems to generate(???) lots of errored packets, suppress error messages!

#define SUPPRESS_ERROR_SHORT_PACKET
#define SUPPRESS_ERROR_FAILED_PARSE

#include <Arduino_SNMP_Manager.h>

// ssid and password of your WiFi
const char *ssid = "alb274276";
const char *password = "alb2762927274bla";

//************************************
//* SNMP Device Info                 *
//************************************
IPAddress target(192, 168, 178, 99);        // IP address of management port SR1

const char *community = "public";
const int snmpVersion = 0; // SNMP Version 1 = 0, SNMP Version 2 = 1

// OIDs
const char *oidloc = ".1.3.6.1.4.1.27928.101.1.1.4.1.0"; // lock of RX1
const char *oidsnr = ".1.3.6.1.4.1.27928.101.1.1.4.4.0"; // SNR of RX1
const char *oidmod = ".1.3.6.1.4.1.27928.101.1.1.4.7.0"; // modcod of RX1
const char *oidlmg = ".1.3.6.1.4.1.27928.101.1.1.4.6.0"; // marge of RX1
const char *oidnbr = ".1.3.6.1.4.1.27928.101.3.3.0";   // serial number

//************************************

//************************************
//* Settings                         *
//************************************
int pollInterval = 100; // delay in milliseconds
unsigned long intervalBetweenPolls=0;

//************************************
//* Initialise                       *
//************************************
// Variables
int locval=0;
int snrval=0;
int modval=0;
int lmgval=0;
int nbrval=0;
unsigned long pollStart = 0;

// SNMP Objects
WiFiUDP udp;                                // UDP object used to send and receive packets
SNMPManager snmp = SNMPManager(community);
SNMPGet snmpRequest = SNMPGet(community, snmpVersion); // 0: version 1, 1: version 2
//SNMPGetResponse snmpresp = SNMPGetResponse();

// Blank callback pointer for each OID
ValueCallback *callbackloc;
ValueCallback *callbacksnr;
ValueCallback *callbackmod;
ValueCallback *callbacklmg;
ValueCallback *callbacknbr;

//************************************
//* Function declarations            *
//************************************
void getSNMP();
void doSNMPCalculations();
void printVariableValues();
//************************************

void setup()
{
  Serial.begin(115200);
  #ifdef OLED
    u8g2.begin();
    u8g2.firstPage(); 
    u8g2.setFont(u8g2_font_t0_15b_mf);
    u8g2.setAutoPageClear(0);
  #endif
  dprintf(0,0,(char *)"Connecting...");

  WiFi.begin(ssid, password);
  Serial.println("");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to SSID: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  dprintf(0,0,(char *)"Connected.");

  snmp.setUDP(&udp); // give snmp a pointer to the UDP object
  snmp.begin();      // start the SNMP Manager

  callbackloc = snmp.addIntegerHandler(target, oidloc, &locval);
  callbacksnr = snmp.addIntegerHandler(target, oidsnr, &snrval);
  callbackmod = snmp.addIntegerHandler(target, oidmod, &modval);
  callbacklmg = snmp.addIntegerHandler(target, oidlmg, &lmgval);
//  callbacknbr = snmp.addIntegerHandler(target, oidnbr, &nbrval);

  delay(1000);
}

void loop()
{
  snmp.loop();  // Call frequently
  intervalBetweenPolls = millis() - pollStart;
  if (intervalBetweenPolls >= pollInterval)
  {
    pollStart += pollInterval; // this prevents drift in the delays
    getSNMP();
    printVariableValues(); // Print the values to the serial console
  }
}

void getSNMP()
{
  static int nn;
  nn=(nn+1)%10;
  if (!nn) locval=-1;

  // Send SNMP Get request
  snmpRequest.addOIDPointer(callbackloc);
  snmpRequest.addOIDPointer(callbacksnr);
  snmpRequest.addOIDPointer(callbackmod);
  snmpRequest.addOIDPointer(callbacklmg);
  snmpRequest.setIP(WiFi.localIP()); //IP of the esp
  snmpRequest.setUDP(&udp);
  snmpRequest.setRequestID(rand() % 5555);
  snmpRequest.sendTo(target);
//printf("stat=%d  index=%d  corrupt=%d\n",snmpresp.errorStatus,snmpresp.errorIndex,snmpresp.isCorrupt);
  snmpRequest.clearOIDList();
}

void printVariableValues()
{
  static int mm;
  // Display response (first call might be empty)
#ifdef OLED
  {
    static int lmlval,lmhval;
    static int n;
    char s[LENOBUF];
    char *vent=(char *)"-\\|/";

    if (locval==-1)
    {
      if (mm<5)
      {
        mm++;
      }
      else
      {
        dprintf(0,0,(char *)"No response!");
        dprintf(0,1,(char *)"");
        dprintf(0,2,(char *)"");
        dprintf(0,3,(char *)"");
      }
      return;
    }
    else
    {
      mm=0;
      if (locval==0)
        sprintf(s,"SNR = %d.%d   ",snrval/10,snrval%10);
      else
        sprintf(s,"Not locked!");
    }

    dprintf(0,0,s);

    if (modval==12)      sprintf(s,"mod = 8psk"); // _3_5");
    else if (modval==18) sprintf(s,"mod = 16apsk"); // _2_3");
    else                 sprintf(s,"mod = %d (unk)",modval);
    dprintf(0,1,s);

    if (modval==12) lmlval=lmgval;
    if (modval==18) lmhval=lmgval;
    sprintf(s,"lml = %d.%d   ",lmlval/10,lmlval%10);
    dprintf(0,2,s);

    sprintf(s,"lmh = %d.%d   ",lmhval/10,lmhval%10);
    dprintf(0,3,s);

    sprintf(s,"%c",vent[n]);
    dprintf(13,3,s);
    n=(n+1)%4;
  }
#endif
//  printf("snr=%d\n",snrval); // print to serial interface
}
