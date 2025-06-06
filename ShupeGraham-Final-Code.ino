#include <Adafruit_HTU31D.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <stdlib.h>

#define DEBUG 1  // Set to 0 to disable debug mode
#define DEBUG_TEMPF_OFFSET 0  // Adds an offset to the temperature (in F) the HTU31D reads. Only applied in debug mode

// Actuators:
#define ONBOARD_LED_PIN 7
#define NEOPIXEL_PIN 10
#define NUM_NEOPIXELS 2
#define BUZZER_PIN 6
#define BUZZER_FREQ 2000

// Sensor pins:
#define ALS_BOTTOM 9
#define ALS_TOP 1
#define BUTTON_PIN 12

// Heat index thresholds:
#define HI_CAUTION 80
#define HI_EX_CAUTION 90
#define HI_DANGER 103
#define HI_EX_DANGER 125


bool buzzerSilenced = false;
bool neoPixelOn = true;

Adafruit_HTU31D htu = Adafruit_HTU31D();  // Connected to SDA and SCL pins
uint32_t timestamp;
uint32_t heaterToggleTimestamp;

Adafruit_NeoPixel neoPixels(NUM_NEOPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // Setup pins:
  pinMode(LIGHT_SENSOR, INPUT);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(BUTTON_PIN, HIGH);

  #if DEBUG
  Serial.begin(9600);
  #endif

  htu.begin(0x40);

  neoPixels.begin();
  neoPixels.show();

  #if DEBUG
  Serial.println("Started HTU and Neopixels");
  #endif

  timestamp = millis();
}


/*
Calculates the difference between the two light sensors.
The larger the difference between the two sensors, the larger the sun level (from 0-10).
*/
int getSunLevel() {
  int topValue = analogRead(ALS_TOP);
  int bottomValue = analogRead(ALS_BOTTOM);
  if (topValue - bottomValue > 200) {
    return (topValue - bottomValue) / 100;
  } else {
    return 0;
  }
}

/*
Gets the heat index given humidity and temperature.
The sun level offset is automatically calculated and included in the returned value.
This relies on the insane heat index calculation formula from the National Weather Service:
https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml
*/
float getHeatIndex(float tempC, float relHumidity) {
  float tempF = (tempC * 1.8) + 32;

  // The sun level adds an offset to account for sun intensity.
  int sunLevel = getSunLevel();

  #if DEBUG
  tempF += DEBUG_TEMPF_OFFSET;
  Serial.print(timestamp);
  Serial.print(" sunLevel: ");
  Serial.println(sunLevel);
  #endif

  // Calculate the simple formula first, returning it if the HI < 80:
  float heatIndex = 0.5 * (tempF + 61.0 + ((tempF - 68.0) * 1.2) + (relHumidity * 0.094)) + sunLevel;
  if (heatIndex < 80.0) {
    return heatIndex;
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

void turnOnHeater()

void loop() {
  if (digitalRead(BUTTON_PIN) == 0) {
    buzzerSilenced = true;
  }

  if (millis() - timestamp < 500) {
    // To preserve battery life, only check sensor data every 0.5 seconds
    return;
  }
  timestamp = millis();

  if ()

  sensors_event_t humidity, temp;
  htu.getEvent(&humidity, &temp);
  int heatIndex = getHeatIndex(temp.temperature, humidity.relative_humidity);

  #if DEBUG
  Serial.print(timestamp);
  Serial.print(" Humidity (%): ");
  Serial.print(humidity.relative_humidity);
  Serial.print(" Temperature (C): ");
  Serial.print(temp.temperature);
  Serial.print(" Heat Index: ");
  Serial.print(heatIndex);
  Serial.print(" Buzzer silenced? ");
  Serial.println(buzzerSilenced);
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
    if (!buzzerSilenced) {
      tone(BUZZER_PIN, BUZZER_FREQ, 600);
    }
  } else if (heatIndex > HI_DANGER) {
    color = neoPixels.Color(255, 100, 0);
    buzzerSilenced = false;
  } else if (heatIndex > HI_EX_CAUTION) {
    color = neoPixels.Color(255, 200, 0);
  } else if (heatIndex > HI_CAUTION) {
    color = neoPixels.Color(255, 255, 0);
  } else {
    color = neoPixels.Color(0, 255, 0);
  }
  Serial.println(color);
  neoPixels.fill(color);
  neoPixels.show();
}





