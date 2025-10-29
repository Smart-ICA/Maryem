#include <Wire.h>
#include <MMA7660.h>
#include <ArduinoJson.h>

// Serial and timing parameters
constexpr uint32_t BAUD_RATE         = 1000000;  // 1 Mbps 
constexpr unsigned long MICRO_DELAY  = 10UL;     // Short delay to prevent busy-wait
constexpr unsigned long TIME_STEP_MS = 1UL;      // 1 ms → approximately 1000 Hz JSON output rate

// Sensors
MMA7660 accel;                     // Accelerometer (I2C)
constexpr uint8_t PIN_SOUND = A0;  // Analog input for the Grove microphone

// JSON buffer
// 128 bytes are sufficient for millis + 3 floats + 1 integer.
StaticJsonDocument<128> doc;

// Timing
unsigned long previousTimeUs = 0;
bool ledState = LOW;

void setup() {
  // Serial communication
  Serial.begin(BAUD_RATE);

  // I2C initialization
  Wire.begin();
#if defined(TWBR)
  Wire.setClock(400000UL);   // Enable I2C Fast Mode (400 kHz)
#endif

  // Accelerometer initialization
  accel.init();

  // Microphone input configuration
  pinMode(PIN_SOUND, INPUT);

  // LED indicator
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  const unsigned long stepUs = TIME_STEP_MS * 1000UL;
  unsigned long nowUs = micros();

  // Maintain a fixed time step (every TIME_STEP_MS milliseconds)
  if (nowUs - previousTimeUs >= stepUs) {
    previousTimeUs = nowUs;

    // LED heartbeat to visualize the sampling rate
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);

    // Sensor readings
    float ax, ay, az;
    accel.getAcceleration(&ax, &ay, &az);   // Acceleration in g

    // Microphone reading (0–1023 on most boards)
    // This is the instantaneous value; a more stable signal can be obtained
    // by applying RMS or moving average filtering.
    int sound = analogRead(PIN_SOUND);

    // JSON construction
    doc.clear();
    doc["millis"] = millis();

    JsonObject acc = doc.createNestedObject("acceleration");
    acc["x_g"] = ax;
    acc["y_g"] = ay;
    acc["z_g"] = az;

    doc["sound_level"] = sound;

    // JSON transmission (NDJSON: one line per object)
    serializeJson(doc, Serial);
    Serial.println();
  }

  // Short sleep to reduce CPU usage while waiting for the next cycle
  delayMicroseconds(MICRO_DELAY);
}

