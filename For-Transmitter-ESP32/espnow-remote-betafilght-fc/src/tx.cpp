#include <Arduino.h>
#include <math.h>
#include "EspNowRcLink/Transmitter.h"

// Analog joystick pins (stick 1 = left, stick 2 = right)
const int JOY1_X_PIN = 34; // throttle (left stick up/down)
const int JOY1_Y_PIN = 35; // yaw    (left stick left/right) 
const int JOY2_X_PIN = 33; // roll   (right stick left/right)
const int JOY2_Y_PIN = 32; // pitch  (right stick up/down)
const int SWITCH_PIN = 25; // toggle switch -> channel 5
const int LED_PIN = 2; // Built-in LED
// Add these with your other pin definitions:
const int AUX2_PIN = 15; // Aux 2 (Channel 6)
const int AUX3_PIN = 2;  // Aux 3 (Channel 7)
const int AUX4_PIN = 4;  // Aux 4 (Channel 8)

// ADC and RC signal ranges
const int ANALOG_MIN = 0;
const int ANALOG_MAX = 4095;
const int RC_MIN = 1000;
const int RC_MAX = 2000;
const int RC_CENTER = 1500;
const int RC_HALF_SPAN = (RC_MAX - RC_MIN) / 2; // 500 us

// Tuning parameters
const uint32_t SEND_INTERVAL_MS = 20;      // 50 Hz update for smoother feel
const float FILTER_ALPHA = 0.15f;          // 0 < alpha <= 1, lower = smoother 25 tha
const float DEADZONE_FRACTION = 0.05f;     // ±5% stick deadzone around center
const int SWITCH_DEBOUNCE_MS = 120;

class EmaFilter {
public:
  explicit EmaFilter(float a = FILTER_ALPHA) : alpha(a), initialized(false), value(0.0f) {}
  float update(float sample) {
    if (!initialized) {
      value = sample;
      initialized = true;
    } else {
      value = alpha * sample + (1.0f - alpha) * value;
    }
    return value;
  }
private:
  float alpha;
  bool initialized;
  float value;
};

EspNowRcLink::Transmitter tx;

struct StickCenters {
  float yaw;
  float throttle;
  float roll;
  float pitch;
};

struct StickTrims {
  int yaw;
  int roll;
  int pitch;
};

static StickCenters centers = {
  ANALOG_MIN + ((ANALOG_MAX - ANALOG_MIN) * 0.5f),
  ANALOG_MIN + ((ANALOG_MAX - ANALOG_MIN) * 0.5f),
  ANALOG_MIN + ((ANALOG_MAX - ANALOG_MIN) * 0.5f),
  ANALOG_MIN + ((ANALOG_MAX - ANALOG_MIN) * 0.5f)
};

static StickTrims trims = {
  0,   // yaw trim (µs)
  0,   // roll trim (µs)
  0    // pitch trim (µs)
};

static float measureStickCenter(int pin, size_t samples = 32) {
  uint32_t total = 0;
  for (size_t i = 0; i < samples; ++i) {
    total += analogRead(pin);
    delay(2);
  }
  float center = static_cast<float>(total) / static_cast<float>(samples);
  return constrain(center, static_cast<float>(ANALOG_MIN), static_cast<float>(ANALOG_MAX));
}

static float readFiltered(int pin, EmaFilter &filter) {
  int raw = analogRead(pin);
  raw = constrain(raw, ANALOG_MIN, ANALOG_MAX);
  return filter.update(static_cast<float>(raw));
}

static int mapCenteredAxis(float rawSample, float center, bool invert = false, int trimUs = 0) {
  float offset = rawSample - center;
  float span = (offset >= 0.0f)
                 ? (static_cast<float>(ANALOG_MAX) - center)
                 : (center - static_cast<float>(ANALOG_MIN));
  if (span < 1.0f) span = 1.0f;
  float centered = constrain(offset / span, -1.0f, 1.0f);
  if (invert) centered = -centered;

  if (fabsf(centered) < DEADZONE_FRACTION) {
    centered = 0.0f;
  } else {
    float sign = (centered < 0.0f) ? -1.0f : 1.0f;
    float magnitude = (fabsf(centered) - DEADZONE_FRACTION) / (1.0f - DEADZONE_FRACTION);
    centered = sign * constrain(magnitude, 0.0f, 1.0f);
  }

  int rcValue = RC_CENTER + static_cast<int>(centered * RC_HALF_SPAN) + trimUs;
  return constrain(rcValue, RC_MIN, RC_MAX);
}

/* static int mapCenterZeroThrottleAxis(float rawSample, float center, bool invert = false) {
  float offset = rawSample - center;
  float normalized = 0.0f;

  if (invert) {
    if (offset >= 0.0f) {
      normalized = 0.0f; 
    } else {
      float span = center - static_cast<float>(ANALOG_MIN);
      if (span < 1.0f) span = 1.0f;
      normalized = fabsf(offset) / span; 
    }
  } else {
    if (offset <= 0.0f) {
      normalized = 0.0f; 
    } else {
      float span = static_cast<float>(ANALOG_MAX) - center;
      if (span < 1.0f) span = 1.0f;
      normalized = offset / span;
    }
  }

  if (normalized < DEADZONE_FRACTION) {
    normalized = 0.0f;
  } else {
    normalized = (normalized - DEADZONE_FRACTION) / (1.0f - DEADZONE_FRACTION);
  }

  normalized = constrain(normalized, 0.0f, 1.0f);
  int rcValue = RC_MIN + static_cast<int>(normalized * (RC_MAX - RC_MIN));
  return constrain(rcValue, RC_MIN, RC_MAX);
}

*/

static int mapFullRangeThrottle(float rawSample, bool invert = false) {
  float span = static_cast<float>(ANALOG_MAX - ANALOG_MIN);
  float normalized = (rawSample - ANALOG_MIN) / span;
  normalized = constrain(normalized, 0.0f, 1.0f);
  if (invert) normalized = 1.0f - normalized;
  int rcValue = RC_MIN + static_cast<int>(normalized * (RC_MAX - RC_MIN));
  return constrain(rcValue, RC_MIN, RC_MAX);
}

static int readToggleButton(int pin, int &channelValue, bool &lastPressed, uint32_t &lastToggleMs) {
  bool pressed = (digitalRead(pin) == LOW);
  uint32_t now = millis();

  if (pressed && !lastPressed && (now - lastToggleMs) > static_cast<uint32_t>(SWITCH_DEBOUNCE_MS)) {
    channelValue = (channelValue == RC_MIN) ? RC_MAX : RC_MIN;
    lastToggleMs = now;
  }

  lastPressed = pressed;
  return channelValue;
}

bool EspNowRcLink::Transmitter::isConnected() const {
  return _state == TRANSMITTING;
}

void setup() {
  Serial.begin(115200);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  // Add inside setup():
pinMode(AUX2_PIN, INPUT_PULLUP);
pinMode(AUX3_PIN, INPUT_PULLUP);
pinMode(AUX4_PIN, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);


  tx.begin(true);
#if defined(ESP32) || defined(ESP_PLATFORM)
  analogReadResolution(12);
#endif
centers.throttle = measureStickCenter(JOY1_X_PIN);
  centers.yaw = measureStickCenter(JOY1_Y_PIN);
  centers.roll = measureStickCenter(JOY2_X_PIN);
  centers.pitch = measureStickCenter(JOY2_Y_PIN);
}

void loop() {
  static uint32_t lastSendMs = 0;
  static EmaFilter yawFilter;
  static EmaFilter throttleFilter;
  static EmaFilter rollFilter;
  static EmaFilter pitchFilter;
  static int aux1Val = RC_MIN, aux2Val = RC_MIN, aux3Val = RC_MIN, aux4Val = RC_MIN;
static bool aux1Last = false, aux2Last = false, aux3Last = false, aux4Last = false;
static uint32_t aux1Time = 0, aux2Time = 0, aux3Time = 0, aux4Time = 0;

  uint32_t now = millis();

  // Built-in LED ON when RX is connected
  if (tx.isConnected()) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }


  if (now - lastSendMs < SEND_INTERVAL_MS) {
    tx.update();
    return;
  }
  lastSendMs = now;

  float yawSample = readFiltered(JOY1_Y_PIN, yawFilter);
  float throttleSample = readFiltered(JOY1_X_PIN, throttleFilter);
  float rollSample = readFiltered(JOY2_X_PIN, rollFilter);
  float pitchSample = readFiltered(JOY2_Y_PIN, pitchFilter);

  int channels[8];
  channels[0] = mapCenteredAxis(pitchSample, centers.pitch, true, trims.pitch);
  channels[1] = mapCenteredAxis(rollSample, centers.roll, true, trims.roll);
  channels[2] = mapFullRangeThrottle(throttleSample, true);
  channels[3] = mapCenteredAxis(yawSample, centers.yaw, true, trims.yaw);
    // AUX Channels 5 through 8
  channels[4] = readToggleButton(SWITCH_PIN, aux1Val, aux1Last, aux1Time);
  channels[5] = readToggleButton(AUX2_PIN, aux2Val, aux2Last, aux2Time);
  channels[6] = readToggleButton(AUX3_PIN, aux3Val, aux3Last, aux3Time);
  channels[7] = readToggleButton(AUX4_PIN, aux4Val, aux4Last, aux4Time);

  for (uint8_t i = 0; i < 8; ++i) {
    tx.setChannel(i, channels[i]);
  }
  tx.commit();

  Serial.print("RC:");
  for (uint8_t i = 0; i < 8; ++i) {
    Serial.print(' ');
    Serial.print(channels[i]);
  }
  Serial.println();

  tx.update();
}

