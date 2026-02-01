// Variables

// reporting and checking intervals
#define REPORT        320001  // 15 min reporting interval is 900k ms;
			      //            10 min is 600k ms
                              // 1/23/24: poor cellular = too many gaps;
			      //           increased reporting freq to ~5m
#define FIVE_MIN      314159  // watch more closely; every ~5min (300k ms)
#define LINE_PWR      1       // we're plugged in (see powerSource)

// power vars
float fuelPercent   = 0;
bool  PowerIsOn     = true;
int   powerSource   = 0;       // 1= line power; 2= usb host power; 5=battery

//ds18b20 temp sensor vars
#define MAXRETRY      4        // max times to poll temperature pin before giving up
double crawlTemp;
bool tempValid      = true;
int tempFailCt      = 0;

// danger tripwire settings
float danger        = 35.00;    // this is the threshold we define as
				//           "pipe freeze eminent"
float allGood       = 37.00;    // not gonna relax until we are seeing
				//           temperature go back solid above danger
float Freezing      = 32.50;    // now we are really in trouble
bool  inDanger      = false;    // start with a clean slate!
float nippy         = 41.00;    // degrees F below which we want to
				//           report more frequently
bool fastReport     = true;     // below nippy degrees, send mqtt every
				//           5 min to offset high packet loss


// mqtt debugging
int   mqttCt        = 0;
int   mqttFails     = 0;        
int   mqttDis       = 0;        // unexpected disconnects
