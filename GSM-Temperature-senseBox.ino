#include "Adafruit_FONA.h"

// Temperature Sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h"

#include <SoftwareSerial.h>

// load config file
#include "./CONFIG.h"


// Adafruit FONA
#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4

// Temperature Sensor PIN
#define ONE_WIRE_BUS 9

//senseBox ID
#define SENSEBOX_ID SECRET_SENSEBOX_ID

//Sensor IDs
#define SIGNAL_STRENGTH_ID SECRET_SIGNAL_STRENGTH_ID // Signalstärke
#define BATTERY_ID SECRET_BATTERY_ID // Batterieladung
#define TEMPERATURE_ID SECRET_TEMPERATURE_ID // Bodentemperatur


// Adafruit FONA
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// get SIM PIN from config.h
char myPIN[4] = SECRET_SIM_PIN;


// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// seconds divided by sleep time (8)
const int FREQUENCY = 30 / 8;

// counter to count wakeups
unsigned int wakeups = 0;

// fona power status
boolean isFONAOn = false;

void setup() {
  Serial.begin(115200);

  // pin to toggle GMS Module
  pinMode(5, OUTPUT);

  if(!isFONAOn) {
    // turn FONA on
    toggleFONAPower();
    // setup FONA
    setupFONA();
  }

  // Initialize temperature sensor
  Serial.println("init sensors...");
  // Start up the library
  sensors.begin();

  delay(1000);

}

void loop() {
  
  if (wakeups % FREQUENCY == 0) {
    wakeups = 0;

    if(!isFONAOn) {
      // turn FONA on
      toggleFONAPower();
      // setup FONA
      setupFONA();
    }
    
    // Reading Temperature
    sensors.requestTemperatures(); // Send the command to get temperature readings
    double temperature = sensors.getTempCByIndex(0);
    postToOsem(String(TEMPERATURE_ID), String(temperature));

    // Reading Battery Percentage
    uint16_t vbat;
    if (!fona.getBattPercent(&vbat)) {
      Serial.println(F("Failed to read Batt"));
    } else {
      Serial.print(F("VPct = ")); Serial.print(vbat); Serial.println(F("%"));
    }
    postToOsem(String(BATTERY_ID), String(vbat));

    // read the RSSI
    uint8_t n = fona.getRSSI();
    int8_t r;

    Serial.print(F("RSSI = ")); Serial.print(n); Serial.print(": ");
    if (n == 0) r = -115;
    if (n == 1) r = -111;
    if (n == 31) r = -52;
    if ((n >= 2) && (n <= 30)) {
      r = map(n, 2, 30, -110, -54);
    }
    Serial.print(r); Serial.println(F(" dBm"));
    postToOsem(String(SIGNAL_STRENGTH_ID), String(r));

    Serial.println("going to sleep...");
    // turn FONA off
    toggleFONAPower();
  } else {
    // Enter power down state for 8 s with ADC and BOD module disabled
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  wakeups++;
}

// post value to sensor in opensensemap
void postToOsem(String sensorID, String value) {
  // build string to send
  String dataString = "{\n";
  dataString += "\t \"value\": " + String(value);
  dataString += "\n}";
  Serial.println(dataString);
  char data[dataString.length() + 1];
  dataString.toCharArray(data, dataString.length() + 1);

  // build POST URL
  String postURLString = "ingress.opensensemap.org:80/boxes/" + String(SENSEBOX_ID) + "/" + String(sensorID);
  char postURL[postURLString.length() + 1];
  postURLString.toCharArray(postURL, postURLString.length() + 1);
  delay(50);
  Serial.println("PostURL:");
  Serial.println(String(postURL));
  delay(50);

  // do post request
  uint16_t statuscode;
  int16_t length;
  if (!fona.HTTP_POST_start(postURL, F("application/json"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length)) {
    Serial.println("Failed!");
  }
  //delay(2000);
  // end post request
  fona.HTTP_POST_end();
  delay(2000);
}

void toggleFONAPower() {
  digitalWrite(5, LOW);
  delay(2500);
  digitalWrite(5, HIGH);

  isFONAOn = !isFONAOn;
}

void setupFONA() {
  // initialize FONA
  Serial.println("init FONA...");
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
  }
  Serial.println(F("FONA is OK"));

  delay(5000);

  // set APN settings
  Serial.println("APN settings...");
  fona.setGPRSNetworkSettings(F(SECRET_APN_PROVIDER), F(""), F(""));

  delay(5000);

  // Unlock SIM
  Serial.print("Unlocking SIM with PIN: ");
  Serial.println(myPIN);
  if (!fona.unlockSIM(myPIN)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }

  delay(5000);

  // Wait till FONA has network access
  Serial.println("Wait for network...");
  while (true) {
    uint8_t n = fona.getNetworkStatus();
    if (n == 0) Serial.println(F("Not registered"));
    if (n == 1) {
      Serial.println(F("Registered (home)"));
      break;
    }
    if (n == 2) Serial.println(F("Not registered (searching)"));
    if (n == 3) Serial.println(F("Denied"));
    if (n == 4) Serial.println(F("Unknown"));
    if (n == 5) {
      Serial.println(F("Registered roaming"));
      break;
    }
    delay(1000);
  }

  delay(5000);

  // Enable GPRS
  Serial.println("Enable GPRS...");
  if (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to turn on"));
    // while(true);
    // asm volatile ("jmp 0");
  }
  delay(2000);
}


