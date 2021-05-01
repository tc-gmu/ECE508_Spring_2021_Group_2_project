/****
 * 
 * This device provides data to the gateway and to run the ML.  See the gateway for overall process
 * 
 * The gateway and the BLE communicate via serial.  This device is using Serial1 to talk to the gateway
 * 
 * Found an odd Bug not sure if it is Mbed OS or the Tensorflow library but you can not have to many functions
 * otherwise a call to tensorflow invoke will lock the device.  To reproduce uncomment out REPRODUCE_BUG
 * 
 * Notes:
 *   1) Using Arduino Mbed OS Nano Boards version 2.0.0
 *   2) Using Arduino_Tensorflow 2.4.0-ALPHA
 * 
 * Connections:
 * - Serial to gateway:  TX to D7 on gateway & RX to D8 on gateway
 * - SD card reader:     3.3V to power bus, GND to GND, DO to D12, CMD to D11, CLK to D13, D3 to D4 
 * - Pir Sensor:         1 to power bus, 2 to GND, 3 to D3 
 * - Power to gateway:   VIN to VIN, GND to GND
 */
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>



#include "model.h"

namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;
int inference_count = 0;

constexpr int kTensorArenaSize = 100 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
}  // namespace

//uncommenting out this define will lock this device
//as soon as tensorflow invoke is called. 
//#define REPRODUCE_BUG


#define BLE_SENSORS

#ifdef BLE_SENSORS
  #include <Arduino_APDS9960.h>
  #include <Arduino_LPS22HB.h>
#endif
#include <SD.h>


#define SERIAL_USB_DEBUG
#define ML_MODE

#ifndef ML_MODE  //having odd results when I do the tensorflowlite functions and other so need to pair down when in ml mode
#include <ECE508Util.h>
#endif

/*
 * 
 */
void setup() 
{  
  delay(10);
  // put your setup code here, to run once:
  #ifdef SERIAL_USB_DEBUG
    Serial.begin(115200);
    while (!Serial)
    {
      delay(10);
    }
  
    Serial.println("Starting Serial1");
  #endif

  Serial1.begin(115200);
  while (!Serial1)
  {
    delay(10);
  }
  #ifdef SERIAL_USB_DEBUG
    Serial.println("Serial1 Started");
  #endif



//SETUP TENSOR
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         tflModel->version(), TFLITE_SCHEMA_VERSION);
//    Serial.println("Model version not good!");
    return;
  }

  // This pulls in all the operation implementations we need.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::AllOpsResolver resolver;

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      tflModel, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  tflInterpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = tflInterpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }

  // Obtain pointers to the model's input and output tensors.
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);

  error_reporter->Report("test error reporter");



//END SETUP TENSOR





  #ifdef BLE_SENSORS
    if (!APDS.begin()) 
    {
      #ifdef SERIAL_USB_DEBUG
        Serial.println("Error initializing APDS9960 sensor.");
      #endif
      while (true); // Stop forever
    }  
  
    if (!BARO.begin()) 
    {
      #ifdef SERIAL_USB_DEBUG
        Serial.println("Failed to initialize pressure sensor!");
      #endif
      while (1);
    }
  #endif

  #ifdef BLE_SENSORS
  //SETUP PIR
    pinMode(3, INPUT);
  //END SETUP PIR
  #endif

  if (!SD.begin(4)) 
  {
    #ifdef SERIAL_USB_DEBUG
      Serial.println("SD Initialization failed.");
    #endif
    while(1)
    {
      
    }
  }
  
}

//vars for the sensors
int _proximity = 0;
int _r = 0, _g = 0, _b = 0;

#ifdef REPRODUCE_BUG
int foo1()
{
  int x = 0;
  x = x + 1;
  return x;
}

int foo2()
{
  int x = 0;
  x = x + 1;
  return x;
}
int foo3()
{
  int x = 0;
  x = x + 1;
  return x;
}
int foo4()
{
  int x = 0;
  x = x + 1;
  return x;
}
int foo5()
{
  int x = 0;
  x = x + 1;
  return x;
}
int foo6()
{
  int x = 0;
  x = x + 1;
  return x;
}

#endif



/**
 * Main function 
 * 
 *   This checks Serial1 for message from the gateway
 */
void loop() 
{
  if (Serial1)
  {
    if (Serial1.available() > 0) //check to see if any data from gateway
    {  
      String strRead = Serial1.readStringUntil('\n');//read message from gateway

      if ( strRead.indexOf("STARTING") > -1 )
      {
          #ifdef SERIAL_USB_DEBUG
            Serial.println("STARTING");
          #endif
          Serial1.println("STARTED");
          return;
      }

      if ( strRead.indexOf("PROCESS JSON") > -1 )
      {
        String strVal = Serial1.readStringUntil('\n');
        int nLen = strVal.toInt();
        
        #ifdef SERIAL_USB_DEBUG
          Serial.print(strVal);
          Serial.print(",");
        #endif
        for (int i=0;i<nLen;i++)
        {
          strVal = Serial1.readStringUntil('\n');
          float fVal = strVal.toFloat();
          #ifdef ML_MODE
            tflInputTensor->data.f[i] = fVal;
          #endif
          #ifdef SERIAL_USB_DEBUG
            Serial.print(strVal);
            Serial.print(",");
          #endif
        }
        
        #ifdef SERIAL_USB_DEBUG
          Serial.println();
        #endif
        #ifdef ML_MODE
          //process
          TfLiteStatus invokeStatus = tflInterpreter->Invoke();
          if (invokeStatus != kTfLiteOk) {
            Serial.println("Invoke failed!");
            while (1);
            return;
          }
      
        #endif
      
        #ifdef ML_MODE
          Serial1.println("5");
          for (int i = 0; i < 5; i++) 
          {
            Serial1.println(tflOutputTensor->data.f[i], 6);
          }
        #else
          Serial1.println("0");
        #endif
        
        #ifdef ML_MODE
          #ifdef SERIAL_USB_DEBUG
            // Loop through the output tensor values from the model
            for (int i = 0; i < 5; i++) {
              Serial.print(tflOutputTensor->data.f[i], 6);
              Serial.print(",");
            }
            Serial.println();
          #endif
        #endif
        return;
      }
  
      if ( strRead.indexOf("GET LIGHT") > -1 )
      {
        #ifdef BLE_SENSORS
          if (APDS.colorAvailable()) {
            APDS.readColor(_r, _g, _b);
          }
        #endif

        Serial1.println(String((int)(0.299*_r + 0.587*_g + 0.114*_b)));
        delay(1);
        return;
      }

      if ( strRead.indexOf("GET PROX") > -1)
      {
        #ifdef BLE_SENSORS
          if (APDS.proximityAvailable()) {
            _proximity = APDS.readProximity();
          }
        #endif
          Serial1.println(String(_proximity) );
          delay(1);
          return;
      }

      if ( strRead.indexOf("GET PRES") > -1)
      {
        #ifdef BLE_SENSORS
          float pressure = BARO.readPressure();
          Serial1.println(String(pressure) );
        #else
          Serial1.println("0");
        #endif
          delay(1);
          return;
      }

      if ( strRead.indexOf("GET PIR") > -1 )
      {
        #ifdef BLE_SENSORS
          //read pir
          Serial1.println(String(digitalRead(3)));
         #else
          Serial1.println("0");
         #endif
          delay(1);
          return;
      }
      
      if ( strRead.indexOf("PUT MACS") > -1 )//write macs from gateway to SD card
      {
        #ifdef SERIAL_USB_DEBUG
          Serial.println("Put Macs");
        #endif
        String strVal = Serial1.readStringUntil('\n');
        byte len = (byte)strVal.toInt();
        byte mac[6];
        if(SD.exists("macs.dat"))
        {
          #ifdef SERIAL_USB_DEBUG
            Serial.println("deleting file");
          #endif
          SD.remove("macs.dat");
        }
        
        File dataFile = SD.open("macs.dat", FILE_WRITE);
        dataFile.write(len);
          #ifdef SERIAL_USB_DEBUG
            Serial.println("File len:" + String(len));
          #endif
        for (int i=0;i<len;i++)
        {
          mac[0] = Serial1.readStringUntil('\n').toInt();//Serial1.read();
          mac[1] = Serial1.readStringUntil('\n').toInt();//.read();
          mac[2] = Serial1.readStringUntil('\n').toInt();//.read();
          mac[3] = Serial1.readStringUntil('\n').toInt();//.read();
          mac[4] = Serial1.readStringUntil('\n').toInt();//.read();
          mac[5] = Serial1.readStringUntil('\n').toInt();//.read();
        
          #ifndef ML_MODE
            #ifdef SERIAL_USB_DEBUG
              Serial.println(macToString( (const unsigned char*)mac));
            #endif
         #endif
        
          dataFile.write(mac,6);          
        }
        dataFile.close();          
        delay(1);
        return;
      }

      if ( strRead.indexOf("GET MACS") > -1 )//read MACs from SD card
      {
        if(!SD.exists("macs.dat"))
        {
          #ifdef SERIAL_USB_DEBUG
            Serial.println("File does not exist");
          #endif
          Serial1.println("NONE" );
          delay(1);
          return;
        }
        Serial1.println("GOOD");
        
        File dataFile = SD.open("macs.dat", FILE_READ);
        
        byte len;
        byte mac[6];
        dataFile.read(&len,1);
        Serial1.println( String(len) );
          #ifdef SERIAL_USB_DEBUG
            Serial.println("File len:" + String(len));
          #endif
        for (int i=0;i<len;i++)
        {
          dataFile.read(mac,6);
          Serial1.println( String( mac[0] ) );
          Serial1.println( String( mac[1] ) );
          Serial1.println( String( mac[2] ) );
          Serial1.println( String( mac[3] ) );
          Serial1.println( String( mac[4] ) );
          Serial1.println( String( mac[5] ) );
          #ifndef ML_MODE
            #ifdef SERIAL_USB_DEBUG
              Serial.println(macToString( (const unsigned char*)mac));
            #endif
         #endif
        }
        dataFile.close();          
        
        delay(1);
        return;
      }
      
      Serial1.println("UNKNOWN" );
      delay(1);
      
    }
    else
    {
      delay(500);
    }
  }
  delay(10);

  #ifdef REPRODUCE_BUG
    int nFoo = 0;
    nFoo += foo6();
    nFoo += foo5();
    nFoo += foo4();
    nFoo += foo3();
    nFoo += foo2();
    nFoo += foo1();
  #endif

}
