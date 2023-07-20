/* Main program for chicken/duck coop management system
 * To keep this file's length under control, ONLY include:
 *  - include statements
 *  - macro #definitions
 *  - setup() and loop() functions
 * ALL OTHER CODE CONTAINED IN FUNCTIONS IN OTHER FILES
 */

#include <WiFi.h>
#include <TB6612_ESP32.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include "time.h" // https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
#include "ArduinoJson.h"

#include "auth.h" // secrets header (.h) file

// motor set-up from https://github.com/sparkfun/SparkFun_TB6612FNG_Arduino_Library
#define AIN1 17
#define AIN2 4
#define PWMA 16
#define STBY 18
#define OFFSET_A 1 // Value can be 1 or -1
#define MANUAL_UP 19    // GPIO for manual motor control
#define MANUAL_DOWN 23  // 2 pins, one for each dir
//#define EMERGENCY_STOP 36 // red button on breadboard, emergency stop
#define OPEN_DR 255
#define CLOSE_DR -255   // motor.drive() vals for open/close

// date-time constants
#define MILLIS 1000
#define HOUR_SEC 3600
#define HOUR_MILLIS HOUR_SEC*MILLIS // 1 hour
#define MINUTE_SEC 60
#define MINUTE_MILLIS 60000 // 1 minute

// data/state macros
#define TZ -7 // timezone GMT-7, Mountain Time (Denver)
#define DATETIME_RDY currentDateTime[0]
#define CURRENT_YEAR currentDateTime[1]
#define CURRENT_MONTH currentDateTime[2]
#define CURRENT_DATE currentDateTime[3]
#define CURRENT_HOUR currentDateTime[4]
#define CURRENT_MINUTE currentDateTime[5]
#define DST_FLAG currentDateTime[6]

#define SUNRISE_RDY sunriseVals[0]
#define SUNRISE_HOUR sunriseVals[1]
#define SUNRISE_MINUTE sunriseVals[2]
#define SUNSET_RDY sunsetVals[0]
#define SUNSET_HOUR sunsetVals[1]
#define SUNSET_MINUTE sunsetVals[2]

#define STATE_NIGHT   0
#define STATE_SUNRISE 1
#define STATE_DAY     2
#define STATE_SUNSET  3

/* USER SETTINGS */
bool doorStatus = false; // true = open, false = closed
bool autoMode = false;
  // true = auto mode, false = manual mode
char flockStatus = 'c'; // flock status, changed by user in UI
  // 'c' = in the coop
  // 'r' = in the run
  // 'y' = in the yard 
// DEFAULT settings - user can adjust motor times and offsets in UI
uint8_t motorInterval_open = 25;
uint8_t motorInterval_close = 15;
uint8_t offset_open = 40;
uint8_t offset_close = 30;

bool googleEnabled = true;
  // when Google connection enabled, ESP will send POST 
  // requests with system status/user setting changes

/* STATE MACHINE GLOBALS */
unsigned long currentTime; // check millisecond time / 1000
unsigned int prevTime; // when the hour was last checked
unsigned int updateInterval;

// motor state machine
Motor motor = Motor(AIN1, AIN2, PWMA, OFFSET_A, STBY, 5000, 8, 1);
uint8_t motorTime; // how many seconds the motor has been running
unsigned int motorStartTime; // when the motor was started
unsigned int motorInterval;
bool motorOn = false;
bool motorDir = true; // true = opening, false = closing

// sunrise/sunset state machine;
char state = 'R'; // state is 'R' in setup/reset state, 
  // setup() will set it to 0-3 depending on time of day

/* CONNECTIVITY / WEB INTEGRATIONS */
// data arrays: sunriseVals/sunsetVals, currentDateTime
// ready byte = 1 when data available, 0 if empty or error

// from sunrise-sunset API, lat/lng coords for Boulder, CO
const char* sunRiseSetAPI = 
  "https://api.sunrise-sunset.org/json?lat=40.014984&lng=-105.270546";
uint8_t sunriseVals[3]; // format: [ready, hour, min]
uint8_t sunsetVals[3];   

// time stuff from NTP
const char* ntpServer = "pool.ntp.org";
int currentDateTime[7]; 
/* format: [ready, year, month, date, hour, min, DST]
- ready: 1 if array data is available, 0 if empty or error
- year: 20XX
- month: 1-12
- date: 1-31 dep on month
- hour: 0-23 for 12AM to 11PM
- min: 0-59 
- DST: (daylight savings flag) +1 if daylight savings, 0 if not, -1 if error
*/

// who's connected
bool clientConnected = false; 
  // when client connected, update every minute for client UI
  // otherwise, just update every hour
//char WSOutBuffer[50];
String message;

// HTTP port for server
const uint8_t port = 80;
AsyncWebServer server(port);
// Websocket for server/client data updates
const uint8_t WSport = 81;
WebSocketsServer webSocket(WSport);

void setup() {
//  Serial.println(millis());
//  Serial.begin(115200);
  pinMode(MANUAL_UP, INPUT_PULLUP);
  pinMode(MANUAL_DOWN, INPUT_PULLUP);
  message.reserve(50);
//  pinMode(EMERGENCY_STOP, INPUT);
  
  if (!SPIFFS.begin()) {
//    Serial.println("error occured while mounting SPIFFS");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    currentTime = millis()/1000;
  }
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // loading page and assets
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", "text/html", false);
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });
  
  server.begin();

  // Init NTP and sunrise/sunset API connections
  long gmtOffset_sec = TZ*HOUR_SEC; // GMT-7 is Mountain time
  int daylightOffset_sec = HOUR_SEC;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateLocalTime();
  getSunTimes();
//  Serial.printf("System time - %u ms\n", millis());
//  printLocalTime();
//  printSunTimes();

  // Init sunrise-sunset state machine: 
  // determine which state to enter
  if (CURRENT_HOUR > SUNSET_HOUR || CURRENT_HOUR < SUNRISE_HOUR-1) {
//    Serial.println("entering night stage");
    state = STATE_NIGHT; // night time, door should be closed
  } else if (CURRENT_HOUR == SUNRISE_HOUR-2) {
//    Serial.println("entering sunrise stage");
    state = STATE_SUNRISE; // sunrise, door should be open(ing)
  } else if (CURRENT_HOUR > SUNRISE_HOUR-2 && CURRENT_HOUR < SUNSET_HOUR) {
//    Serial.println("entering day stage");
    state = STATE_DAY; // day time, door should be open
  } else if (CURRENT_HOUR == SUNSET_HOUR) {
//    Serial.println("entering sunset stage");
    state = STATE_SUNSET; // sunset, door should be close(ing)
  } 
  if (googleEnabled) {
    message = F("ESP has booted into state ");
    message += int(state);
    postToGoogle(message);
  }
}

void loop() {
//  if (digitalRead(EMERGENCY_STOP)) {
//    motorOn = stopDoor();
//  }
  currentTime = millis()/1000;
  wifiLoop();
  webSocket.loop();
  dayNightLoop();
  
  // motor state machine, only runs if motorOn = true
  if (motorOn) {
    // stop the door automatically after motorInterval
    //  in auto mode: always and mark door open/close
    //  in manual mode: if the door starts off closed and has been opening
    //    OR if the door starts off open and has been closing
    //    (doorState != motorDir)
    if (motorTime >= motorInterval) {
      motorOn = stopDoor();
      updateDoorStatus();
      if (autoMode && doorStatus) {
        flockStatus = 'r';      
        broadcastChange('f');
      } 
      return;
    } else if (currentTime - motorStartTime >= motorTime) {
      // either  auto or manual mode, if the motor was turned on, keep time
      // on how long it runs for
      motorTime++;
      broadcastChange('d');
    }
  } else { // manual motor controls - physical buttons
    if (digitalRead(MANUAL_UP) && digitalRead(MANUAL_DOWN)) {
      // no manual commands
      stopDoor();
    } else if ((!digitalRead(MANUAL_UP)) && digitalRead(MANUAL_DOWN)) {
      openDoor();
    } else if (digitalRead(MANUAL_UP) && (!digitalRead(MANUAL_DOWN))) {
      closeDoor();
    }
  }
}

void dayNightLoop() {
  // sunrise/sunset state machine
  switch (state) {
    case STATE_NIGHT: // night time, wait for sunrise
      updateInterval = (clientConnected) ? MINUTE_SEC : HOUR_SEC;
      if (DATETIME_RDY && SUNRISE_RDY) {
        if (CURRENT_HOUR == SUNRISE_HOUR-2) {
          // if it's less than 2 hours to sunrise, switch to state 2
          state = STATE_SUNRISE;
          broadcastChange('s');
          return;
        }
      }
      if (currentTime - prevTime > updateInterval) {
        prevTime = currentTime;
        updateLocalTime();
        // update sunrise/sunset at midnight local time
        if (CURRENT_HOUR == 0) {
          getSunTimes();
          updateGoogle('c'); // post auto/manual
        }
      }
      break;
    case STATE_SUNRISE: // sunrise: open door
      updateInterval = MINUTE_SEC;
      if (currentTime - prevTime > updateInterval) {
        prevTime = currentTime;
        updateLocalTime();
      }
      if (timeToOpen()) { // no offset: CURRENT_MINUTE >= SUNRISE_MINUTE 
//        if (googleEnabled) {
//          postToGoogle("time to auto open");
//        }
        if (autoMode) { 
          startDoor(true, motorInterval_open);
//          motorInterval = motorInterval_open;
//          motorStartTime = currentTime;
//          motorTime = 0;
//          motorOn = openDoor();
//          updateDoorStatus();
        }
        state = STATE_DAY;
        broadcastChange('s');
        return;
      }
      break;
    case STATE_DAY: // day time, wait for sunset
      updateInterval = (clientConnected) ? MINUTE_SEC : HOUR_SEC;
      if (DATETIME_RDY && SUNSET_RDY) {
        if (CURRENT_HOUR == SUNSET_HOUR) {
          // if it's less than 1 hour to sunset, switch to state 2
          state = STATE_SUNSET;
          broadcastChange('s');
          return;
        }
      }
      if (currentTime - prevTime > updateInterval) {
        updateLocalTime();
      }
      break;
    case STATE_SUNSET: // sunset: close door, reset sunrise/sunset times
      updateInterval = MINUTE_SEC;
      if (currentTime - prevTime > updateInterval) {
        prevTime = currentTime;
        updateLocalTime();
      }
      if (timeToClose()) {
//        if (googleEnabled) {
//          message = F("time to auto close");
//          postToGoogle(message);
//        }
        if (autoMode) {
          startDoor(false, motorInterval_close);
//          motorInterval = motorInterval_close;
//          motorStartTime = currentTime;
//          motorTime = 0;
//          motorOn = closeDoor();
//          updateDoorStatus();
        }
        state = STATE_NIGHT;
        broadcastChange('s');
        return;
      }
      break;
    default:
      break;
  }
}

void wifiLoop() {
  static unsigned int lastConnectAttempt;
  if (currentTime - lastConnectAttempt >= MINUTE_SEC) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      lastConnectAttempt = currentTime;
    }
  }
}
