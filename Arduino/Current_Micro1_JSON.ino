/*
  Analog pins for current (single-phase) and microphone → JSON NDJSON output
  One JSON line per sample is sent to the serial port (terminated by '\n')
  Fields: millis, I_A (A), P_W (estimated W), sound_level (0..1023)

  IMPORTANT NOTE
  The power P_W is only an ESTIMATE for single-phase systems: P = U_PH * I * cosφ.
  If the power factor varies (motor + variable drive), the error can be significant.
  Measuring both voltage and current simultaneously gives the true active power.
*/

#include <Arduino.h>

// Hardware configuration
constexpr uint8_t PIN_CURRENT = A3;  // Current sensor (analog output)
constexpr uint8_t PIN_SOUND   = A0;  // Grove analog microphone (LM2904)

// Acquisition settings
constexpr uint32_t BAUD       = 1000000;  // 1 Mb/s, suitable for 500 Hz+
constexpr unsigned long SAMPLE_US   = 2000; // 2000 µs → 500 Hz 
constexpr unsigned long MICRO_DELAY = 10;   // Small delay to avoid busy-waiting

// Current conversion
/*
  CURRENT_CONVERSION_FACTOR converts the ADC reading (0..1023) into amperes.
  Example (adjust according to the sensor):
  - 10-bit ADC → step = 5.0V / 1024
  - Total amplification ≈ 2.8 (for example: ACS + op-amp)
  - 20A full-scale sensor → multiply by 20.0
  => A = ADC * (5.0/1024.0/2.8) * 20.0
*/
constexpr float CURRENT_CONVERSION_FACTOR = (5.0f / 1024.0f / 2.8f) * 20.0f;

// Power estimation
/*
  Single-phase assumption: voltage U_PH is nearly constant, cosφ is assumed.
  Adjust U_PH and COS_PHI after calibration or measurement.
*/
float U_PH   = 230.0f;  // V
float COS_PHI = 0.85f;  // Dimensionless (to be calibrated)

// Optional microphone smoothing
/*
  The microphone output ranges from 0 to 1023.
  The code can send the raw value or a smoothed envelope (simple EMA).
*/
constexpr bool   USE_EMA_SOUND   = true;    // Enable smoothing
constexpr float  EMA_ALPHA       = 0.2f;    // 0..1 (0.2 = soft smoothing)
float            sound_ema       = 0.0f;

// Utility functions
static inline float readCurrentA(uint8_t pin) {
  // Convert ADC reading to amperes using the defined factor
  return analogRead(pin) * CURRENT_CONVERSION_FACTOR;
}

static inline uint16_t readSoundRaw(uint8_t pin) {
  // Read raw microphone value (0..1023)
  return static_cast<uint16_t>(analogRead(pin));
}

// Setup
void setup() {
  Serial.begin(BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
  // Initialize EMA to the first sample to avoid a startup peak
  sound_ema = readSoundRaw(PIN_SOUND);
}

// Real-time loop
void loop() {
  static unsigned long next_t = micros();
  unsigned long now = micros();

  // Wait for the sampling instant (fixed rate)
  if ((long)(now - next_t) < 0) {
    delayMicroseconds(MICRO_DELAY);
    return;
  }
  next_t += SAMPLE_US;

  // Sensor readings
  float    I_A  = readCurrentA(PIN_CURRENT);     // In amperes
  uint16_t Sraw = readSoundRaw(PIN_SOUND);       // 0..1023

  // Optional microphone smoothing (envelope)
  float S = Sraw;
  if (USE_EMA_SOUND) {
    sound_ema = EMA_ALPHA * S + (1.0f - EMA_ALPHA) * sound_ema;
    S = sound_ema;
  }

  // Estimated active power (single-phase)
  float P_W = U_PH * COS_PHI * I_A;

  // JSON NDJSON publication (one line per sample)
  // Format: {"millis":123456,"I_A":5.93,"P_W":2054.0,"sound_level":71}
  Serial.print('{');
  Serial.print("\"millis\":");      Serial.print(millis());
  Serial.print(",\"I_A\":");        Serial.print(I_A, 5);
  Serial.print(",\"P_W\":");        Serial.print(P_W, 1);
  Serial.print(",\"sound_level\":");Serial.print((int)S);
  Serial.println('}');

  // Catch-up in case of delay (if the loop took too long)
  now = micros();
  if ((long)(now - next_t) > 0) {
    unsigned long late = now - next_t;
    next_t += ((late / SAMPLE_US) + 1) * SAMPLE_US;
  }

  // LED indicator (blinks at f/2)
  static bool led = false;
  led = !led;
  digitalWrite(LED_BUILTIN, led);
}

