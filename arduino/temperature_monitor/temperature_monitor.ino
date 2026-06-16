/**
 * Temperature Display and Serial Monitor Firmware
 * 
 * Target: Arduino Uno
 * LCD: 16x2 LCD with I2C adapter (pins SDA -> A4, SCL -> A5)
 * Name: UWASE Sonia
 * 
 * Features:
 * - Reads temperature from a 3-pin sensor (configurable for LM35, DHT11, or DS18B20)
 * - Displays candidate name on the 1st row (scrolls if > 16 characters)
 * - Displays temperature value on the 2nd row
 * - Sends the temperature value via Serial port to PC at 9600 baud
 * - Non-blocking scrolling and sensor reading using millis()
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==========================================
// CONFIGURATION
// ==========================================
#define SENSOR_LM35    1
#define SENSOR_DHT11   2
#define SENSOR_DS18B20 3

// SELECT SENSOR TYPE HERE (Default is now DHT11 Digital based on user connection)
#define ACTIVE_SENSOR SENSOR_DHT11

// PIN DEFINITIONS
#define TEMP_PIN_ANALOG A0 // For LM35
#define TEMP_PIN_DIGITAL 2 // For DHT11 / DS18B20

// CANDIDATE NAME
// Note: If name length is > 16, it will auto-scroll horizontally.
const String CANDIDATE_NAME = "UWASE Sonia"; 

// LCD Configuration (Typically address is 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Conditional Library Imports
#if ACTIVE_SENSOR == SENSOR_DS18B20
  #include <OneWire.h>
  #include <DallasTemperature.h>
  OneWire oneWire(TEMP_PIN_DIGITAL);
  DallasTemperature sensors(&oneWire);
#endif

// Timing Variables
unsigned long prevSensorMillis = 0;
const unsigned long sensorInterval = 2000; // Read sensor every 2 seconds

unsigned long prevScrollMillis = 0;
const unsigned long scrollInterval = 350;  // Scroll speed in ms

// Scrolling Variables
int scrollPos = 0;
String scrollText = "";

// Temperature reading
float currentTemp = 0.0;

void setup() {
  // Initialize Serial Communication
  Serial.begin(9600);
  
  // Initialize I2C LCD
  lcd.init();
  lcd.backlight();
  
  // Sensor Initialization
  #if ACTIVE_SENSOR == SENSOR_DHT11
    pinMode(TEMP_PIN_DIGITAL, INPUT_PULLUP);
  #elif ACTIVE_SENSOR == SENSOR_DS18B20
    sensors.begin();
  #endif

  // Prepare scrolling text if candidate name is too long
  if (CANDIDATE_NAME.length() > 16) {
    // Pad with spaces to loop cleanly
    scrollText = CANDIDATE_NAME + "                ";
  } else {
    // Center alignment helper for name <= 16
    int spaces = (16 - CANDIDATE_NAME.length()) / 2;
    lcd.setCursor(spaces, 0);
    lcd.print(CANDIDATE_NAME);
  }

  // Initial read
  readTemperature();
  updateLCDTemp();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Read temperature and send over Serial periodically
  if (currentMillis - prevSensorMillis >= sensorInterval) {
    prevSensorMillis = currentMillis;
    readTemperature();
    updateLCDTemp();
    
    // Print to USB Serial
    Serial.println(currentTemp, 1);
  }

  // 2. Scroll the Candidate Name if it exceeds 16 chars
  if (CANDIDATE_NAME.length() > 16) {
    if (currentMillis - prevScrollMillis >= scrollInterval) {
      prevScrollMillis = currentMillis;
      lcd.setCursor(0, 0);
      
      // Get the 16 character substring to display
      String toDisplay = scrollText.substring(scrollPos, scrollPos + 16);
      lcd.print(toDisplay);
      
      scrollPos++;
      if (scrollPos > scrollText.length() - 16) {
        scrollPos = 0; // Reset scroll loop
      }
    }
  }
}

// Custom Library-free DHT11 reading logic
bool readDHT11Direct(float &temp) {
  uint8_t data[5] = {0,0,0,0,0};
  
  // 1. Send Start Signal
  pinMode(TEMP_PIN_DIGITAL, OUTPUT);
  digitalWrite(TEMP_PIN_DIGITAL, LOW);
  delay(18); // Keep low for 18ms
  digitalWrite(TEMP_PIN_DIGITAL, HIGH);
  delayMicroseconds(40);
  pinMode(TEMP_PIN_DIGITAL, INPUT_PULLUP);
  
  // 2. Wait for DHT response (Acknowledge)
  unsigned long timeout = 10000;
  while(digitalRead(TEMP_PIN_DIGITAL) == HIGH) {
    if (--timeout == 0) return false;
  }
  timeout = 10000;
  while(digitalRead(TEMP_PIN_DIGITAL) == LOW) {
    if (--timeout == 0) return false;
  }
  timeout = 10000;
  while(digitalRead(TEMP_PIN_DIGITAL) == HIGH) {
    if (--timeout == 0) return false;
  }
  
  // 3. Read 40 bits of data
  for (int i = 0; i < 40; i++) {
    timeout = 10000;
    while(digitalRead(TEMP_PIN_DIGITAL) == LOW) {
      if (--timeout == 0) return false;
    }
    
    // Measure high duration
    unsigned long startTime = micros();
    timeout = 10000;
    while(digitalRead(TEMP_PIN_DIGITAL) == HIGH) {
      if (--timeout == 0) return false;
    }
    
    // High time > 40 microseconds represents a '1'
    if ((micros() - startTime) > 40) {
      data[i/8] |= (1 << (7 - (i%8)));
    }
  }
  
  // 4. Verify checksum
  if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    // For DHT11, data[2] is the integral part of temperature, data[3] is the fractional part
    temp = data[2] + (data[3] * 0.1);
    return true;
  }
  
  return false;
}

// Function to read temperature based on configured sensor type
void readTemperature() {
  #if ACTIVE_SENSOR == SENSOR_LM35
    // LM35 Temperature Calculation:
    int raw = analogRead(TEMP_PIN_ANALOG);
    float voltage = raw * (5.0 / 1023.0);
    currentTemp = voltage * 100.0;
    
  #elif ACTIVE_SENSOR == SENSOR_DHT11
    float t = 0.0;
    if (readDHT11Direct(t)) {
      currentTemp = t;
    }
    
  #elif ACTIVE_SENSOR == SENSOR_DS18B20
    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);
    if (t != DEVICE_DISCONNECTED_C) {
      currentTemp = t;
    }
  #endif
}

// Update the 2nd row of the LCD with temperature value
void updateLCDTemp() {
  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(currentTemp, 1);
  lcd.print((char)223); // Degree symbol
  lcd.print("C   ");   // Clear trailing characters
}
