#ifndef CURRENT_TIME_FOO
#define CURRENT_TIME_FOO

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ) )

/**
 * 
 * Utility class for keeping & formatting time
 * 
 * */

class CurrentTime
{
public:
  CurrentTime() {_currentEpoc = 0; _lastUpdate = 0;};

  void updateTime(unsigned long currentEpoc)
  {
    this->_lastUpdate = millis();//going to have some delay in it
    this->_currentEpoc = currentEpoc;
  };

  String getFormattedTime(unsigned long secs) {
    unsigned long rawTime = secs ? secs : this->getEpochTime();
    unsigned long hours = (rawTime % 86400L) / 3600;
    String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);
  
    unsigned long minutes = (rawTime % 3600) / 60;
    String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);
  
    unsigned long seconds = rawTime % 60;
    String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);
  
    return hoursStr + ":" + minuteStr + ":" + secondStr;
  }  

// Based on https://github.com/PaulStoffregen/Time/blob/master/Time.cpp
// currently assumes UTC timezone, instead of using this->_timeOffset
  String getPretty()
  {
    unsigned long rawTime = this->getEpochTime() / 86400L;  // in days
    unsigned long days = 0, year = 1970;
    uint8_t month;
    static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};

    while((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
      year++;
    rawTime -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
    days=0;
    for (month=0; month<12; month++) {
      uint8_t monthLength;
      if (month==1) { // february
        monthLength = LEAP_YEAR(year) ? 29 : 28;
      } else {
        monthLength = monthDays[month];
      }
      if (rawTime < monthLength) break;
      rawTime -= monthLength;
    }
    String monthStr = ++month < 10 ? "0" + String(month) : String(month); // jan is month 1  
    String dayStr = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime); // day of month  
    return String(year) + "-" + monthStr + "-" + dayStr + "T" + this->getFormattedTime(0) + "Z";
  }

  unsigned long getEpochTime() const {
    return this->_currentEpoc + // Epoc returned by the NTP server
           ((millis() - this->_lastUpdate) / 1000); // Time since last update
  };  

protected:
    unsigned long _currentEpoc;      // In s
    unsigned long _lastUpdate;      // In ms

    
};


#endif