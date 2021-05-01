/**
 * This is a sensor in a wifi sensor array.  See the ESP8266WiFi_Server_UDP comments for description of how the system works
 * 
 * Tested with ESP8266
 * 
 */
#include <ESP8266WiFi.h>
#include <stdio.h>
#include <ArduinoOTA.h>


#include <WiFiUdp.h>
#include <SensorMessage.h>

#ifndef APSSID
#define APSSID_FRONT "SENSOR_"
#define APPSK  "SENSOR_"
#endif

#include <ECE508Util.h>
#include <CurrentTime.h>

#define CONTROLLER_TIMEOUT 10000 //10 seconds

byte _nError = 0;
unsigned long _ulTimeOut = 0;

bool isTimedOut(unsigned long maxWait=5000)
{
  if (_ulTimeOut==0)
  {
    _ulTimeOut = millis();
    return false;
  }
  
  if ( (millis()-_ulTimeOut) > maxWait )
  {
    Serial.println("Timed out:"  + String(millis()-_ulTimeOut) );
    _ulTimeOut = 0;
    return true;
  }

  return false;
    
}


//how to communicate with the controller
SensorMessage _sensorMessages;

//list of unique channels to scan for the controller and the other sensors
std::vector<int> arChannelds;

//not used right now
//WiFiEventHandler probeRequestPrintHandler;

//this sensor's unique index if this is annoying we can just send the mac each time
byte _index = 255;

//true when in scan and send mode
bool bRun = false;

//class that will give up the current time
CurrentTime currentTime;

//scans local wifi and finds the controller to connect to
String getControllerSSID()
{
  while (true)
  {
    int numSsid = WiFi.scanNetworks();
    if (numSsid != -1) 
    {
      for (int thisNet = 0; thisNet < numSsid; thisNet++) 
      {
        String strSSID = WiFi.SSID(thisNet);
        if ( strSSID.startsWith("CONTROLLER_") )
        {
          return strSSID;
        }
      }
    }
  }
}



//get the ssid this device should use
String getSensorSSID()
{
  byte mac[6];
  WiFi.macAddress(mac);
  String strAPSSID = APSSID_FRONT;
  return strAPSSID + macToString(mac);
}



//connect to the controller
void setupControllerLink()
{
  Serial.println("");
  Serial.println("Client Startup");
  String strAPSSID = getControllerSSID();
  String strPass = "";
  if (strAPSSID.length() > 11)
    strPass = strAPSSID.substring(11);
  Serial.println(strAPSSID);
  Serial.println(strPass);

  while(WiFi.status() != WL_CONNECTED)
  {
    int nTrys = 0;
    WiFi.begin(strAPSSID, strPass.c_str());
    while (WiFi.status() != WL_CONNECTED && nTrys<20)
    {
      delay(500);
      Serial.print(".");
      nTrys++;
    }

    if ( WiFi.status() != WL_CONNECTED )
      delay(5000);
  }
  Serial.println("");
  Serial.println("Connected");  
}

//function for OTA update
void setupOTA()
{
  ArduinoOTA.setHostname("SENSOR");
  ArduinoOTA.setPassword("ECE508");  

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
  ArduinoOTA.begin();//(WiFi.localIP(), );
  Serial.println("OTA ready");
}

/*
 * 1) Get Mac address so can craft this device's SSID
 * 2) Get the Controller's SSID by scanning
 * 3) Setup this device's AP & connect to controller
 */
void setup() {
  WiFi.persistent(false);

  Serial.begin(115200);
  while (!Serial)
  {
    Serial.print("+");
    delay(10);
  }



  Serial.println("Starting Wifi");
  WiFi.mode(WIFI_AP_STA);

  setupControllerLink();  
  

  String strAPSSID = getSensorSSID();
  String strPass = "FOOECE508FOO";

  IPAddress local_IP(192, 168, 210, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, local_IP, subnet);
  WiFi.softAP(strAPSSID.c_str(), strPass.c_str());

  
  Serial.print("SoftAPIP:");
  Serial.println(WiFi.softAPIP());
  Serial.print("Localip:");
  Serial.println(WiFi.localIP());


  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  setupOTA();

  //registering callbacks with sensor message class
  _sensorMessages.set_sen_start(got_sensor_start);
  _sensorMessages.set_sen_timesync(got_time_sync);
  _sensorMessages.set_sen_stop(got_sensor_stop);
  _sensorMessages.set_sen_scan(got_scan);
  _sensorMessages.set_sen_heartbeat(got_sensor_heartbeat);
  _sensorMessages.setup();

  register_self();
}


//adds a channel to the unique channel vector
void addChannel(int nChannel)
{
  for (int i=0;i<arChannelds.size();i++)
  {
    if ( arChannelds[i] == nChannel )
      return;
  }
  arChannelds.push_back(nChannel);
}


/**
 * Rescans the env looking for what channels need to be periodcially scanned
 * 
 * Note: think about not doing this in the call back function this locks up this device
 */
void got_scan()
{
  Serial.println("got scan");
  arChannelds.clear();
  int numSsid = WiFi.scanNetworks();
  if (numSsid != -1) 
  {
    for (int thisNet = 0; thisNet < numSsid; thisNet++) 
    {
      String strSSID = WiFi.SSID(thisNet);
      if (strSSID.startsWith(APSSID_FRONT) || strSSID.startsWith("CONTROLLER_"))
      {
        addChannel(WiFi.channel(thisNet));
      }
    }
  }
  _ulTimeOut = 0;

}

//callback for when to stop scanning
void got_sensor_stop()
{
  Serial.println("Sensor stop");
  _ulTimeOut = 0;
//  probeRequestPrintHandler = WiFiEventHandler();
  bRun = false;
}

void got_sensor_heartbeat()
{
  Serial.println("Sensor heartbeat");
  _ulTimeOut = 0;
}

//callback for when to start scanning
void got_time_sync(sen_timesync message)
{
  got_scan();
  currentTime.updateTime(message._ulTime);
  _index = message._index;
  _ulTimeOut = 0;
//  probeRequestPrintHandler = WiFi.onSoftAPModeProbeRequestReceived(&onProbeRequestPrint);

//  bRun = true; 
}

void got_sensor_start()
{
    Serial.println("Sensor start");
    _nError = 0;
  _ulTimeOut = 0;
    bRun = true;
}


//called to tell the controller you are out there
void register_self()
{
  byte mac[6];
  WiFi.macAddress(mac);
  _ulTimeOut = 0;


  _sensorMessages.send_register(WiFi.gatewayIP(),(uint32_t)WiFi.localIP(),mac);
}

//scanNetworks(bool async, bool show_hidden, uint8 channel, uint8* ssid)
//https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiScan/WiFiScan.ino
void wifiProbe()
{
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
  uint8_t* bssid;
  int32_t channel;
  bool hidden;
  int scanResult;

  for (int y=0;y<arChannelds.size();y++)
  {
    scanResult = WiFi.scanNetworks(/*async=*/false, /*hidden=*/false,arChannelds[y]/*11*/);
    if (scanResult == 0) 
    {
        Serial.println(F("No networks found"));
        got_scan();
    } 
    else if (scanResult > 0) 
    {
      for (int8_t i = 0; i < scanResult; i++) 
      {
        WiFi.getNetworkInfo(i, ssid, encryptionType, rssi, bssid, channel, hidden);
//        Serial.println( macToString(bssid) + " " +  ssid);
        if (ssid.startsWith(APSSID_FRONT) || ssid.startsWith("CONTROLLER_"))
          if (!_sensorMessages.send_wifi_sample(WiFi.gatewayIP(),_index,bssid,rssi,currentTime.getEpochTime()))
          {
            _nError++;
            Serial.println("Failed to send_wifi_sample");
            if ( _nError > 5 )
              bRun = false;
          }
      }
      
    }
  }

}


void loop() 
{
  ArduinoOTA.handle();
  _sensorMessages.checkMessage();
  
  if ( bRun )//check to see if in collecting mode
  {
    wifiProbe();
    if ( isTimedOut(CONTROLLER_TIMEOUT*3) )
    {
      bRun = false;
    }

  }
  else //not in collection mode need to make sure the controller is still around
  {
    if (WiFi.status() != WL_CONNECTED )
    {
      Serial.println("Reconnecting No WiFi! ");
      if (!WiFi.status() != WL_CONNECTED)
        setupControllerLink();
      register_self();
    }
  }

  if ( isTimedOut(CONTROLLER_TIMEOUT) )
  {
      Serial.println("Reconnecting to! ");
      if (!WiFi.status() != WL_CONNECTED)
        setupControllerLink();
      register_self();
  }

}
