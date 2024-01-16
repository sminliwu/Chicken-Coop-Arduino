/* Callback function for websocket events. */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: // if a client disconnects
      if (webSocket.connectedClients() == 0) { // if no more clients
        clientConnected = false;
      }
      break;
    case WStype_CONNECTED: { // if a new websocket client connects
        if (!clientConnected) { // update clientConnected
          clientConnected = true;
        }
        IPAddress ip = webSocket.remoteIP(num);
        updateWS('f');
        updateWS('d');
        updateWS('m');
        updateWS('e');
        updateWS('c');
        updateWS('g');
        updateWS('n');
        break;
      }
    case WStype_TEXT: { // if new text data is received
        if (payload[0] == 'o') {
          // clicked open button
          startDoor(true, 30);
        } else if (payload[0] == 'c') {
          // clicked close button
          startDoor(false, 30);
        } else if (payload[0] == 'h') {
          // released open/close button
          motorOn = stopDoor();
          updateDoorStatus();
        } else if (payload[0] == 's') {
          if (payload[1] == 'o') {
            // clicked manual set open button
            doorStatus = true;
          } else if (payload[1] == 'c') {
            // clicked manual set close button
            doorStatus = false;
          }
          broadcastChange('d');
        } else if (payload[0] == 'f') {
          flockStatus = payload[1];
          broadcastChange('f');
          if (googleEnabled) {
            String message = String((char*)payload);
            message += webSocket.remoteIP(num).toString();
            postToGoogle(message);
          }
        } else if (payload[0] == 'a') {
          // clicked manual button
          updateAutoMode();
        } else if (payload[0] == 'g') {
          // toggle Google POST requests
          googleEnabled = !googleEnabled;
          broadcastChange('g');
        } else if (payload[0] == 'm') {
          // user changed motorInterval thru client interface
          // client will send "m[o/c][seconds]"
          char* param = (char*) payload + 2;
          if (payload[1] == 'o') {
            motorInterval_open = String(param).toInt();
          } else {
            motorInterval_close = String(param).toInt();
          }
          broadcastChange('m');
        } else if (payload[0] == 'e') {
          // user adjusted open/close offsets
          char* param = (char*) payload + 2;
          if (payload[1] == 'o') {
            offset_open = String(param).toInt();
          } else {
            offset_close = String(param).toInt();
          }
          broadcastChange('e');
        } else if (googleEnabled) {
          message = F("client message [");
          message += webSocket.remoteIP(num).toString() + "]: ";
          message += String((char*)payload);
          postToGoogle(message);
        }
        break;
      }
      default:
        break;
  }
}

/* send a POST request to the Google cloud service
   src: https://github.com/arduino-libraries/ArduinoHttpClient/blob/master/src/HttpClient.cpp
*/
int postToGoogle(String data) {
  HTTPClient http;
  http.begin(googleScript);
  http.addHeader(F("Content-Type"), F("text/plain"), true);
  int httpResponseCode = http.POST(data);
  http.end();
  return httpResponseCode;
}

void broadcastChange(char code) {
  static const bool debug = true;
  switch(code) {
    case 'g': // googleEnabled
        if (!googleEnabled) {
          updateGoogle(code);
        }
    case 'c': // autoMode
    case 'm': // motorInterval
    case 'f': // flockStatus
    case 'd': // doorStatus, motorOn, or motorDir
    case 'e': // offset adjustment
    case 'n': // sun times / date-time
        if (clientConnected) {
          updateWS(code);
        }
        if (googleEnabled) {
          updateGoogle(code);
        }
      break;    
    case 't': // time only
      if (clientConnected) {
        updateWS(code);
      }
      if (debug && googleEnabled) {
        updateGoogle(code);
      }
      break;
    case 's': // state
      if (debug && googleEnabled) {
        updateGoogle(code);
      }
      break;
    default:
      break;  
  }
}

void updateWS(char code) {
  switch(code) {
    case 'd': // doorStatus, motorOn, or motorDir
      if (motorOn) {
        if (motorDir) {
          message = F("opening ");
        } else {
          message = F("closing ");
        }
        message += int(motorTime);
      } else if (doorStatus) {
        message = F("open");
      } else {
        message = F("closed");
      }
      webSocket.broadcastTXT(message);
      break;
    case 'f': // flockStatus
      message = F("flock ");
      message += flockStatus;
      webSocket.broadcastTXT(message);
      break; 
    case 't': // update time only
      if (DATETIME_RDY) {
        buildTimeString();
        webSocket.broadcastTXT(message);
      }
      break;
    case 'n': // sun times / date (only sent once per client session)
    /* if there are any new date/time or sunrise/sunset time 
     *  data ready, send to connected clients
     */
      if (DATETIME_RDY) {
        message = F("date: ");
        message += String(CURRENT_MONTH) + '/' + String(CURRENT_DATE) + '/' + CURRENT_YEAR;
        webSocket.broadcastTXT(message);
        buildTimeString();
        webSocket.broadcastTXT(message);
      }    
      if (SUNRISE_RDY && SUNSET_RDY) {
        message = F("sunrise/sunset: ");
        message += String(SUNRISE_HOUR) + ':';// + minute_R + F(" AM");
        if (SUNRISE_MINUTE < 10) {
          message += '0';
        }
        message += String(SUNRISE_MINUTE) + F(" AM/");
        message += String(SUNSET_HOUR - 12) + ':';
        if (SUNSET_MINUTE < 10) {
          message += '0';
        }
        message += String(SUNSET_MINUTE) + F(" PM");
        webSocket.broadcastTXT(message);
      }
      break;
    case 'c': // autoMode
      if (autoMode) {
        message = F("control AUTO");
      } else {
        message = F("control MANUAL");
      }
      webSocket.broadcastTXT(message);
      break;
    case 'm': // motorInterval
      message = F("mo ");
      message += motorInterval_open;
      webSocket.broadcastTXT(message);
      message = F("mc ");
      message += motorInterval_close;
      webSocket.broadcastTXT(message);
      break;
    case 'e': // offsets
      message = F("eo ");
      message += offset_open;
      webSocket.broadcastTXT(message);
      message = F("ec ");
      message += String(offset_close);
      webSocket.broadcastTXT(message);
      break;
    case 'g': // googleEnabled
      if (googleEnabled) {
        message = F("google ON");
      } else {
        message = F("google OFF");
      }
      webSocket.broadcastTXT(message);
      break;
    case 's': // state
      message = F("state ");
      message += int(state);
      webSocket.broadcastTXT(message);
      break;
    default:
      break;  
  }
}

void updateGoogle(char code) {
  switch(code) {
    case 'd': // doorStatus, motorOn, or motorDir
      if (!motorOn) {
        if (doorStatus) {
          message = F("do");
        } else {
          message = F("dc");
        }      
        postToGoogle(message);
      }
      break;
    case 't': // sun times / date-time
    case 'n':
      break;
    case 'c': // autoMode
      if (autoMode) {
        message = F("ca");
      } else {
        message = F("cm");
      }
      postToGoogle(message);
      if (autoMode) {
        updateGoogle('e');
        updateGoogle('m');
      }
      break;
    case 'm': // motorInterval
      message = F("mo: ");
      message += String(motorInterval_open);
      message += F("  mc: ");
      message += String(motorInterval_close);
      postToGoogle(message);
      break;
    case 'e': // offsets
      message = F("eo: ");
      message += String(offset_open);
      message += F("  ec: ");
      message += String(offset_close);
      postToGoogle(message);
      break;
    case 'f': // flockStatus
      message = F("flock: ");
      message += String(flockStatus);
      postToGoogle(message);
      break; 
    case 'g': // googleEnabled
      if (googleEnabled) {
        message = F("Hello from the ESP");
      } else {
        message = F("Go away Google");
      }
      postToGoogle(message);
      break;
    case 's': // state
      message = F("state ");
      message += int(state);  
      postToGoogle(message);
      break;
    default:
      break;  
  }
}
