#include <Arduino.h>
#include <time.h> // gmtime_r()
#include <AceTime.h>
#include <WiFi.h>

using namespace ace_time;

#define WIFI_SSID "WhiskeyJack"
#define WIFI_PASSWORD "Georgia4"
#define NTP_SERVER "pool.ntp.org"

// Value of time_t for 2000-01-01 00:00:00, used to detect invalid SNTP
// responses.
static const time_t EPOCH_2000_01_01 = 946684800;

// Number of millis to wait for a WiFi connection before doing a software
// reboot.
static const unsigned long REBOOT_TIMEOUT_MILLIS = 15000;

//-----------------------------------------------------------------------------

// C library day of week uses Sunday==0.
static const char* const DAYS_OF_WEEK[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

//-----------------------------------------------------------------------------

// Define zone processors to handle timezones efficiently.
ExtendedZoneProcessor zoneProcessorLosAngeles;
ExtendedZoneProcessor zoneProcessorParis;
ExtendedZoneProcessor zoneProcessorNewYork;

// Print the UTC time, America/Los_Angeles time, Europe/Paris time, and America/New_York time
// using the AceTime library. TimeZone objects are light-weight and can be created on
// the fly.
void printNowUsingAceTime(time_t now, TimeZone& tz) {
  // Utility to convert ISO day of week with Monday=1 to human readable string.
  DateStrings dateStrings;

  // Convert to UTC time.
  LocalDateTime ldt = LocalDateTime::forUnixSeconds64(now);
  ldt.printTo(Serial);
  Serial.print(' ');
  Serial.print(dateStrings.dayOfWeekLongString(ldt.dayOfWeek()));
  Serial.println(F(" (AceTime)"));

  // Convert Unix time to the specified time zone.
  ZonedDateTime zdt = ZonedDateTime::forLocalDateTime(ldt, tz);
  zdt.printTo(Serial);
  Serial.print(' ');
  Serial.println(dateStrings.dayOfWeekLongString(zdt.dayOfWeek()));
}


// Prompt the user to select a timezone and return the corresponding TimeZone object.
TimeZone selectTimeZone() {
  Serial.println("Select a timezone from it's IANA:");
  Serial.println("1. Los Angeles (America/Los_Angeles)");
  Serial.println("2. Paris (Europe/Paris)");
  Serial.println("3. New York (America/New_York)");

  while (true) {
    if (Serial.available() > 0) {
      String selectedZone = Serial.readStringUntil('\n');
      Serial.println();
      selectedZone.trim();

      if (selectedZone.equalsIgnoreCase("America/Los_Angeles")) {
        return TimeZone::forZoneInfo(&zonedbx::kZoneAmerica_Los_Angeles, &zoneProcessorLosAngeles);
      } else if (selectedZone.equalsIgnoreCase("Europe/Paris")) {
        return TimeZone::forZoneInfo(&zonedbx::kZoneEurope_Paris, &zoneProcessorParis);
      } else if (selectedZone.equalsIgnoreCase("America/New_York")) {
        return TimeZone::forZoneInfo(&zonedbx::kZoneAmerica_New_York, &zoneProcessorNewYork);
      } else {
        Serial.println("Invalid input. Please select a valid option.");
      }
    }
  }
}


//-----------------------------------------------------------------------------

// Connect to WiFi. Sometimes the board will connect instantly. Sometimes it
// will struggle to connect. I don't know why. Performing a software reboot
// seems to help, but not always.
void setupWifi() {
  WiFi.begin(WIFI_SSID
, WIFI_PASSWORD);
Serial.print("Connecting to Wi-Fi");
while (WiFi.status() != WL_CONNECTED)
{
Serial.print(".");
delay(300);
}
Serial.println();
Serial.print("Connected with IP: ");
Serial.println(WiFi.localIP());
Serial.println();
}

// Setup the SNTP client. Set the local time zone to be UTC, with no DST offset,
// because we will be using AceTime to perform the timezone conversions. The
// built-in timezone support provided by the ESP8266/ESP32 API has a number of
// deficiencies, and the API can be quite confusing.
void setupSntp() {
  Serial.print(F("Configuring SNTP"));
  configTime(0 /*timezone*/, 0 /*dst_sec*/, NTP_SERVER);

  // Wait until SNTP stabilizes by ignoring values before year 2000.
  unsigned long startMillis = millis();
  while (true) {
    Serial.print('.'); // Each '.' represents one attempt.
    time_t now = time(nullptr);
    if (now >= EPOCH_2000_01_01) {
      Serial.println(F(" Done."));
      break;
    }

    // Detect timeout and reboot.
    unsigned long nowMillis = millis();
    if ((unsigned long) (nowMillis - startMillis) >= REBOOT_TIMEOUT_MILLIS) {
    #if defined(ESP8266)
      Serial.println(F(" FAILED! Rebooting..."));
      delay(1000);
      ESP.reset();
    #elif defined(ESP32)
      Serial.println(F(" FAILED! Rebooting..."));
      delay(1000);
      ESP.restart();
    #else
      Serial.print(F(" FAILED! But cannot reboot. Continuing"));
      startMillis = nowMillis;
    #endif
    }

    delay(500);
  }
}

//-----------------------------------------------------------------------------

void setup() {
  delay(1000);
  Serial.begin(115200);

  setupWifi();
  setupSntp();
}

void loop() {
  TimeZone selectedTimeZone = selectTimeZone();
  time_t now = time(nullptr);
  
  printNowUsingAceTime(now, selectedTimeZone);
  Serial.println();

  delay(5000);
}
