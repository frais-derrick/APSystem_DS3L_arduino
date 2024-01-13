/*
    This sketch establishes a TCP connection to a "quote of the day" service.
    It sends a "hello" message, and then prints received data.
*/


//// INSPIRED FROM : https://github.com/ksheumaker/homeassistant-apsystems_ecur


#include <WiFi.h>
//#include "settings_fred.h" 
#include "settings.h"   // all your ids !

#ifndef STASSID
#define STASSID USER_STASSID
#define STAPSK  USER_STAPSK
#endif

const char* ssid 	    = STASSID;				  // WIFI SSID
const char* password  = STAPSK;				    // WIFI PASSWORD

const char* ecu_id 	  = USER_ECU_ID;	    // ECU ID
const char* ecu_id_ip = USER_ECU_IP;  	  // IP of ECU
const uint16_t port   = USER_ECU_PORT;	  // STD ECU PORT

#define REQUEST_DELAY   USER_POLLING_RATE

#define START_REQUEST_RAW	"APS1100160001"
#define START_REQUEST_INV "APS1100280002"
#define START_REQUEST_SIG "APS1100280030"
#define STOP_REQUEST		  "END\n"

#define DEFAULT_BUFLEN 1024

//#define DEBUG_CON			  // !! OH 'CON', DEBUG THE CONNECTION...
//#define DEBUG_ECU       // !! DEBUG ECU MESSAGES
//#define DEBUG_PROTOCOL    // !! DEBUG ECU DATA EXTRACTION

#define MAX_INVERTERS   2

/**
 * \brief struct of each inverter info
 *
 * \param 	uid					identifier of the inverter
 * \param 	online				is online
 * \param 	reserved			unused
 * \param 	line_frequency		network frequency
 * \param 	temperature			inverter temperature
 * \param 	ds3l_power_1		power in W
 * \param 	ds3l_voltage_1		AC in volts
 * \param 	ds3l_power_2		power in W
 * \param 	ds3l_voltage_2		AC in volts
 */
struct inverter_data
{
    unsigned char   uid[6];
    unsigned char   online;
    unsigned char   reserved[2];
    unsigned char   line_frequency[2];
    unsigned char   temperature[2];
    unsigned char   ds3l_power_1[2];
    unsigned char   ds3l_voltage_1[2];
    unsigned char   ds3l_power_2[2];
    unsigned char   ds3l_voltage_2[2];
};

/**
 * \brief struct of each inverter info
 *
 * \param 	uid					identifier of the inverter
 * \param 	signal				zigbee signal strenght
 */
struct inverter_radio
{
    unsigned char   uid[6];
    unsigned char   signal;
};



/**
 * \brief struct of each inverter info MEM
 *
 * \param 	uid					    identifier of the inverter
 * \param 	online				  is online
  * \param 	line_frequency	network frequency
 * \param 	temperature			inverter temperature
 * \param 	ds3l_power_1		power in W
 * \param 	ds3l_voltage_1	AC in volts
 * \param 	ds3l_power_2		power in W
 * \param 	ds3l_voltage_2	AC in volts
 */
struct inverter_data_norm
{
    char            uid[13];
    unsigned char   online;
    float           line_frequency;
    float           temperature;
    unsigned short  ds3l_power_1;
    unsigned short  ds3l_voltage_1;
    unsigned short  ds3l_power_2;
    unsigned short  ds3l_voltage_2;
};

/**
 * \brief struct of each inverter info
 *
 * \param 	uid					identifier of the inverter
 * \param 	signal				zigbee signal strenght
 */
struct inverter_radio_norm
{
    char            uid[13];
    unsigned short  signal;
};

/**
 * \brief struct time stamps
 *
 * \param 	year					
 * \param 	month;
 * \param 	day;
 * \param 	hour;
 * \param 	minute;
 * \param 	second;   			
 */
struct mytimestamp
{
    unsigned short  year;
    unsigned short  month;
    unsigned short  day;
    unsigned short  hour;
    unsigned short  minute;
    unsigned short  second;    
};

/**
 * \brief struct global for ECUs and INVERTERS
 *
 * \param 	ecu_id					id of ECU gateway
 * \param 	currentPower		instant karma power
 * \param 	dayPower		    lifetime power
 * \param 	curtime		      current time
 * \param 	inv_data		    inverters data
 * \param 	inv_radio		    inverters radio
 */
struct ecu_datas
{
  char                ecu_id[15];
  unsigned long       currentPower;
  float               dayPower;
  mytimestamp         curtime;
  unsigned short      inverters_count;
  inverter_data_norm  inv_data [MAX_INVERTERS];
  inverter_radio_norm inv_radio[MAX_INVERTERS];
};

/**
 *
 * \brief APSystemsSocket class
 *
 */
class APSystemsSocket
{
public:
    APSystemsSocket(const char *ip, long port);
    ~APSystemsSocket();

    int  QueryECU(const char* request);
    int  ProcessData();
    void DisplayData();

private:
    int Open();
    int Close();

    unsigned long charToLong(char* buf);
    unsigned long AsciiToLong(char* buf);

private:

    WiFiClient client;
    

    char sendbuf[DEFAULT_BUFLEN];
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    int lastreclen = 0;
    char loc_ip[32];
    long loc_port;

    ecu_datas   ecuData;
  
};

/**
 *
 * \brief   constructor
 *
 * \param 	ip					string with ECU ip
 * \param 	port				string with ECU port
 */
APSystemsSocket::APSystemsSocket(const char* ip, long port)
{
    strcpy(loc_ip, ip);
    loc_port = port;
}

/**
 *
 * \brief   destructor
 *
 */
APSystemsSocket::~APSystemsSocket()
{
}

/**
 * \brief   charToLong
 *
 * \param 	4 char buffer with long value : buf: 00 01 02 03 : 123(h)
 * \return	converted long
 */
unsigned long APSystemsSocket::charToLong(char* buf)
{
  unsigned long ulconv = 0;
  ulconv |= (((*(buf))     & 0xFF) << 24);
  ulconv |= (((*(buf + 1)) & 0xFF) << 16);
  ulconv |= (((*(buf + 2)) & 0xFF) <<  8);
  ulconv |= (((*(buf + 3)) & 0xFF)      );

  return ulconv;
}

/**
 * \brief   AsciiToLong
 *
 * \param 	4 char buffer with long value : buf: 30 31 31 33 : '0' '1' '2' 3' => 123(d)
 * \return	converted long
 */
unsigned long APSystemsSocket::AsciiToLong(char* buf)
{
  unsigned long ulconv = 0;
  ulconv += (*(buf    ) - '0') * 1000;
  ulconv += (*(buf + 1) - '0') * 100;
  ulconv += (*(buf + 2) - '0') * 10;
  ulconv += (*(buf + 3) - '0') * 1; 

  return ulconv;
}

/**
 * \brief   Open the socket to ECU
 *
 * \param 	none
 * \return	0: KO, else OK
 */
int APSystemsSocket::Open()
{
#ifdef DEBUG_CON
  Serial.print("connecting to ");
  Serial.print(loc_ip);
  Serial.print(':');
  Serial.println(loc_port);
#endif

  // Use WiFiClient class to create TCP connections
  if (!client.connect(loc_ip, loc_port)) 
  {
    Serial.println("connection failed");
    delay(5000);
    return 0;
  } 
#ifdef DEBUG_CON
  Serial.println("client connected");
#endif
  return 1;
}

/**
 * \brief   Close the socket to ECU
 *
 * \param 	none
 * \return	0: KO, else OK
 */
int APSystemsSocket::Close()
{
  if(client.connected())
  {
    // Close the connection
#ifdef DEBUG_CON
    Serial.println();
    Serial.println("closing connection");
#endif
    client.stop();
	return 1;
  }    
  return 0;
}

/**
 * \brief   QueryECU
 *
 * \param 	string with request (3 coded)
 * \return	0: KO, else OK
 */
int APSystemsSocket::QueryECU(const char* request)
{
    if (Open() != 0)
    {
      // buffer copy      
      strcpy(sendbuf, request);

      // Send an initial buffer
      client.println(sendbuf);

      // wait for data to be available
      unsigned long timeout = millis();
      while (client.available() == 0) 
      {
        if (millis() - timeout > 5000) 
        {
          Serial.println(">>> Client Timeout !");
          Close();
          delay(30000);   /// 30s
          return 0;
        }
      } 

      // Read all the lines of the reply from server and print them to Serial
#ifdef DEBUG_CON
      Serial.println("receiving from remote server");
#endif
      // not testing 'client.connected()' since we do not need to send data here
      memset(recvbuf, 0 , DEFAULT_BUFLEN);
      lastreclen = 0;
      while (client.available()) 
      {
        char ch = static_cast<char>(client.read());
        recvbuf[lastreclen]=ch;
        lastreclen++;    
#ifdef DEBUG_CON
        Serial.print(ch);
#endif
      }
#ifdef DEBUG_CON
      Serial.print(lastreclen);
      Serial.println(" bytes received");
#endif      

      Close();

      return 0;
    }
    else
    {
      return 1;
    }
}

/**
 * \brief   ProcessData of the previous reception stream
 *
 * \param 	none
 * \return	0: KO, else OK
 */
int  APSystemsSocket::ProcessData()
{
    if (lastreclen == 0)  return 0;

    /// CHECK START FRAME
    if (strncmp(recvbuf, "APS", 3) != 0)
    {
        Serial.println("ERR : not valid START frame");
        return 0;
    }
    else
    {
#ifdef DEBUG_ECU		
       Serial.println("Valid START frame");
#endif
    }

    /// CHECK STOP FRAME
    if (strncmp(recvbuf+ lastreclen-4, "END\n", 4) != 0)
    {
        Serial.println("ERR : not valid END frame");
        return 0;
    }
    else
    {
#ifdef DEBUG_ECU
      Serial.println("Valid END frame");
#endif
    }

    /// CHECK CHECKSUM
    char* csm = recvbuf + 5;       /// 5 : DATA OFFSET
    unsigned long vcsm = 0;    
    vcsm = AsciiToLong(csm);

    if (vcsm != (lastreclen-1))
    {
        Serial.println("ERR : not valid CHECKSUM frame");
        return 0;
    }
    else
    {
#ifdef DEBUG_ECU
      Serial.println("Valid CHECKSUM");
#endif
    }	

    /// CHECK ANSWER ID 
    char* ansid = recvbuf + 9;       /// 9 : DATA OFFSET
    unsigned long ulansid = 0;

    ulansid = AsciiToLong(ansid);

	  /// SWITCH ANSWER ID
    if (ulansid == 1)
    {
#ifdef DEBUG_PROTOCOL      
        Serial.println("POWER GLOBAL REQUEST\r\n");
#endif

        /// GET ECU ID       
        strncpy(ecuData.ecu_id, recvbuf + 13, 12);
        ecuData.ecu_id[12] = '\0';
#ifdef DEBUG_PROTOCOL      
        Serial.print("ECU ID : ");
        Serial.println(ecuData.ecu_id);
#endif

        /// GET LIFE TIME ENERGY
        char* lte = (recvbuf + 27);       /// 27 : DATA OFFSET
        unsigned long ullte = 0;
        ullte = charToLong(lte);
        ecuData.dayPower = (float)(ullte) / 10.0f;
#ifdef DEBUG_PROTOCOL      
        Serial.print("LIFETIME_POWER : ");
        Serial.print(ecuData.dayPower);
        Serial.print(" Wh\r\n");
#endif

        /// GET CURRENT ENERGY
        char* ce = (recvbuf + 31);        /// 31 : DATA OFFSET
        unsigned long ulce = 0;
        ulce = charToLong(ce);
        ecuData.currentPower = ulce;
#ifdef DEBUG_PROTOCOL      
        Serial.print("CURRENT POWER  : ");
        Serial.print(ecuData.currentPower);
        Serial.print(" W\r\n");
#endif
    }
    else if (ulansid == 2)
    {
#ifdef DEBUG_PROTOCOL            
        Serial.print("POWER INVERTERS ECU ID REQUEST\r\n");
#endif
        int msb_year = *(recvbuf + 19);     // 20
        int lsb_year = *(recvbuf + 20);     // 24 for 2024

        int month    = *(recvbuf + 21);     // month
        int day      = *(recvbuf + 22);     // day

        int hours    = *(recvbuf + 23);     // hours
        int minutes  = *(recvbuf + 24);     // minutes
        int seconds  = *(recvbuf + 25);     // seconds

        ecuData.curtime.year   = 0;   // todo convert  0x20 0x24 => 2024 :-)
        ecuData.curtime.month  = 0;
        ecuData.curtime.day    = 0;
        ecuData.curtime.hour   = 0;
        ecuData.curtime.minute = 0;
        ecuData.curtime.second = 0;

#ifdef DEBUG_PROTOCOL      
        Serial.print(" timeStamp  : ");
        Serial.print(msb_year, HEX);
        Serial.print(lsb_year, HEX);
        Serial.print(".");
        Serial.print(month, HEX);
        Serial.print(".");
        Serial.print(day, HEX);
        Serial.print("  ");
        Serial.print(hours, HEX);
        Serial.print(":");
        Serial.print(minutes, HEX);
        Serial.print(":");
        Serial.print(seconds, HEX);
        Serial.print("\r\n");
#endif

        inverter_data *data;
        data = (inverter_data*)(recvbuf + 26);

        ecuData.inverters_count=0;
        int data_struct_count=0;
        while (1)
        {
            char inv_uid[13];
            sprintf(inv_uid, "%02X%02X%02X%02X%02X%02X", data->uid[0], data->uid[1], data->uid[2], data->uid[3], data->uid[4], data->uid[5]);      
            strcpy(ecuData.inv_data[data_struct_count].uid, inv_uid);

			      /// CHECK DS3L, other not coded
            if (inv_uid[0] == '7' && inv_uid[1] == '0' && inv_uid[2] == '3')
            {
#ifdef DEBUG_PROTOCOL                    
                Serial.print(" uid           : ");
                Serial.print(inv_uid);
                Serial.print("\r\n");
#endif

                ecuData.inv_data[data_struct_count].online =  data->online;

                unsigned short line_freq = ((data->line_frequency[0] & 0xFF) << 8) | (data->line_frequency[1] & 0xFF);
                float fline_freq = (float)(line_freq) / 10.0f;
                ecuData.inv_data[data_struct_count].line_frequency = fline_freq;
#ifdef DEBUG_PROTOCOL                      
                Serial.print(" frequency     : ");
                Serial.print(fline_freq);
                Serial.print(" Hz\r\n");
#endif
                unsigned short temperature = ((data->temperature[0] & 0xFF) << 8) | (data->temperature[1] & 0xFF);
                float ftemperature = (float)(temperature)-100.0f;
                ecuData.inv_data[data_struct_count].temperature = ftemperature;
#ifdef DEBUG_PROTOCOL                      
                Serial.print(" temperature   : ");
                Serial.print(ftemperature);
                Serial.print(" deg\r\n");
#endif
                unsigned short power_01 = ((data->ds3l_power_1[0] & 0xFF) << 8) | (data->ds3l_power_1[1] & 0xFF);
                ecuData.inv_data[data_struct_count].ds3l_power_1 = power_01;
#ifdef DEBUG_PROTOCOL                      
                Serial.print(" power inv01   :");
                Serial.print(power_01);
                Serial.print(" W\r\n");
#endif
                unsigned short power_02 = ((data->ds3l_power_2[0] & 0xFF) << 8) | (data->ds3l_power_2[1] & 0xFF);
                ecuData.inv_data[data_struct_count].ds3l_power_2 = power_02;
#ifdef DEBUG_PROTOCOL                      
                Serial.print(" power inv02   :");
                Serial.print(power_02);
                Serial.print(" W\r\n");
#endif
                unsigned short voltage_01 = ((data->ds3l_voltage_1[0] & 0xFF) << 8) | (data->ds3l_voltage_1[1] & 0xFF);
                ecuData.inv_data[data_struct_count].ds3l_voltage_1 = voltage_01;
#ifdef DEBUG_PROTOCOL                      
                Serial.print(" voltage inv01 :");
                Serial.print(voltage_01);
                Serial.print(" V\r\n");
#endif
                unsigned short voltage_02 = ((data->ds3l_voltage_2[0] & 0xFF) << 8) | (data->ds3l_voltage_2[1] & 0xFF);
                ecuData.inv_data[data_struct_count].ds3l_voltage_2 = voltage_02;
#ifdef DEBUG_PROTOCOL                      
                Serial.print(" voltage inv02 :");
                Serial.print(voltage_02);
                Serial.print(" V\r\n");

                Serial.print(" \r\n");
#endif                
                data++;
                data_struct_count++;
                ecuData.inverters_count++;

                if(data_struct_count>=MAX_INVERTERS)
                  break; 
            }
            else
            {
#ifdef DEBUG_ECU				
                Serial.print(" End of data (or not available data for inverters)\r\n");
#endif				
                break;
            }
        }
    }
    else if (ulansid == 30)
    {
#ifdef DEBUG_PROTOCOL            
        Serial.print("SIGNAL INVERTERS ECU ID REQUEST\r\n");
#endif

        inverter_radio* data;
        data = (inverter_radio*)(recvbuf + 15);

        int data_struct_count=0;
        while (1)
        {
            char inv_uid[13];
            sprintf(inv_uid, "%02X%02X%02X%02X%02X%02X", data->uid[0], data->uid[1], data->uid[2], data->uid[3], data->uid[4], data->uid[5]);
            strcpy(ecuData.inv_radio[data_struct_count].uid, inv_uid);

      			/// CHECK DS3L, other not coded
            if (inv_uid[0] == '7' && inv_uid[1] == '0' && inv_uid[2] == '3')
            {
#ifdef DEBUG_PROTOCOL                    
                Serial.print(" uid           :");
                Serial.print(inv_uid);
                Serial.print("\r\n");
#endif
                unsigned long power_signal = ((data->signal&0xFF) * 100) / 255;
                ecuData.inv_radio[data_struct_count].signal = power_signal;
#ifdef DEBUG_PROTOCOL      
                Serial.print(" power         : ");
                Serial.print(power_signal);
                Serial.print(" dB\r\n");

                Serial.print(" \r\n");
#endif
                data++;
                data_struct_count++;

                if(data_struct_count>=MAX_INVERTERS)
                  break; 
            }
            else
            {
#ifdef DEBUG_CON								
                Serial.print(" End of data (or not available data for inverters)\r\n");
#endif
                break;
            }           
        }
    }        
    else
    {
      Serial.println("ERR : Invalid Request Answer");
    }
  
    return  ulansid;
}

/**
 * \brief   DisplayData
 *
 * \param 	none
 * \return	none
 */
void  APSystemsSocket::DisplayData()
{
  /*
  struct inverter_data_norm
{
    char            uid[13];
    unsigned char   online;
    float           line_frequency;
    float           temperature;
    unsigned short  ds3l_power_1;
    unsigned short  ds3l_voltage_1;
    unsigned short  ds3l_power_2;
    unsigned short  ds3l_voltage_2;
};

struct inverter_radio_norm
{
    char            uid[13];
    unsigned short  signal;
};

struct mytimestamp
{
    unsigned short  year;
    unsigned short  month;
    unsigned short  day;
    unsigned short  hour;
    unsigned short  minute;
    unsigned short  second;    
};

struct ecu_datas
{
  char                ecu_id[15];
  unsigned long       currentPower;
  float               dayPower;
  mytimestamp         curtime;
  unsigned short      inverters_count;
  inverter_data_norm  inv_data [MAX_INVERTERS];
  inverter_radio_norm inv_radio[MAX_INVERTERS];
};
*/
  Serial.print("ECU ID : ");
  Serial.println(ecuData.ecu_id);

  Serial.print("LIFETIME_POWER : ");
  Serial.print(ecuData.dayPower);
  Serial.print(" Wh\r\n");
		
	Serial.print("CURRENT POWER  : ");
  Serial.print(ecuData.currentPower);
  Serial.print(" W\r\n");
				
	Serial.print(" timeStamp  : ");
  Serial.print(ecuData.curtime.year);
  Serial.print(".");
  Serial.print(ecuData.curtime.month);
  Serial.print(".");
  Serial.print(ecuData.curtime.day);
  Serial.print("  ");
  Serial.print(ecuData.curtime.hour);
  Serial.print(":");
  Serial.print(ecuData.curtime.minute);
  Serial.print(":");
  Serial.print(ecuData.curtime.second);
  Serial.print("\r\n");

	for(int data_struct_count=0; data_struct_count<ecuData.inverters_count; data_struct_count++)
	{
    Serial.print(" uid           : ");
    Serial.print(ecuData.inv_data[data_struct_count].uid);
    Serial.print("\r\n");

    Serial.print(" is online ?   : ");
    Serial.print((ecuData.inv_data[data_struct_count].online?"YES":"NO"));
    Serial.print("\r\n");

    Serial.print(" frequency     : ");
    Serial.print(ecuData.inv_data[data_struct_count].line_frequency);
    Serial.print(" Hz\r\n");

    Serial.print(" temperature   : ");
    Serial.print(ecuData.inv_data[data_struct_count].temperature);
    Serial.print(" deg\r\n");

    Serial.print(" power inv01   : ");
    Serial.print(ecuData.inv_data[data_struct_count].ds3l_power_1);
    Serial.print(" W\r\n");

    Serial.print(" power inv02   : ");
    Serial.print(ecuData.inv_data[data_struct_count].ds3l_power_2);
    Serial.print(" W\r\n");

    Serial.print(" voltage inv01 : ");
    Serial.print(ecuData.inv_data[data_struct_count].ds3l_voltage_1);
    Serial.print(" V\r\n");

    Serial.print(" voltage inv02 : ");
    Serial.print(ecuData.inv_data[data_struct_count].ds3l_voltage_2);
    Serial.print(" V\r\n");

    Serial.print(" rssi          : ");
    Serial.print( ecuData.inv_radio[data_struct_count].signal);
    Serial.print(" dB\r\n");

    Serial.print("\r\n");	
	}
}


/**
 * \brief   setup : Arduino setup routine
 *
 * \param 	none
 * \return	none
 */
void setup() {

  /// SERIAL LINE DEBUG
  Serial.begin(115200);

  /// STATUS LED
  pinMode(LED_BUILTIN, OUTPUT);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(500);                       // wait for an half second
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    delay(500);                       // wait for an half second
  }
  digitalWrite(LED_BUILTIN, HIGH);    // connected
  
  /// NOW CONNECTED
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  /// LET TALKS TO DS3L
}

/**
 * \brief   setup : Arduino loop routine
 *
 * \param 	none
 * \return	none
 */
void loop() {

    /// string for CMD
    String command_string;
	  
    /// RECONNECT then DISCONNECT each time (mandatory for some ECU)
    APSystemsSocket my_ecu(ecu_id_ip, port);
	  
    /// Create CMD RAW
	  command_string = START_REQUEST_RAW;
    command_string+= STOP_REQUEST;

    my_ecu.QueryECU(command_string.c_str());
    my_ecu.ProcessData();
	
    /// Create CMD INVERTED
	  command_string = START_REQUEST_INV;
    command_string+= ecu_id ;
    command_string+= STOP_REQUEST;

    my_ecu.QueryECU(command_string.c_str());
    my_ecu.ProcessData();
    
    /// Create CMD SIGNAL POWER
	  command_string = START_REQUEST_SIG;
    command_string+= ecu_id ;
    command_string+= STOP_REQUEST;

    my_ecu.QueryECU(command_string.c_str());
    my_ecu.ProcessData();

    /// Display all
    my_ecu.DisplayData();
    
    /// WAIT some seconds
    delay(REQUEST_DELAY);

}

