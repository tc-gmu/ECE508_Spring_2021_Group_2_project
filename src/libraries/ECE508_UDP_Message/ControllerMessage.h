/**
 *   Class for sending and recieving messages between controller and consumer
 *   These messages are all TCP messages
 * 
 *   Note:
 *   The structs have byte pad vars for esp8266 compiler byte aligning
 * 
 * */



#ifndef CONTROLLER_MESSAGE_ECE508
#define CONTROLLER_MESSAGE_ECE508


#define ECE508_TCP_SERVER_PORT 3323
#define CON_MESSAGE_START 0xCE
#define CON_MESSAGE_GOODBYE 0xCF

#define CON_STOP 1
#define CON_START 2
#define CON_PROCESSED_DATA 3



//12
struct sen_sensor_data {
    byte _mac[6];
	byte pad[2];
    float _fValue;
};
//104
struct sen_sensor_info {
    byte _mac[6];
    byte _nReadings;
	byte pad;
    sen_sensor_data _sensor_data[8];
};
//840
struct rec_processed_data {
  byte _nSensors;
  byte pad[3];
  long _lTime;	
  sen_sensor_info _sensor_info[8];  
//  byte _pad[2];
};

//848
struct send_processed_data {
  byte messageStart;
  byte messageType;
  byte pad[2];
  rec_processed_data _data;
  byte goodbye;
  byte pad2[3];
};



/*
 * 
 * https://github.com/esp8266/Arduino/blob/master/cores/esp8266/IPAddress.h
 * https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiUdp.h
 */
class ControllerMessage
{
  public:
    ControllerMessage() : _tcpListener(ECE508_TCP_SERVER_PORT)
    {
      _nErrorCount = 0;
	  cb_con_start = NULL;
	  cb_con_stop = NULL;
	  cb_con_processed_data = NULL;
	  
	  _pProcessedDataMessage = NULL;
    };

    void setup() 
    {
		_tcpListener.begin();
    };
	
	
    /**
     * 
     * 
     * */
    int send_start(const IPAddress& sendIp)
    {
      int nCount = 0;
	  byte arVal[]= {CON_MESSAGE_START,CON_START,CON_MESSAGE_GOODBYE};
      while (!sendPacket(sendIp, arVal, sizeof(arVal)) && nCount < 4 ) 
      {
        nCount++;
      }

      if (nCount <4 )
	  {
        return 0;
	  }
	  Serial.println("Failed to send start");
      return 1;
    };

    /**
     * 
     * 
     * */
    int send_stop(const IPAddress& sendIp)
    {
      int nCount = 0;
	  byte arVal[]= {CON_MESSAGE_START,CON_STOP,CON_MESSAGE_GOODBYE};
      while (!sendPacket(sendIp, arVal, sizeof(arVal)) && nCount < 4 ) 
      {
        nCount++;
      }

      if (nCount <4 )
	  {
        return 0;
	  }
	  Serial.println("Failed to send stop");
      return 1;
    };
	
	
	int sendPacket(const IPAddress& sendIp, const uint8_t *buffer, size_t size  )
	{
		WiFiClient client;
		
		int nCount = 0;
		while ( !client.connect(sendIp, ECE508_TCP_SERVER_PORT) && nCount < 4 )
		{
			Serial.println("Trying again");
			nCount++;
		}
		if ( nCount > 3 )
		{
			Serial.println("ControllerMessage failed to client.connect");
			return 0;
		}

		if ( size > 0 )
		{
			int nVal = client.write( buffer, size ); 
			if (nVal != size )
			{
				Serial.println("Failed to write message body");
				client.stop();
				return 0;
			}
		}
		
		client.stop();
		return 1;
	}


    int sendPacket(const IPAddress& sendIp, byte messageType, const uint8_t *buffer, size_t size )
    {
		WiFiClient client;
		
		int nCount = 0;
		while ( !client.connect(sendIp, ECE508_TCP_SERVER_PORT) && nCount < 4 )
		{
			Serial.println("Trying again");
			nCount++;
		}
		if ( nCount > 3 )
		{
			Serial.println("ControllerMessage failed to client.connect");
			return 0;
		}

		int nVal = client.write( CON_MESSAGE_START );
		if (nVal != 1 )
		{
			Serial.println("Failed to write message start");
			client.stop();
			return 0;
		}
		nVal = client.write( messageType );
		if (nVal != 1 )
		{
			Serial.println("Failed to write message type");
			client.stop();
			return 0;
		}
		if ( size > 0 )
		{
			nVal = client.write( buffer, size ); 
			if (nVal != size )
			{
				Serial.println("Failed to write message body");
				client.stop();
				return 0;
			}
		}
		nVal = client.write( CON_MESSAGE_GOODBYE );
		if (nVal != 1 )
		{
			Serial.println("Failed to write message goodbye");
			client.stop();
			return 0;
		}
		
		client.stop();
		return 1;
    };
	
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
		Serial.println("Timed out c mesages:");//  + String(millis()-_ulTimeOut) );
		_ulTimeOut = 0;
		return true;
	  }

	  return false;
		
	}
	
	int send_processedData(const IPAddress& sendIp,  send_processed_data& message)
    {
      int nCount = 0;
	  message.messageStart = CON_MESSAGE_START;
	  message.messageType = CON_PROCESSED_DATA;
	  message.goodbye = CON_MESSAGE_GOODBYE;
	  message.pad[0] = 1;
	  message.pad[1] = 2;
	  message.pad2[0] = 3;
	  message.pad2[1] = 4;
	  message.pad2[2] = 5;
	  
	  
      while (!sendPacket(sendIp, (const uint8_t *)&message, sizeof(send_processed_data)) && nCount < 4 ) 
	  {
        nCount++;
      }

      if (nCount <4 )
        return 1;
	  Serial.println("Failed to send to " + sendIp.toString());
      return 0;
    };
	
	
	
	//function that checks for any incoming messages.
	//needs to be called each time loop is called
    void checkMessage()
    {
		WiFiClient cl = _tcpListener.available();
		this->_ulTimeOut = 0;

		if ( cl )
		{
			while ( (cl.connected() || cl.available() >0 ) && !this->isTimedOut(10000)) //sometimes the messge is in the buffer so the connection is already closed  but bytes are available
			{
				if ( cl.available() >2 )//make sure we have atleast start type and end
				{
					byte val = cl.read();
					if ( val == CON_MESSAGE_START )
					{
						val = cl.read();
						switch(val)
						{
							case CON_STOP://client would get this from controller
							{
								Serial.println("STOP REC");
								if ( cl.read() != CON_MESSAGE_GOODBYE )
								{
									Serial.println("Bad message end CON STOP");
									//should do something !
								}
								if ( cb_con_stop != NULL )
								{
									(*cb_con_stop)();
								}
									
								cl.stop();
								return;
							}
							case CON_START://client would get this from controller
							{
								Serial.println("START REC");
								IPAddress ip = cl.remoteIP();
								
								if ( cl.read() != CON_MESSAGE_GOODBYE )
								{
									Serial.println("Bad message end CON START");
									//should do something
								}
								if ( cb_con_start != NULL )
								{
									(*cb_con_start)(ip);
								}
								cl.stop();
								return;
							}
							case CON_PROCESSED_DATA:
							{
//								int nGrand = 2;
//								Serial.println("Got processed data: " + String(sizeof(rec_processed_data)) + " " + String(sizeof(send_processed_data) ));
//								Serial.println("SIZE: " + String(sizeof(sen_sensor_data)) + " " + String(sizeof(sen_sensor_info) ));
								if ( _pProcessedDataMessage != NULL )
								{
									//need to read two pads
//									nGrand += 1;
									if ( cl.read() != 1)
									{
										Serial.println("Bad Pad");
									}
//									nGrand += 1;
									if ( cl.read() != 2)
									{
										Serial.println("Bad Pad2");
									}
									
									unsigned char* pData = (unsigned char*)_pProcessedDataMessage;
									int nRead = cl.read( pData, sizeof(rec_processed_data) );
									int nTotal = nRead;
									while ( nRead>0 && (nTotal) < sizeof(rec_processed_data)  )
									{
										while ( nTotal < sizeof(rec_processed_data)  && cl.available() < 1 )
										{
											if ( this->isTimedOut(10000) )
											{
												Serial.println("Timed out! CON_PROCESSED_DATA ");
												cl.stop();
												return;
											}
//											Serial.println("Waiting");
										}
//										Serial.println("Reading extra");
										pData = ( (unsigned char*)_pProcessedDataMessage ) + (nTotal);
										nRead = cl.read( pData, sizeof(rec_processed_data) - nTotal );
										nTotal = (nTotal+nRead);
									}
									if (nTotal !=  sizeof(rec_processed_data))
									{
										Serial.println("Processed data did not read all data:" + String( nTotal) + " " + String(sizeof(rec_processed_data) ) );
										cl.flush();
										cl.stop();
										return;
									}
									
	//								nGrand += nTotal;
									while ( cl.available() < 1 )
									{
//										Serial.println("Waiting for end");
										if ( this->isTimedOut(10000) )
										{
											Serial.println("Timed out! CON_PROCESSED_DATA ");
											cl.stop();
											return;
										}
									}
		//							nGrand += 1;
									byte nVal=cl.read(); 
									if (  nVal != CON_MESSAGE_GOODBYE )
									{
										Serial.println("Bad message end CON_PROCESSED_DATA " + String(nVal) );
										//should do something
									}
									//should be three padding but do not care
/*									while( cl.available() > 0 )
									{
										Serial.println(  String(cl.read() ) );
										nGrand += 1;

									}
									
									Serial.println("Total:" + String(nGrand));
	*/								
									if ( cb_con_processed_data != NULL )
									{
										(*cb_con_processed_data)(_pProcessedDataMessage);
									}
									cl.flush();
									cl.stop();
									return;
								}
								else
								{
									Serial.println("No data memory for PROCSSED Data");
									cl.flush();
									cl.stop();
									return;
								}
								
							}							/*
							case CON_PROCESSED_DATA:
							{
								Serial.println("Got processed data");
								if ( _pProcessedDataMessage != NULL )
								{
									unsigned char* pData = NULL;
									int nTotal = 0;
									int nRead = cl.read( (unsigned char*)_pProcessedDataMessage, sizeof(rec_processed_data) );
									nTotal = nRead;
									while ( nRead>0 && (nTotal) < sizeof(rec_processed_data)  )
									{
										while ( nTotal < sizeof(rec_processed_data)  && cl.available() < 1 )
										{
											Serial.println("Waiting");
										}
										Serial.println("Reading extra");
										pData = ( (unsigned char*)_pProcessedDataMessage ) + (nTotal);
										nRead = cl.read( pData, sizeof(rec_processed_data) - nTotal );
										nTotal = (nTotal+nRead);
									}
									if (nTotal !=  sizeof(rec_processed_data))
									{
										Serial.println("Processed data did not read all data:" + String( nTotal) + " " + String(sizeof(rec_processed_data) ) );
										cl.flush();
										cl.stop();
										return;
									}
									
									while ( cl.available() < 1 )
									{
										Serial.println("Waiting for end");
									}
									byte nVal=cl.read(); 
									if (  nVal != CON_MESSAGE_GOODBYE )
									{
										Serial.println("Bad message end CON_PROCESSED_DATA " + String(nVal) );
										//should do something
									}
									
									while( cl.available() > 0 )
									{
										Serial.println(  String(cl.read() ) );
									}
									
									if ( cb_con_processed_data != NULL )
									{
										(*cb_con_processed_data)(_pProcessedDataMessage);
									}
									cl.stop();
									return;
								}
								else
								{
									Serial.println("No data memory for PROCSSED Data");
									cl.flush();
									cl.stop();
									return;
								}
								
							}
							*/
							default:
							{
								Serial.println("Bad Message:" + String(val) );
								cl.stop();
								return;
							}
						}
					}
					else
					{
						Serial.println("Not message start:" + String(val));
						if ( cl && cl.connected() )
							cl.stop();
						return;
					}
				}
			}
			if ( cl && cl.connected() )
				cl.stop();
		}
	};
			



  public:
	void set_con_processed_data( void(*cb_con_processed_data)(rec_processed_data*), rec_processed_data* pProcessedData )
	{
		this->cb_con_processed_data = cb_con_processed_data;
		this->_pProcessedDataMessage = pProcessedData;
	}
  
	void set_con_start( void (*cb_con_start)(IPAddress))
	{
		this->cb_con_start=cb_con_start;
	};


	void set_con_stop( void (*cb_con_stop)(void))
	{
		this->cb_con_stop=cb_con_stop;
	};



  protected:
	WiFiServer _tcpListener;//(323);

    int _nErrorCount;
	rec_processed_data* _pProcessedDataMessage;

	//call back function pointer for the stop message
    void (*cb_con_stop)(void); 
	//call back function pointer for the start message
	void (*cb_con_start)(IPAddress);
	//call back function pointer for the receipt of data
	void (*cb_con_processed_data)(rec_processed_data*);
};


#endif