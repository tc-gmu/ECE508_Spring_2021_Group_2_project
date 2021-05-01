
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "PersonDetectReadingsTable.h"
#include <ArduinoJson.h>


/*
 * Fills strBody string with json created from the passed in vars
 */
void writeJsonData(const sTableEntry& entry, const String& strName, const String& strLabel, const String& strMacs , String& strBody){
    char temp[20];
    
    StaticJsonDocument<1024> doc;
    doc["source"] = (strName);
    doc["t"] = entry._lTime;
    doc["label"] = strLabel;
    doc["macs"] = strMacs;
    JsonArray arr = doc.createNestedArray("data");
    copyArray(entry._RSSIGrid,arr);

    JsonArray arr2 = doc.createNestedArray("ancillary_data");
    copyArray(entry.otherSensors,arr2);

    serializeJson(doc, strBody);
//    serializeJsonPretty(doc, usbSerial);
}
