/***************************************
 * 
 * This is the gateway device.  Tested with ESP8266 this could be an Nano 33 IOT
 * 
 * Flow:
 * 1) Connects to local wifi network
 * 2) starts listening for wifi sensor multicast message
 * 3) upon getting multicast message then it starts a collection segment:
 *    a) Get saved macs from the BLE(on SD card)
 *       i) if no macs enter learning mode see below
 *    b) clears out old results
 *    c) sends a start message to the wifi sensor network
 *    d) gets values from the BLE (light prox etc)
 *    e) after the collect timeout counter expires it sends stop
 *    f) waits for the wifi sensor network to send data
 *    g) it cleans and averages the data
 *    h) passes the data to the BLE for ML to do its thing
 *  
 *  Learning mode:
 *   1) Start wifi sensor array and keep track of macs
 *   2) order the mac array
 *   3) send the macs to BLE to be saved
 *   4) start collecting samples for each zone and send to cloud
 *   5) start at b above
 * 
 * Connections:
 *  Screen:        GND to GND, VCC to v3.3(bus), SCL to D1, SDA to D2
 *  Serial to BLE: D7 to TX on ESP & D8 to RX on ESP
 *  Power to BLE:  VIN to VIN, GND to GND
 * 
 * Notes:
 *   1) The order of the data sent to the BLE sense for ML needs to be the same each time that is why macs are saved to the BLE sense
 *   2) To use the screen program: // screen /dev/ttyUSB0 57600
 *   3) I have only tested it with ESP at this point if switching to IOT need to fix the serial stuf
 * *************************************/
//enable for debugging serial
#define CONSUMER_SERIAL_USB


#ifdef ARDUINO_SAMD_NANO_33_IOT
  #include <WiFiNINA.h>
#else
  #include <ESP8266WiFi.h> 
#endif
#include <ESP8266HTTPClient.h>


#ifdef CONSUMER_SERIAL_USB
  #include <SoftwareSerial.h>

  //https://arduino-esp8266.readthedocs.io/en/latest/reference.html#serial
  //https://github.com/esp8266/Arduino/blob/3e567e94898df2ace9264630900697bc30c5d69c/cores/esp8266/HardwareSerial.h#L104-L111
  SoftwareSerial usbSerial;

  #define RX (3)
  #define TX (1)
  #define BAUD_RATE 57600
#endif

#define USE_DISPLAY

#ifdef USE_DISPLAY

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#endif



#include <stdio.h>

#include <ArduinoOTA.h>


#include <WiFiUdp.h>
#include <NTPClient.h>

#include <ControllerMessage.h>
#include <ECE508Util.h>

#include "DO_NOT_CHECK_IN.h" //header file with passwords defined such as SSID & password for wifi
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)  

#include "PersonDetectReadingsTable.h"

#ifdef USE_DISPLAY
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String _strTopLine;
String _strBottomLine;

#endif


unsigned long _ulTimeOut = 0;//timeout counter

bool isTimedOut(unsigned long maxWait=5000, bool bPrint=true)
{
  if (_ulTimeOut==0)
  {
    _ulTimeOut = millis();
    return false;
  }
  
  if ( (millis()-_ulTimeOut) > maxWait )
  {
    if (bPrint)
    {
      #ifdef CONSUMER_SERIAL_USB
        usbSerial.println("Timed out:"  + String(millis()-_ulTimeOut) );
      #endif
    }
    _ulTimeOut = 0;
    return true;
  }

  return false;
    
}

WiFiUDP ntpUDP;
NTPClient  timeClient(ntpUDP,TIMEOFFSET);

//class that handles the message to and from the wifi sensor controller
ControllerMessage _controllerMesssages;
uint32_t _wifiControllerIp;

#define MAX_MACS 6
//#define SAMPLES_FOR_ZONE_LEARN 100
#define SAMPLES_FOR_ZONE_LEARN 10

//keeping the controller mac seperate from the other macs since in the BLE array want the controller mac last 
byte _arMacs[MAX_MACS][6] = { {0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0}};  
byte _controllerMac[6] = {0,0,0,0,0,0};

void setupDisplay()
{
  #ifdef USE_DISPLAY
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  #endif
}

void waitForBleStart()
{
  String strStart;
  while (strStart.indexOf("STARTED")<0)
  {
    Serial.println("STARTING");
    strStart = Serial.readStringUntil('\n');
  }
}


/*
 * Function for updating the the display both the content and the hardware
 */
void displayTextOLED(String strTop, String strBottom) 
{
#ifdef USE_DISPLAY
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println(strTop);
  display.println("");
  display.setTextSize(4);

  display.println(strBottom);

 
  display.display();  
  #endif
}

//
//  Gets the IP of the Wifi Sensor array controller
//
void getIpViaMultiCast()
{
  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("getting IP");
  #endif
  WiFiUDP multiUdp;
  IPAddress mip(239, 100, 100, 100);
  const int port = 5007;

  multiUdp.beginMulticast(WiFi.localIP(), mip, port);

  while (multiUdp.parsePacket()<1)
  {
    delay(10);
  }
  int nSize = multiUdp.available();
  multiUdp.read((unsigned char*)&_wifiControllerIp,sizeof(uint32_t));
  multiUdp.read((unsigned char*)&_controllerMac,6);
  multiUdp.stop();
  _controllerMac[0] = 0;
  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("Found wifi sensor:" + IPAddress(_wifiControllerIp).toString() + " " + macToString( (const unsigned char*)_controllerMac));
  #endif
}
//_controllerMac

void setupOTA()
{
  ArduinoOTA.setHostname("CONSUMER");
  ArduinoOTA.setPassword("ECE508");  

  ArduinoOTA.onStart([]() {
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.println("Start");
    #endif
  });
  ArduinoOTA.onEnd([]() {
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.println("\nEnd");
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.printf("Progress: %u%%\r", (progress / (total / 100)));
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) usbSerial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) usbSerial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) usbSerial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) usbSerial.println("Receive Failed");
      else if (error == OTA_END_ERROR) usbSerial.println("End Failed");
    #endif
  });
  ArduinoOTA.begin();//(WiFi.localIP(), );
  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("OTA ready");
  #endif
}

rec_processed_data _processed_data;//data from wifi sensor

//the different modes for the loop function
#define MODE_COLLECTING 1
#define MODE_NOT_COLLECTING 2
#define MODE_WAITING_FOR_DATA 3
#define MODE_DATA_PREP 4
#define MODE_SEND_DATA 5
#define MODE_GET_MACS 10
#define MODE_DETECT_MACS 12

//vars to keep track of learn mode state
int _nLearn = -1;
int _nCount = 0;
byte _Mode = MODE_GET_MACS;


sTableEntry _reads;//data for BLE & cloud

int _nMacs = 0;//number of macs
String _strMacs;//var used during learning



/**
 * Try to get MACs from the BLE sense connected via serial
 * if not there then detect them
 * */
void getMacs()
{
  waitForBleStart();
  
  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("Getting macs");
  #endif
  Serial.println("GET MACS");

  String strStatus = Serial.readStringUntil('\n');
  if ( strStatus.indexOf("GOOD") > -1 )
  {
    
    String strVal = Serial.readStringUntil('\n');
    int len = strVal.toInt();
//    byte len = Serial.read();
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.println("Found " + String(len) + " Macs " );
    #endif
    if (len > 6 || len < 1)
    {
      _Mode = MODE_DETECT_MACS;
      return;
    }
    _nMacs = len;
    for (int i=0;i<len;i++)
    {
      _arMacs[i][0] = Serial.readStringUntil('\n').toInt();//.read();
      _arMacs[i][1] = Serial.readStringUntil('\n').toInt();//.read();
      _arMacs[i][2] = Serial.readStringUntil('\n').toInt();//.read();
      _arMacs[i][3] = Serial.readStringUntil('\n').toInt();//.read();
      _arMacs[i][4] = Serial.readStringUntil('\n').toInt();//.read();
      _arMacs[i][5] = Serial.readStringUntil('\n').toInt();//.read();
    }
    printMacIndex();
    _Mode = MODE_NOT_COLLECTING;
    _nCount = 0;
    _nLearn = -1;
  }
  else
  {
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.println("Detecting macs");
    #endif
    displayTextOLED("Learning", "MACS");
    _Mode = MODE_DETECT_MACS;
    _nCount = 0;
    _nLearn = 0;
  }
}


void saveMacs()
{
  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("PUT macs");
  #endif
  Serial.println("PUT MACS");
  byte len = _nMacs;
  Serial.println( String(len) );
//  Serial.write(len);
  
  for (int i=0;i<len;i++)
  {
     Serial.println( String(_arMacs[i][0]) );
     Serial.println( String(_arMacs[i][1]) );
     Serial.println( String(_arMacs[i][2]) );
     Serial.println( String(_arMacs[i][3]) );
     Serial.println( String(_arMacs[i][4]) );
     Serial.println( String(_arMacs[i][5]) );
  }
}

/**
 * Compares two macs used for the sorting function
 * need to ignore the first byte noticing that with esp
 * the mac and the bssid differ in the first byte
 */
bool macless(byte* j, byte* a)
{
//    if ( j[0] < a[0] )
//        return true;
//    if (j[0] == a[0] )
    {
        if ( j[1] < a[1] )
            return true;
        if (j[1] == a[1] )
        {
            if ( j[2] < a[2] )
                return true;
            if (j[2] == a[2] )
            {
                if ( j[3] < a[3] )
                    return true;
                if (j[3] == a[3] )
                {
                    if ( j[4] < a[4] )
                        return true;
                    if (j[4] == a[4] )
                    {
                        if ( j[5] < a[5] )
                            return true;
                    }
                }
            }
        }
    }
    return false;
}

/**
 * Bubble sort the macs
 */
void reorderMacs(int n)
{
  for (int i = 1; i < n; ++i)
  {
    byte j[6];
    memcpy(j,_arMacs[i],6);
    int k;
    for (k = i - 1; (k >= 0) && ( macless(j,_arMacs[k]) ); k--)
    {
        memcpy(_arMacs[k + 1],_arMacs[k],6);
    }
    memcpy(_arMacs[k + 1],j,6);
  }   
}

//get the macs from the results
//loop through the results and pull out any macs that
//are not already known.  Discard the rssi values
void getMacsForLearning()
{

  for (int nSen=0;nSen<_processed_data._nSensors;nSen++)
  {
    int nSenIndex = getMacIndex( _processed_data._sensor_info[nSen]._mac );
    if ( nSenIndex < 0 )
    {
      if ( _nMacs < MAX_MACS )
      {
        //add the mac
        memcpy( _arMacs[_nMacs], _processed_data._sensor_info[nSen]._mac, 6 );
        _nMacs = _nMacs+1;
      }
    }
    for (int nRead=0;nRead<_processed_data._sensor_info[nSen]._nReadings;nRead++)
    {
      int nReadIndex = getMacIndex( _processed_data._sensor_info[nSen]._sensor_data[nRead]._mac );
      if ( nReadIndex < 0 )
      {
        if ( _nMacs < MAX_MACS )
        {
          memcpy( _arMacs[_nMacs], _processed_data._sensor_info[nSen]._sensor_data[nRead]._mac, 6 );
          _nMacs = _nMacs+1;
        }
      }
    }
  }

}

/*
 * Code for learning all of the macs
 * 
 */
void detectMacs()
{
  switch(_nLearn)
  {
    case 0://starting to collect
    {
      _controllerMesssages.send_start(IPAddress(_wifiControllerIp));
      _nLearn = 1;
      _ulTimeOut = 0;
      break;
    }
    case 1://waiting for timeout
    {
      if (isTimedOut(1000,false))
      {
        _controllerMesssages.send_stop(IPAddress(_wifiControllerIp));
        _nLearn = 2;
        _ulTimeOut = 0;
      }
      break;
    }
    case 2://waiting for data or timeout
    {
      if (isTimedOut(5000,false))
      {
        _nLearn = 0;   
      }
    }
    case 3:
    {
    //get macs from results
      getMacsForLearning();
      _nLearn = 0;
      _nCount++;
      break;
    }
  }

  if ( _nCount > 6 )
  {
    displayTextOLED("Learning", "SAVE");

    //reoder the macs
    reorderMacs(_nMacs);
    saveMacs();

    _strMacs = "";//macToString(_controllerMac);
    for (int z=0;z<_nMacs;z++)
    {
      _strMacs += macToString(_arMacs[z]) + "|";
    }
    _strMacs += macToString(_controllerMac);
    
    _nLearn = 1;
    _nCount = 0;
    _Mode = MODE_NOT_COLLECTING;
    printMacIndex();
    displayTextOLED("Learning", "Z 1");

  }
}

/*

*/
void printMacIndex()
{
  #ifdef CONSUMER_SERIAL_USB
    for (int i=0;i<MAX_MACS;i++)
      usbSerial.println(macToString( (const unsigned char*)_arMacs[i]));
    usbSerial.println(macToString( (const unsigned char*)_controllerMac));
  #endif
}



/**
 * Takes the results vector and returns the zone that has more the 50%
 * 
 */

void displayAnswer(float* fAnswer)
{
  if ( fAnswer[0] > .50 )
  {
    displayTextOLED("Answer", " Z N");
    return;
  }
    
  if ( fAnswer[1] > .50 )
  {
    displayTextOLED("Answer", " Z 1");
    return;
  }

  if ( fAnswer[2] > .50 )
  {
    displayTextOLED("Answer", " Z 2");
    return;
  }

  if ( fAnswer[3] > .50 )
  {
    displayTextOLED("Answer", " Z 3");
    return;
  }

  if ( fAnswer[4] > .50 )
  {
    displayTextOLED("Answer", " Z 4");
    return;
  }

  displayTextOLED("Answer", " NONE ");
  return;

}

/**
 * 
 * 
 */
void sendData()
{
  if ( _nLearn > -1 )
  {
    sendDataToCloud();
  }
  else
  {
    Serial.println("PROCESS JSON");//send to BLE
    Serial.println(String( _nMacs*_nMacs + _nMacs ) );//send to BLE the number results about the send
    for (int nRows=0;nRows< NUMBER_OF_SENSORS;nRows++)
    {
      for (int nCol=0;nCol<=NUMBER_OF_SENSORS;nCol++)
      {
//        if (nRows == nCol ) //code to drop the cells that will be zero all the time no readings from mac1 to mac1 
//          continue;
          
        Serial.println( String(_reads._RSSIGrid[nRows][nCol]) );
      }
    }

    Serial.setTimeout(2000);
    String strResult = Serial.readStringUntil('\n');
    usbSerial.println(strResult);
    if ( strResult.indexOf("Error") > -1 || strResult.indexOf("Error") > -1 )
    {
      usbSerial.println(Serial.read());
      usbSerial.println(Serial.read());
      usbSerial.println(Serial.read());
      usbSerial.println(Serial.read());
      usbSerial.println(Serial.read());
      usbSerial.println(Serial.read());
      usbSerial.println(Serial.read());
    }
    int nLen = strResult.toInt();
    float fAnswer[5];
    for (int z=0;z<nLen;z++)
    {
      strResult = Serial.readStringUntil('\n');
      fAnswer[z] = strResult.toFloat();
    }
    if ( nLen > 0 )
      displayAnswer(fAnswer);
    Serial.setTimeout(1000);
    
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.print( String(_reads.otherSensors[0]) );
      usbSerial.print( "," );
      usbSerial.print( String(_reads.otherSensors[1]) );
      usbSerial.print( "," );
      usbSerial.print( String(_reads.otherSensors[2]) );
      usbSerial.println();
    #endif
  }
}

/**
 * Send training data to the cloud
 * */
void sendDataToCloud()
{
  String requestBody;
  writeJsonData(_reads,"SETUP_ONE",String(_nLearn),_strMacs,requestBody);
  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println( requestBody );
  #endif

  

  WiFiClient client;
  HTTPClient http;

  if (http.begin("https://ugzlq44ro9.execute-api.us-east-1.amazonaws.com/rssi/data","BD 64 89 B2 BE 54 4E 51 75 D0 0B 20 91 9A A5 C7 52 36 AB DD"))
  {
    http.addHeader("Content-Type", "application/json");   
    
    int httpResponseCode = http.POST(requestBody);
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.println( String(httpResponseCode) );
    #endif

    if(httpResponseCode>199 &&httpResponseCode<300)
    {
      //all good
      #ifdef CONSUMER_SERIAL_USB
        usbSerial.println( "Good Post:" +  http.getString());
      #endif
    }
    else
    {
      displayTextOLED("Error:" + String(httpResponseCode), "Fail" );
      #ifdef CONSUMER_SERIAL_USB
        usbSerial.println( "failed to post:" +  String(httpResponseCode));
      #endif
      //error
    }
    http.end();
  }
  else
  {
  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println( "failed to connect" );
  #endif
    displayTextOLED("Error: Unable to connect", "Fail" );
  }
}

//having an issue where the highest mac octet is different between mac and BSSI so ignoring it for this comparision
int getMacIndex(const byte arMac[6])
{

  if ( arMac[5] == _controllerMac[5] &&
       arMac[4] == _controllerMac[4] &&
       arMac[3] == _controllerMac[3] &&
       arMac[2] == _controllerMac[2] &&
       arMac[1] == _controllerMac[1] )
  {
    return _nMacs;
  }
  
  for (int i=0;i<_nMacs;i++)
  {
    if ( arMac[5] == _arMacs[i][5] && 
         arMac[4] == _arMacs[i][4] &&
         arMac[3] == _arMacs[i][3] &&
         arMac[2] == _arMacs[i][2] &&
         arMac[1] == _arMacs[i][1] /*&&
         arMac[0] == _arMacs[i][0]*/)
         return i;
  }
  return -1;
}

/*
 * Converts RSSI values to what we want to report
 * 
 */
float getRSSIValue(const float& fVal)
{
  return fVal;
}


//Make _processed_data into a grid of data
bool aggregateData()
{
  int nValues = 0; 
//  printMacIndex();
  if ( _processed_data._nSensors != (_nMacs) )
  {
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.println("Missing " + String(_nMacs - _processed_data._nSensors) );
    #endif
  }

  for (int nSen=0;nSen<_processed_data._nSensors;nSen++)
  {
    int nSenIndex = getMacIndex( _processed_data._sensor_info[nSen]._mac );
    if ( nSenIndex < 0 )
    {
      #ifdef CONSUMER_SERIAL_USB
        usbSerial.println("Bad MAC:" + macToString(_processed_data._sensor_info[nSen]._mac)  );
      #endif
      continue;
    }

    for (int nRead=0;nRead<_processed_data._sensor_info[nSen]._nReadings;nRead++)
    {
      int nReadIndex = getMacIndex( _processed_data._sensor_info[nSen]._sensor_data[nRead]._mac );
      if ( nReadIndex < 0 )
      {
        #ifdef CONSUMER_SERIAL_USB
          usbSerial.println("Bad MAC2:" + macToString(_processed_data._sensor_info[nSen]._sensor_data[nRead]._mac));
        #endif
        continue;
      }
      _reads._RSSIGrid[nSenIndex][nReadIndex] = getRSSIValue( _processed_data._sensor_info[nSen]._sensor_data[nRead]._fValue );
      nValues++;
    }
  }
  if (nValues!=_nMacs*_nMacs)
  {
      #ifdef CONSUMER_SERIAL_USB
        usbSerial.println("Bad Reading Values:" + String(nValues) );
      #endif
      return false;
  }
  return true;
}


void controller_data_rec(rec_processed_data* pData)
{
//  usbSerial.println("Got Data");
  if (_Mode == MODE_DETECT_MACS )
  {
    _nLearn = 3;  
  }
  else
    _Mode = MODE_DATA_PREP;
  

}



void setup() 
{
  _wifiControllerIp = 0;
  WiFi.persistent(false);

  Serial.begin(115200);//setup serial to talk to BLE
  while (!Serial)
  {
    delay(10);
  }    
  Serial.pins(15,13);//switch to pins connected to BLE  could use swap
  delay(500);
  

  #ifdef CONSUMER_SERIAL_USB
    usbSerial.begin(BAUD_RATE, SWSERIAL_8N1, RX, TX, false, 95);//setup serial to debug via USB

    while (!usbSerial)
    {
      delay(10);
    }

    usbSerial.println("Starting Serial2");
  #endif


  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("Starting Wifi");
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    #ifdef CONSUMER_SERIAL_USB
      usbSerial.print(".");
    #endif
  }

  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("Wifi Connected:" );
    usbSerial.println(WiFi.localIP().toString());
  #endif

  _controllerMesssages.set_con_processed_data(controller_data_rec,&_processed_data);//setup callbacks for getting wifi sensor data
  _controllerMesssages.setup();

  setupOTA();

  setupDisplay();
  displayTextOLED("Starting","");


  timeClient.begin();
  timeClient.update();

  #ifdef CONSUMER_SERIAL_USB
    usbSerial.println("About to wait for BLE");
  #endif
  waitForBleStart();
  displayTextOLED("Connected","");
 
}

/**
 * 
 * 
 */
void loop() 
{
    ArduinoOTA.handle(); //look for ota requests
  _controllerMesssages.checkMessage();//check for data from wifi sensor

  if ( _wifiControllerIp == 0 )
    getIpViaMultiCast();
  else
  {
    switch( _Mode )
    {
      case MODE_NOT_COLLECTING:
      {
        if (_nLearn > -1 )
        {
          _nCount++;
          if (_nCount > SAMPLES_FOR_ZONE_LEARN)
          {
            displayTextOLED("Learning", "NEXT");
            #ifdef CONSUMER_SERIAL_USB
              usbSerial.println("Just finished zone:" + String(_nLearn) + " Pausing for two seconds");
            #endif
            
            if (_nLearn == 0 )
            {
              displayTextOLED("Learning", "DONE");
              _nLearn = -1;
              _Mode = MODE_NOT_COLLECTING;
              delay(2000);
              return;
            }
            else
            {
              if (_nLearn==4)
              {
                displayTextOLED("Learning", "Z N");
                _nLearn = 0;
              }
              else
              {
                _nLearn++;
                displayTextOLED("Learning", "Z " + String(_nLearn) );
              }
            }
            delay(2000);
            _nCount = 0;
          }
        }
        _ulTimeOut = 0;

        memset(&_reads,0,sizeof(sTableEntry));//clear out old results
        _reads._lTime = timeClient.getEpochTime();//set the start time
        _controllerMesssages.send_start(IPAddress(_wifiControllerIp));

        //get values from BLE
        Serial.println("GET LIGHT");
        String strLight = Serial.readStringUntil('\n');
        _reads.otherSensors[0] = strLight.toInt();;

        Serial.println("GET PROX");
        String strProx = Serial.readStringUntil('\n');
        _reads.otherSensors[1] = strProx.toInt();

        Serial.println("GET PIR");
        String strPir = Serial.readStringUntil('\n');
        _reads.otherSensors[2] = strPir.toInt();

        _Mode = MODE_COLLECTING;
       break; 
      }
      case MODE_COLLECTING:
      {
        if (isTimedOut(1500,false))
//        if (isTimedOut(1000,false))
        {
          _controllerMesssages.send_stop(IPAddress(_wifiControllerIp));
          _Mode = MODE_WAITING_FOR_DATA; 
        }
        break;
      }
      case MODE_WAITING_FOR_DATA:
      {
        if (isTimedOut(5000))
        {
          _Mode = MODE_NOT_COLLECTING;
        }
        break;
      }
      case MODE_DATA_PREP:
      {
        if (aggregateData())
          _Mode = MODE_SEND_DATA;
        else
          _Mode = MODE_NOT_COLLECTING;
        break;
      }
      case MODE_SEND_DATA:
      {
        sendData();
        _Mode = MODE_NOT_COLLECTING;
        break;
      }
      case MODE_GET_MACS:
      {
        getMacs();
        break;
      }
      case MODE_DETECT_MACS:
      {
        detectMacs();
        break;
      }
    }
  }
}
