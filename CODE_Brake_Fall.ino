/*
 * IMU brake & fall detector with TacHammer haptic
 * ---------------------------------------------------------------
 *  Target board: ATmega32u4 (Arduino Leonardo / Micro / Pro Micro)
 *                -- required for Timer4 hardware PWM on pin 13 (PC7)
 *
 *  Hardware:
 *    - MPU6050        (I2C: SDA/SCL)
 *    - LED: pin 5 - RED
 *
 *  Behaviour:
 *    - Deceleration detected  -> red light gets brighter
 *    - Fall / crash detected  -> red light starts flashing
 * ---------------------------------------------------------------
 */

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------- Pin map ----------
// Single red LED only (brake light) on pin 5. Default powered-on
// brightness is PWM ~30%; full brightness on brake; blanked on crash
const uint8_t PIN_RED = 5; // single red LED (brake light)
// PWM output to TacHammer is hardwired to pin 13 (PC7) via Timer4.
const uint8_t LED_DEFAULT_PWM =40; // ~30% of 255
const uint8_t LED_FULL_PWM    = 255;

// ---------- Tunable thresholds ----------
// Brake detection: how much the forward acceleration must drop below the
// long-term baseline (in m/s^2) to count as braking. Higher = needs harder brake.
// TUNING: open Serial Monitor @ 115200, watch the "fwd=... base=... brake=..."
// lines while braking vs. accelerating, then set this to a value comfortably
// above the largest "brake=" value you see while just cruising / accelerating.
const float DECEL_THRESHOLD       = 0.6f;
// Require this many consecutive samples above the threshold (debounce).
const uint8_t DECEL_CONSEC_SAMPLES = 3;
// Forward axis selection (0=X, 1=Y, 2=Z) and direction sign.
// If braking instead triggers on acceleration, flip FORWARD_SIGN to -1.
const uint8_t FORWARD_AXIS = 1;     // X by default
const float   FORWARD_SIGN = +1.0f;

// Print live signal every PRINT_PERIOD_MS so you can tune the threshold.
// Set to 0 to disable.
const unsigned long PRINT_PERIOD_MS = 200;

const float CRASH_ACC_THRESHOLD   = 30.0f;  // m/s^2 (~1.8 g) -- lower = easier to trigger
const float CRASH_GYRO_THRESHOLD  = 6.0f;   // rad/s
const unsigned long BRAKE_HOLD_MS   = 600;
const unsigned long CRASH_ALERT_MS  = 1500;
const unsigned long CRASH_BLINK_MS  = 200;

// Baseline tracking: very slow low-pass (seconds) representing gravity offset +
// average forward acceleration. The brake signal is (baseline - current).
const float BASELINE_ALPHA = 0.01f;  // ~1% new per sample @ 50 Hz -> ~2 s time constant

// ---------- Globals ----------
Adafruit_MPU6050 mpu;

float forwardBaseline = 0.0f;
bool  haveBaseline    = false;
uint8_t decelStreak   = 0;

unsigned long brakeUntil = 0;
unsigned long crashUntil = 0;
bool crashLatched = false;

// ================================================================================
// TacHammer haptic removed — no haptic/vibration on crash.

// ================================================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED, OUTPUT);
  // start LED at default (powered-on dim) PWM
  analogWrite(PIN_RED, LED_DEFAULT_PWM);

  Wire.begin();
  delay(2);
  // No haptic/TacHammer present in this build.
  Serial.println(F("No haptic (TacHammer) present"));

  // ---- MPU6050 init ----
  if (!mpu.begin()) {
    Serial.println(F("MPU6050 not found!"));
    while (1) {
      digitalWrite(PIN_RED, !digitalRead(PIN_RED));
      delay(200);
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println(F("Ready. Commands: h=haptic c=crash b=brake"));
}

// ================================================================================
void loop() {
  // ---- Serial test commands (type single chars in Serial Monitor) ----
  //   h = test haptic only
  //   c = simulate crash (haptic + LED blink)
  //   b = simulate brake (LEDs on)
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'h' || ch == 'H') {
      Serial.println(F("[TEST] no haptic available"));
    } else if (ch == 'c' || ch == 'C') {
      Serial.println(F("[TEST] crash"));
      brakeUntil = 0;
      decelStreak = 0;
      // latch crash state: blank LED until reset
      crashUntil = millis() + CRASH_ALERT_MS;
      crashLatched = true;
      analogWrite(PIN_RED, 0);
      // crash latched; LED will flash until reset
    } else if (ch == 'b' || ch == 'B') {
      Serial.println(F("[TEST] brake"));
      brakeUntil = millis() + BRAKE_HOLD_MS;
    }
  }

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  // ---- Pick the forward axis ----
  float axes[3] = { a.acceleration.x, a.acceleration.y, a.acceleration.z };
  float forward = FORWARD_SIGN * axes[FORWARD_AXIS];

  if (!haveBaseline) {
    forwardBaseline = forward;
    haveBaseline = true;
  }

  float aMag = sqrt(a.acceleration.x * a.acceleration.x +
                    a.acceleration.y * a.acceleration.y +
                    a.acceleration.z * a.acceleration.z);
  float gMag = sqrt(g.gyro.x * g.gyro.x +
                    g.gyro.y * g.gyro.y +
                    g.gyro.z * g.gyro.z);

  unsigned long now = millis();

  // ---- Crash / fall (highest priority) ----
  if (aMag > CRASH_ACC_THRESHOLD || gMag > CRASH_GYRO_THRESHOLD) {
    if (now > crashUntil) {
      Serial.print(F("CRASH! aMag="));
      Serial.print(aMag);
      Serial.print(F(" gMag="));
      Serial.println(gMag);
      crashUntil = now + CRASH_ALERT_MS;
      // Cancel any brake activity so the two effects don't overlap.
      brakeUntil  = 0;
      decelStreak = 0;
      // latch crash: flash the single red LED until reset.
      crashLatched = true;
      crashUntil = now + CRASH_ALERT_MS; // keep crashActive true for a short window
    }
  }

  bool crashActive = (millis() < crashUntil);  // re-read clock; haptic was blocking

  // ---- Brake detection ----
  // Signed: positive only when forward axis drops BELOW its long-term baseline,
  // i.e. when the rider is decelerating. Acceleration produces a negative value
  // and does not trigger.
  float brakeSignal = forwardBaseline - forward;

  if (!crashActive && brakeSignal > DECEL_THRESHOLD) {
    if (decelStreak < 255) decelStreak++;
    if (decelStreak >= DECEL_CONSEC_SAMPLES) {
      if (now >= brakeUntil) {   // log only on rising edge
        Serial.print(F("BRAKE  signal="));
        Serial.println(brakeSignal);
      }
      brakeUntil = now + BRAKE_HOLD_MS;
    }
  } else {
    decelStreak = 0;
  }

  // Update baseline ONLY during truly quiet periods: |signal| below a fraction
  // of the trigger threshold. This way neither braking nor accelerating events
  // pull the baseline, so the system stays symmetric and can't get "tricked"
  // into firing on acceleration.
  const float BASELINE_QUIET_BAND = 0.5f * DECEL_THRESHOLD;
  if (!crashActive && fabs(brakeSignal) < BASELINE_QUIET_BAND) {
    forwardBaseline += BASELINE_ALPHA * (forward - forwardBaseline);
  }

  bool brakeOn = !crashLatched && !crashActive && (millis() < brakeUntil);
  // LED behaviour:
  // - crashLatched: LED flashes until reset
  // - brakeOn: full brightness
  // - otherwise: default dim (powered-on PWM)
  if (crashLatched) {
    bool blinkState = ((now / CRASH_BLINK_MS) % 2) == 0;
    analogWrite(PIN_RED, blinkState ? LED_FULL_PWM : 0);
  } else if (brakeOn) {
    analogWrite(PIN_RED, LED_FULL_PWM);
  } else {
    analogWrite(PIN_RED, LED_DEFAULT_PWM);
  }

  // ---- Live debug stream (for tuning DECEL_THRESHOLD / sign) ----
  static unsigned long lastPrint = 0;
  if (PRINT_PERIOD_MS && (now - lastPrint) >= PRINT_PERIOD_MS) {
    lastPrint = now;
    Serial.print(F("fwd="));   Serial.print(forward, 2);
    Serial.print(F(" base=")); Serial.print(forwardBaseline, 2);
    Serial.print(F(" brake=")); Serial.print(brakeSignal, 2);
    Serial.print(F(" aMag=")); Serial.print(aMag, 1);
    Serial.print(F(" gMag=")); Serial.print(gMag, 1);
    if (brakeOn)     Serial.print(F(" [BRAKE]"));
    if (crashActive) Serial.print(F(" [CRASH]"));
    Serial.println();
  }

  // No separate crash LEDs with single-LED setup. Crash latch keeps LED off

  delay(20);   // ~50 Hz loop
}
