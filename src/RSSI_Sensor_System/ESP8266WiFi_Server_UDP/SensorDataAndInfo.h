#ifndef SENSORDATAANDINFO
#define SENSORDATAANDINFO



class signal_sample
{
public:
  signal_sample() {
    lRSSI = 0;
    lTime = 0;
  }

  long lRSSI;
  long lTime;

    signal_sample& operator=(const signal_sample& other)
    {
        // Guard self assignment
        if (this == &other)
            return *this;

        lRSSI = other.lRSSI;
        lTime = other.lTime;

        return *this;
    };       
  
};

#define SIGNAL_SAMPLES_SIZE 30 //number of samples from a single device 30 seems high
 
class sensor_object
{
public:
    sensor_object() {memset(_mac,0, sizeof(byte)*6); nIndex = 0; nLen = 0;};
    sensor_object(byte mac[6]) {memcpy(_mac,mac, sizeof(byte)*6); nIndex = 0; nLen = 0;};
    byte _mac[6];

    void setMac(const byte mac[6]) {memcpy(_mac,mac, sizeof(byte)*6); };

    bool isValid() { if (_mac[0] == 0 && _mac[1] == 0  && _mac[2] == 0 && _mac[3] == 0) {return false;} return true; };

    void clean() { nIndex = 0; nLen = 0;}
   
    signal_sample arSamples[SIGNAL_SAMPLES_SIZE];
    int nIndex;
    int nLen;

    signal_sample* getReading(int i)
    {
      if (i>=nLen)
        return NULL;
      int t = i+nIndex;
      if ( t >= SIGNAL_SAMPLES_SIZE )
        t = t-SIGNAL_SAMPLES_SIZE;

      return &arSamples[t];
    };


    float getAverageWithoutHighLow()
    {
      if (nLen<3)
      {
        if (nLen<1)
          return -1000;
        if (nLen == 1 )
          return getReading(0)->lRSSI;
        return (getReading(0)->lRSSI+getReading(1)->lRSSI)/2.0;
      }
      long lVal = getReading(0)->lRSSI;
      long lSum = lVal;
      long lMax = lVal;
      long lMin = lVal;

      for (int i=1;i<nLen;i++)
      {
        lVal = getReading(i)->lRSSI;
        lSum += lVal;
        if (lVal>lMax)
          lMax = lVal;
        else if (lVal<lMin)
          lMin = lVal;
      }

      lSum = lSum - lMax - lMin;
      return ((float)lSum)/(nLen-2.0);
    }

    void addRSSI(const long& lRSSI,const long& lTime)
    {
//      Serial.println("Adding " + String(nLen));
      if (nLen>=SIGNAL_SAMPLES_SIZE)
      {
        arSamples[nIndex].lRSSI = lRSSI;
        arSamples[nIndex].lTime = lTime;
        nIndex = nIndex + 1;
        if (nIndex >= SIGNAL_SAMPLES_SIZE)
          nIndex = 0;
        return;
      }
      arSamples[nIndex+nLen].lRSSI = lRSSI;
      arSamples[nIndex+nLen].lTime = lTime;
      nLen = nLen + 1;
    };

    sensor_object& operator=(const sensor_object& other)
    {
        // Guard self assignment
        if (this == &other)
            return *this;

        memcpy(_mac,other._mac, sizeof(byte)*6);
        nIndex = other.nIndex;
        nLen = other.nLen;

        for (int i=0;i<SIGNAL_SAMPLES_SIZE;i++)
        {
          arSamples[i] = other.arSamples[i];
        }
        
        return *this;
    };       

    String toString()
    {
      String strReturn = "\t" + macToString(_mac);
      for (int i=0;i<nLen;i++)
      {
        signal_sample* reading = getReading(i);
        strReturn = strReturn + "\t\t" + String(reading->lTime) + " " + String(reading->lRSSI) + "\n";
      }
    }
  
};

#define MAX_NUMBER_SENSORS 8 //8 sensors -minus this one + the controller

class SensorInfo
{
public:
  SensorInfo(){ _sensorLen = 0;};
  SensorInfo(String strIp, const byte arMac[6]){_strIp = strIp; memcpy(_Mac, arMac, 6);_sensorLen = 0;};

  void addSample(int nIndex, const long& lRSSI,const long& lTime)
  {
    _arSensors[nIndex].addRSSI(lRSSI,lTime);
  }

  void addSample(const byte mac[6], const long& lRSSI,const long& lTime)
  {
    for (int i=0;i<_sensorLen;i++)
    {
      if ( _arSensors[i]._mac[5] == mac[5] && 
           _arSensors[i]._mac[4] == mac[4] &&
           _arSensors[i]._mac[3] == mac[3] &&
           _arSensors[i]._mac[2] == mac[2] &&
           _arSensors[i]._mac[1] == mac[1] &&
           _arSensors[i]._mac[0] == mac[0])
      {
        addSample(i,lRSSI,lTime);
        return;
      }
    }
    if ( _sensorLen< MAX_NUMBER_SENSORS )
    {
      _arSensors[_sensorLen].setMac(mac);
      addSample(_sensorLen,lRSSI,lTime);
      _sensorLen++;
    }
  };
  
public:
    sensor_object _arSensors[MAX_NUMBER_SENSORS];
    int _sensorLen;

    String _strIp;
    byte _Mac[6];
    
    String getSSID() { return "SENSOR_" + macToString(_Mac); };

    void clean() {_sensorLen=0;};

//    String toString() { return _strIp + "," + getSSID(); };

    SensorInfo& operator=(const SensorInfo& other)
    {
        // Guard self assignment
        if (this == &other)
            return *this;
  
        _strIp = other._strIp;
        memcpy(_Mac,other._Mac,6);
//        _strMac = other._strMac;
        _sensorLen = other._sensorLen;

        for (int i=0;i<MAX_NUMBER_SENSORS;i++)
        {
          _arSensors[i] = other._arSensors[i];
        }
        
        return *this;
    };   
/*
    String toString()
    {
      String strReturn = _strMac + "(" + _strIp + ")\n";
      for (int i=0;i<_sensorLen;i++)
      {
        strReturn = strReturn + _arSensors[i].toString();
      }
      return strReturn;
    }
*/    
};

#endif
