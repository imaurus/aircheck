//INCLUDES

#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2cScd30.h>
#include <sps30.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "time.h"
#include <WiFiManager.h>

///////////////////////////////////

// ThingSpeak
String thing_api_key = "INSERT API KEY";
//#define SEND_DATA_TO_THINGSPEAK // <---- UNCOMMENT THIS TO ENABLE ThingSpeak. Don-t forget to insert you API Key.
WiFiClient client;

// Display setup (GDEW029Z10 - 2.9" 3-color)
GxEPD2_3C<GxEPD2_290c, GxEPD2_290c::HEIGHT> display(GxEPD2_290c(/*CS=*/5, /*DC=*/25, /*RST=*/26, /*BUSY=*/4));

//WIFI
WiFiManager wm;

//TIME
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "CET-1CEST,M3.5.0/2,M10.5.0/3";  //SET YOUR TIMEZONE using this abbreviation: https://remotemonitoringsystems.ca/time-zone-abbreviations.php


// SPS30 object
SPS30 sps30;
int pm1;
int pm2;
int pm4;
int pm10;
float partsize;


// SCD30 object
SensirionI2cScd30 sensor;
static char errorMessage[128];
static int16_t error;
#define altitudine 45 //For more accuracy insert meters above the sea of the place you are living in.



void setup() { 
    
  Serial.begin(115200);

delay (2000);

/////////////////////////////////////////////DISPLAY
 display.init();
 display.setRotation(3); // orizzontale: 1 o 3
  
 Serial.println("Display initialization... ");


  delay(1000);

/////////////////////////////////////////////WIFI

WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

//wm.resetSettings();

bool res;
res = wm.autoConnect("AirCheck_Config"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect to WIFI");
        

          display.firstPage();
             
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);  

      display.setTextSize(2);
  
      display.setCursor(57, 18);
      display.print("AirCheck v1.0");
      
      display.setCursor(10, 58);
      display.print("Config WIFI to continue");

      display.setCursor(22, 98);
      display.print("SSID: AirCheck_Config");

      display.setFont(&FreeMonoBold12pt7b);
      display.nextPage();
      
    } else {
      
        //if you get here you have connected to the WiFi    
        Serial.println("WIFI connected...yeey :)");

          display.firstPage();
             
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);  

      display.setTextSize(3);
     
      display.setCursor(37, 55);
      display.print("AirCheck v1.0");

      display.setFont(&FreeMonoBold12pt7b);
      display.nextPage();
      
    }



delay (1000);


///////////////////////////////////////////////////TIME
  // Imposta fuso orario italiano con ora legale
  configTzTime(timeZone, ntpServer);


/////////////////////////////////////////////scd30

    delay (5000);

    Wire.begin(27,14);
    sensor.begin(Wire, SCD30_I2C_ADDR_61);

    sensor.stopPeriodicMeasurement();
    sensor.softReset();
    sensor.setAltitudeCompensation(altitudine); 
    delay(2000);
    uint8_t major = 0;
    uint8_t minor = 0;
    error = sensor.readFirmwareVersion(major, minor);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute readFirmwareVersion(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    Serial.print("Detected SCD30. Firmware version: ");
    Serial.print(major);
    Serial.print(".");
    Serial.println(minor);
    
    error = sensor.startPeriodicMeasurement(0);
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute startPeriodicMeasurement(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }
    else
    {
      Serial.println(F("SCD30: Measurement started"));      
    }
  
/////////////////////////////////////////////sps30

delay (1000);

  sps30.SetSerialPin(17,16);



      // Begin communication channel;
  if (! sps30.begin(SERIALPORT1))
    Errorloop((char *) "could not initialize communication channel.", 0);
  // check for SPS30 connection
  if (! sps30.probe()) Errorloop((char *) "could not probe / connect with SPS30.", 0);
  else Serial.print(F("Detected SPS30."));
  // reset SPS30 connection
  if (! sps30.reset()) Errorloop((char *) "could not reset.", 0);

  delay (1000);

  uint8_t ret;
  SPS30_version v;

    // try to get version info
  ret = sps30.GetVersion(&v);
  if (ret != SPS30_ERR_OK) {
    Serial.println(F("Can not read version info"));
    return;
  }

  Serial.print(F(" Firmware version: "));  Serial.print(v.major);
  Serial.print("."); Serial.println(v.minor);


  // start measurement
  if (sps30.start()) Serial.println(F("SPS30: Measurement started"));
  else Errorloop((char *) "Could NOT start measurement", 0);

 
  delay(2000);


}

void loop() {

  struct sps_values val;
  sps30.GetValues(&val);
  struct tm timeinfo;

/////////////////// WEEKDAYS and MONTHS to configure

const char* giorniSettimana[] = {
  "Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab"
};
const char* mesi[] = {
  "Gen", "Feb", "Mar", "Apr", "Mag", "Giu",
  "Lug", "Ago", "Set", "Ott", "Nov", "Dic"
};
////////////////////////////////////////////////////

  pm1 = (int)val.MassPM1;
  pm2= (int)val.MassPM2;
  pm4= (int)val.MassPM4;
  pm10 = (int)val.MassPM10;
  partsize = (float)val.PartSize;

  
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Errore nel recupero dell'ora");
    return;
  }
  
  String dataCompleta = String(giorniSettimana[timeinfo.tm_wday]) + " " +
                        String(timeinfo.tm_mday) + " " +
                        String(mesi[timeinfo.tm_mon]) + " " +
                        String(1900 + timeinfo.tm_year);
                        

  float co2Concentration;
    float temperature;
    float humidity;
     error = sensor.blockingReadMeasurementData(co2Concentration, temperature, humidity);
    
    if (error != NO_ERROR) {
        Serial.print("Error trying to execute blockingReadMeasurementData(): ");
        errorToString(error, errorMessage, sizeof errorMessage);
        Serial.println(errorMessage);
        return;
    }


    Serial.printf("PM1.0: %i µg/m³ - PM2.5: %i µg/m³ - PM10: %i µg/m³\n",pm1 , pm2, pm10);
    Serial.printf("Temp: %f °C - Humidity: %f - Co2: %f \n", temperature, humidity, co2Concentration);

#ifdef SEND_DATA_TO_THINGSPEAK
    sendToThingspeak(pm1, pm2, pm4, pm10, (float) partsize, (int)co2Concentration, (float)temperature, (float)humidity);
#endif

    // Disegna sul display
    display.firstPage();
    do {
            
      display.fillScreen(GxEPD_WHITE);

      display.drawLine(0, 102, 296, 102, GxEPD_BLACK);
      display.drawLine(0, 103, 296, 103, GxEPD_BLACK);
      

      display.drawLine(82, 103, 82, 130, GxEPD_BLACK);
      display.drawLine(154, 103, 154, 130, GxEPD_BLACK);
      display.drawLine(209, 103, 209, 130, GxEPD_BLACK);

      display.setTextSize(4);
      display.setTextColor(GxEPD_BLACK);  
      display.setCursor(7, 60);
      display.print(&timeinfo, "%H:%M");
      
      display.setTextSize(1); 
      display.setCursor(50, 91);
      display.print(dataCompleta);
  
   
///////////////////////////////////////////////////


      if (temperature > 33) {
      display.setTextColor(GxEPD_RED);
      } else {
      display.setTextColor(GxEPD_BLACK);
      }      
      drawThermometer(4, 108, GxEPD_BLACK); 
      display.setTextSize(0);
      display.setCursor(20, 124);     
     // display.print("t ");
     display.print(String((int)temperature) + "." + String((int)(temperature * 10) % 10));
     //display.print("°");
     
////////////////////////////////////////////////
     
      if (humidity > 65) {
      display.setTextColor(GxEPD_RED);
      } else {
      display.setTextColor(GxEPD_BLACK);
      }   
         
    drawDrop(96, 113, 12, GxEPD_BLACK);
      display.setCursor(108, 124);
    //  display.print("ug/m3");
    //display.print("h ");
    display.print((int)humidity);
    display.print("%");

///////////////////////////////////////////////////

 

    drawDust(160, 110, GxEPD_BLACK); 
         if (pm10 > 25) {
      display.setTextColor(GxEPD_RED);
      } else {
      display.setTextColor(GxEPD_BLACK);
      }

      if (pm10 > 99) pm10=99;
      
      display.setCursor(176, 124);
      display.print(pm10, 1);

///////////////////////////////////////////////////
      
      if (co2Concentration > 750) {
      display.setTextColor(GxEPD_RED);
      } else {
      display.setTextColor(GxEPD_BLACK);
      } 
      
    drawCloud(216, 113, GxEPD_BLACK);
      display.setCursor(232, 124);
    //  display.print("ug/m3");
    //display.print("c ");
    display.print((int)co2Concentration);
     
   
    } while (display.nextPage());
  

  delay(60000); // aggiorna ogni 60 secondi

}

void drawThermometer(int x, int y, uint16_t color) {
  // Pallina in basso (bulbo)
  display.fillCircle(x + 6, y + 14, 4, color); // centro (6,10), raggio 4

  // Stelo verticale
  display.fillRect(x + 5, y + 1, 2, 14, color); // rettangolo da (5,2) larghezza 2, altezza 14

  // Cornice stelo (opzionale, bordo)
  display.drawRect(x + 4, y + 1, 4, 10, GxEPD_BLACK); // contorno nero
}

void drawDrop(int x, int y, int size, uint16_t color) {
  int radius = size / 2;

  // Parte tonda inferiore
  display.fillCircle(x, y + radius, radius, color);

  // Parte a punta sopra (triangolo)
  display.fillTriangle(
    x - radius, y + radius,   // sinistra
    x + radius, y + radius,   // destra
    x, y - radius              // punta in alto
  , color);
}

void drawCloud(int x, int y, uint16_t color) {
  // Cerchi sovrapposti per la forma nuvolosa
  display.fillCircle(x + 2, y + 6, 4, color);  // sinistra
  display.fillCircle(x + 6, y + 2, 4, color);  // centro alto
  display.fillCircle(x + 9, y + 6, 4, color);  // destra
  display.fillRect(x + 3, y + 6, 6, 5, color); // base piatta
}

void drawDust(int x, int y, uint16_t color) {
  // Puntini sparsi per simulare particelle di polvere
  display.fillCircle(x + 3, y + 1, 2, color);   
  display.fillCircle(x + 9, y + 4, 2, color);  
  display.fillCircle(x + 2, y + 7, 2, color);    
  display.fillCircle(x + 8, y + 10, 2, color);  
  display.fillCircle(x + 3, y + 13, 2, color);     
}

void Errorloop(char *mess, uint8_t r)
{
  if (r) ErrtoMess(mess, r);
  else Serial.println(mess);
  //Serial.println(F("Program on hold"));
  //for(;;) delay(100000);
}

/**
    @brief : display error message
    @param mess : message to display
    @param r : error code

*/
void ErrtoMess(char *mess, uint8_t r)
{
  char buf[80];

  Serial.print(mess);

  sps30.GetErrDescription(r, buf, 80);
  Serial.println(buf);
}

#ifdef SEND_DATA_TO_THINGSPEAK
void sendToThingspeak(int pm1, int pm2_5, int pm4, int pm10, float partsize, int co2,  float temp, float hum)
{

  String tsData = String("&field1=") + String(pm1) + String("&field2=") + String(pm2_5) + String("&field3=") + String(pm4) + String("&field4=") + String(pm10) + String("&field5=") + String(partsize) + String("&field6=") + String(co2) + String("&field7=") + String(hum)+ String("&field8=") + String(temp) ;

  if (client.connect("api.thingspeak.com", 80))
  {

    client.print("POST /update HTTP/1.1\n");

    client.print("Host: api.thingspeak.com\n");

    client.print("Connection: close\n");

    client.print("X-THINGSPEAKAPIKEY: " + thing_api_key + "\n");

    client.print("Content-Type: application/x-www-form-urlencoded\n");

    client.print("Content-Length: ");

    client.print(tsData.length());

    client.print("\n\n");

    client.print(tsData);


    if (client.connected())
    {

      Serial.println("Sent to ThingSpeak.");
      Serial.println("--------------------");


    }

  }


}
#endif
