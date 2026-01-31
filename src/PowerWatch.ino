/*
  Monitor cabin power
    C. Catlett Apr 2019
    2020-Oct
      - added MQTT to connect to remote Home Assistant
      - added ds18b20 temperature sensor to monitor crawlspace
    2021-Jan
      - added connection check to MQTT just to be safe                       
    2026-Feb
      - convert to use Particle.publish to eliminate the need for
        port forwarding at server side.
 */

#include <Particle.h>
#include <MQTT.h>
#include "secrets.h" 
#include <OneWire.h>
#include <DS18B20.h>
// include topics for mqtt
#include "topics.h"
// include misc variables
#include "vars.h"

FuelGauge fuel;                   // lipo battery
DS18B20  sensor(D1, true);        // DS18B20 temperature sensor 


Timer checkTimer(FIVE_MIN, checkPower);
Timer reportTimer(REPORT, reportPower);
bool  TimeToCheck     = TRUE;
bool  TimeToReport    = TRUE;

// Application watchdog - in case of wedges
int DOGTIME = 120000;           // wait 2 minutes before pulling the ripcord
retained bool REBORN  = FALSE;  // did app watchdog restart us?
ApplicationWatchdog *wd;
void watchdogHandler() {
  REBORN = TRUE;
  System.reset(RESET_NO_WAIT);
}
retained bool SELF_RESTART = FALSE;
int fails = 0;
int GIVE_UP = 3;

void setup() {
    Time.zone (-5);
    Particle.syncTime();

    wd = new ApplicationWatchdog(DOGTIME, watchdogHandler, 1536); // restart after DOGTIME sec no pulse
    if (REBORN) {
      Particle.publish("****WEDGED****", "app watchdog restart", 3600, PRIVATE);
      REBORN = FALSE;
    }
    if (SELF_RESTART) {
      Particle.publish("----STUCK----", "self-reboot", 3600, PRIVATE);
      SELF_RESTART = FALSE;
    }

    fuelPercent = fuel.getSoC();
    Particle.publish("START", "Starting Lake Watch", 3600, PRIVATE);
    
    //client.disconnect();
    checkTimer.start();
    reportTimer.start();
}

void loop() {

// check everything when timer fires; notify only state changes
    if (TimeToCheck) {
        TimeToCheck = FALSE;
        fuelPercent = fuel.getSoC(); 
        powerSource = System.powerSource();
        if (powerSource == LINE_PWR) {
          if (!PowerIsOn) {
            publishPowerwatch("power_change");
            Particle.publish("POWER-start ON", String(powerSource), PRIVATE);
            reportTimer.changePeriod(REPORT);
          }
          PowerIsOn = TRUE;
        } else {
          if (PowerIsOn) {
            publishPowerwatch("power_change");
            Particle.publish("POWER OUT", String(powerSource), PRIVATE);
            reportTimer.changePeriod(FIVE_MIN);
          }
          PowerIsOn = FALSE;
        }
        // check crawlspace
        crawlTemp = getTemp();
        /* if (crawlTemp > allGood) {
          if (inDanger) {
            tellHASS(TOPIC_E, String(crawlTemp));
            inDanger=FALSE;
          }
        } */
        if (crawlTemp < danger)    { 
          publishPowerwatch("temp_alert");
          /*if (crawlTemp < Freezing)  { 
            tellHASS(TOPIC_G, String(crawlTemp)); 
            Particle.publish("CRAWLSPACE DANGER", String(powerSource), PRIVATE);
            inDanger=TRUE;
          }*/
        }
    }

    if (TimeToReport) {
      TimeToReport = FALSE;
      wd->checkin(); // poke app watchdog we're going in...
      publishPowerwatch("periodic");

      void myWatchdogHandler(void); // reset the dog
    }
} 
/************************************/
/***         FUNCTIONS       ***/
/************************************/

// Checking timer interrupt handler
void checkPower() {  TimeToCheck = TRUE;  }

// Reporting timer interrupt handler
void reportPower() {  TimeToReport = TRUE;  }

// Use Particle.publish to send data and alerts as needed to HASSIO

void publishPowerwatch(const char* reason) {
  // Keep payload small; Particle event data limit depends on Device OS/platform. :contentReference[oaicite:3]{index=3}
  char json[320];

  snprintf(json, sizeof(json),
    "{\"fuelPercent\":%.1f,"
    "\"powerSource\":%d,"
    "\"powerIsOn\":%s,"
    "\"crawlTempF\":%.2f,"
    "\"inDanger\":%s,"
    "\"fw\":\"%s\","
    "\"reason\":\"%s\"}",
    fuelPercent,
    powerSource,
    PowerIsOn ? "true" : "false",
    crawlTemp,
    inDanger ? "true" : "false",
    APP_VERSION,
    reason
  );

  // WITH_ACK confirms the Particle Cloud received it (not that your webhook endpoint did). :contentReference[oaicite:4]{index=4}
  Particle.publish("ha/cabin/powerwatch", json, PRIVATE | WITH_ACK);
}


//  Check the crawlspace on the DS18B20 sensor (code from Lib examples)

double getTemp() {  
  float _temp;
  float fahrenheit = 0;
  int   i = 0;

  do {  _temp = sensor.getTemperature();
  } while (!sensor.crcCheck() && MAXRETRY > i++);
  if (i < MAXRETRY) {  
    fahrenheit = sensor.convertToFahrenheit(_temp);
  } else {
    Particle.publish("ERROR", "Invalid reading", PRIVATE);
  }
  return (fahrenheit); 
}

