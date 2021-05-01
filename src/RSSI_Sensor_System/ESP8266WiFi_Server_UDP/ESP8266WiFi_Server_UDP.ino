/****************
 * 
 * This device aggrigates all of the data from the sensors that are actively gathering RSSI values
 * 
 * 
 * Flow right now:
 *   1) Plug in server (this device)
 *      a) it will connect to your wifi 
 *      b) create a network with CONTROLLER_<MAC> for the sensors 
 *      c) periodically send a multicast advertisment on your network for consumers to hear and connect 
 *      d) listen for consumers to request a reading
 *      e) listen for clients to connect
 *      f) If no consumer has requested data in the past 5 seconds send a heartbeat message to each of the sensors. This resets clients timeout counters
 *      g) Each time a client registers with the server the server sends a rescan message to each client.  
 *         This message tells the clients to do a full wifi scan of the area and keep track of the channels with CONTROLLER & SENSOR SSIDs. This cuts down on scan time during operation
 *   2) Plug in client(s)
 *      a) it will create a network with the SSID SENSOR_<MAC> 
 *      b) it will connect to the first CONTROLLER_<MAC> network it sees
 *      c) it will register with the server that it connected to
 *   3) Start a consumer
 *      a) consumer listens on the home wifi for a multicast advertisment from the server
 *      b) once it finds a server then send a start reading message 
 *      c) the server sends a message to each client telling them to start readings
 *      d) each client scans each of the channels that it learned in the last full scan it did.  sending the RSSI values for each found CONTROLLER & SENSOR back to the CONTROLLER
 *      e) after the amount of time that the consumer wants has elapsed it calls stop sensor reading
 *      f) the server sends a stop message to each client
 *      g) the server sends all of the aggrigated data to the consumer in one large message
 * 
 * 
 * Notes:  
 * 1) You need a DO_NOT_CHECK_IN.h with 
 *  #define SECRET_SSID "SSID"
 *  #define SECRET_PASS "password"
 *  #define TIMEOFFSET 5
 * 2) Timeout counters are relyed on to keep sensors & the controller in line.  If the timers go off then devices
 *    backoff to initial state and try to reconnect.  Sensors have been running in my basement for well over a week with not issues
 * 3) all client to server traffic is UDP all server to consumer traffic is TCP except the UDP multicast advertisment that the server does
 *  
 *  
 * */




#include <ESP8266WiFi.h>
#include <stdio.h>
#include <ArduinoOTA.h>

#ifndef APSSID
#define APSSID_FRONT "CONTROLLER_"
#define APPSK  "SENSOR_"
#endif

#include <WiFiUdp.h>
#include <NTPClient.h>

#include <SensorMessage.h>
#include <ControllerMessage.h>
#include <ECE508Util.h>
#include "SensorDataAndInfo.h"


#include "DO_NOT_CHECK_IN.h" //header file with passwords defined see notes about

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)  


/* Defines for the different modes that this device can be in*/
#define MODE_COLLECT 1
#define MODE_RUNNING 2
#define MODE_CLEAN_DATA 3
#define MODE_PROCESS_DATA 4
#define MODE_REPORT 5
#define MODE_PAUSE 6
int _nMode = MODE_PAUSE;
unsigned long _ulTimeOut = 0;//time out counter

//function for seeing if timeout has elapsed
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


//list of known clients
std::vector<SensorInfo> arWifiSensorClients;

WiFiUDP ntpUDP;
NTPClient  timeClient(ntpUDP,TIMEOFFSET);

uint32_t _uiConsumerIp = 0;//ip address to send results to

ControllerMessage _controllerMesssages; //class for communication with consumer
SensorMessage _sensorMessages;          //class for communication with sensor
send_processed_data _processedData; //memory to send to consumer



//sends UDP message advertising as a wifi sensor array controller
void advertiseSelf()
{
  WiFiUDP multiUdp;
  IPAddress mip(239, 100, 100, 100);
  const int port = 5007;
  uint32_t nIp = (uint32_t)WiFi.localIP();
  byte mac[6];
  WiFi.macAddress(mac);


  multiUdp.beginPacketMulticast(mip, port, WiFi.localIP());
  multiUdp.write((const char*)(&nIp),sizeof(uint32_t));
  multiUdp.write((const char*)mac,6);
  multiUdp.endPacket();
}

/*
 * Adds a client to the sensor vector
 * 
 */
int addClientInfo(const String strIp, const byte arMac[6] )
{
  for (int i=0;i<arWifiSensorClients.size();i++)
  {
    if ( arWifiSensorClients[i]._Mac[5] == arMac[5] && 
         arWifiSensorClients[i]._Mac[4] == arMac[4] && 
         arWifiSensorClients[i]._Mac[3] == arMac[3] && 
         arWifiSensorClients[i]._Mac[2] == arMac[2] && 
         arWifiSensorClients[i]._Mac[1] == arMac[1] && 
         arWifiSensorClients[i]._Mac[0] == arMac[0] )
    {
      return i;
    }
  }
  arWifiSensorClients.push_back( SensorInfo(strIp, arMac) );
  return arWifiSensorClients.size()-1;
}

/*
 * callback function from sensor message class 
 * 
 */
void got_wifi_sample(sen_wifi_sample message) 
{
//  Serial.println("Adding info for index:" + String(message._index));
  if ( message._index < arWifiSensorClients.size() )
    arWifiSensorClients[message._index].addSample(message._mac,message._lRSSI,message._lTime);
  else
    Serial.println("Error bad index");
}

/*
 *callback function from sensor message class
 * 
 */
void got_register(sen_register message)
{
//  String strMac = macToString(message._mac);
  IPAddress ip = IPAddress(message._nIp);
  byte nIndex = addClientInfo( ip.toString(),  message._mac );

  _sensorMessages.send_timesync(ip, nIndex, timeClient.getEpochTime());

  send_scan();//since we got a new registered sensor need to tell other sensors to rescan
}


/*
 * Tell all sensors to rescan to find the other sensors
 * 
 */
void send_scan()
{
  IPAddress ip;
  for(int i=0;i<arWifiSensorClients.size();i++)
  { 
    ip.fromString(arWifiSensorClients[i]._strIp);
    _sensorMessages.send_scan(  ip  );
  }
}


//do OTA update
void setupOTA()
{
  ArduinoOTA.setHostname("CONTROLLER");
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

//do not think this is being used
void sensor_start()
{
  _ulTimeOut = 0;
  Serial.println("Sensor start");
  _nMode = MODE_COLLECT;
}

//callback function for controller stop message
void sensor_stop()
{
  _ulTimeOut = 0;
  Serial.println("Sensor stop");
  pauseSensors();
  _nMode = MODE_CLEAN_DATA;
}

//callback function for controller start message
void controller_start(IPAddress ip)
{
  _ulTimeOut = 0;
  Serial.println("Controller start");
  _uiConsumerIp = ip;
  _nMode = MODE_COLLECT;
  
}


/*
 * 1) Get Mac address so can craft this device's SSID
 * 2) Get the Controller's SSID by scanning
 * 3) Setup this device's AP & connect to controller
 */
void setup() 
{
  WiFi.persistent(false);
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  Serial.println("Starting Wifi");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Wifi Connected");

  byte mac[6];
  WiFi.macAddress(mac);
  String strAPSSID = APSSID_FRONT;
  strAPSSID = strAPSSID + macToString(mac);
  Serial.println("Mac Address:" + macToString(mac));

  String strPass = macToString(mac);

//  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(strAPSSID.c_str(), strPass.c_str(), /*9*/11, 0, 6 );//it is not actually payting attention to the channel I pass in

  Serial.println("AP setup");

  //print info about network  
  Serial.print("SoftAPIP:");
  Serial.println(WiFi.softAPIP());
  Serial.print("Localip:");
  Serial.println(WiFi.localIP());

  _sensorMessages.set_sen_wifi_sample(got_wifi_sample);
  _sensorMessages.set_sen_register(got_register);
  _sensorMessages.set_register_consumer(got_register_consumer);
  _sensorMessages.set_sen_start(sensor_start);
  _sensorMessages.set_sen_stop(sensor_stop);

  _sensorMessages.setup();

  _controllerMesssages.set_con_stop(sensor_stop);
  _controllerMesssages.set_con_start(controller_start);
  _controllerMesssages.setup();

  setupOTA();

  timeClient.begin();
  timeClient.update();//sync with ntp server
}

//consumer message call back function
void got_register_consumer(sen_register_consumer consumer)
{
  IPAddress ip(consumer._nIp);
  Serial.println("Consumer registered:" + ip.toString());
  _uiConsumerIp = consumer._nIp;
}

void cleanData()
{
  Serial.println("Clean data:");
  
}


 /*
  * 
  *   max 8 sensors 8 readings 
  * 
  */
//#define PRINT_RESULTS_TO_SERIAL 1
void processData()
{
  Serial.println("Processing data:");

  _processedData._data._nSensors = arWifiSensorClients.size();

  for (int i=0;i<arWifiSensorClients.size();i++)
  {
      #ifdef PRINT_RESULTS_TO_SERIAL
        Serial.println( macToString(arWifiSensorClients[i]._Mac) + "(" + arWifiSensorClients[i]._strIp + ")");
      #endif
      memcpy(_processedData._data._sensor_info[i]._mac,arWifiSensorClients[i]._Mac,6);
      _processedData._data._sensor_info[i]._nReadings= arWifiSensorClients[i]._sensorLen;

      for (int j=0;j<arWifiSensorClients[i]._sensorLen;j++)
      {
        float fAvg = arWifiSensorClients[i]._arSensors[j].getAverageWithoutHighLow();
        #ifdef PRINT_RESULTS_TO_SERIAL
          Serial.println("\t" + macToString( arWifiSensorClients[i]._arSensors[j]._mac ) );
          Serial.println("\t\tAverage:" + String(fAvg) + " number of samples:" + String(arWifiSensorClients[i]._arSensors[j].nLen));
        #endif
        memcpy(_processedData._data._sensor_info[i]._sensor_data[j]._mac, arWifiSensorClients[i]._arSensors[j]._mac, 6);
        _processedData._data._sensor_info[i]._sensor_data[j]._fValue = fAvg;
        arWifiSensorClients[i]._arSensors[j].clean();
      }
      arWifiSensorClients[i].clean();
  }

}


//sends the start message to all of the sensors
void startSensors()
{
  Serial.println("Start sensors:" + String(arWifiSensorClients.size()) );
  IPAddress ip;
  for(int i=0;i<arWifiSensorClients.size();i++)
  { 
    ip.fromString(arWifiSensorClients[i]._strIp);
    _sensorMessages.send_start(  ip  );
  }
}

//sends heart beat to all sensors to reset sensor internal timer
void send_heartbeat()
{
  IPAddress ip;
  for(int i=0;i<arWifiSensorClients.size();i++)
  { 
    ip.fromString(arWifiSensorClients[i]._strIp);
    _sensorMessages.send_heartbeat(  ip  );
  }
}


//send stop to all of the sensor starts
void pauseSensors()
{
  IPAddress ip;
  for(int i=0;i<arWifiSensorClients.size();i++)
  { 
    ip.fromString(arWifiSensorClients[i]._strIp);
    _sensorMessages.send_stop(  ip  );
  }
}


//send data to the consumer
void reportData()
{
  if (_uiConsumerIp!=0)
  {
    Serial.println("Sending data");
    IPAddress ip(_uiConsumerIp);
   
    if (!_controllerMesssages.send_processedData(  ip, _processedData  ))
    {
      Serial.println("Failed to send processedData");
      _uiConsumerIp = 0;
    }
  }

}

//main function of the program
void loop() 
{
  ArduinoOTA.handle();//look for OTA request
  _sensorMessages.checkMessage();//look for sensor message
  _controllerMesssages.checkMessage();//look for consumer message
  
 switch(_nMode)
 {
  case MODE_COLLECT:
  {
    startSensors();
    memset(&_processedData,0,sizeof(send_processed_data));//clear out old results
    _processedData._data._lTime = timeClient.getEpochTime();
    _nMode = MODE_RUNNING;
    break;
  }
  case MODE_RUNNING:
  {
    if (isTimedOut(10000))//10 seconds just incase something happened to the consumer do not want to be in a bad state
    {
      processData();//cleans up the data
      _nMode = MODE_PAUSE;
    }
    break;
  }
  case MODE_CLEAN_DATA:
  {
    Serial.println("Cleandata");
    cleanData();
    _nMode = MODE_PROCESS_DATA;
    break;
  }
  case MODE_PROCESS_DATA:
  {
    processData();//likely not move on until processing is done
    _nMode = MODE_REPORT;
    break;
  }
  case MODE_REPORT:
  {
    reportData();
    _nMode = MODE_PAUSE;    
    break;
  }
  case MODE_PAUSE:
  {
    if (isTimedOut(3000))
    {
      advertiseSelf();//tell the local wifi network that you are around multicast
      send_heartbeat();//tell the sensors that you are still around and reset timeout counter
    }
    break;
  }
  default:
  {
    _nMode = MODE_PAUSE;
    break;
  }
 }
}
