#include <Adafruit_HTU31D.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <stdlib.h>

#define DEBUG 1  // Set to 0 to disable debug mode
#define DEBUG_TEMPF_OFFSET 0  // Adds an offset to the temperature (in F) the HTU31D reads. Only applied in debug mode

// Actuators:
#define ONBOARD_LED_PIN 7
#define NEOPIXEL_PIN 0
#define NUM_NEOPIXELS 2
#define BUZZER_PIN 1
#define BUZZER_FREQ 1200

// Sensor pins (HTU uses I2C):
#define LIGHT_SENSOR 9

// Heat index thresholds:
#define HI_CAUTION 80
#define HI_EX_CAUTION 90
#define HI_DANGER 103
#define HI_EX_DANGER 125

#define SUNLIGHT_MIN 600  // Minimum analog reading to be considered sunlight. Adjust this based on your phototransistor.


bool neoPixelOn = true;
bool heaterOn = false;

Adafruit_HTU31D htu = Adafruit_HTU31D();  // Connected to SDA and SCL pins
uint32_t timestamp;
uint32_t heaterTimestamp;
float prevTemperatureC = 0;
float prevRelHumidity = 0;

Adafruit_NeoPixel neoPixels(NUM_NEOPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // Setup pins:
  pinMode(LIGHT_SENSOR, INPUT);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  #if DEBUG
  Serial.begin(9600);
  while (!Serial);  // wait for serial monitor to initialize
  #endif

  if (!htu.begin(0x40)) {
    digitalWrite(ONBOARD_LED_PIN, HIGH);
    #if DEBUG
    Serial.println("FATAL: could not find HTU sensor");
    #endif
    while (1);
  }

  neoPixels.begin();
  neoPixels.setBrightness(180);
  neoPixels.show();

  #if DEBUG
  Serial.println("Setup complete");
  #endif

  timestamp = millis();
  heaterTimestamp = millis();
}


/*
Determines the amount of sunlight hitting the phototransistor.
Returns 0 to 5 depending on the sun level.
*/
int getSunLevel() {
  int lightValue = analogRead(LIGHT_SENSOR);
  int sunLevel;
  if (lightValue - SUNLIGHT_MIN < 0) {
    sunLevel = 0;
  } else {
    // Translate the light level into one of five sunlight levels:
    sunLevel = (lightValue - SUNLIGHT_MIN) / ((1023 - SUNLIGHT_MIN) / 5);
  }

  #if DEBUG
  Serial.print(timestamp);
  Serial.print(" lightValue: ");
  Serial.print(lightValue);
  Serial.print(" sunLevel: ");
  Serial.println(sunLevel);
  #endif

  return sunLevel;
}

/*
Returns the heat index given humidity and temperature.
The sun level offset is automatically calculated and included in the returned value.
This relies on the insane heat index calculation formula from the National Weather Service:
https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
*/
float getHeatIndex(float tempC, float relHumidity) {
  float tempF = (tempC * 1.8) + 32;

  // The sun level adds an offset to account for sun intensity
  int sunLevel = getSunLevel();

  #if DEBUG
  tempF += DEBUG_TEMPF_OFFSET;
  #endif

  // Calculate the simple formula first, returning it if the HI < 80:
  float heatIndex = 0.5 * (tempF + 61.0 + ((tempF - 68.0) * 1.2) + (relHumidity * 0.094));
  if (heatIndex < 80.0) {
    return heatIndex + sunLevel;
  }

  // Since HI >= 80, we have to use the regression formula:
  heatIndex = -42.379 + 2.04901523*tempF+ 10.14333127*relHumidity - 0.22475541*tempF*relHumidity
   - 0.00683783*tempF*tempF - 0.05481717*relHumidity*relHumidity + 0.00122874*tempF*tempF*relHumidity
   + 0.00085282*tempF*relHumidity*relHumidity - 0.00000199*tempF*tempF*relHumidity*relHumidity;

  // Adjustment for specific temp/humidity ranges:
  if (relHumidity < 13 && tempF > 80 && tempF < 112) {
    heatIndex -= ((13 - relHumidity) / 4) * sqrt((17 - abs(tempF - 95.0)) / 17);
  } else if (relHumidity > 85 && tempF > 80 && tempF < 87) {
    heatIndex += ((relHumidity - 85) / 10) * ((87 - tempF) / 5);
  }
  
  return heatIndex + sunLevel;
}

void loop() {
  // Cycle the heater every 5 seconds if humidity > 70%:
  if (millis() - heaterTimestamp > 5000 && prevRelHumidity > 70) {
    heaterOn = !heaterOn;

    #if DEBUG
    Serial.print(timestamp);
    if (heaterOn) {
      Serial.println("Turning heater on");
    } else {
      Serial.println("Turning heater off");
    }
    #endif

    if (!htu.enableHeater(heaterOn)) {
      #if DEBUG
      Serial.println("ERROR: could not send enableHeater command");
      #endif
    }
    heaterTimestamp = millis();
  }

  if (millis() - timestamp < 500 || heaterOn) {
    // For better efficiency, only check sensor data every 0.5 seconds
    return;
  }
  timestamp = millis();

  // Get heat index:
  sensors_event_t humidity_event, temp_event;
  float tempC, relHumidity;
  if (!htu.getEvent(&humidity_event, &temp_event) || temp_event.temperature == 0 || humidity_event.relative_humidity == 0) {
    // Failed to read sensor data, so just use the last known data
    tempC = prevTemperatureC;
    relHumidity = prevRelHumidity;
  } else {
    tempC = temp_event.temperature;
    relHumidity = humidity_event.relative_humidity;
    prevTemperatureC = tempC;
    prevRelHumidity = relHumidity;
  }
  float heatIndex = getHeatIndex(tempC, relHumidity);

  #if DEBUG
  Serial.print(timestamp);
  Serial.print(" Humidity (%): ");
  Serial.print(humidity_event.relative_humidity);
  Serial.print(" Temperature (C): ");
  Serial.print(temp_event.temperature);
  Serial.print(" Heat Index: ");
  Serial.print(heatIndex);
  #endif

  uint32_t color;

  // Set color and buzzer state based on heatIndex:
  if (heatIndex > HI_EX_DANGER) {
    // Full red, flashing at 2 Hz
    if (neoPixelOn) {
      color = neoPixels.Color(0, 0, 0);
    } else {
      color = neoPixels.Color(255, 0, 0);
    }
    neoPixelOn = !neoPixelOn;
    tone(BUZZER_PIN, BUZZER_FREQ, 600);
  } else if (heatIndex > HI_DANGER) {
    color = neoPixels.Color(255, 50, 0);
  } else if (heatIndex > HI_EX_CAUTION) {
    color = neoPixels.Color(255, 220, 0);
  } else if (heatIndex > HI_CAUTION) {
    color = neoPixels.Color(180, 255, 0);
  } else {
    color = neoPixels.Color(0, 255, 0);
  }
  Serial.println(color);
  neoPixels.fill(color);
  neoPixels.show();
}



