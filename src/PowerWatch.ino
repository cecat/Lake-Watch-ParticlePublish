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
#include <OneWire.h>
#include <DS18B20.h>
// include misc variables
#include "vars.h"
#include <math.h> 

#ifndef APP_VERSION
#define APP_VERSION "Lake-Watch-ParticlePublish-v1"
#endif


FuelGauge fuel;                   // lipo battery
DS18B20  sensor(D1, true);        // DS18B20 temperature sensor 

void checkPower();
void reportPower();
double getTemp();
void publishPowerwatch(const char* reason);

Timer checkTimer(FIVE_MIN, checkPower);
Timer reportTimer(REPORT, reportPower);
bool  TimeToCheck     = true;
bool  TimeToReport    = true;

// Application watchdog - in case of wedges
int DOGTIME = 120000;           // wait 2 minutes before pulling the ripcord
retained bool REBORN  = false;  // did app watchdog restart us?
ApplicationWatchdog *wd;
void watchdogHandler() {
  REBORN = true;
  System.reset(RESET_NO_WAIT);
}
retained bool SELF_RESTART = false;
int fails = 0;
int GIVE_UP = 3;

void setup() {
    Time.zone (-5);
    Particle.syncTime();

    wd = new ApplicationWatchdog(DOGTIME, watchdogHandler, 1536); // restart after DOGTIME sec no pulse
    if (REBORN) {
      Particle.publish("wedged", "app_watchdog_restart", 3600, PRIVATE);
      REBORN = false;
    }
    if (SELF_RESTART) {
      Particle.publish("stuck", "self-reboot", 3600, PRIVATE);
      SELF_RESTART = false;
    }

    fuelPercent = fuel.getSoC();
    Particle.publish("START", "Starting Lake Watch", 3600, PRIVATE);
    
    //client.disconnect();
    checkTimer.start();
    reportTimer.start();
}

void loop() {

    wd->checkin(); // poke app watchdog we're going in...

// check everything when timer fires; notify only state changes
    if (TimeToCheck) {
        TimeToCheck = false;
        fuelPercent = fuel.getSoC(); 
        powerSource = System.powerSource();
        if (powerSource == LINE_PWR) {
          if (!PowerIsOn) {
            publishPowerwatch("power_change");
            Particle.publish("power_on", String(powerSource), PRIVATE);
            reportTimer.changePeriod(REPORT);
          }
          PowerIsOn = true;
        } else {
          if (PowerIsOn) {
            publishPowerwatch("power_change");
            Particle.publish("power_outage", String(powerSource), PRIVATE);
            reportTimer.changePeriod(FIVE_MIN);
          }
          PowerIsOn = false;
        }
        // check crawlspace
        crawlTemp = getTemp();
        /* if (crawlTemp > allGood) {
          if (inDanger) {
            tellHASS(TOPIC_E, String(crawlTemp));
            inDanger=false;
          }
        } */
        if (tempValid && !isnan(crawlTemp) && crawlTemp < danger) {
            publishPowerwatch("temp_alert");
          /*if (crawlTemp < Freezing)  { 
            tellHASS(TOPIC_G, String(crawlTemp)); 
            Particle.publish("CRAWLSPACE DANGER", String(powerSource), PRIVATE);
            inDanger=true;
          }*/
        }
    }

    if (TimeToReport) {
      TimeToReport = false;
      publishPowerwatch("periodic");
    }
} 
/************************************/
/***         FUNCTIONS       ***/
/************************************/

// Checking timer interrupt handler
void checkPower() {  TimeToCheck = true;  }

// Reporting timer interrupt handler
void reportPower() {  TimeToReport = true;  }

// Use Particle.publish to send data and alerts as needed to HASSIO

void publishPowerwatch(const char* reason) {
  char json[320];

  // Render crawlTemp as JSON null if invalid
  char tempPart[64];
  if (tempValid && !isnan(crawlTemp)) {
    snprintf(tempPart, sizeof(tempPart), "%.2f", crawlTemp);
  } else {
    snprintf(tempPart, sizeof(tempPart), "null");
  }

  snprintf(json, sizeof(json),
    "{\"fuelPercent\":%.1f,"
    "\"powerSource\":%d,"
    "\"powerIsOn\":%s,"
    "\"crawlTempF\":%s,"
    "\"tempValid\":%s,"
    "\"tempFailCt\":%d,"
    "\"reason\":\"%s\"}",
    fuelPercent,
    powerSource,
    PowerIsOn ? "true" : "false",
    tempPart,
    tempValid ? "true" : "false",
    tempFailCt,
    reason
  );

  Particle.publish("ha/cabin/powerwatch", json, PRIVATE | WITH_ACK);
}


//  Check the crawlspace on the DS18B20 sensor (adapted from Lib examples)

double getTemp() {
  float _temp;
  int i = 0;

  do {
    _temp = sensor.getTemperature();
  } while (!sensor.crcCheck() && (MAXRETRY > i++));

  if (i < MAXRETRY) {
    tempValid = true;
    // optional: clear fail counter on success
    // tempFailCt = 0;
    return sensor.convertToFahrenheit(_temp);
  } else {
    tempValid = false;
    tempFailCt++;
    return NAN;
  }
}

