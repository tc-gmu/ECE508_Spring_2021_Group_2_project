/**
 * Class for handling message between controller and sensors in the wifi array
 * This class uses UDP to pass these messages
 * 
 * Message format:
 * Byte     0        1                              n
 *       |0xCC|message type|<body depends on type>|0xCD|
 * */

#ifndef SENSOR_MESSAGE
#define SENSOR_MESSAGE


#define UDP_SERVER_PORT 323
#define SEN_MESSAGE_START 0xCC
#define SEN_MESSAGE_GOODBYE 0xCD

#define SEN_TIMESYNC 1
#define SEN_STOP 2
#define SEN_REGISTER 3
#define SEN_WIFI_SAMPLE 4
#define SEN_SCAN 5
#define SEN_START 6
#define SEN_REGISTER_CONSUMER 7
//#define SEN_PROCESSED_DATA 8
#define SEN_HEARTBEAT 9





struct sen_timesync {
  byte _index;
  unsigned long _ulTime;
  byte goodbye;
};

struct sen_register
{
  uint32_t _nIp;
  byte _mac[6];
  byte goodbye;
};

struct sen_wifi_sample
{
  byte _index;
  byte _mac[6];
  long _lRSSI;
  long _lTime;
  byte goodbye;
};

struct sen_register_consumer
{
  uint32_t _nIp;
  byte goodbye;
};

/*
 * 
 * https://github.com/esp8266/Arduino/blob/master/cores/esp8266/IPAddress.h
 * https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiUdp.h
 */
class SensorMessage
{
  public:
    SensorMessage()
    {
      _nErrorCount = 0;
	  cb_sen_timesync = NULL;
	  cb_sen_start = NULL;
	  cb_sen_stop = NULL;
	  cb_sen_register = NULL;
	  cb_sen_wifi_sample = NULL;
	  cb_sen_scan = NULL; 
	  cb_sen_register_consumer = NULL;
	  cb_sen_heartbeat = NULL;
    }

    void setup() 
    {
      _udpListener.begin(UDP_SERVER_PORT);
    };


//SEN_SCAN
    /**
     * 
     * 
     * */
    int send_scan(const IPAddress& sendIp)
    {
      byte message = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_SCAN, (const uint8_t *)&message, sizeof(byte)) && nCount < 4 ) 
      {
        nCount++;
      }

      if (nCount <4 )
        return 0;
      return 1;
    };
	
    /**
     * 
     * 
     * */
    int send_heartbeat(const IPAddress& sendIp)
    {
      byte message = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_HEARTBEAT, (const uint8_t *)&message, sizeof(byte)) && nCount < 4 ) 
      {
        nCount++;
      }

      if (nCount <4 )
        return 0;
      return 1;
    };	
/*	
	int send_processedData(const IPAddress& sendIp, const sen_processed_data& message)
    {
//      message.goodbye = SEN_MESSAGE_GOODBYE;
	  
//	  Serial.println(  "Size of message:" + String( sizeof(sen_processed_data) ) );
//      byte message2 = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_PROCESSED_DATA, (const uint8_t *)&message, sizeof(sen_processed_data)) && nCount < 4 ) 
//      while (!sendPacket(sendIp, SEN_PROCESSED_DATA, (const uint8_t *)&message2, sizeof(byte)) && nCount < 4 ) 
	  {
        nCount++;
      }
//	  Serial.println("Count " + String( nCount) );

      if (nCount <4 )
        return 1;
	  Serial.println("Failed to send to " + sendIp.toString());
      return 0;
    };
*/	

    /**
     * 
     * 
     * */
    int send_register(const IPAddress& sendIp, int nIp, byte mac[6])
    {
      sen_register message;
      message._nIp = nIp;
      memcpy(message._mac,mac, sizeof(byte)*6);
      message.goodbye = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_REGISTER, (const uint8_t *)&message, sizeof(sen_register) ) && nCount < 4 )
      {
        nCount++;
      }

      if (nCount <4 )
        return 0;
      return 1;
    };

    /**
     * 
     * 
     * */
    int send_timesync(const IPAddress& sendIp, byte index, unsigned long ulTime)
    {
      sen_timesync message;
      message._index = index;
      message._ulTime = ulTime;
      message.goodbye = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_TIMESYNC, (const uint8_t *)&message, sizeof(sen_timesync)) && nCount < 4 )
      {
        nCount++;
      }

      if (nCount <4 )
        return 0;
      return 1;
    };
    

    /**
     * 
     * 
     * */
    int send_stop(const IPAddress& sendIp)
    {
      byte message = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_STOP, (const uint8_t *)&message, sizeof(byte)) && nCount < 4 ) 
      {
        nCount++;
      }

      if (nCount <4 )
        return 0;
      return 1;
    };

    /**
     * 
     * 
     * */
    int send_start(const IPAddress& sendIp)
    {
      byte message = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_START, (const uint8_t *)&message, sizeof(byte)) && nCount < 4 ) 
      {
        nCount++;
      }

      if (nCount <4 )
        return 0;
      return 1;
    };


    /**
     * 
     * 
     * */
    int send_wifi_sample(const IPAddress& sendIp, byte nIndex, byte mac[6], long lRSSI, long lTime)
    {
      sen_wifi_sample message;
      message._index = nIndex;
      memcpy(message._mac,mac, sizeof(byte)*6);
      message._lRSSI = lRSSI;
      message._lTime = lTime;
      message.goodbye = SEN_MESSAGE_GOODBYE;

      int nCount = 0;
      while (!sendPacket(sendIp, SEN_WIFI_SAMPLE, (const uint8_t *)&message, sizeof(sen_wifi_sample)) && nCount < 4 ) 
      {
        nCount++;
      }

      if (nCount <4 )
        return 1;
      return 0;
    };

    int sendPacket(const IPAddress& sendIp, byte messageType, const uint8_t *buffer, size_t size )
    {
      int nCount = 0;
      while (_udpListener.beginPacket(sendIp,UDP_SERVER_PORT) != 1 && nCount < 4 )
      {
        nCount++;
      }
      if ( nCount > 3 )
        return 0;
      _udpListener.write( SEN_MESSAGE_START );
      _udpListener.write( messageType );
      _udpListener.write( buffer, size );
      return _udpListener.endPacket();
    };

    void checkMessage()
    {
      int packetSize = _udpListener.parsePacket();
      if ( packetSize )
      {
        int nByte = _udpListener.read();
        if ( nByte == SEN_MESSAGE_START )//check byte
        {
          nByte = _udpListener.read();
          switch(nByte)
          {
            case SEN_TIMESYNC://client would get this from controller
            {
              sen_timesync message;
              _udpListener.read( (unsigned char*)&message, sizeof(sen_timesync) );
              if ( cb_sen_timesync != NULL )
              {
                (*cb_sen_timesync)(message);
              }
              break;
            }
            case SEN_STOP://client would get this from controller
            {
              byte message;
              _udpListener.read( (unsigned char*)&message, sizeof(byte) );
              if ( cb_sen_stop != NULL )
              {
                (*cb_sen_stop)();
              }
              break;
            }
            case SEN_START://client would get this from controller
            {
              byte message;
              _udpListener.read( (unsigned char*)&message, sizeof(byte) );
              if ( cb_sen_start != NULL )
              {
                (*cb_sen_start)();
              }
              break;
            }
            case SEN_HEARTBEAT://client would get this from controller
            {
              byte message;
              _udpListener.read( (unsigned char*)&message, sizeof(byte) );
              if ( cb_sen_heartbeat != NULL )
              {
                (*cb_sen_heartbeat)();
              }
              break;
            }
            case SEN_REGISTER://server would get this from client
            {
              sen_register message;
              _udpListener.read( (unsigned char*)&message, sizeof(sen_register) );
              
              if ( cb_sen_register != NULL )
              {
                (*cb_sen_register)(message);
              }
              break;
            }
            case SEN_WIFI_SAMPLE:
            {
              sen_wifi_sample message;
              _udpListener.read( (unsigned char*)&message, sizeof(sen_wifi_sample) );
              if ( cb_sen_wifi_sample != NULL )
              {
                (*cb_sen_wifi_sample)(message);
              }
              break;
            }
			case SEN_SCAN:
			{
              byte message;
              _udpListener.read( (unsigned char*)&message, sizeof(byte) );
              if ( cb_sen_scan != NULL )
              {
                (*cb_sen_scan)();
              }
              break;
			}
			case SEN_REGISTER_CONSUMER:
			{
				sen_register_consumer message;
				_udpListener.read( (unsigned char*)&message, sizeof(sen_register_consumer) );
				if ( cb_sen_register_consumer != NULL )
				{
					(*cb_sen_register_consumer)(message);
				}
				break;
			}/*
			case SEN_PROCESSED_DATA:
			{
				_udpListener.flush();
				Serial.println("Got processed Data need to write something for this!  to pass in memory");
			}*/
			default:
			{
				Serial.println("Unknown message type");
			}
          }
        }
        else
        {
          //error
          _udpListener.flush();
          _nErrorCount++;
          Serial.println("Flushed error");
        }
      }
    };

  public:
	void set_sen_heartbeat( void(*cb_sen_heartbeat)(void) )
	{
		this->cb_sen_heartbeat = cb_sen_heartbeat;
	}
  
    void set_sen_wifi_sample( void (*cb_sen_wifi_sample)(sen_wifi_sample) )
    {
      this->cb_sen_wifi_sample = cb_sen_wifi_sample;
    };

    void set_sen_register(void (*cb_sen_register)(sen_register))
    {
      this->cb_sen_register = cb_sen_register;
    };
	
	void set_sen_timesync( void (*cb_sen_timesync)(sen_timesync))
	{
		this->cb_sen_timesync=cb_sen_timesync;
	}

	void set_sen_stop( void (*cb_sen_stop)(void))
	{
		this->cb_sen_stop=cb_sen_stop;
	}

	void set_sen_start( void (*cb_sen_start)(void))
	{
		this->cb_sen_start=cb_sen_start;
	}


	void set_sen_scan( void (*cb_sen_scan)(void))
	{
		this->cb_sen_scan=cb_sen_scan;
	}
	
	void set_register_consumer(void (*cb_sen_register_consumer)(sen_register_consumer))
	{
		this->cb_sen_register_consumer = cb_sen_register_consumer;
	}


  protected:
    WiFiUDP _udpListener;
    int _nErrorCount;

  //function pointer for time sync callback
  //this message sets the time on a sensor
  void (*cb_sen_timesync)(sen_timesync);

  //function pointer for sensor stop messsage
  //this message tells the sensor to stop collecting
  void (*cb_sen_stop)(void);

  //function pointer for sensor register message
  //this message registers a sensor with the controller
  void (*cb_sen_register)(sen_register);

  //function pointer for sample message
  //this message sends a data sample about a wifi network to the controller
  void (*cb_sen_wifi_sample)(sen_wifi_sample);

  //function pointer for scan message
  //this message tells a sensor to scan all channels look for channels with sensors or controllers on them
	void (*cb_sen_scan)(void);

  //function pointer for start message
  //this message tells the sensor to start collecting
	void (*cb_sen_start)(void);

  //function pointer register consumer message (not used any more)
	void (*cb_sen_register_consumer)(sen_register_consumer);

  //function pointer for heartbeat message
  //this message tells the sensors that they are still connected to a controller and to reset there timeout counter
	void (*cb_sen_heartbeat)(void);
};


#endif