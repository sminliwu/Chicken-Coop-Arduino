#include <WiFi.h>
#include <TB6612_ESP32.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include "time.h" // https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
#include "ArduinoJson.h"

// motor set-up from https://github.com/sparkfun/SparkFun_TB6612FNG_Arduino_Library
#define AIN1 17
#define AIN2 4
#define PWMA 16
#define STBY 18
#define MANUAL_UP 19    // GPIO for manual motor control
#define MANUAL_DOWN 23  // 2 pins, one for each dir
#define OPEN_DR 255
#define CLOSE_DR -255   // motor.drive() vals for open/close

// date-time constants
#define MILLIS = 1000;
#define HOUR_SEC = 3600;
#define HOUR_MILLIS = HOUR_SEC * MILLIS; // 1 hour
#define MINUTE_MILLIS = 60000; // 1 minute

// other macros
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

const int offsetA = 1; // Value can be 1 or -1
Motor motor = Motor(AIN1, AIN2, PWMA, offsetA, STBY, 5000, 8, 1);

/* STATE MACHINE PARAMETERS */
unsigned long currentMillis = millis(); // check millisecond time
unsigned long updateInterval;

bool autoMode = false;    // false = manual control

// motor state machine
int motorInterval = 40;
long motorIntervalMillis = motorInterval * MILLIS; // 40 seconds (in milliseconds)
int motorTime = 0; // how many seconds the motor has been running
long motorStartMillis = 0; // when the motor was started
bool motorOn = false;
bool motorDir = true; // true = opening, false = closing
bool doorStatus = false; // true = open, false = closed


// sunrise/sunset state machine;
long prevTimeMillis = 0; // when the hour was last checked
char state = 'R'; // state is 'R' in setup/reset state, 
                  // setup() will set it to 0-3 depending on time

/* CONNECTIVITY / WEB INTEGRATIONS */

// from sunrise-sunset API, lat/lng coords for Boulder, CO
const char* sunRiseSetAPI = 
  "https://api.sunrise-sunset.org/json?lat=40.014984&lng=-105.270546";
static int sunriseVals[3]; // format: [ready, hour, min]
static int sunsetVals[3];   
  // ready byte = 1 when data available, 0 if empty or error

// time stuff from NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = TZ*HOUR_SEC; // GMT-7 is Mountain time
const int   daylightOffset_sec = HOUR_SEC;
bool clientConnected = false; 
  // when client connected, update every minute for client UI
  // otherwise, just update every hour
static int currentDateTime[7]; 
/* format: [ready, year, month, date, hour, min, DST]
- ready: 1 if array data is available, 0 if empty or error
- year: 20XX
- month: 1-12
- date: 1-31 dep on month
- hour: 0-23 for 12AM to 11PM
- min: 0-59 
- DST: (daylight savings flag) +1 if daylight savings, 0 if not, -1 if error
*/

// HTTP port for server
const int port = 80;
AsyncWebServer server(port);
// Websocket for server/client data updates
const int WSport = 81;
WebSocketsServer webSocket(WSport);

void setup() {
  Serial.begin(115200);
  pinMode(MANUAL_UP, INPUT_PULLUP);
  pinMode(MANUAL_DOWN, INPUT_PULLUP);
  
  if (!SPIFFS.begin()) {
    Serial.println("error occured while mounting SPIFFS");
    return;
  }

  WiFi.begin(ssid, password);
  Serial.println("connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }

  Serial.println("SUCCESS!");
  Serial.print("server running at IP:\t");
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Websocket server started.");

  // loading page and assets
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", "text/html", false);
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  // client clicked manual button
  server.on("/manual", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("toggle manual mode");
    autoMode = !autoMode;
    if (autoMode) {
      webSocket.broadcastTXT("control AUTO");
    } else {
      webSocket.broadcastTXT("control MANUAL");
    }
    request->send(SPIFFS, "/index.html", "text/html", false); 
  })

  // client clicked open button
  server.on("/open", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("received open command");
    motorOn = openDoor();
    Serial.println("motor opening");
    request->send(SPIFFS, "/index.html", "text/html", false);
  });
  //client clicked close button
  server.on("/close", HTTP_GET, [](AsyncWebServerRequest * request) {
    motorOn = closeDoor();
    Serial.println("motor closing");
    request->send(SPIFFS, "/index.html", "text/html", false);
  });
  //client released open/close button
  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest * request) {
    motorOn = stopDoor();
    Serial.println("motor stopped");
    request->send(SPIFFS, "/index.html", "text/html", false);
  });
  
  server.begin();
  Serial.println("Async HTTP server started.");

  // Init NTP and sunrise/sunset API connections
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  updateLocalTime();
  getSunTimes();
  Serial.printf("System time - %u ms\n", millis());
  printLocalTime();
  printSunTimes();

  // Init sunrise-sunset state machine: 
  // determine which state to enter
  if (CURRENT_HOUR > SUNSET_HOUR || CURRENT_HOUR < SUNRISE_HOUR) {
    Serial.println("entering night stage");
    state = STATE_NIGHT; // night time, door should be closed
  } else if (CURRENT_HOUR == SUNRISE_HOUR) {
    Serial.println("entering sunrise stage");
    state = STATE_SUNRISE; // sunrise, door should be open(ing)
  } else if (CURRENT_HOUR > SUNRISE_HOUR && CURRENT_HOUR < SUNSET_HOUR) {
    Serial.println("entering day stage");
    state = STATE_DAY; // day time, door should be open
  } else if (CURRENT_HOUR == SUNSET_HOUR) {
    Serial.println("entering sunset stage");
    state = STATE_SUNSET; // sunset, door should be close(ing)
  } else {
    Serial.println("ERROR ENTERING SUNRISE/SUNSET STAGE");
    return;
  }
}

void loop() {
  webSocket.loop();

  // sunrise/sunset state machine
  switch (state) {
    case STATE_NIGHT: // night time, wait for sunrise
      updateInterval = (clientConnected) ? MINUTE_MILLIS : HOUR_MILLIS;
      if (DATETIME_RDY && SUNRISE_RDY) {
        if (CURRENT_HOUR == SUNRISE_HOUR) {
          // if it's less than 1 hour to sunrise, switch to state 2
          state = STATE_SUNRISE;
          return;
        }
      }
      if (currentMillis - prevTimeMillis > updateInterval) {
        Serial.println("zzzzz");
        prevTimeMillis = currentMillis;
        updateLocalTime();
        // update sunrise/sunset at 3:00AM local tiem
        if (CURRENT_HOUR == 3) {
          Serial.println("let's check on the sun");
          getSunTimes();
        }
      }
      break;
    case STATE_SUNRISE: // sunrise: open door
      updateInterval = MINUTE_MILLIS;
      if (currentMillis - prevTimeMillis > updateInterval) {
        Serial.println("waiting for the sun to say hello");
        prevTimeMillis = currentMillis;
        updateLocalTime();
      }
      if (CURRENT_MINUTE >= SUNRISE_MINUTE) {
        Serial.println("wakey wakey!");
        if (autoMode) {
          motorOn = openDoor();
          Serial.println("motor opening");
        }
      }
      // don't leave this state until door is fully open
      if (doorStatus) {
        state = STATE_DAY;
        return;
      }
      break;
    case STATE_DAY: // day time, wait for sunset
      updateInterval = (clientConnected) ? MINUTE_MILLIS : HOUR_MILLIS;
      if (DATETIME_RDY && SUNSET_RDY) {
        if (CURRENT_HOUR == SUNSET_HOUR) {
          // if it's less than 1 hour to sunset, switch to state 2
          state = STATE_SUNSET;
          return;
        }
      }
      if (currentMillis - prevTimeMillis > updateInterval) {
        Serial.println("just waiting...");
        updateLocalTime();
      }
      break;
    case STATE_SUNSET: // sunset: close door, reset sunrise/sunset times
      updateInterval = minuteInterval;
      if (currentMillis - prevTimeMillis > updateInterval) {
        Serial.println("waiting for the sun to go away");
        prevTimeMillis = currentMillis;
        updateLocalTime();
      }
      if (CURRENT_MINUTE >= SUNSET_MINUTE) {
        Serial.println("nightey night...");
        if (autoMode) {
          motorOn = closeDoor()
          Serial.println("motor closing");
        }
      }
      // don't leave this state until door is fully closed
      if (!doorStatus) {
        state = STATE_NIGHT;
        for (int i = 0; i < 3; i++) {
          sunsetVals[i] = 0;
          sunriseVals[i] = 0;
        }
        return;
      }
      break;
    default:
      break;
  }
  
  // motor state machine, only runs if motorOn = true
  if (motorOn && autoMode) {
    // if it's more than X seconds after starting motor
    if (currentMillis - motorStartMillis > motorIntervalMillis) {
      motorOn = stopDoor();
      bool result = updateDoorStatus();
      return;
    } 
    if (currentMillis - motorStartMillis > motorTime*1000) {
      motorTime++;
      bool result = updateDoorStatus();
    }
  } else { // manual motor controls
    if (digitalRead(MANUAL_UP) && digitalRead(MANUAL_DOWN)) {
      // no manual commands
      if (motorOn) { 
        motorOn = stopDoor();
      }
    } else if ((!digitalRead(MANUAL_UP)) && digitalRead(MANUAL_DOWN)) {
      motorOn = openDoor();
    } else if (digitalRead(MANUAL_UP) && (!digitalRead(MANUAL_DOWN))) {
      motorOn = closeDoor();
    }
  }
}

/* Callback function for websocket events. */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: // if a client disconnects
      Serial.printf("[%u] Client disconnected :(", num);
      if (webSocket.connectedClients() == 0) { // if no more clients
        clientConnected = false;
      }
      break;
    case WStype_CONNECTED: { // if a new websocket client connects
        if (!clientConnected) { // update clientConnected
          clientConnected = true;
        }
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", 
          num, ip[0], ip[1], ip[2], ip[3], payload);
        updateClients();  
        break;
      }
    case WStype_TEXT: { // if new text data is received
        Serial.printf("[%u] sent: %s\n", num, payload);
        break;
      }
  }
}

void updateClients() {
  if (DATETIME_RDY) {
    String AMPM = (CURRENT_HOUR > 12) ? "PM" : "AM";
    String hourStr = (CURRENT_HOUR > 12) ? String(CURRENT_HOUR-12) : String(CURRENT_HOUR);
    String minuteStr = (CURRENT_MINUTE < 10) ? "0"+String(CURRENT_MINUTE) : String(CURRENT_MINUTE);
    String timeStr = hourStr + ":" + minuteStr + " " + AMPM;
    String dateStr = String(CURRENT_MONTH) + "/" + String(CURRENT_DATE) + "/" + String(CURRENT_YEAR);
    webSocket.broadcastTXT("date: " + dateStr);
    webSocket.broadcastTXT("time: " + timeStr);
  }
  
  if (SUNRISE_RDY && SUNSET_RDY) {
    String minute_R = (SUNRISE_MINUTE < 10) ? "0"+String(SUNRISE_MINUTE) : String(SUNRISE_MINUTE);
    String minute_S = (SUNSET_MINUTE < 10) ? "0"+String(SUNSET_MINUTE) : String(SUNSET_MINUTE);
    String sunriseStr = String(SUNRISE_HOUR) + ":" + minute_R + " AM";
    String sunsetStr = String(SUNSET_HOUR-12) + ":" + minute_S + " PM";
    webSocket.broadcastTXT("sunrise/sunset: " + sunriseStr + "/" + sunsetStr);
  }

  // TODO: update clients with control mode
  // TODO: reorganize, all broadcastTXT should be here?
}
