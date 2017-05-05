///////////////////////////////////////////////////////////////////////
//  Project Tilte:  Orange Web matrix display (Iot project)          //
//  Version: 1.0                                                     //
//  Editor: Erwann Caroff                                            //
//  Last uptdate: 16/03/20017                                        //
///////////////////////////////////////////////////////////////////////
/*
  NodeMCU pins:

  -->Matrix 4 modul:
  .CLK -> D5(14)
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
#include <ArduinoOTA.h>
#include "Timer.h"
Timer t;
Timer t1;


//Sensor lib
#include <DHT11.h>
#include <Adafruit_Sensor.h>        //Librairie pour le capteur KY 052 (présion atmosphérique) 
#include <Wire.h>;
#include <Adafruit_BMP085.h>;
Adafruit_BMP085 bmp;

//Mqtt librarie
#include <ESPAsyncTCP.h>
#include <AsyncMqttClient.h>

//WOL librarie
#include <WakeOnLan.h>

WiFiClient client;

// NTP Servers:(date/heur)
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
ESP8266WebServer server(80);                             // HTTP server will listen at port 80

//Capteur DHT
int pin = 2;
DHT11 dht11(pin);
// ******************* String form to sent to the client-browser ************************************
char form[3000] = "";

long period;
int offset = 1, refresh = 0;
int pinCS = 0; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4;//Nombre de matrice de Led (Horizontal)
int numberOfVerticalDisplays = 1;//Nombre de matrice de Led (Vertical)
String decodedMsg;
int a, b, c = 0;
int msgScrol = 0;
char ChipId [20];                               //Chip ID array
int cptChipId = 0;
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
unsigned int maxPeakToPeak = 0;   // peak-to-peak level
// constants won't change :
const long interval = 1500;  
unsigned long previousMillis = 0;   

////   WOL ///////////////////////////////////////////////////////////////
bool TvPowerOn = false;
/**
 * The target IP address to send the magic packet to.
 */
IPAddress computer_ip(192, 168, 1, 40); 

/**
 * The targets MAC address to send the packet to "TV salle de créativité"
 */
byte mac[] = {0x40,0x16,0x3B,0xDB,0x61,0x90};
void sendWOL();
#define wakeUpTv 10
//////////////////////////////////////////////////////////////////////////

//Rss parser//////////////////////////////////////////////////////////////
const char* hostRss = "www.france24.com";
// We now create a URI for the request
String urlRss = "http://www.france24.com/fr/france/rss";

String RssTiltle[100];
String RssDescrption[100];
int  MaxRssData =  0;
int PointerDisplayRss =2;
String RssTiltleClear[100];
///////////////////////////////////////////////////////////////////////////

#define NoiseLimit 4 //Noise limit to standby mode
#define secondeConexionTry 10
#define SeuilleMarcheCafetiere 30
#define SensorNumber 14
//La luminosité et l'altitude ne sont pas utilisé, cependant les varaible sont inscites afin de garder une compatiboilité avec la page web  utiliser par la webradio
String sensor[][SensorNumber] =
{
  {"Temp", "Hygro", "PsAt", "Lum", "Bruit", "Alt", "tempCaf", "Message", "Heure", "Date", "IP","RSSI","Box","SourceRss"},
  {"Nc", "Nc", "Nc", "Nc", "Nc", "Nc", "\"Nc\"", "Nc", "Nc", "Nc", "Nc","Nc","Nc","Nc"},
  {"oC", "%", "hPa", "", "%", "m", "oC", "", "", "", "", "","",""}
};

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

int wait = 25; // In milliseconds
int spacer = 2;
int width = 5 + spacer; // Espaces entre caracteres

/*
//  handles the messages coming from the webbrowser, restores a few special characters and
//  constructs the strings that can be sent to the oled display
*/
void handle_msg() {
  matrix.fillScreen(LOW);
  webPageEdit();
  refresh = 1;
  // Display msg on Oled
  String msg = server.arg("msg");
  Serial.println("Msg: ");
  Serial.println(msg);

  Serial.println(msg);
  decodedMsg = msg;
//  // Restore special characters that are misformed to %char by the client browser
  decodedMsg.replace("+", " ");
  decodedMsg.replace("%21", "!");
  decodedMsg.replace("%22", "");
  decodedMsg.replace("%23", "#");
  decodedMsg.replace("%24", "$");
  decodedMsg.replace("%25", "%");
  decodedMsg.replace("%26", "&");
  decodedMsg.replace("%27", "'");
  decodedMsg.replace("%28", "(");
  decodedMsg.replace("%29", ")");
  decodedMsg.replace("%2A", "*");
  decodedMsg.replace("%2B", "+");
  decodedMsg.replace("%2C", ",");
  decodedMsg.replace("%2F", "/");
  decodedMsg.replace("%3A", ":");
  decodedMsg.replace("%3B", ";");
  decodedMsg.replace("%3C", "<");
  decodedMsg.replace("%3D", "=");
  decodedMsg.replace("%3E", ">");
  decodedMsg.replace("%3F", "?");
  decodedMsg.replace("%40", "@");
  decodedMsg.replace("é", "e");
  decodedMsg.replace("è", "e");
  decodedMsg.replace("ç", "c");
  decodedMsg.replace("l'", "l");
  decodedMsg.replace("ê", "e");
  decodedMsg.replace("à", "a");
  decodedMsg.replace("ô", "o");
  decodedMsg.replace("L'", "L");
  decodedMsg.replace("À", "A");
  decodedMsg.replace("’", "'");
  decodedMsg.replace("–", "-");
  decodedMsg.replace("É", "E");
  decodedMsg.replace("?", " ");
  decodedMsg.replace("Â", " ");

}

void handle_ConfigRestart()
{
  webPageEdit();
  String ssid = server.arg("ssid");
  Serial.print("Ssid: ");
  Serial.println(ssid);
  String password = server.arg("password");
  Serial.print("Password: ");
  Serial.println(password);

  const int  ssidlength = ssid.length();
  const int  passwordlength = password.length();
  const int totalconfigLength = ssidlength  + passwordlength + 5;

  String stringConfig =  "d/" + ssid + "/" + password + "/" ;
  char arrayConfig [totalconfigLength];

  Serial.print("string config: ");
  Serial.println(stringConfig);

  stringConfig.toCharArray(arrayConfig, totalconfigLength);

  Serial.print("array config: ");
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

void handle_Configintensity()
{
  webPageEdit();
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
    
   //memcpy(bufIntens, buffintensity , sizeof(bufIntens ));
   webPageEdit();

    EEPROM.commit();
  }
}


void handle_configSpeedDisplay()
{
  webPageEdit();
  int speedScroling = server.arg("speedScroling").toInt();

  String speedScrolingString = String(speedScroling);
  int speedScrolingLength = speedScrolingString.length();
  //bufspeedScroling= ' ';
  speedScrolingString.toCharArray(bufspeedScroling, speedScrolingLength + 1);
  Serial.print("speed:");
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
    webPageEdit();
    EEPROM.commit();
  }
}

void handle_configStartBySound()
{
  webPageEdit();
  int StartBySound = server.arg("StartBySound").toInt();

  //Value Filter
  if (StartBySound == 1 | StartBySound == 0)
  {
    lockStartBySound = StartBySound ; 
    EEPROM.write(120, StartBySound);
    EEPROM.commit();
  }


}

void webPageEdit()
{
// <head><meta http-equiv='refresh' content='5'>
  sprintf ( form,
   "<html>\
  <style>h1{ color: orange;} </style>\  
  <h1>Orange smart display</h1>\
  <head></head>\
  <br/>\
  <br/>\
  <form action='msg'><p>Entrez un Message <input type='text' name='msg' size=70 > <input type='submit' value='Submit' ></form>\
  <br/>\
  <br/>\
  <p>-----------------------------------------------------Configurations du display------------------------------------------------</p>\
  <br/>\
  <form action='config&restart'><p>Wifi SSID:  <input type='text' name='ssid' size=5> Password: <input type='text' name='password' size=5 autofocus > <input type='submit' value='Enregistre et redemarre'> </form>\
  <p>Actuellement configure : ssid: %s, Password %s</p>\
  <form action='configintensity'><p>Luminosite ((-)0-->15(+)): <input type='text' name='intensity' size=5 autofocus > <input type='submit' value='submit'></form>\
  <p>Actuellement configure : Luminosite: %s</p>\
  <form action='configSpeedDisplay'><p> Vitesse de defilement ((-)11-->0(+)): <input type='text' name='speedScroling' size=5 autofocus> <input type='submit' value='submit'></form>\
  <p>Actuellement configure : Vitesse defilement: %s</p>\
  <form action='configStartBySound'><p> Mise en veille de l'afficheur (0=Marche forcee)(1=Veille automatique) <input type='text' name='StartBySound' size=5 autofocus> <input type='submit' value='submit'></form>\
  <a href='https://smart-display.kmt.orange.com/' target='_blank'>Lien page web</a>\
  <p></p>\
  <form><img src='https://upload.wikimedia.org/wikipedia/commons/c/c8/Orange_logo.svg'></form>\
  </html>"
   , bufSSID,bufPASS,bufIntens,bufspeedScroling);


  server.on("/", []() {
 server.send(200, "text/html", form);
  });
  server.send(200, "text/html", form);
//  Serial.print("ssid : ");
//  Serial.println(String(bufSSID));
//  
//  Serial.print("form1: ");
//  Serial.println(form);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("** Connected to the broker **");
  //  Serial.print("Session present: ");
  //  Serial.println(sessionPresent);
  //  uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
  //  Serial.print("Publishing at QoS 1, packetId: ");
  //  Serial.println(packetIdPub1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("** Disconnected from the broker **");
  Serial.println("**  Reconnecting to MQTT... **");
  mqttClient.connect();
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("** Subscribe acknowledged **");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("** Unsubscribe acknowledged **");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("** Publish received **");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("** Publish acknowledged **");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void otaConfiguration()
{
  // Port defaults to 8266
  //ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  //ArduinoOTA.setHostname("Smart_display");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"123");

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


void GatewayServerMode()
{

  connexionMode = "Gateway";
  server.on("/", []() {
    webPageEdit();
  });


  server.on("/msg", handle_msg);                  // And as regular external functions:
  server.begin();                                 // Start the server

  sprintf(result, "%3d.%3d.%1d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  
  decodedMsg = "Connectez-vous au site: https://smart-display.kmt.orange.com/";
  Serial.println("WebServer ready!   ");

  Serial.println(WiFi.localIP());                 // Serial monitor prints localIP

  //Initialisation serveur liveobject
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer("liveobjects.orange-business.com", 1883);


  memset(SourceIdArray,'\0',30);
  String SourceId = "SmartDisplay:" + String(ChipId);
  int SourceIdLength = SourceId.length()+1;  
  SourceId.toCharArray(SourceIdArray, SourceIdLength);
  
  
  Serial.print("Source: ");
  Serial.println(String(SourceIdArray));
  mqttClient.setKeepAlive(20).setCleanSession(false).setWill("dev/data", 0, true, "no").setCredentials("json+device", "7ef7949fb37c454aa3087f63c276573d").setClientId(SourceIdArray);
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();


  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  //Configuration of wifi uploading
  otaConfiguration();

  server.on("/msg", handle_msg);
  server.on("/config&restart", handle_ConfigRestart);
  server.on("/configintensity", handle_Configintensity);
  server.on("/configSpeedDisplay", handle_configSpeedDisplay);
  server.on("/configStartBySound", handle_configStartBySound);
  
  //Set timer for publisch data on liveobject(every 30s)
  t.every(30000, mqttPublish);
  
  sensorRead();
}

void ApMode()
{
   connexionMode = "AccesPoint";
  /* Set these to your desired credentials. */
  const char *ssid = "OrangetDisp";
 // const char *password = "123456789";
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid);

  server.on("/", []() {
    webPageEdit();
  });

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/msg", handle_msg);
  server.on("/config&restart", handle_ConfigRestart);
  server.on("/configintensity", handle_Configintensity);
  server.on("/configSpeedDisplay", handle_configSpeedDisplay);
  server.on("/configStartBySound", handle_configStartBySound);
   WiFi.mode(WIFI_AP);
  server.begin();
  Serial.println("HTTP server started");

  //Configuration of wifi uploading
  otaConfiguration();
}


// Déclenche les actions à la réception d'un message
// D'après http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
void callback(char* topic, byte* payload, unsigned int length) {

  int i = 0;

  Serial.println("Message recu =>  topic: " + String(topic));
  Serial.print(" | longueur: " + String(length, DEC));

  // create character buffer with ending null terminator (string)
  for (i = 0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';

  String msgString = String(message_buff);

  Serial.println("Payload: " + msgString);

  if ( msgString == "ON" ) {
    //Do something
  } else {
    //Do something
  }
}
////////////////////////////////////////////////////////////          Sensor Read           ////////////////////////////////////
void sensorRead()
{
  unsigned long currentMillis = millis();

  
  //Condition qui évite de lire dans des petits interval --> probleme de lecture du DHT11
  
 if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

      # define offsetHumid 30
      float humidite = 0, temperatur = 0;
      int temp = 0, humi = 0, PsAtmo = 0, tempCaf = 0, altitude = 0;
    
    
      Serial.println();
      Serial.print("Sensor values: ");
    
      dht11.read(humidite, temperatur);
      temp = temperatur;//Convertion float to int
      Serial.print("Temperature:");
      Serial.print(temp);
      Serial.print("*C");
    
      Serial.print(" | humidity:");
      humi = humidite + offsetHumid;
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
        Serial.print(" | TemperatureCaf: ");
        tempCaf = bmp.readTemperature();
        Serial.print(tempCaf);
        Serial.print(" *C");
    
        Serial.print(" | Pression = ");
        PsAtmo = bmp.readPressure() / 100;
        Serial.print(PsAtmo);
        Serial.print(" hPa");
    
        Serial.print(" | Altitude = ");
        altitude = bmp.readAltitude();
        altitude = altitude * -1;
        Serial.print(altitude);
        Serial.print(" metres");
      }
    
    
      Serial.print(" | Bruit = ");
      Serial.print(maxPeakToPeak);
    
    
      //RAZ du tabeau de valeurs
      for (int i = 0 ; i < SensorNumber ; i++) //Scan de la structure pour trouver la zone à modifier
      {
        sensor [1] [i] = ' ';
      }
    
    
      // digital clock display of the time
      getTime();
      Serial.print(" | Time: ");
      Serial.print(hour());
      printDigits(minute());
      printDigits(second());
      Serial.print(" ");
      Serial.print("| Date: ");
      Serial.print(day());
      Serial.print(".");
      Serial.print(month());
      Serial.print(".");
      Serial.print(year());
    
      sprintf(result, "%3d.%3d.%1d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    
      Serial.print(" | Ip:");
      Serial.print(String(result));
    
      Serial.print(" | rssi:") ;
      Serial.print( WiFi.RSSI());
    
       Serial.print(" | Box:") ;
      Serial.println(String(bufSSID));
//      RssTiltle[i]
//  RssDescrption[i]
    
      //Enregistrement des valeurs dans le tableau
      sensor [1] [0] =  String(temp);
      sensor [1] [1] = String (humi);
      sensor [1] [2] = String(PsAtmo) ;
      sensor [1] [3] =  '"' + String ("NC") + '"' ;
      sensor [1] [4] = String(maxPeakToPeak) ;
      sensor [1] [5] = String(altitude) ;
      sensor [1] [6] = String(tempCaf) ;
      sensor [1] [7] = '"' + String(decodedMsg) + '"';
      sensor [1] [8] = '"' + String(hour()) + ":" + String(minute()) + '"';
      sensor [1] [9] = '"' + String(day()) + "/" + String(month()) + "/" + String(year()) + '"';
      sensor [1] [10] = '"' + String(result) + '"' ; 
      sensor [1] [11] = '"' + String(WiFi.RSSI()) + '"' ;
      sensor [1] [12] = '"' + String(bufSSID) + '"' ;
      sensor [1] [13] = '"' + urlRss + '"' ;
      
  }
}

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
  
//     Serial.print("Peak to peak: ");
//    Serial.println( maxPeakToPeak);

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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                                      mqttPublish
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttPublish()
{
   maxPeakToPeak = 0 ;//RAZ noise peak value
  if (functionLock == 0)
  {
# define arrayLenght 1600 //max 1600

    String strucDataAndOnlineCheck = "{\"s\":\"urn:lo:nsid:smartDisplay:ChId!Data\",\"m\":\"smartDisplay_V1_0\",\"v\":{ ";
    char buf1[arrayLenght];
    char buf2[arrayLenght];


    //////////////////////////////////////////////////////////////////////////
    strucDataAndOnlineCheck.toCharArray(buf1, arrayLenght);
    strncpy(buf2, buf1, arrayLenght);

    for (int i = 0 ; i < sizeof(buf1) ; i++) //Scan de la structure pour trouver la zone à modifier
    {
      if ((buf1[i] == 'C') & (buf1[i + 1] == 'h') & (buf1[i + 2] == 'I') & (buf1[i + 3] == 'd')) //Détéction de la zone à modifier
      {

        for (int l = 0 ; l < (sizeof(buf1) - i) ; l++) //1ere étape: Ecrire l'ID dans la structure
        {
          buf2 [i + l] = char(ChipId[l]);
        }


        for (int j = 0 ; j < 49 ; j++)//2eme étape: Ajouter le reste de la structure
        {
          buf2 [i + j + cptChipId ]  = buf1 [i + j + 4];
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
    String tags ="\"tags\":[\"erwann\"]}";
    int tagsLength = tags.length();
    stringTag = stringTag + tags;

    Serial.print("tags: ");
    Serial.println(tags);
    
    stringTag.toCharArray(buf1, arrayLenght+tagsLength ) ;
    
    mqttClient.publish ("dev/data", 1, true, buf1) ; //Envoie des données

    Serial.print("Json Envoye:");
    Serial.println(String(buf1));
    memset(buf1, 0, sizeof(buf1));                     //Clear buffer

    //rssParser();//Update Rss Data
  }

}
/////////////////////////////////////////////////////////////////////       Get time        ////////////////////////////////////////
void getTime()
{
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
    }
  }
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
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
  Serial.print(": ");
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

void matrixDisplay ()
{
  int lengthbufName = sensor[0] [msgScrol].length() + 1;
  int lengthbufValue = sensor[1] [msgScrol].length() + 1;
  int lengthbufUnit = sensor[2] [msgScrol].length() + 1;
  int TotalLength = lengthbufValue + lengthbufName + lengthbufUnit;

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



  bool  lock = 0;
  while (lock != 1)
  {
    msgScrol =  msgScrol + 1;
    //msgScrol ==6 |
    if (msgScrol == 1 |msgScrol == 2 |msgScrol == 3 | msgScrol == 4 | msgScrol == 5 | msgScrol == 6| msgScrol == 10| msgScrol == 11| msgScrol == 12| msgScrol == 13)
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
    msgScrol = 0;
  }

  //Buiding à the string to display (Name of value : value (unit))
  String stringbuf = String(bufName) + ':' + String(bufCleanValue) + String(bufUnit);
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

    matrix.write(); // Send bitmap to display
    server.handleClient();                        // checks for incoming messages
    ArduinoOTA.handle();                          // checks for incoming uptdate
    getNoise();
    delay(wait);
  }

}


void DisplayAccesPointMsg()
  {
    String DeconexionMsg ="Display is disconnected from wifi, please set up wifi settings! ";
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
//                                     Wake on Lan                                      //
//  Description: Allume la Tv via le réseau                                             //
//////////////////////////////////////////////////////////////////////////////////////////
  void wake_on_lan()
  {
        Serial.println("Sending WOL Packet to TV salle creativite");
        WakeOnLan::sendWOL(computer_ip, Udp, mac, sizeof mac);
  }

//////////////////////////////////////////////////////////////////////////////////////////
//                                     Rss Parser                                       //
//  Description: cherche et traite les données rss                                      //
//////////////////////////////////////////////////////////////////////////////////////////
void rssParser()
{
    Serial.print("connecting to ");
    Serial.println(hostRss);
    
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(hostRss, httpPort)) {
      Serial.println("connection failed");
      return;
    }
    
    Serial.print("Requesting URL: ");
    Serial.println(urlRss);
    
    // This will send the request to the server
    client.print(String("GET ") + urlRss + " HTTP/1.1\r\n" +
                 "Host: " + hostRss + "\r\n" + 
                 "Connection: close\r\n\r\n");
                 
    delay(1000);
                   
    String line= "";
    bool TitleLock = false;
    int cptTitle= 0;
    int cptDescription = 1;//Offset des description pour que le titre soit en concordence avec la description 
    String compar = "Server";
  
    while(client.available())
    {
       line = client.readStringUntil('\n');
      // Serial.println("test");
       
       int lineLength = line.length();
            
        if (lineLength > 100)
        {
          lineLength = 100;
        }
        
       char lineAray[lineLength];
       memset(lineAray,'\0',lineLength);
       
       //Serial.print("nb line: ");
       //Serial.println(lineLength);
  
         line.toCharArray(lineAray, lineLength) ;
  
  
         if (strstr(lineAray, "<title>")) 
         {
          //Serial.println("title match");
          RssTiltle[cptTitle] = line;
          cptTitle = cptTitle+1;
         }
         
          if (strstr(lineAray, "<description>")) 
         {
         // Serial.println("description match");
          RssDescrption[cptDescription] = line;
          cptDescription = cptDescription+1;
         }
         //Serial.println(line);
         
    }
  
  //Serial.println("closing connection");
   MaxRssData = cptTitle;
  
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
   
   RssTiltle[i].remove(0, 6); //Supression des espaces au début de la chaine de caractere
  
  }
  
  //Fonction d'amélioration des chaines de caracteres (description)
  for(int i=0 ; i<cptDescription ; i++)
  {
   RssDescrption[i].replace("<description>", "\0");
   RssDescrption[i].replace("</description>", "\0");
   RssDescrption[i].replace("é", "e");
   RssDescrption[i].replace("è", "e");
   RssDescrption[i].replace("ç", "c");
   RssDescrption[i].replace("l'", "l");
   RssDescrption[i].replace("ê", "e");
   RssDescrption[i].replace("à", "a");
   RssDescrption[i].replace("ô", "o");
   RssDescrption[i].replace("L'", "L");
   RssDescrption[i].replace("À", "A");
   RssDescrption[i].replace("’", "'");
   RssDescrption[i].replace("–", "-");
   RssDescrption[i].replace("É", "E");
   RssDescrption[i].replace("?", " ");
   RssDescrption[i].replace("Â", " ");

   RssDescrption[i].remove(0, 6); //Supression des espaces au début de la chaine de caractere
  
  }
  
  Serial.print("nb Title: ");
  Serial.println(cptTitle);
  
  Serial.print("nb Description: ");
  Serial.println(cptDescription);
  
  //Fonction pour montrer les données rss
  
  for(int i=0 ; i<cptDescription ; i++)
  {
  Serial.print("T ");
  Serial.print(i);
  Serial.print(" :");
  Serial.println(RssTiltle[i]);

  Serial.print("D ");
  Serial.print(i);
  Serial.print(" :");
  Serial.println(RssDescrption[i]);
  Serial.println();
  Serial.println();
  }
}
//////////////////////////////////////////////////////////////////////////////////////////
//                                     Rss matrix display                               //
//  Description: Affiche les données Rss sur le display                                 //
//////////////////////////////////////////////////////////////////////////////////////////
  void RssMatrixDisplay()
  {
    String MsgRss ="Actu:- " + RssTiltle[PointerDisplayRss] +" -" ;
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
        
        matrix.write(); // Send bitmap to display
        delay(wait+20);
      }
      PointerDisplayRss =PointerDisplayRss+1;
      if (PointerDisplayRss >= MaxRssData)
      {
        PointerDisplayRss =2;
      }
  }
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

  String str = String(ESP.getChipId());               //Get the chip id
  str.toCharArray(ChipId, 20);                        //Put the Chip Id in a array

  for (int i = 0 ; i < sizeof(ChipId) ; i++)
  {
    if (isDigit(ChipId[i]))
    {
      cptChipId = cptChipId + 1;
    }
  }

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

  //Read ssid and wifi pasword in eeprom
  bufIntens[0] = EEPROM.read(100);
  bufIntens[1] = EEPROM.read(101);

  //Read speed scroling in eeprom

  bufspeedScroling[0] = EEPROM.read(110);
  bufspeedScroling[1] = EEPROM.read(111);
  bufspeedScroling[2] = EEPROM.read(112);

  //Read StartBySound flag in eeprom
  lockStartBySound  = EEPROM.read(120);


  Serial.print("Ssid conect: ");
  Serial.println(String(bufSSID));
  
  Serial.print("Pasword conect: ");
  Serial.println(String(bufPASS));
  dispInstensity = atoi(bufIntens);
  Serial.print("Int Disp: ");
  Serial.println(dispInstensity);
  matrix.setIntensity(dispInstensity); // Use a value between 0 and 15 for brightness
  InitMatrixDisplay();
  if (atoi(bufspeedScroling) >= 0 & atoi(bufspeedScroling) <= 11)
  {
    wait = atoi(bufspeedScroling);
  }
  Serial.print("Speed scroling: ");
  Serial.println(wait);

  int cpt = 0;
  bool ExitWhile = 0;

  WiFi.begin(bufSSID, bufPASS);                         // Connect to WiFi network
  while ( ExitWhile == 0 )
  { // Wait for connection

    delay(1000);
    Serial.print("Connexion Try: ");
    Serial.print(cpt);
    Serial.println("/10");
    cpt = cpt +  1;

    if (WiFi.status() == WL_CONNECTED)
    {
      functionLock = 1;
      Serial.println("Gateway connexion OK");
      GatewayServerMode();
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
  rssParser();
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                          Loop                                                                        //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop(void)
{

  if(connexionMode== "Gateway")
  {
    sensorRead();
      if (veille == 0 | lockStartBySound == 0)
      {
        //Display rss news only after end of LIveobject data
        if(msgScrol==0)
        {
          RssMatrixDisplay();
        }
        matrixDisplay();
      }
      else
      { 
       getNoise();
      }
      t.update();
  }
  else
  {
       server.handleClient();                        // checks for incoming messages
       ArduinoOTA.handle(); 
       DisplayAccesPointMsg();
       loopCpt = loopCpt +1;
       
       //wait (5min) before restart (1 loop = 10sec)
       if(loopCpt == 30)
       {
          ESP.restart();
       }
  }
}


