///////////////////////////////////////////////////////////////////////
//  Project Tilte:  Orange Web matrix display (Iot project)          //
//  Version: 1.0                                                     //
//  Editor: Erwann Caroff                                            //
//  Last uptdate: 16/03/20017                                        //
///////////////////////////////////////////////////////////////////////
/******************************************** NodeMCU pins:************************************************************
 

  -->Matrix 4 modul:
  .CLK -> D5 (14)
  .CS  -> D3 (0)
  .DIN-> D7 (13)
  .GND-> GND
  .VCC-> 3.3V

  -->BMP180 (presure/temperatur/altitude sensor)
  .VCC --> 3.3V / 5V
  .GND --> GND
  .SCL-> D1
  .SDA-> D2

  -->DHT11 (humidity / temperatur)
  .GND (- on Conrad breakout board) -> GND
  .VCC                              -> 3.3V
  .D4 (s on Conrad breakout board)  ->  Din

  -->Microphone (Conrad breakout board)
  .GND (G on Conrad breakout board) -> Gnd
  .VCC (+ on Conrad breakout board) -> 3.3V
  .A0                               -> AD0
*/


/************************************************Sommaire des fonction*****************************************************
////////////////////////////////Configuration via page web

void handle_ConfigRestart
void handle_Configintensit
void handle_configSpeedDisplay
void handle_configStartBySound
void webPageEdit

////////////////////////////////Protocole Mqttt

void onMqttConnect
void onMqttDisconnect
void onMqttSubscribe
void onMqttUnsubscribe
void onMqttMessage
void onMqttPublish

////////////////////////////////Téléversement via WIFI OTA

void otaConfiguration

////////////////////////////////Configuration du mode (server mode si connexion WFI ok / Acese Point si connexion echec)

void GatewayServerMode
void ApMode

/////////////////////////////////Prise de mesure capteurs

void sensorRead
void getNoise

/////////////////////////////////Envoie des données sur Live Objects

void mqttPublish

//////////////////////////////////Aquisition de l'heure via WIFI

void getTime
void printDigits
void sendNTPpacket

///////////////////////////////////Affichage sur le Smart Display 

void matrixDisplay
void DisplayAccesPointMsg
void InitMatrixDisplay
void IpDisplay

///////////////////////////////////Lecturez des données sur Live Objects

void GetLiveObjectData
void LiveObjectDataParser

////////////////////////////////////Allumage de la Tv automatique

void wake_on_lan

///////////////////////////////////Lecture et affichage des données RSS
void rssParser
void RssMatrixDisplay

////////////////////////////////////Fonction de configuration
void setup

//////////////////////////////////Fonction principale
void loop
*/

//////////////////////////////////////////////////////////////////////////////////////////////////// Library
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <time.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "Timer.h"
#include <DHT11.h>//Capteur température
#include "DHT.h"   // Librairie des capteurs DHT
#include <Adafruit_Sensor.h>        //Librairie pour le capteur KY 052 (présion atmosphérique) 
#include <Wire.h>;
#include <Adafruit_BMP085.h>;
Adafruit_BMP085 bmp;
#include <ESPAsyncTCP.h>  //Mqtt librarie
#include <AsyncMqttClient.h>
#include <WakeOnLan.h>//WOL librarie
WiFiClient client;
Timer t;
Timer t1;



///////////////////////////////////////////////////////////////////////////////////////////////////Global Variable
String WebPageString;
long period;
int offset = 1, refresh = 0;
String decodedMsg;
int a, b, c = 0;
int msgScrol = 0;
String ChipIdString;
bool functionLock = 0;
bool bmplock = 0;
int dispInstensity = 0;
bool veille = 0;
bool lockStartBySound = 0;
char bufSSID[100] = "";  
char bufPASS[100] = "";
char bufIntens[2] = "";
char bufspeedScroling[100] = "";
String connexionMode;
int loopCpt=0;
char result[16];
char SourceIdArray[30]; 
String DispStatus = "Starting";
int temp = 0, humi = 0;





/////////////////////////////////////////////////////////////////////////////////configuration pour Live Objects 

String url = "https://liveobjects.orange-business.com/api/v0/data/streams/urn%3Alo%3Ansid%3AsmartDisplay%3AWebPage!Msg?limit=1";
String UserAgent = "SMART DISPLAY";
const char* host = "liveobjects.orange-business.com";
const char* topic_online ="dev/data";
const char* username ="json+device";
const char* ApiKey ="7ef7949fb37c454aa3087f63c276573d";
const char* mqttHost = "liveobjects.orange-business.com";
String Msg="Test";
unsigned int GetMsgFlag =0;
const int httpsPort = 443;
const char* fingerprint = "53 7F E0 E9 1B E9 F1 84 DD 07 5D 5D F7 B2 73 53 CD C3 37 81"; // Use web browser to view and copy SHA1 fingerprint of the certificate
String LiveObjectData;

/////////////////////////////////////////////////////////////////////////////// Noise sensore config
#define NoiseLimit 4 //Noise limit to standby mode
#define  interval 1500 
unsigned int maxPeakToPeak = 0;   // peak-to-peak level
unsigned long previousMillis = 0;  

/////////////////////////////////////////////////////////////////////////////// Connexion try time
#define secondeConexionTry 20

//////////////////////////////////////////////////////////////////////////////  cofee ready Temperatur
#define SeuilleMarcheCafetiere 30

////////////////////////////////////////////////////////////////////////////  Data store 
#define SensorNumber 15
//La luminosité et l'altitude ne sont pas utilisé, cependant les varaible sont inscites afin de garder une compatiboilité avec la page web  utiliser par la webradio
String sensor[][SensorNumber] =
{
  {"Temp", "Hygro", "PsAt", "Lum", "Bruit", "Alt", "tempCaf", "Msg", "Heure", "Date", "IP","RSSI","Box","SourceRss","Status"},
  {"Nc", "Nc", "Nc", "Nc", "Nc", "Nc", "\"Nc\"", "Nc", "Nc", "Nc", "Nc","Nc","Nc","Nc","Nc"},
  {"oC", "%", "hPa", "", "%", "m", "oC", "", "", "", "", "","","",""}
};



////////////////////////////////////////////////////////////////////////////// Rss config
char hostRss [50] = "www.france24.com";
#define MaxRssData 6
// We now create a URI for the request
char urlRss [50] = "http://www.france24.com/fr/france/rss";

String RssTiltle[MaxRssData+1];
//String RssDescrption[MaxRssData+1];
int  nbRssData =  0;
int PointerDisplayRss =2;

/////////////////////////////////////////////////////////////////////////// Display config
unsigned int wait = 25; // In milliseconds
#define spacer  2
unsigned int width =5 + spacer; // Espaces entre caracteres
#define pinCS  0 // Attach CS to this pin, DIN to MOSI and CLK to SCK 
#define numberOfHorizontalDisplays   4 //Nombre de matrice de Led (Horizontal)
#define numberOfVerticalDisplays   1 //Nombre de matrice de Led (Vertical)
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);
///////////////////////////////////////////////////////////////////////////////////  NTP Servers:(date/heur)
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

const int timeZone = 2;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)
WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff[100];

long lastMsg = 0;   //Horodatage du dernier message publié sur MQTT
long lastRecu = 0;

AsyncMqttClient mqttClient;
ESP8266WebServer server(80);// HTTP server will listen at port 80

//////////////////////////////////////////////////////////////////////////////////////  Configuration Wake on Lan 
bool TvPowerOn = false;
IPAddress computer_ip(192, 168, 1, 61); //The target IP address to send the magic packet to.
byte mac[] = {0xE4,0x7D,0xBD,0xFD,0x45,0x8A}; //The targets MAC address to send the packet to "TV salle de créativité"
void sendWOL();
#define wakeUpTv 10


/////////////////////////////////////////////////////////////////////////////////////Capteur DHT (températur et humidité)

//Version DHT22:
#define DHTPIN 2    // Changer le pin sur lequel est branché le DHT
// Dé-commentez la ligne qui correspond à votre capteur 
#define DHTTYPE DHT11     // DHT 11 
/////////////////////////////////#define DHTTYPE DHT22      // DHT 22  (AM2302)
//#define DHTTYPE DHT21     // DHT 21 (AM2301)
// Initialisation du capteur pour un Arduino à 16mhz par défaut
// Il faudra modifier le 3ème paramètres pour une autre carte (sinon le capteur renvoie 0). Quelques valeurs : 8mhz => 3, 16mhz => 6, 84mhz => 84
DHT dht(DHTPIN, DHTTYPE); 
///////////////////////////////////////////////////////////////////////////////////////////////////////
//***************************************************************************************************//
//*************************************** Functions **************************************************//
//***************************************************************************************************//
///////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////
//                                     handle Config Restart                            //
//  Description: Configuration des identifiants de box WIFI via navigateur              //
//////////////////////////////////////////////////////////////////////////////////////////

void handle_ConfigRestart()
{
  webPageEdit();
  String ssid = server.arg("ssid");
  Serial.print(F("Ssid: "));
  Serial.println(ssid);
  String password = server.arg("password");
  Serial.print(F("Password: "));
  Serial.println(password);

  const int  ssidlength = ssid.length();
  const int  passwordlength = password.length();
  const int totalconfigLength = ssidlength  + passwordlength + 5;

  String stringConfig =  "d/" + ssid + "/" + password + "/" ;
  char arrayConfig [totalconfigLength];

  Serial.print(F("string config: "));
  Serial.println(stringConfig);

  stringConfig.toCharArray(arrayConfig, totalconfigLength);

  Serial.print(F("array config: "));
  Serial.println(arrayConfig);
  //eeprom clear
  for (int i = 0; i < totalconfigLength; i++)
  {
    EEPROM.write(i, 0);
  }

  //Write  in eeprom
  for (int i = 0 ; i < totalconfigLength ; i ++)
  {
    EEPROM.write(i, arrayConfig[i]);
  }

  EEPROM.commit();
  Serial.println("Restart..");

  ESP.restart();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     handle Config intensity                          //
//  Description: Configuration de l'intensité lumineuse de l'afficheur via le navigateur//
//////////////////////////////////////////////////////////////////////////////////////////

void handle_Configintensity()
{
  int intensity = server.arg("intensity").toInt();
  String intensityString = String(intensity);
  int intensityLength = intensityString.length();
  //bufIntens = ' ';
  intensityString.toCharArray(bufIntens, intensityLength + 1);

  if (intensity < 16)
  {
    matrix.setIntensity(intensity); // Use a value between 0 and 15 for brightness

     //clear eepom for Disp ligth intensity
    for (int i = 100 ; i < 2 + 100 ; i ++)
    {
      EEPROM.write(i, ' ');
    }

    //Write  in eeprom
    for (int i = 100 ; i < 2 + 100 ; i ++)
    {
      EEPROM.write(i, bufIntens[i - 100]);
    }
    
    EEPROM.commit();  
  }
     webPageEdit();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     handle config Speed Display                      //
//  Description: Configuration du défilement des infos sur l'afficheur (navigateur)     //
//////////////////////////////////////////////////////////////////////////////////////////
void handle_configSpeedDisplay()
{
  int speedScroling = server.arg("speedScroling").toInt();

  String speedScrolingString = String(speedScroling);
  int speedScrolingLength = speedScrolingString.length();
  speedScrolingString.toCharArray(bufspeedScroling, speedScrolingLength + 1);
  Serial.print(F("speed:"));
   Serial.println(speedScroling);

  if (speedScroling < 11 & speedScroling > 0)
  {
    wait = speedScroling; 
    Serial.println("speed change!!");

    //clear eepom for Disp ligth intensity
    for (int i = 110 ; i < 3 + 110 ; i ++)
    {
      EEPROM.write(i, ' ');
    }

    //Write  in eeprom
    for (int i = 110 ; i < 3 + 110 ; i ++)
    {
      EEPROM.write(i, bufspeedScroling[i - 110]);
    } 
    EEPROM.commit();
  }
    webPageEdit();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     handle config Start By Sound                     //
//  Description: Configuration de l'allumage automatique de l'afficheur                 //
//////////////////////////////////////////////////////////////////////////////////////////
void handle_configStartBySound()
{

  int StartBySound = server.arg("StartBySound").toInt();

  //Value Filter
  if (StartBySound == 1 | StartBySound == 0)
  {
    lockStartBySound = StartBySound ; 
    EEPROM.write(120, StartBySound);
    EEPROM.commit();
    webPageEdit();
  }
}
//////////////////////////////////////////////////////////////////////////////////////////
//                                     handle config RSS                                //
//  Description: Configuration du défilement des infos sur l'afficheur (navigateur)     //
//////////////////////////////////////////////////////////////////////////////////////////
void handle_config_RSS()
{
  String StringhostRss = server.arg("LienRss");
  int hostRssLength = StringhostRss.length();
  StringhostRss.toCharArray(hostRss, hostRssLength + 1);
  Serial.print(F("hostRss:"));
  Serial.println(hostRss);

    Serial.println("host Rss change!!");

    //clear eepom for Disp ligth intensity
    for (int i = 121 ; i < 100 + 121 ; i ++)
    {
      EEPROM.write(i, '\0');
    }

    //Write  in eeprom
    for (int i = 121 ; i < 100 + 121 ; i ++)
    {
      EEPROM.write(i, hostRss[i - 121]);
    } 
    EEPROM.commit();

    webPageEdit();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     web Page Edit                                   //
//  Description: Edition du script de la page web en mode aces point local              //
//////////////////////////////////////////////////////////////////////////////////////////
void webPageEdit()
{
  char charArrayScriptWebPage[2000];
  memset(charArrayScriptWebPage,'\0',2000);
  
  sprintf ( charArrayScriptWebPage,
   "<html>\
  <style>h1{ color: orange;} </style>\  
  <h1>Orange smart display</h1>\
  <head></head>\
  <p>-----------------------------------------------------Configurations du display------------------------------------------------</p>\
  <br/>\
  <form action='config&restart'><p>Wifi SSID:  <input type='text' name='ssid' size=5> Password: <input type='text' name='password' size=5 autofocus > <input type='submit' value='Enregistre et redemarre'> </form>\
  <p>Actuellement configure : ssid: %s, Password %s</p>\
  <form action='configintensity'><p>Luminosite ((-)0-->15(+)): <input type='text' name='intensity' size=5 autofocus > <input type='submit' value='submit'></form>\
  <p>Actuellement configure : Luminosite: %s</p>\
  <form action='configSpeedDisplay'><p> Vitesse de defilement ((-)11-->0(+)): <input type='text' name='speedScroling' size=5 autofocus> <input type='submit' value='submit'></form>\
  <p>Actuellement configure : Vitesse defilement: %s</p>\
  <form action='configStartBySound'><p> Mise en veille de l'afficheur (0=Marche forcee)(1=Veille automatique) <input type='text' name='StartBySound' size=5 autofocus> <input type='submit' value='submit'></form>\
  <form action='handleconfigRSS'><p> Site web <input type='text' name='LienRss' size=30 autofocus> <input type='submit' value='submit'></form>\
  <a href='https://smart-display.kmt.orange.com/' target='_blank'>Lien page web</a>\
  <p></p>\
  <form><img src='https://upload.wikimedia.org/wikipedia/commons/c/c8/Orange_logo.svg'></form>\
  </html>"
   , bufSSID,bufPASS,bufIntens,bufspeedScroling);
   
  WebPageString =  String(charArrayScriptWebPage);
  Serial.println("Handle server");

 server.send(200, "text/html", WebPageString);

}
//////////////////////////////////////////////////////////////////////////////////////////
//                                     on Mqtt Connect                                 //
//  Description: Indique la bonne connexion au broker                                  //
//////////////////////////////////////////////////////////////////////////////////////////

void onMqttConnect(bool sessionPresent) {
  Serial.println("** Connected to the broker **");
  //  Serial.print("Session present: ");
  //  Serial.println(sessionPresent);
  //  uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
  //  Serial.print("Publishing at QoS 1, packetId: ");
  //  Serial.println(packetIdPub1);
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     on Mqtt Disconnect                               //
//  Description: Indique la bonne connexion au broker                                   //
//////////////////////////////////////////////////////////////////////////////////////////

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("** Disconnected from the broker **");
  Serial.println("**  Reconnecting to MQTT... **");
  mqttClient.connect();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     on Mqtt Subscribe                               //
//  Description: Indique la bonne souscription au topic                                 //
//////////////////////////////////////////////////////////////////////////////////////////
void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("** Subscribe acknowledged **");
  Serial.print(F("  packetId: "));
  Serial.println(packetId);
  Serial.print(F("  qos: "));
  Serial.println(qos);
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     on Mqtt Unsubscribe                               //
//  Description: Indique la bonne désouscription au topic                               //
//////////////////////////////////////////////////////////////////////////////////////////

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("** Unsubscribe acknowledged **");
  Serial.print(F("  packetId: "));
  Serial.println(packetId);
}


//////////////////////////////////////////////////////////////////////////////////////////
//                                     on Mqtt Message                                  //
//  Description: Indique la bonne reception  d'une informations                         //
//////////////////////////////////////////////////////////////////////////////////////////
void onMqttPublish(uint16_t packetId) {
  Serial.println("** Publish acknowledged **");
    Serial.print(F("  packetId: "));
    Serial.println(packetId);
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     ota Configuration                                //
//  Description: Configuration du téléversement via WIFI  OTA                           //
//////////////////////////////////////////////////////////////////////////////////////////
void otaConfiguration()
{
  // Port defaults to 8266
  //ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  //ArduinoOTA.setHostname("Smart_display");

  // No authentication by default
 // ArduinoOTA.setPassword((const char *)"a");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     Gateway Server Mode                              //
//  Description: Configuration du mode connecté à la box                                 //
//////////////////////////////////////////////////////////////////////////////////////////
void GatewayServerMode()
{

  connexionMode = "Gateway";
  server.on("/", webPageEdit); 
  server.begin();                                 // Start the server

  sprintf(result, "%3d.%3d.%1d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  
  decodedMsg = result;
  Serial.println("WebServer ready!   ");

  Serial.println(WiFi.localIP());                 // Serial monitor prints localIP

   memset(SourceIdArray,'\0',30);
  String SourceId = "SmartDisplay:" + ChipIdString;
  int SourceIdLength = SourceId.length()+1;  
  SourceId.toCharArray(SourceIdArray, SourceIdLength);

  Serial.print(F("Source: "));
  Serial.println(String(SourceIdArray));
  
  //Initialisation serveur liveobject
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(mqttHost, 1883);
  mqttClient.setKeepAlive(50).setCleanSession(false).setWill(topic_online, 2, true, "no").setCredentials(username, ApiKey).setClientId(SourceIdArray);
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();


  Serial.print(F("IP number assigned by DHCP is "));
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print(F("Local port: "));
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  //Configuration of wifi uploading
  otaConfiguration();

  server.on("/config&restart", handle_ConfigRestart);
  server.on("/configintensity", handle_Configintensity);
  server.on("/configSpeedDisplay", handle_configSpeedDisplay);
  server.on("/configStartBySound", handle_configStartBySound);
  server.on("/handleconfigRSS", handle_config_RSS);
  
  //Set timer for publisch data on liveobject(every 30s)
  t.every(30000, mqttPublish);
  sensorRead();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     Access point Mode                              //
//  Description: Configuration du mode en point d'accés quand pas de connexion WIFI   //
//////////////////////////////////////////////////////////////////////////////////////////
void ApMode()
{
   connexionMode = "AccesPoint";
  /* Set these to your desired credentials. */
  const char *ssid = "Config_smart_display";
 // const char *password = "123456789";
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid);
  server.on("/", webPageEdit); 

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/config&restart", handle_ConfigRestart);
  server.on("/configintensity", handle_Configintensity);
  server.on("/configSpeedDisplay", handle_configSpeedDisplay);
  server.on("/configStartBySound", handle_configStartBySound);
  server.on("/handleconfigRSS", handle_config_RSS);
  
   WiFi.mode(WIFI_AP);
  server.begin();
  Serial.println("HTTP server started");

  //Configuration of wifi uploading
  otaConfiguration();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                     Sensor Read                                      //
//  Description: Lecture des capteurs et stockage des données                          //
//////////////////////////////////////////////////////////////////////////////////////////
void sensorRead()
{
  unsigned long currentMillis = millis();

  
  //Condition qui évite de lire dans des petits interval --> probleme de lecture du DHT11
  
 if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

      //# define offsetHumid 30
      float humidite = 0, temperatur = 0;
      int PsAtmo = 0, tempCaf = 0, altitude = 0;
    
    
      Serial.println();
      Serial.print(F("Sensor values: "));

     
     //dht22 (with value filter)
      humidite = dht.readHumidity();
      temperatur = dht.readTemperature();

      if (isnan(temperatur))
      {
        Serial.println("Dht sensor False value");
      }
      else
      {
        Serial.println("Dht sensor Valid values");
       temp = temperatur;//Convertion float to int
       humi = humidite;//Convertion float to int
      }
      

      Serial.print(F("Temperature:"));
      Serial.print(temp);
      Serial.print(F("*C"));


      Serial.print(F(" | humidity:"));
     // humi = humidite + offsetHumid;
      Serial.print(humi);
      Serial.print("%");
    
      //Lock bmp read (avoid arduino reset if no sensor connected)
      if (!bmp.begin())
      {
        Serial.print(" | BMP180 / BMP085 introuvable ! Verifier le branchement ");
        bmplock = 1;
      }
      if ( bmplock  == 0)
      {
        Serial.print(F(" | TemperatureCaf: "));
        tempCaf = bmp.readTemperature();
        Serial.print(tempCaf);
        Serial.print(F(" *C"));
    
        Serial.print(F(" | Pression = "));
        PsAtmo = bmp.readPressure() / 100;
        Serial.print(PsAtmo);
        Serial.print(F(" hPa"));
    
        Serial.print(F(" | Altitude = "));
        altitude = bmp.readAltitude();
        altitude = altitude * -1;
        Serial.print(altitude);
        Serial.print(F(" metres"));
      }
    
    
      Serial.print(F(" | Bruit = "));
      Serial.print(maxPeakToPeak);
    
    
      //RAZ du tabeau de valeurs
      for (int i = 0 ; i < SensorNumber ; i++) //Scan de la structure pour trouver la zone à modifier
      {
        sensor [1] [i] = ' ';
      }
    
      // digital clock display of the time
      getTime();
      Serial.print(F(" | Time: "));
      Serial.print(hour());
      printDigits(minute());
      printDigits(second());
      Serial.print(F(" "));
      Serial.print(F("| Date: "));
      Serial.print(day());
      Serial.print(F("."));
      Serial.print(month());
      Serial.print(".");
      Serial.print(year());
    
      sprintf(result, "%3d.%3d.%1d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    
      Serial.print(F(" | Ip:"));
      Serial.print(String(result));
    
      Serial.print(F(" | rssi:")) ;
      Serial.print( WiFi.RSSI());
    
      Serial.print(F(" | Box:")) ;
      Serial.println(String(bufSSID));
      
      if (GetMsgFlag==1)
      {
        GetLiveObjectData();
      }
      if (GetMsgFlag == 2)
      {
        rssParser();
      }
      GetMsgFlag = GetMsgFlag + 1;

      //Enregistrement des valeurs dans le tableau
      sensor [1] [0] =  String(temp);
      sensor [1] [1] = String (humi);
      sensor [1] [2] = String(PsAtmo) ;
      sensor [1] [3] =  '"' + String ("NC") + '"' ;
      sensor [1] [4] = String(maxPeakToPeak) ;
      sensor [1] [5] = String(altitude) ;
      sensor [1] [6] = String(tempCaf) ;
      sensor [1] [7] = Msg;
      sensor [1] [8] = '"' + String(hour()) + ":" + String(minute()) + '"';
      sensor [1] [9] = '"' + String(day()) + "/" + String(month()) + "/" + String(year()) + '"';
      sensor [1] [10] = '"' + String(result) + '"' ; 
      sensor [1] [11] = '"' + String(WiFi.RSSI()) + '"' ;
      sensor [1] [12] = '"' + String(bufSSID) + '"' ;
      sensor [1] [13] = '"' + String(urlRss) + '"' ;
      sensor [1] [14] = '"' +  DispStatus + '"' ;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                    get Noise                                     //
//  Description: Lecture du bruit (microphone) et renvoie la valeurs max obtenue    //
//////////////////////////////////////////////////////////////////////////////////////////
void getNoise()
{
  const int sampleWindow = 20; // Sample window width in mS (50 mS = 20Hz)
  unsigned int sample;
  unsigned long startMillis = millis(); // Start of sample window
  int signalMax = 0;
  int signalMin = 676;
  unsigned int peakToPeak=0;

  //Annalyse du gain
  while (millis() - startMillis < sampleWindow)
  {
    sample = analogRead(A0);
    server.handleClient();                        // checks for incoming messages
    ArduinoOTA.handle();  

    if (sample < 676 )  // toss out spurious readings
    {
      if (sample > signalMax)
      {
        signalMax = sample;  // save just the max levels
      }
      else if (sample < signalMin)
      {
        signalMin = sample;  // save just the min levels
      }
    }
  }
  peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude
if (maxPeakToPeak < peakToPeak)
{
  maxPeakToPeak = peakToPeak;
}
 if (maxPeakToPeak >=100)
  {
    maxPeakToPeak = 100;
  }

  if( maxPeakToPeak > NoiseLimit)
  {
    veille =0;//Bruit donc active affichage
  }
  else
  {
    veille =1;//Pas de bruit donc n'active pas l'affichage
    TvPowerOn = false;
  }
  if ((maxPeakToPeak>wakeUpTv)&& (TvPowerOn == false))
  {
    wake_on_lan();
    TvPowerOn = true;
  }
}


time_t prevDisplay = 0; // when the digital clock was displayed


//////////////////////////////////////////////////////////////////////////////////////////
//                                    mqtt Publish                                      //
//  Description: Publication des données sur le broker                                   //
//////////////////////////////////////////////////////////////////////////////////////////
void mqttPublish()
{
   maxPeakToPeak = 0 ;//RAZ noise peak value
   GetMsgFlag =0;
  if (functionLock == 0)
  {
# define arrayLenght 1500 //max 1600

    String strucDataAndOnlineCheck = "{\"s\":\"urn:lo:nsid:smartDisplay:ChId!Data\",\"m\":\"smartDisplay_V1_0\",\"v\":{ ";
    char buf1[arrayLenght];
    char buf2[arrayLenght];


    //////////////////////////////////////////////////////////////////////////
    strucDataAndOnlineCheck.toCharArray(buf1, arrayLenght);
    strncpy(buf2, buf1, arrayLenght);

    char ChipId [sizeof(ChipIdString)+1]; 
    int cptChipId=0;

    ChipIdString.toCharArray(ChipId, sizeof(ChipIdString));   

    for (int i = 0 ; i < sizeof(ChipId) ; i++)
    {
      if (isDigit(ChipId[i]))
      {
        cptChipId = cptChipId + 1;
      }
    }

    for (int i = 0 ; i < sizeof(buf1) ; i++) //Scan de la structure pour trouver la zone à modifier
    {
      if ((buf1[i] == 'C') & (buf1[i + 1] == 'h') & (buf1[i + 2] == 'I') & (buf1[i + 3] == 'd')) //Détéction de la zone à modifier
      {

        for (int l = 0 ; l < (sizeof(buf1) - i) ; l++) //1ere étape: Ecrire l'ID dans la structure
        {
          buf2 [i + l] = ChipId[l];
        }

        for (int j = 0 ; j < 49 ; j++)//2eme étape: Ajouter le reste de la structure
        {
          buf2 [i + j + cptChipId]  = buf1 [i + j + 4];
        }
      }
    }
    strucDataAndOnlineCheck = String(buf2);  //Réintégration de la structure modifier dans ca chaine de catactere par défaut

    memset(buf1, 0, sizeof(buf1));
    memset(buf2, 0, sizeof(buf2));
    /////////////////////////////////////////////////////

    for (int h = 0 ; h < SensorNumber ; h++)
    {
      strucDataAndOnlineCheck = strucDataAndOnlineCheck + '"';
      strucDataAndOnlineCheck = strucDataAndOnlineCheck + sensor[0][h];
      strucDataAndOnlineCheck = strucDataAndOnlineCheck + '"';
      strucDataAndOnlineCheck = strucDataAndOnlineCheck + ':';
      strucDataAndOnlineCheck = strucDataAndOnlineCheck + sensor[1][h];

      if (h != SensorNumber - 1)
      {
        strucDataAndOnlineCheck = strucDataAndOnlineCheck + ",";
      }
    }

    strucDataAndOnlineCheck = strucDataAndOnlineCheck + "},";
    strucDataAndOnlineCheck.toCharArray(buf1, arrayLenght) ;


    ///////////////////////////////////////////////////////////////////////////
    String stringTag = String(buf1);
    String tags ="\"tags\":[\"erwann_Bureau\"]}";
    int tagsLength = tags.length();
    stringTag = stringTag + tags;
    
    stringTag.toCharArray(buf1, arrayLenght+tagsLength ) ;
    
    mqttClient.publish ("dev/data", 1, true, buf1) ; //Envoie des données

    Serial.print(F("Json Envoye:"));
    Serial.println(String(buf1));
    DispStatus = "No error " ;


  }

}

//////////////////////////////////////////////////////////////////////////////////////////
//                                   get Time                                            //
//  Description: Retourne le temps (date/heur)                                           //
//////////////////////////////////////////////////////////////////////////////////////////
void getTime()
{
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                   print Digits                                       //
//  Description:                                                                         //
//////////////////////////////////////////////////////////////////////////////////////////
void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(F(":"));
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(F(": "));
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}


//////////////////////////////////////////////////////////////////////////////////////////
//                                   send NTP packet                                     //
//  Description: Envoie une requete NTP au serveur pour avoir le temps                    //
//////////////////////////////////////////////////////////////////////////////////////////
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                   matrix Display                                     //
//  Description: Affiche les données sur l'afficheur                                   //
//////////////////////////////////////////////////////////////////////////////////////////
void matrixDisplay ()
{
  int lengthbufName = sensor[0] [msgScrol].length() + 1;
  int lengthbufValue = sensor[1] [msgScrol].length() + 1;
  int lengthbufUnit = sensor[2] [msgScrol].length() + 1;
  int TotalLength = lengthbufValue + lengthbufName + lengthbufUnit;
  int loopCpt = 0;

  char buf [TotalLength];

  for (int j = 0; j < TotalLength; j++)
  {
    buf[j] = ' ';
  }

  char bufName[lengthbufName];
  sensor [0] [msgScrol].toCharArray(bufName, lengthbufName);


  char bufValue[lengthbufValue];
  sensor [1] [msgScrol].toCharArray(bufValue, lengthbufValue);

  char bufCleanValue [lengthbufValue];
  int j = 0;

  //Erase the " or space into data values for display
  for ( int i = 0 ; i < lengthbufValue; i++ )
  {
    if (bufValue[i] == '"') //Avoid the " in the display msg
    {
    }
    else
    {
      bufCleanValue [j] = bufValue[i];
      j = j + 1;
    }
  }

  char bufUnit[lengthbufUnit];
  sensor [2] [msgScrol].toCharArray(bufUnit, lengthbufUnit);



  /////Liste des infos à ne pas afficher sur l'écran

  //  0:Températur ambiante
  //  1:Hygrométrie-->Ne pas afficher
  //  2:Ps atmo-->Ne pas afficher
  //  3:Luminosité -->Ne pas afficher
  //  4:Bruit-->Ne pas afficher
  //  5:Altitude -->Ne pas afficher
  //  6:Temp Cafetiere-->Ne pas afficher
  //  7:Méssage
  //  8:Heure
  //  9:Date
  //  10:IP-->Ne pas afficher
  //  11:RSSI-->Ne pas afficher
  //  12:Box-->Ne pas afficher
  //  13:SourceRss-->Ne pas afficher
  //  14:Status-->Ne pas afficher



  bool  lock = 0;
  while (lock != 1)
  {
    msgScrol =  msgScrol + 1;
    //msgScrol ==6 |
    if (msgScrol == 1 |msgScrol == 2 |msgScrol == 3 | msgScrol == 4 | msgScrol == 5 | msgScrol == 6| msgScrol == 10| msgScrol == 11| msgScrol == 12| msgScrol == 13|msgScrol ==14)
    {
      lock = 0;
    }
    else
    {
      lock = 1;
    }
  }
  if (msgScrol >= SensorNumber)
  {
    RssMatrixDisplay();//Exécute l'affichage des actus à la fin (fonction la plus lourde) 
    
    if (year() == 1970)//Resyncronisation du temps si defaut syncro
    {
    getNtpTime();
    Serial.println("Resyncro Ntp server");
    }
    msgScrol = 0;
  }

  //Buiding à the string to display (Name of value : value (unit))
  String stringbuf = String(bufName) + ':' + String(bufCleanValue) + String(bufUnit);
  
  //Rectifie les carcateres spécifiques  pour que le carctere soit lisible sur l'afficheur

         stringbuf.replace("é", "e");
         stringbuf.replace("ç", "c");
         stringbuf.replace("è", "e");
         stringbuf.replace("ç", "c");
         stringbuf.replace("l'", "l");
         stringbuf.replace("ê", "e");
         stringbuf.replace("à", "a");
         stringbuf.replace("ô", "o");
         stringbuf.replace("L'", "L");
         stringbuf.replace("À", "A");
         stringbuf.replace("’", "'");
         stringbuf.replace("–", "-");
         stringbuf.replace("É", "E");
         stringbuf.replace("?", " ");
         stringbuf.replace("Â", " ");
         stringbuf.replace("î", "i");
  
  stringbuf.toCharArray( buf, TotalLength);


 

  int OffsetSizeTotalLength = TotalLength - 2; //Diminution des incéments inutiles (réduit le temp entre chaque méssage)

  for ( int i = 0 ; i < width * OffsetSizeTotalLength + matrix.width() - 0 - spacer; i++ )
  {
    if (refresh == 1) i = 0;
    refresh = 0;
    matrix.fillScreen(LOW);
    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // center the text vertically

    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if ( letter < sizeof(buf) )
      {
        matrix.drawChar(x, y,  buf[letter], HIGH, LOW, 1);

      }
      letter--;
      x -= width;
    }
    //(rotation des afficheurs)
    matrix.setRotation(0, 3);
    matrix.setRotation(1, 3);
    matrix.setRotation(2, 3);
    matrix.setRotation(3, 3);

    getNoise();
    matrix.write(); // Send bitmap to display
    server.handleClient();                        // checks for incoming messages
    ArduinoOTA.handle();                          // checks for incoming uptdate
    delay(wait);
  }

}


//////////////////////////////////////////////////////////////////////////////////////////
//                                   Display Access Point Msg                           //
//  Description: Affiche le méssage de déconnexion sur l'afficheur                      //
//////////////////////////////////////////////////////////////////////////////////////////
void DisplayAccesPointMsg()
  {
    String DeconexionMsg ="Display is disconnected from wifi, please set up wifi settings on 192.168.4.1! ";
    int DeconexionMsgLength=DeconexionMsg.length();
    char DeconexionMsgArray[DeconexionMsgLength];
    DeconexionMsg.toCharArray( DeconexionMsgArray, DeconexionMsgLength);
    
      for ( int i = 0 ; i < width * DeconexionMsgLength + matrix.width() - 0 - spacer; i++ )
      {
        if (refresh == 1) i = 0;
        refresh = 0;
        matrix.fillScreen(LOW);
        int letter = i / width;
        int x = (matrix.width() - 1) - i % width;
        int y = (matrix.height() - 8) / 2; // center the text vertically
    
        while ( x + width - spacer >= 0 && letter >= 0 ) {
          if ( letter < sizeof(DeconexionMsgArray) )
          {
            matrix.drawChar(x, y,  DeconexionMsgArray[letter], HIGH, LOW, 1);
            server.handleClient();                        // checks for incoming messages
            ArduinoOTA.handle();                          // checks for incoming uptdate
          }
          letter--;
          x -= width;
        }
        //(rotation des afficheurs)
        matrix.setRotation(0, 3);
        matrix.setRotation(1, 3);
        matrix.setRotation(2, 3);
        matrix.setRotation(3, 3);
        
        matrix.write(); // Send bitmap to display
        delay(wait+20);
      }
  }
  
//////////////////////////////////////////////////////////////////////////////////////////
//                                   Display Access Point Msg                           //
//  Description: Affiche le méssage d'initialisation sur l'afficheur                     //
//////////////////////////////////////////////////////////////////////////////////////////
     void InitMatrixDisplay()
  {
     String InitMatrix ="Init..." ;
    int InitMatrixLength=InitMatrix.length()+1;
    char InitMatrixArray[InitMatrixLength];
   InitMatrix.toCharArray( InitMatrixArray, InitMatrixLength);
    
      for ( int i = 0 ; i < width * InitMatrixLength + matrix.width() - 0 - spacer; i++ )
      {
        if (refresh == 1) i = 0;
        refresh = 0;
        matrix.fillScreen(LOW);
        int letter = i / width;
        int x = (matrix.width() - 1) - i % width;
        int y = (matrix.height() - 8) / 2; // center the text vertically
    
        while ( x + width - spacer >= 0 && letter >= 0 ) {
          if ( letter < sizeof(InitMatrixArray) )
          {
            matrix.drawChar(x, y,  InitMatrixArray[letter], HIGH, LOW, 1);
            server.handleClient();     
          }
          letter--;
          x -= width;
        }
        //(rotation des afficheurs)
        matrix.setRotation(0, 3);
        matrix.setRotation(1, 3);
        matrix.setRotation(2, 3);
        matrix.setRotation(3, 3);
        
        matrix.write(); // Send bitmap to display
        delay(wait+20);
      }
  }
////////////////////////////////////////////////////////////////////////////
//                                Get Live Object Data                    //
//  Description: Cherche les données sous format json sur Live Obect      //
///////////////////////////////////////////////////////////////////////////

void GetLiveObjectData()
{
  bool MsgFail = false;
  
    // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print(F("connecting to "));
  Serial.println(host);
  
  if (!client.connect(host, httpsPort)) //Si message non reçus
  {
    Serial.println("connection failed");
    MsgFail = true;
  }
  else//Si message reçus
  {
         MsgFail = false;
         
        if (client.verify(fingerprint, host)) 
        {
          Serial.println("certificate matches");
        }
        else
        {
          Serial.println("certificate doesn't match");
        }
      
         client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                       "Host: " + host + "\r\n" +
                       "User-Agent:"+ UserAgent +"\r\n"  +
                       "X-API-KEY:"+ ApiKey + "\r\n" + 
                       "Connection: close\r\n\r\n");
                        
        String line= "";
        delay(500);
        while(client.available())
        {
           line += client.readStringUntil('\r');
        }
        
        int stringLength = line.length();
        
        char jsonAray[1000] = "";
        int cpt = 0;
        bool jsonlock = 0;
      
          for(int i=0;i<stringLength;i++)
        {
      
          //si début de json, on commence à écrire dans le tableau
          if((line[i] =='[') &&  (line[i+2] ='{') )
          {
            jsonlock = 1;
            i = i +1;
          }
          
          //si fin de json, on arrete de écrire dans le tableau
          if((line[i] ==']') &&  (line[i+1] =='\n'))
          {
            jsonlock = 0;
          }
      
          //Condition d'écriture dans le tableau
          if (jsonlock == 1)
          {
            jsonAray[cpt] = line [i];
            cpt = cpt +1;
          }
        }
        char jsonArayOK [cpt];
        memset(jsonArayOK,'\0',cpt);
        
        int h = 0;
        
         for(int i=0;i<cpt;i++)
        {
          if ((jsonAray[i] == '\n') || (jsonAray[i] == '\r'  ))
          {
            //Suprime les caracteres indésirables
          }
          else
          {
            jsonArayOK[h] = jsonAray[i];
            h = h+1;
          }
        }
        
        //Convert local char array data to global data string
        LiveObjectData = String(jsonArayOK);
  }
  
  Serial.print(F("Live objects json: "));
  Serial.println(LiveObjectData);
  LiveObjectDataParser(MsgFail);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                           Live Object Data Parser                                                 //
//Selectionne la températur cafetiere et le bruit en salle de créativité (permet de notifier la présence de persone ou de café prêt) //
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LiveObjectDataParser(bool MsgFail)
{
    Msg = ' ';
    if(MsgFail ==true)//Si aucun message
    {
      Msg = "\"No message\"";
    }
        
    else//Si un message est en attente
    {
    char MsgArray[1000];
    int Jsonlength = LiveObjectData.length();
    char jsonArayOK[Jsonlength]; 

    LiveObjectData.toCharArray(jsonArayOK, Jsonlength);   
    memset(MsgArray,'\0',1000);
       for(int i=0;i<Jsonlength;i++)
     { 
       if(jsonArayOK[i]=='M'&& jsonArayOK[i+1]=='s'&& jsonArayOK[i+2]=='g'&& jsonArayOK[i+3]=='"'&& jsonArayOK[i+4]==' '&& jsonArayOK[i+5]==':'&& jsonArayOK[i+6]==' ')
       {   
          Serial.println("Message match");
          int j = 0;
          
          while(jsonArayOK[i+j+6] !='}')
          {
            MsgArray[j]=jsonArayOK[i+6+j];
            j=j+1;
          }
       }  
     }
      Msg = String(MsgArray);
    }
    Msg.replace("  ", "\0");
    Serial.print(F("Message string: "));
    Serial.println(Msg);
} 

//////////////////////////////////////////////////////////////////////////////////////////
//                                     Wake on Lan                                      //
//  Description: Allume la Tv via le réseau                                             //
//////////////////////////////////////////////////////////////////////////////////////////

  void wake_on_lan()
  {
        Serial.println("Sending WOL Packet to TV salle creativite");
        DispStatus = "Commande allumage TV";
        WakeOnLan::sendWOL(computer_ip, Udp, mac, sizeof mac);
  }

  //////////////////////////////////////////////////////////////////////////////////////////
//                                     Rss Parser                                       //
//  Description: cherche et traite les données rss                                      //
//////////////////////////////////////////////////////////////////////////////////////////

void rssParser()
{
    Serial.print(F("connecting to "));
    Serial.println(hostRss);
    
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(hostRss, httpPort)) 
    {
      Serial.println("connection failed");
      
      for(int i=0 ; i<nbRssData ; i++)
      {
            RssTiltle[i]="No news";
  //         RssDescrption[i] = "No news";
      }
    }
    else
    {
        Serial.print("Requesting URL: ");
        Serial.println(urlRss);
        
        // This will send the request to the server
        client.print(String("GET ") + urlRss + " HTTP/1.1\r\n" +
                     "Host: " + hostRss + "\r\n" + 
                     "Connection: close\r\n\r\n");
                     
        delay(500);
                       
        String line= "";
        bool TitleLock = false;
        int cptTitle= 0;
//        int cptDescription = 1;//Offset des description pour que le titre soit en concordence avec la description 
        String compar = "Server";
      
        while(client.available())//Data reading from rss 
        {
             line = client.readStringUntil('\n');
            if(cptTitle < MaxRssData)
            {
             int lineLength = line.length();
                  
              if (lineLength > 100)
              {
                lineLength = 100;
              }
              
             char lineAray[lineLength];
             memset(lineAray,'\0',lineLength);
        
               line.toCharArray(lineAray, lineLength) ;
        
        
               if (strstr(lineAray, "<title>")) 
               {
                //Serial.println("title match");
                RssTiltle[cptTitle] = line;
                cptTitle = cptTitle+1;
               }
               
///////////////////////////////////////////////////////// description Not used               
//                if (strstr(lineAray, "<description>")) 
//               {
//               // Serial.println("description match");
//                RssDescrption[cptDescription] = line;
//                cptDescription = cptDescription+1;
//               }
             }
        }

      //Serial.println("closing connection");
       nbRssData = cptTitle;
      
      //Fonction d'amélioration des chaines de caracteres (titre)
      for(int i=0 ; i<cptTitle ; i++)
      {
       RssTiltle[i].replace("<title>", "\0");
       RssTiltle[i].replace("</title>", "\0");
       RssTiltle[i].replace("é", "e");
       RssTiltle[i].replace("ç", "c");
       RssTiltle[i].replace("è", "e");
       RssTiltle[i].replace("ç", "c");
       RssTiltle[i].replace("l'", "l");
       RssTiltle[i].replace("ê", "e");
       RssTiltle[i].replace("à", "a");
       RssTiltle[i].replace("ô", "o");
       RssTiltle[i].replace("L'", "L");
       RssTiltle[i].replace("À", "A");
       RssTiltle[i].replace("’", "'");
       RssTiltle[i].replace("–", "-");
       RssTiltle[i].replace("É", "E");
       RssTiltle[i].replace("?", " ");
       RssTiltle[i].replace("Â", " ");
       RssTiltle[i].replace("î", "i");
       
       RssTiltle[i].remove(0, 6); //Supression des espaces au début de la chaine de caractere
      
      }
      
//      //Fonction d'amélioration des chaines de caracteres (description)
//      for(int i=0 ; i<cptDescription ; i++)
//      {
//       RssDescrption[i].replace("<description>", "\0");
//       RssDescrption[i].replace("</description>", "\0");
//       RssDescrption[i].replace("é", "e");
//       RssDescrption[i].replace("è", "e");
//       RssDescrption[i].replace("ç", "c");
//       RssDescrption[i].replace("l'", "l");
//       RssDescrption[i].replace("ê", "e");
//       RssDescrption[i].replace("à", "a");
//       RssDescrption[i].replace("ô", "o");
//       RssDescrption[i].replace("L'", "L");
//       RssDescrption[i].replace("À", "A");
//       RssDescrption[i].replace("’", "'");
//       RssDescrption[i].replace("–", "-");
//       RssDescrption[i].replace("É", "E");
//       RssDescrption[i].replace("?", " ");
//       RssDescrption[i].replace("Â", " ");
//    
//       RssDescrption[i].remove(0, 6); //Supression des espaces au début de la chaine de caractere
//      
//      }
      
      Serial.print("nb Title: ");
      Serial.println(cptTitle);
      
//      Serial.print("nb Description: ");
//      Serial.println(cptDescription);
      
      //Fonction pour montrer les données rss
      
      for(int i=0 ; i<cptTitle ; i++)
      {
      Serial.print(F("T "));
      Serial.print(i);
      Serial.print(F(" :"));
      Serial.println(RssTiltle[i]);
    
//      Serial.print("D ");
//      Serial.print(i);
//      Serial.print(" :");
//      Serial.println(RssDescrption[i]);
//      Serial.println();
//      Serial.println();
      }
   }
}
//////////////////////////////////////////////////////////////////////////////////////////
//                                     Rss matrix display                               //
//  Description: Affiche les données Rss sur le display                                 //
//////////////////////////////////////////////////////////////////////////////////////////

  void RssMatrixDisplay()
  {
    String MsgRss ="Actu: " + RssTiltle[PointerDisplayRss] ;
    int MsgRssLength=MsgRss.length()+1;
    char MsgRssArray[MsgRssLength];
    MsgRss.toCharArray( MsgRssArray, MsgRssLength);
    
      for ( int i = 0 ; i < width * MsgRssLength + matrix.width() - 0 - spacer; i++ )
      {
        if (refresh == 1) i = 0;
        refresh = 0;
        matrix.fillScreen(LOW);
        int letter = i / width;
        int x = (matrix.width() - 1) - i % width;
        int y = (matrix.height() - 8) / 2; // center the text vertically
    
        while ( x + width - spacer >= 0 && letter >= 0 ) {
          if ( letter < sizeof(MsgRssArray) )
          {
            matrix.drawChar(x, y,  MsgRssArray[letter], HIGH, LOW, 1);
            server.handleClient();                        // checks for incoming messages
            ArduinoOTA.handle();                          // checks for incoming uptdate
          }
          letter--;
          x -= width;
        }
        //(rotation des afficheurs)
        matrix.setRotation(0, 3);
        matrix.setRotation(1, 3);
        matrix.setRotation(2, 3);
        matrix.setRotation(3, 3);

        getNoise();
        matrix.write(); // Send bitmap to display
        delay(wait);
      }
      PointerDisplayRss =PointerDisplayRss+1;
      if (PointerDisplayRss >= nbRssData)
      {
        PointerDisplayRss =2;
      }
  }
//////////////////////////////////////////////////////////////////////////////////////////
//                                   Ip Display                                          //
//  Description: Affiche l'adresse IP sur l'afficheur au début du fonctionement          //
//////////////////////////////////////////////////////////////////////////////////////////
 void IpDisplay()
  {
     String InitMatrix ="IP:"+ String(result) ;
    int InitMatrixLength=InitMatrix.length()+1;
    char InitMatrixArray[InitMatrixLength];
   InitMatrix.toCharArray( InitMatrixArray, InitMatrixLength);
    
      for ( int i = 0 ; i < width * InitMatrixLength + matrix.width() - 0 - spacer; i++ )
      {
        if (refresh == 1) i = 0;
        refresh = 0;
        matrix.fillScreen(LOW);
        int letter = i / width;
        int x = (matrix.width() - 1) - i % width;
        int y = (matrix.height() - 8) / 2; // center the text vertically
    
        while ( x + width - spacer >= 0 && letter >= 0 ) {
          if ( letter < sizeof(InitMatrixArray) )
          {
            matrix.drawChar(x, y,  InitMatrixArray[letter], HIGH, LOW, 1);
          }
          letter--;
          x -= width;
        }
        //(rotation des afficheurs)
        matrix.setRotation(0, 3);
        matrix.setRotation(1, 3);
        matrix.setRotation(2, 3);
        matrix.setRotation(3, 3);
        
        matrix.write(); // Send bitmap to display
        delay(wait+20);
      }
  }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*                                                                                      SETUP                                                               */
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);                           // full speed to monitor
  dht.begin();

  ChipIdString = String(ESP.getChipId());               //Get the chip id
  
  EEPROM.begin(512);

  // Adjust to your own needs (à configurer selon la taille du nombre de matrcie)
  matrix.setPosition(0, 3, 0); // The first display is at <0, 7>
  matrix.setPosition(1, 2, 0); // The second display is at <1, 0>
  matrix.setPosition(2, 1, 0); // The third display is at <2, 0>
  matrix.setPosition(3, 0, 0); // And the last display is at <3, 0>

  //eeprom storage:(d/ ;     ssid     ; / ;     pasword ; /)


  int p = 0;
  int h = 0;
  int g = 0;
  int j = 0;

  //read and Store data from eepom
  for (int i = -1; i <= 99; i++)
  {
    if (EEPROM.read(i) == '/')
    {
      if (p == 2)
      {
        i = 99;
      }
      else
      {
        p = p + 1;
      }
    }
    if (p == 1)
    {
      if (EEPROM.read(i) == '/')
      {

      }
      else
      {
        bufSSID[g] = EEPROM.read(i);
        g = g + 1;
      }

    }
    else if (p == 2)
    {
      if (EEPROM.read(i) == '/')
      {

      }
      else
      {
        bufPASS [h] = EEPROM.read(i);
        h = h + 1;
      }
    }
  }
  
  for (int i = h-1; i <= 99; i++)
{
  bufPASS [i] = '\0';
}

  Serial.print(F("Ssid conect: "));
  Serial.println(String(bufSSID));
  
  Serial.print(F("Pasword conect: "));
  Serial.println(String(bufPASS));

  //Read ssid and wifi pasword in eeprom
  bufIntens[0] = EEPROM.read(100);
  bufIntens[1] = EEPROM.read(101);

  //Read speed scroling in eeprom

  bufspeedScroling[0] = EEPROM.read(110);
  bufspeedScroling[1] = EEPROM.read(111);
  bufspeedScroling[2] = EEPROM.read(112);

  //Read StartBySound flag in eeprom
  lockStartBySound  = EEPROM.read(120);



char urlRssNonParser [100];
memset(urlRssNonParser,'\0',100);
memset(hostRss,'\0',50);
memset(urlRss,'\0',50);
bool UrlparserFlag = false;
int cptRss = 0; 


 
  for (int i = 121; i <= 220; i++)
{
  urlRssNonParser [i-121] = EEPROM.read(i);
}

String(urlRssNonParser).toCharArray(urlRss, 50);
 Serial.print(F("url Rss before parser: "));
 Serial.println(String(urlRssNonParser));
 
  Serial.print(F("urlRss: "));
  Serial.println(String(urlRss));

 //URL parser
  for (int i = 0; i <= 100; i++)
{
  
  if (urlRssNonParser[i] == '/' && urlRssNonParser[i+1] == '/')
  {
    //Serial.println("Url Rss parser begin..");
    UrlparserFlag = true;
    i=i+2;
  }
 
  if(UrlparserFlag == true)
  {
     if (urlRssNonParser[i] == '/')
     {
      UrlparserFlag = false;
      //Serial.println("Url Rss parser end..");
     }
     else
     {
    hostRss[cptRss] = urlRssNonParser[i];
    cptRss = cptRss+1;
   // Serial.println(hostRss[cptRss]);
     }
  }
}

  Serial.print(F("Host Rss: "));
  Serial.println(String(hostRss));
  

  dispInstensity = atoi(bufIntens);
  Serial.print(F("Int Disp: "));
  Serial.println(dispInstensity);
  matrix.setIntensity(dispInstensity); // Use a value between 0 and 15 for brightness
  InitMatrixDisplay();
  if (atoi(bufspeedScroling) >= 0 & atoi(bufspeedScroling) <= 11)
  {
    wait = atoi(bufspeedScroling);
  }
  Serial.print(F("Speed scroling: "));
  Serial.println(wait);

  int cpt = 0;
  bool ExitWhile = 0;

  WiFi.begin(bufSSID, bufPASS);                         // Connect to WiFi network
  while ( ExitWhile == 0 )
  { // Wait for connection

    delay(1000);
    Serial.print(F("Connexion Try: "));
    Serial.print(cpt);
    Serial.println("/20");
    cpt = cpt +  1;

    if (WiFi.status() == WL_CONNECTED)
    {
      functionLock = 1;
      Serial.println("Gateway connexion OK");
      GatewayServerMode();
      IpDisplay();
      functionLock = 0;
      ExitWhile = 1;
      Serial.println("Unlocked");
    }

    if (cpt > secondeConexionTry) //(After 10s, Ap conexion mode)
    {
      Serial.println("Gateway connexion fail");
      ApMode();
      Serial.println("Mode: Access point");
      ExitWhile = 1;
      functionLock = 1;
      Serial.println("locked Mode");
    }
  }

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                          Loop                                                                        //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop(void)
{
  if(connexionMode== "Gateway")
  {
    sensorRead();
      if (veille == 0 | lockStartBySound == 0)//Marche de l'écran
      {
        matrixDisplay();
        server.handleClient();          
      }
      else//Vielle de l'écran
      { 
       getNoise();
       server.handleClient();   
      }
      t.update();
  }
  else
  {
       DisplayAccesPointMsg();
       loopCpt = loopCpt +1;
       
       //wait (5min) before restart (1 loop = 10sec)
       if(loopCpt == 10)
       {
          ESP.restart();
       }
  }
}



