 /*----------------------------------------------------------------------------------------------------
  Project Name : Solar Powered WiFi Weather Station V2.34
  Features: temperature, dewpoint, dewpoint spread, heat index, humidity, absolute pressure, relative pressure, battery status and
  Authors: Keith Hungerford, Debasish Dutta and Marc Stähli (modified by Patrick)
  Website : www.opengreenenergy.com
  Github: https://github.com/3KUdelta/Solar_WiFi_Weather_Station
  Modified: https://github.com/PatrickSt1991/solarWeatherStation

/****** WiFi Settings ********************************************************/

char ssid[] = "********";                           // WiFi Router ssid
char pass[] = "********";             // WiFi Router password

/****** MQTT Settings ********************************************************/

const char* mqtt_server = "192.168.0.***";      // MQTT Server (broker) address


/****** Additional Settings **************************************************/

#define LANGUAGE 'NL'               //check translation.h for available languages. Currently EN/DE/FR/IT/PL/RO/SP/TR/NL/NO

#define TEMP_CORR (-1)              //Manual correction of temp sensor (mine reads 1 degree too high)

#define ELEVATION (505)             //Enter your elevation in m ASL to calculate rel pressure (ASL/QNH) at your place

#define sleepTimeMin (10)           //setting of deepsleep time in minutes (default: 10)

// NTP   --> Just a remark - the program needs the time only for the timestamp, so for the Zambretti forecast
//           the timezone and the DST (Daylight Saving Time) is irrelevant. This is why I did not take care of DST 
//           in the code. I saw a fork on Github (truckershitch) which I believe has covered this.

#define NTP_SERVER      "nl.pool.ntp.org"
#define TZ              1           // (utc+) TZ in hours
#define DST_MN          60          // use 60mn for summer time in some countries

#define TZ_SEC          ((TZ)*3600)  // don't change this
#define DST_SEC         ((DST_MN)*60)// don't change this
