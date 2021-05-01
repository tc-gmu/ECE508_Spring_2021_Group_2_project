#ifndef PERSON_DETECT_READINGS_TABLE_H
#define PERSON_DETECT_READINGS_TABLE_H 1001


#define NUMBER_OF_SENSORS 5
#define NUMBER_OF_OTHER_SENSORS 3 //light and prox

struct sTableEntry {
  long _lTime;
//  uint16_t _RSSIGrid[NUMBER_OF_SENSORS][NUMBER_OF_SENSORS+1]; //number of sensors by number of sensors+1 (the controller)
  float _RSSIGrid[NUMBER_OF_SENSORS][NUMBER_OF_SENSORS+1]; //number of sensors by number of sensors+1 (the controller)
  uint8_t otherSensors[NUMBER_OF_OTHER_SENSORS];
};



void writeJsonData(const sTableEntry& entry, const String& strName, const String& strLabel, const String& strMacs , String& strBody);

#endif
