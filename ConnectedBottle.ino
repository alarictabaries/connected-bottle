#include <SigFox.h>
#include <ArduinoLowPower.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_GPS.h>

// Test values
#define SENSORS_READING_TIME 5000 // prod value : 36000000ms
#define KEEPALIVE_TIME 30000 // prod value : 180000ms

// # of "wrong state" needed to confirm
#define STATE_TRIGGER 3

// Adjust the gap defining the "wrong state"
#define ANGLE_MARGIN 45

#define GPSSerial Serial1

#define GPSECHO false
#define DEBUG false
#define SEND true


// Flag allows to send different types of alerts
// Keepalive : "K" | Fallen : "F"
struct Payload {
  char flag;
  float l_lat;
  float l_long;
};

struct Coordinates {
  float l_lat;
  float l_long;
};

struct Accel {
  float x;
  float y;
  float z;
  float roll;
  float pitch;
};

Adafruit_GPS GPS(&GPSSerial);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(1);

uint32_t timer = millis();

int timer_increment = 0;

int state = 0;
int current_state;
float latitude  = 0.0f;
float longitude = 0.0f;
Coordinates tmp_coordinates;

void setup() {

  if(DEBUG) {
    Serial.begin(9600);
    while (!Serial) {
      ;
    }
  }

  pinMode(LED_BUILTIN, OUTPUT);

  if (!SigFox.begin()) {
    reboot();
  }

  SigFox.debug();

  SigFox.end();

  GPS.begin(9600);
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  GPS.sendCommand(PGCMD_ANTENNA);
  delay(1000);
  if (DEBUG == true) {
    GPSSerial.println(PMTK_Q_RELEASE);
  }

  if (!accel.begin())
  {
    if (DEBUG == true) {
      Serial.println("! Accelerometer not detected");
    }
    digitalWrite(LED_BUILTIN, HIGH);
    while (1);
  }
  accel.setRange(ADXL345_RANGE_16_G);
}

void loop() {

  GPS.read();

  if (GPS.newNMEAreceived()) {
    GPS.lastNMEA();
    GPS.parse(GPS.lastNMEA());
  }

  if (timer > millis()) timer = millis();
  if (millis() - timer > SENSORS_READING_TIME) {
    timer = millis();

    timer_increment++;

    current_state = updateState(getAccel());

    if(current_state == 1) {
      state++;
      if (DEBUG == true) {
        Serial.print("- State increment : ");
        Serial.print(state);
        Serial.print("/");
        Serial.println(STATE_TRIGGER);
      }
    } else {
      state = 0;
      digitalWrite(LED_BUILTIN, LOW);
      if (DEBUG == true) {
        Serial.println("- Increment (re)initialized");
      }
    }

    if(state == STATE_TRIGGER) {
      if (DEBUG == true) {
        Serial.println("- State definitely abnormal, sending alarm");
      }

      // Sending alarm
      // Alarm code : F
      if (SEND == true) {
        tmp_coordinates = getLocation();
        delay(100);
        sendMessage(tmp_coordinates, 'F');
      } else {
        getLocation();
      }

      // Onboard light is flashing once alarm is sent
      for(int i = 0; i < 3; i++) {
       digitalWrite(LED_BUILTIN, HIGH);
       delay(200);
       digitalWrite(LED_BUILTIN, LOW);
       delay(400);
      }
    }

    // Sending keepalive
    // Alarm code : K
    if (timer_increment == (KEEPALIVE_TIME / SENSORS_READING_TIME)) {
       if (DEBUG == true) {
         Serial.println("- Sending keepalive");
       }
       if (SEND == true) {
          tmp_coordinates = getLocation();
          delay(100);
          sendMessage(tmp_coordinates, 'K');
       } else {
         getLocation();
       }
       timer_increment = 0;
    }

  }
}

// Getting accelerometer current values
Accel getAccel() {

  Accel d_accel;
  float x_angle, y_angle, roll, pitch;

  sensors_event_t event_accel;
  accel.getEvent(&event_accel);

  if (DEBUG == true) {
    Serial.print("- Accelerometer (m/s^2) : X");
    Serial.print(event_accel.acceleration.x);
    Serial.print(" Y");
    Serial.print(event_accel.acceleration.y);
    Serial.print(" Z");
    Serial.println(event_accel.acceleration.z);
  }

  d_accel.x = event_accel.acceleration.x;
  d_accel.y = event_accel.acceleration.y;
  d_accel.z = event_accel.acceleration.z;

  x_angle = atan(event_accel.acceleration.y / sqrt((event_accel.acceleration.x * event_accel.acceleration.x) + (event_accel.acceleration.z * event_accel.acceleration.z)));
  y_angle = atan(event_accel.acceleration.x / sqrt((event_accel.acceleration.y * event_accel.acceleration.y) + (event_accel.acceleration.z * event_accel.acceleration.z)));

  d_accel.roll = x_angle * 180 / M_PI;
  d_accel.pitch  = y_angle * 180 / M_PI;

  if (DEBUG == true) {
    Serial.print("- Angles (°) : roll ");
    Serial.print(d_accel.roll);
    Serial.print(" pitch ");
    Serial.println(d_accel.pitch);
  }

  return d_accel;

}

// Detecting if the state is wrong or not
// Wrong : 1 | OK : 0
int updateState(Accel d_accel) {
  if (((d_accel.roll < (90 - ANGLE_MARGIN)) || (d_accel.roll > (90 + ANGLE_MARGIN))) || ((d_accel.pitch < (0 - ANGLE_MARGIN)) || (d_accel.pitch > (0 + ANGLE_MARGIN)))) {

    if (DEBUG == true) {
      Serial.println("- Device state abnormal");
    }
    return 1;
  } else {
    if (DEBUG == true) {
      Serial.println("- Device state OK");
    }
    return 0;
  }
}

// Getting current location
// Can't get location values : -1, -1
Coordinates getLocation() {

  Coordinates location;

  if (GPS.fix) {
    if (DEBUG == true) {
      Serial.print("- Location : ");
      Serial.print(GPS.latitudeDegrees, 4);
      Serial.print(", ");
      Serial.println(GPS.longitudeDegrees, 4);
    }
    location.l_lat = GPS.latitudeDegrees;
    location.l_long = GPS.longitudeDegrees;
  } else {
    if (DEBUG == true) {
      Serial.println("! Can't fix GPS signal");
    }
    location.l_lat = -1;
    location.l_long = -1;
  }

  return location;
}

// Sending a message to SigFox
void sendMessage(Coordinates location, char flag) {

  Payload payload;

  payload.flag = flag;
  payload.l_lat = location.l_lat;
  payload.l_long = location.l_long;

  int ret;

  Serial.println("- Sending message to SigFox");

  SigFox.begin();
  delay(100);

  SigFox.status();
  delay(1);

  SigFox.beginPacket();
  SigFox.write(payload);

  ret = SigFox.endPacket();

  if (DEBUG == true) {
    Serial.println(SigFox.status(SIGFOX));
    Serial.println(SigFox.status(ATMEL));
    Serial.println(ret);
    if (ret > 0) {
      Serial.println("- Error while sending");
    } else {
      Serial.println("! Sent to SigFox network");
    }
  }

  SigFox.end();

}

void reboot() {
  NVIC_SystemReset();
  while (1);
}
