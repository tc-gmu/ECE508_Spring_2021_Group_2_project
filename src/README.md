This is all of the code and data need to run the Group 2 Spring 2021 final project demonstration system.  

Folders:

BLE_Process_and_sensor: Contains the machine learning device software was run on Nano 33 BLE Sense

WiFi_Sensor_Consumer: Contains the Gateway device software was run on ESP8266.  To compile you will need a DO_NOT_CHECK_IN.h see below

RSSI_Sensor_System: Contains two subfolders:
    ESP8266WiFi_Client_UDP: 5 ESP8266 devices were running this software
    ESP8266WiFi_Server_UDP: 1 ESP8266 device was running this software.  to compile you will need a DO_NOT_CHECK_IN.h

libraries: Contains two subfolders:
    ECE508_UDP_message: Needs to be placed in the Arduino library directory(I used symlink)
    ECE508Libs: Needs to be placed in the Arduino library directory(I used symlink)

ml: Contains the jupyter notebook file & data used to generate the gesture_mdel.tflite & model.h.  The jupyter notebook file needs to be opened in https://colab.research.google.com/ and the data uploaded.

Note: The checked in gesture_mdel.tflite & model.h are specific to the 5 ESP8266 devices & there specific locations of the test configuration.  To reproduce you will need to setup your own test configuration and gather your own training data.  

To Run: (section the area you wnat to detect into 4 zones)
1) configure and install your 5 ESP8266WiFi_Client_UDP devices
2) configure and install your 1 ESP8266WiFI_Server_UDP devices
3) Turn out all RSSI_Sensor_System devices order does not matter
4) configure and start your BLE_Process_and_sensor device 
5) configure and start your WiFi_Sensor_Consumer device (location does not matter just needs to be on same wifi network as ESP8266WiFI_Server_UDP)
6) At this point the WiFi_Sensor_Consumer's display will prompt the user move to specific zones for training.
7) Once training is complete (you will want to run many training runs) pull data from cloud and break data into 5 csv files one for each zone. 
8) Upload the jupyter notebook file along with the data to google's colab and run the notebook to create the gesture_mdel.tflite & model.h files
9) Download the gesture_mdel.tflite & model.h and overwrite the ones in the BLE_Process_and_sensor recompile and upload to the device
10) start the system up again and start detecting


DO_NOT_CHECK_IN.h
#define SECRET_SSID "Something"
#define SECRET_PASS "Somethingspassphrase"
#define TIMEOFFSET 5