// M5Stack AtomS3R - servo sweep toggled by the front button (BtnA)
//
// Wiring: connect the servo to the Grove (Port.A) connector.
//   Grove black (GND) -> servo GND (brown on the tested kit)
//   Grove red (5V)    -> servo power (orange on the tested kit)
//   Grove yellow (G2) -> servo signal (yellow on the tested kit)
// Servo wire colors vary by product; confirm the actual pin functions.
// If your servo cable uses the other signal wire, change SERVO_PIN to 1 (G1).
//
// Display:
//   - SWEEP OFF             -> default face
//   - SWEEP ON               -> random idle animation (see ANIM_PATTERNS below)
//   - Landscape held 2 sec   -> return to rest and pause an active sweep
//   - Shake                  -> guruguru face briefly

#include <M5Unified.h>
#include <ESP32Servo.h>
#include <math.h>
#include "img_default.h"
#include "img_left.h"
#include "img_right.h"
#include "img_sleep.h"
#include "img_guruguru.h"

static const int SERVO_PIN = 2;      // Grove G2
static const int SWEEP_MIN_ANGLE = 20;
static const int SWEEP_MAX_ANGLE = 65;
static const int SWEEP_STEP_DEG = 2; // degrees moved per update
static const unsigned long SWEEP_INTERVAL_MS = 25;
static const int REST_ANGLE = SWEEP_MAX_ANGLE; // position when OFF

static const float ORIENTATION_MARGIN_G = 0.15f;  // hysteresis between portrait/landscape
static const unsigned long LANDSCAPE_SLEEP_HOLD_MS = 2000; // landscape must hold this long (while sweeping) to pause+sleep
static const float SHAKE_THRESHOLD_G = 1.4f;      // deviation from 1g that counts as a shake
static const unsigned long SHAKE_DISPLAY_MS = 800;

static const unsigned long ARROW_DISPLAY_MS = 800;
static const int DOUBLE_CLICK_ANGLE = 20;         // every double-click commands this same position

// Idle animation shown while sweeping, picked randomly (weighted) each time a pattern finishes.
struct AnimStep {
    const uint16_t *image;
    unsigned long durationMs;
};

static const AnimStep ANIM_PATTERN_DEFAULT[] = { { img_default, 2000 } };
static const AnimStep ANIM_PATTERN_LOOK_AROUND[] = { { img_right, 1000 }, { img_default, 1000 }, { img_left, 1000 } };
static const AnimStep ANIM_PATTERN_BLINK[] = { { img_sleep, 500 } };

static const AnimStep *ANIM_PATTERNS[] = { ANIM_PATTERN_DEFAULT, ANIM_PATTERN_LOOK_AROUND, ANIM_PATTERN_BLINK };
static const int ANIM_PATTERN_LENGTHS[] = { 1, 3, 1 };
static const int ANIM_PATTERN_WEIGHTS[] = { 70, 10, 20 }; // must sum to 100
static const int ANIM_PATTERN_COUNT = 3;

Servo servo;
bool sweeping = false;
int angle = REST_ANGLE;
int direction = -1; // first move away from REST_ANGLE (which sits at the max end)
unsigned long lastStepMs = 0;

bool isLandscape = false;
bool prevIsLandscape = false;
unsigned long landscapeStartMs = 0;
bool sweepPaused = false;
bool returningToRest = false; // easing angle back to REST_ANGLE instead of snapping
unsigned long lastShakeMs = 0;
unsigned long lastDoubleClickMs = 0;
bool shakeActive = false;
bool arrowActive = false;

static const uint16_t *IMAGE_ARROW = reinterpret_cast<const uint16_t *>(1); // sentinel: procedurally drawn, not pushImage'd
const uint16_t *currentImage = nullptr;

int animPatternIndex = -1; // -1 = needs to pick a fresh pattern
int animStepIndex = 0;
unsigned long animStepStartMs = 0;

int pickAnimPattern() {
    int r = random(100); // 0..99
    int acc = 0;
    for (int i = 0; i < ANIM_PATTERN_COUNT; i++) {
        acc += ANIM_PATTERN_WEIGHTS[i];
        if (r < acc) return i;
    }
    return ANIM_PATTERN_COUNT - 1;
}

const uint16_t *getIdleAnimationImage() {
    unsigned long now = millis();
    if (animPatternIndex < 0 ||
        now - animStepStartMs >= ANIM_PATTERNS[animPatternIndex][animStepIndex].durationMs) {
        if (animPatternIndex < 0) {
            animPatternIndex = pickAnimPattern();
            animStepIndex = 0;
        } else {
            animStepIndex++;
            if (animStepIndex >= ANIM_PATTERN_LENGTHS[animPatternIndex]) {
                animPatternIndex = pickAnimPattern();
                animStepIndex = 0;
            }
        }
        animStepStartMs = now;
    }
    return ANIM_PATTERNS[animPatternIndex][animStepIndex].image;
}

void drawArrowUp() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.fillTriangle(64, 20, 30, 68, 98, 68, TFT_WHITE);
    M5.Display.fillRect(50, 68, 28, 40, TFT_WHITE);
}

void showImage(const uint16_t *img) {
    if (img == currentImage) return;
    if (img == IMAGE_ARROW) {
        drawArrowUp();
    } else {
        M5.Display.pushImage(0, 0, 128, 128, img);
    }
    currentImage = img;
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    randomSeed(esp_random());

    servo.setPeriodHertz(50);
    servo.attach(SERVO_PIN, 500, 2400);
    servo.write(angle);

    showImage(img_default);
}

void loop() {
    M5.update();

    if (M5.BtnA.wasSingleClicked()) {
        sweeping = !sweeping;
        if (sweeping) {
            returningToRest = false; // resume actively right away
            sweepPaused = false;
            animPatternIndex = -1; // start fresh
        } else {
            returningToRest = true; // ease down to REST_ANGLE instead of snapping
        }
    }
    if (M5.BtnA.wasDoubleClicked()) {
        lastDoubleClickMs = millis();
        arrowActive = true;
        // Keep the next sweep/return step relative to the last commanded position.
        angle = DOUBLE_CLICK_ANGLE;
        direction = (angle == REST_ANGLE) ? -1 : 1;
        lastStepMs = lastDoubleClickMs;
        servo.write(angle);
    }

    M5.Imu.update();
    auto imu = M5.Imu.getImuData();
    float ax = imu.accel.x;
    float ay = imu.accel.y;
    float az = imu.accel.z;

    if (fabsf(ax) > fabsf(ay) + ORIENTATION_MARGIN_G) {
        isLandscape = true;
    } else if (fabsf(ay) > fabsf(ax) + ORIENTATION_MARGIN_G) {
        isLandscape = false;
    }

    if (isLandscape && !prevIsLandscape) {
        landscapeStartMs = millis();
    }
    if (!isLandscape && sweepPaused) {
        sweepPaused = false; // wake up as soon as it's back to portrait
        returningToRest = false; // resume actively right away
    }
    prevIsLandscape = isLandscape;

    if (sweeping && !sweepPaused && isLandscape && (millis() - landscapeStartMs >= LANDSCAPE_SLEEP_HOLD_MS)) {
        sweepPaused = true;
        returningToRest = true; // ease down to REST_ANGLE before holding still
    }

    float magnitude = sqrtf(ax * ax + ay * ay + az * az);
    if (fabsf(magnitude - 1.0f) > SHAKE_THRESHOLD_G) {
        lastShakeMs = millis();
        shakeActive = true;
    }
    if (shakeActive && millis() - lastShakeMs >= SHAKE_DISPLAY_MS) {
        shakeActive = false;
    }
    if (arrowActive && millis() - lastDoubleClickMs >= ARROW_DISPLAY_MS) {
        arrowActive = false;
    }

    if (arrowActive) {
        showImage(IMAGE_ARROW);
    } else if (shakeActive) {
        showImage(img_guruguru);
    } else if (sweeping) {
        showImage(sweepPaused ? img_sleep : getIdleAnimationImage());
    } else {
        // not sweeping: screen just tracks orientation directly, no hold delay
        showImage(isLandscape ? img_sleep : img_default);
    }

    if (returningToRest) {
        unsigned long now = millis();
        if (now - lastStepMs >= SWEEP_INTERVAL_MS) {
            lastStepMs = now;
            if (angle > REST_ANGLE) {
                angle = max(REST_ANGLE, angle - SWEEP_STEP_DEG);
            } else if (angle < REST_ANGLE) {
                angle = min(REST_ANGLE, angle + SWEEP_STEP_DEG);
            }
            servo.write(angle);
            if (angle == REST_ANGLE) {
                returningToRest = false;
                direction = -1; // first move away from REST_ANGLE (which sits at the max end)
            }
        }
    } else if (sweeping && !sweepPaused) {
        unsigned long now = millis();
        if (now - lastStepMs >= SWEEP_INTERVAL_MS) {
            lastStepMs = now;
            angle += direction * SWEEP_STEP_DEG;
            if (angle >= SWEEP_MAX_ANGLE) {
                angle = SWEEP_MAX_ANGLE;
                direction = -1;
            } else if (angle <= SWEEP_MIN_ANGLE) {
                angle = SWEEP_MIN_ANGLE;
                direction = 1;
            }
            servo.write(angle);
        }
    }
}
