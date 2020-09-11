/* date/time API calling and parsing/printing data
 * - NTP and sunrise-sunset
 * - look at NTP documentation
 */

void updateLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    DATETIME_RDY = 0;
    return;
  }
  DATETIME_RDY = 1;
  CURRENT_YEAR = timeinfo.tm_year+1900;
  CURRENT_MONTH = timeinfo.tm_mon+1;
  CURRENT_DATE = timeinfo.tm_mday;
  CURRENT_HOUR = timeinfo.tm_hour;
  CURRENT_MINUTE = timeinfo.tm_min;
  DST_FLAG = timeinfo.tm_isdst;
  prevTimeMillis = millis();
  
  if (clientConnected) {
    updateClients();
  }
}

void getSunTimes() {
  // https://www.dfrobot.com/blog-917.html
  HTTPClient http;
  http.begin(sunRiseSetAPI);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    http.end();
    StaticJsonDocument<520> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
      Serial.println("Parsing failed");
      Serial.println(err.c_str());
      return;
    }
    String sunrise = doc["results"]["sunrise"];
    String sunset = doc["results"]["sunset"];
    //Serial.println(sunrise);
    parseUTCString(sunrise, sunriseVals);
    parseUTCString(sunset, sunsetVals);
    //updateClients();
  } else {
    Serial.println("Error on HTTP request");
  }
}

// unpack the string from sunrise-sunset API
// input format: "HH:MM:SS XM" in UTC or "H:MM:SS XM"
// output: [ready, hour, min] in 24-hour Mountain time
void parseUTCString(String UTCString, int (&buffArray)[3]) {
  int hour, minute;
  int firstColon = UTCString.indexOf(':');
  if (firstColon == 1 || firstColon == 2) {
    hour = UTCString.substring(0, firstColon).toInt();
    minute = UTCString.substring(firstColon+1, firstColon+3).toInt();
    char AMPM = (UTCString.charAt(firstColon+7));
    int DST;
    if (DST_FLAG > 0) { // if daylight savings
      DST = 1;} else {DST = 0;}
    if (AMPM == 'A') { // for AM, subtract GMT offset
      buffArray[1] = (hour+24+TZ+DST) % 24;
    } else if (AMPM == 'P') {
      buffArray[1] = (hour+36+TZ+DST) % 24;
    }
    buffArray[0] = 1;
    buffArray[2] = minute;
    return;
  }
  buffArray[0] = 0;
  buffArray[1] = -1;
  buffArray[2] = -1; // error when hour/min is negative
}

void printLocalTime() {
  char DST;
  if (DATETIME_RDY > 0) {
    if (DST_FLAG > 0) { // if daylight savings,
      DST = 'D';
    } else { DST = 'S';}
    Serial.printf("Today's date - %u/%u/%u\n", 
      CURRENT_YEAR, CURRENT_MONTH, CURRENT_DATE);
    Serial.printf("Local time - %u:%u M%cT\n", 
      CURRENT_HOUR, CURRENT_MINUTE, DST);
  } else {
    Serial.printf("TIMEDATE ERRROR [%u] - %u/%u/%u %u:%u DST:%u\n", 
      DATETIME_RDY, CURRENT_YEAR, CURRENT_MONTH, 
      CURRENT_DATE, CURRENT_HOUR, CURRENT_MINUTE, 
      DST_FLAG);
  }
}

void printSunTimes() {
  if (SUNRISE_RDY) {
    Serial.printf("Sunrise time - %u:%u\n", 
      SUNRISE_HOUR, SUNRISE_MINUTE);
  } else {
    Serial.printf("SUNRISE TIME ERROR [%u] - %u:%u\n", 
      SUNRISE_RDY, SUNRISE_HOUR, SUNRISE_MINUTE);
  }
  if (SUNSET_RDY) {
    Serial.printf("Sunset time - %u:%u\n", 
      SUNSET_HOUR, SUNSET_MINUTE);
  } else {
    Serial.printf("SUNSET TIME ERROR [%u] - %u:%u\n", 
      SUNSET_RDY, SUNSET_HOUR, SUNSET_MINUTE);
  }
}