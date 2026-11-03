
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// =====================================================
// 1. WIFI
// =====================================================

const char* WIFI_SSID = "MediRoute_Robot";
const char* WIFI_PASSWORD = "mediroute123";

WebServer server(80);

// =====================================================
// 2. MOTOR DRIVER - TB6612FNG
// CHANNEL A = RIGHT MOTOR
// CHANNEL B = LEFT MOTOR
// =====================================================

#define PWMA 25
#define AIN1 26
#define AIN2 27

#define PWMB 14
#define BIN1 13
#define BIN2 12

#define STBY 33

const int LEFT_SPEED = 190;
const int RIGHT_SPEED = 160;
const int AVOID_SPEED = 125;

const bool LEFT_MOTOR_REVERSED = false;
const bool RIGHT_MOTOR_REVERSED = false;

// =====================================================
// 3. ULTRASONIC SENSORS
// =====================================================

#define LEFT_TRIG 32
#define LEFT_ECHO 34

#define RIGHT_TRIG 21
#define RIGHT_ECHO 35

const int OBSTACLE_DISTANCE = 30;
const int CLEAR_DISTANCE = 40;

const unsigned long SENSOR_INTERVAL = 200;
const unsigned long ECHO_TIMEOUT = 18000;
const unsigned long SENSOR_STALE_TIME = 2000; // tolerate angled-sensor missed echoes

const int FILTER_SIZE = 5;
const int SAFETY_WINDOW = 3;
const int MAX_MISSED_ECHOES = 5; // diagnostic threshold; misses alone do not invalidate a fresh reading

// Left sensor
int leftSamples[FILTER_SIZE];
int leftSafetySamples[SAFETY_WINDOW];

int leftSampleCount = 0;
int leftSampleIndex = 0;
int leftSafetyCount = 0;
int leftSafetyIndex = 0;

int leftSensorReading = -1;
int leftSafetyDistance = -1;
int leftMissCount = 0;

unsigned long leftLastValidTime = 0;

// Right sensor
int rightSamples[FILTER_SIZE];
int rightSafetySamples[SAFETY_WINDOW];

int rightSampleCount = 0;
int rightSampleIndex = 0;
int rightSafetyCount = 0;
int rightSafetyIndex = 0;

int rightSensorReading = -1;
int rightSafetyDistance = -1;
int rightMissCount = 0;

unsigned long rightLastValidTime = 0;

unsigned long lastSensorSample = 0;

bool obstacleDetected = false;

String leftSensorHealth = "WAITING";
String rightSensorHealth = "WAITING";

// =====================================================
// 4. RFID RC522
// =====================================================

#define RFID_SS 5
#define RFID_RST 22

MFRC522 rfid(RFID_SS, RFID_RST);

byte authorisedCard1[] = {
  0x62, 0x47, 0xEE, 0x51
};

byte authorisedCard2[] = {
  0xB8, 0x66, 0x8F, 0x12
};

String rfidStatus = "Waiting for card";

// =====================================================
// 5. SG90 SERVO
// =====================================================

#define SERVO_PIN 4

Servo medicineServo;

const int LOCK_ANGLE = 0;
const int UNLOCK_ANGLE = 90;

const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2400;

const unsigned long UNLOCK_DURATION = 5000;

bool servoReady = false;
bool boxUnlocked = false;

unsigned long unlockStartTime = 0;

String boxStatus = "LOCKED";
String servoStatus = "NOT INITIALISED";

// =====================================================
// 6. DELIVERY TIMER
// TOTAL TIME = 10 SECONDS
// Includes movement, steering, avoidance and waiting.
// =====================================================

const unsigned long TOTAL_DELIVERY_TIME = 10000;

unsigned long deliveryStartTime = 0;
unsigned long deliveryElapsedMs = 0;

bool robotRunning = false;
bool deliveryComplete = false;
bool emergencyStop = false;

String robotStatus = "Ready";

// =====================================================
// 7. MOTOR CONTROL
// =====================================================

void driveRightMotor(int speedValue) {

  if (RIGHT_MOTOR_REVERSED) {
    speedValue = -speedValue;
  }

  speedValue = constrain(speedValue, -255, 255);

  if (speedValue > 0) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  }
  else if (speedValue < 0) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  }
  else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
  }

  ledcWrite(PWMA, abs(speedValue));
}

void driveLeftMotor(int speedValue) {

  if (LEFT_MOTOR_REVERSED) {
    speedValue = -speedValue;
  }

  speedValue = constrain(speedValue, -255, 255);

  if (speedValue > 0) {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
  }
  else if (speedValue < 0) {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
  }
  else {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
  }

  ledcWrite(PWMB, abs(speedValue));
}

void setMotor(int leftSpeed, int rightSpeed) {

  if (emergencyStop) return;

  driveRightMotor(rightSpeed);
  driveLeftMotor(leftSpeed);
}

void stopMotors() {

  ledcWrite(PWMA, 0);
  ledcWrite(PWMB, 0);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
}

void moveForward() {

  setMotor(LEFT_SPEED, RIGHT_SPEED);
}

// =====================================================
// 8. DELIVERY TIMER
// =====================================================

void updateDeliveryTime() {

  if (robotRunning && !emergencyStop) {

    deliveryElapsedMs = millis() - deliveryStartTime;

    if (deliveryElapsedMs > TOTAL_DELIVERY_TIME) {
      deliveryElapsedMs = TOTAL_DELIVERY_TIME;
    }
  }
}

bool deliveryTimeExpired() {

  updateDeliveryTime();

  return deliveryElapsedMs >= TOTAL_DELIVERY_TIME;
}

// =====================================================
// 9. MEDIAN FILTER
// =====================================================

int calculateMedian(int samples[], int count) {

  if (count <= 0) return -1;

  int sorted[FILTER_SIZE];

  for (int i = 0; i < count; i++) {
    sorted[i] = samples[i];
  }

  for (int i = 1; i < count; i++) {

    int key = sorted[i];
    int j = i - 1;

    while (j >= 0 && sorted[j] > key) {

      sorted[j + 1] = sorted[j];
      j--;
    }

    sorted[j + 1] = key;
  }

  return sorted[count / 2];
}

// =====================================================
// 10. CONSERVATIVE SAFETY DISTANCE
// Uses minimum of latest 3 valid readings.
// =====================================================

int calculateSafetyDistance(int samples[], int count) {

  if (count <= 0) return -1;

  int minimum = samples[0];

  for (int i = 1; i < count; i++) {

    if (samples[i] < minimum) {
      minimum = samples[i];
    }
  }

  return minimum;
}

// =====================================================
// 11. RAW ULTRASONIC READING
// =====================================================

int readDistance(int trigPin, int echoPin) {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  unsigned long duration =
      pulseIn(echoPin, HIGH, ECHO_TIMEOUT);

  if (duration == 0) {
    return -1;
  }

  int distance =
      (int)(duration * 0.0343 / 2.0);

  if (distance < 2 || distance > 300) {
    return -1;
  }

  return distance;
}

// =====================================================
// 12. LEFT SENSOR FILTER
// =====================================================

void addLeftSample(int distance) {

  if (distance < 0) {

    leftMissCount++;

    // Angled HC-SR04 sensors can miss echoes when the surface reflects
    // sound away. Keep the last valid distance until it becomes stale.
    leftSensorHealth = (leftSampleCount == 0) ? "WAITING" : "ECHO MISSED";
    return;
  }

  leftMissCount = 0;
  leftLastValidTime = millis();

  leftSamples[leftSampleIndex] = distance;

  leftSampleIndex =
      (leftSampleIndex + 1) % FILTER_SIZE;

  if (leftSampleCount < FILTER_SIZE) {
    leftSampleCount++;
  }

  leftSafetySamples[leftSafetyIndex] = distance;

  leftSafetyIndex =
      (leftSafetyIndex + 1) % SAFETY_WINDOW;

  if (leftSafetyCount < SAFETY_WINDOW) {
    leftSafetyCount++;
  }

  leftSensorReading =
      calculateMedian(leftSamples, leftSampleCount);

  leftSafetyDistance =
      calculateSafetyDistance(
        leftSafetySamples,
        leftSafetyCount
      );

  leftSensorHealth = "OK";
}

// =====================================================
// 13. RIGHT SENSOR FILTER
// =====================================================

void addRightSample(int distance) {

  if (distance < 0) {

    rightMissCount++;

    // A missed echo is not automatically a hardware fault.
    rightSensorHealth = (rightSampleCount == 0) ? "WAITING" : "ECHO MISSED";
    return;
  }

  rightMissCount = 0;
  rightLastValidTime = millis();

  rightSamples[rightSampleIndex] = distance;

  rightSampleIndex =
      (rightSampleIndex + 1) % FILTER_SIZE;

  if (rightSampleCount < FILTER_SIZE) {
    rightSampleCount++;
  }

  rightSafetySamples[rightSafetyIndex] = distance;

  rightSafetyIndex =
      (rightSafetyIndex + 1) % SAFETY_WINDOW;

  if (rightSafetyCount < SAFETY_WINDOW) {
    rightSafetyCount++;
  }

  rightSensorReading =
      calculateMedian(rightSamples, rightSampleCount);

  rightSafetyDistance =
      calculateSafetyDistance(
        rightSafetySamples,
        rightSafetyCount
      );

  rightSensorHealth = "OK";
}

// =====================================================
// 14. SENSOR HEALTH
// =====================================================

bool leftSensorHealthy() {

  if (leftSampleCount == 0) return false;

  // Do not call an angled sensor faulty just because it missed
  // several echoes. It is usable while its last valid sample is fresh.
  if (millis() - leftLastValidTime > SENSOR_STALE_TIME) return false;

  return true;
}

bool rightSensorHealthy() {

  if (rightSampleCount == 0) return false;

  if (millis() - rightLastValidTime > SENSOR_STALE_TIME) return false;

  return true;
}

bool sensorsHealthy() {

  return leftSensorHealthy() &&
         rightSensorHealthy();
}

// =====================================================
// 15. OBSTACLE STATUS
// =====================================================

void updateObstacleFlag() {

  if (!leftSensorHealthy() ||
      !rightSensorHealthy()) {

    obstacleDetected = false;
    return;
  }

  obstacleDetected =
      leftSafetyDistance < OBSTACLE_DISTANCE ||
      rightSafetyDistance < OBSTACLE_DISTANCE;
}

// =====================================================
// 16. LIVE SENSOR UPDATING
// Runs while idle AND during delivery.
// =====================================================

void sampleSensorsNow() {

  // Read sensors sequentially to reduce ultrasonic crosstalk.
  int leftRaw = readDistance(LEFT_TRIG, LEFT_ECHO);
  addLeftSample(leftRaw);

  delay(35);

  int rightRaw = readDistance(RIGHT_TRIG, RIGHT_ECHO);
  addRightSample(rightRaw);

  // Update health from the age of the last VALID echo.
  if (leftSampleCount == 0) {
    leftSensorHealth = (leftMissCount >= MAX_MISSED_ECHOES) ?
                       "NO ECHO" : "WAITING";
  } else if (!leftSensorHealthy()) {
    leftSensorHealth = "INVALID";
  } else if (leftMissCount > 0) {
    leftSensorHealth = "ECHO MISSED";
  } else {
    leftSensorHealth = "OK";
  }

  if (rightSampleCount == 0) {
    rightSensorHealth = (rightMissCount >= MAX_MISSED_ECHOES) ?
                        "NO ECHO" : "WAITING";
  } else if (!rightSensorHealthy()) {
    rightSensorHealth = "INVALID";
  } else if (rightMissCount > 0) {
    rightSensorHealth = "ECHO MISSED";
  } else {
    rightSensorHealth = "OK";
  }

  updateObstacleFlag();

  Serial.print("LEFT: ");
  if (leftSensorReading < 0) {
    Serial.print("--");
  } else {
    Serial.print(leftSensorReading);
    Serial.print(" cm");
  }

  Serial.print(" [");
  Serial.print(leftSensorHealth);
  Serial.print("] | RIGHT: ");

  if (rightSensorReading < 0) {
    Serial.print("--");
  } else {
    Serial.print(rightSensorReading);
    Serial.print(" cm");
  }

  Serial.print(" [");
  Serial.print(rightSensorHealth);
  Serial.println("]");
}

void updateSensorsIfDue() {

  unsigned long now = millis();

  if (now - lastSensorSample < SENSOR_INTERVAL) {
    return;
  }

  lastSensorSample = now;
  sampleSensorsNow();
}

// =====================================================
// 17. SERVO INITIALISATION
// =====================================================

void initialiseServo() {

  Serial.println();
  Serial.println("INITIALISING SG90 SERVO");

  medicineServo.setPeriodHertz(50);

  int servoChannel =
      medicineServo.attach(
        SERVO_PIN,
        SERVO_MIN_US,
        SERVO_MAX_US
      );

  if (servoChannel < 0 ||
      !medicineServo.attached()) {

    servoReady = false;

    servoStatus = "ATTACH FAILED";
    boxStatus = "SERVO ERROR";

    Serial.println("ERROR: SERVO ATTACH FAILED");

    return;
  }

  servoReady = true;

  Serial.print("Servo attached on GPIO ");
  Serial.println(SERVO_PIN);

  Serial.print("Servo channel: ");
  Serial.println(servoChannel);

  // Explicit initial lock command.
  medicineServo.write(LOCK_ANGLE);

  delay(500);

  servoStatus = "READY";
  boxStatus = "LOCKED";

  Serial.println("Servo initialised and lock command sent");
}

// =====================================================
// 18. SERVO LOCK
// =====================================================

void lockBox() {

  if (!servoReady) {

    boxStatus = "SERVO ERROR";
    servoStatus = "NOT READY";

    Serial.println("Cannot lock: servo not attached");

    return;
  }

  medicineServo.write(LOCK_ANGLE);

  boxUnlocked = false;

  boxStatus = "LOCK COMMAND SENT";
  servoStatus = "LOCK COMMAND SENT";

  Serial.println("SERVO COMMAND: LOCK - 0 DEGREES");
}

// =====================================================
// 19. SERVO UNLOCK
// =====================================================

void unlockBox() {

  if (!servoReady || !medicineServo.attached()) {

    servoReady = false;

    boxStatus = "SERVO ERROR";
    servoStatus = "ATTACH FAILED";

    Serial.println("ERROR: SERVO IS NOT ATTACHED");

    return;
  }

  // Explicit pulse-width command.
  medicineServo.writeMicroseconds(1700);

  boxUnlocked = true;

  unlockStartTime = millis();

  boxStatus = "UNLOCK COMMAND SENT";
  servoStatus = "UNLOCK COMMAND SENT";

  Serial.println();
  Serial.println("==============================");
  Serial.println("SERVO UNLOCK COMMAND SENT");
  Serial.println("GPIO: 4");
  Serial.println("PULSE: 1700 MICROSECONDS");
  Serial.println("TARGET ANGLE: APPROX 90 DEGREES");
  Serial.println("==============================");
}

// =====================================================
// 20. SERVO AUTOMATIC RELock
// =====================================================

void updateServoLock() {

  if (boxUnlocked &&
      millis() - unlockStartTime >= UNLOCK_DURATION) {

    lockBox();

    Serial.println("5 SECONDS COMPLETED - LOCK COMMAND SENT");
  }
}

// =====================================================
// 21. RFID AUTHENTICATION
// =====================================================

bool compareUID(byte *readUID, byte *allowedUID) {

  for (byte i = 0; i < 4; i++) {

    if (readUID[i] != allowedUID[i]) {
      return false;
    }
  }

  return true;
}

bool isAuthorisedCard() {

  if (rfid.uid.size != 4) {
    return false;
  }

  return
      compareUID(rfid.uid.uidByte, authorisedCard1) ||
      compareUID(rfid.uid.uidByte, authorisedCard2);
}

void handleRFID() {

  if (rfid.PICC_IsNewCardPresent() &&
      rfid.PICC_ReadCardSerial()) {

    Serial.print("RFID UID: ");

    for (byte i = 0; i < rfid.uid.size; i++) {

      if (rfid.uid.uidByte[i] < 0x10) {
        Serial.print("0");
      }

      Serial.print(rfid.uid.uidByte[i], HEX);
      Serial.print(" ");
    }

    Serial.println();

    if (!isAuthorisedCard()) {

      rfidStatus = "ACCESS DENIED";

      Serial.println("UNAUTHORISED CARD");

    }
    else if (emergencyStop) {

      rfidStatus = "EMERGENCY STOP ACTIVE";

    }
    else if (!deliveryComplete) {

      rfidStatus = "WAITING FOR DELIVERY";

      Serial.println("DELIVERY NOT COMPLETED");

    }
    else if (!servoReady) {

      rfidStatus = "SERVO ERROR";

      Serial.println("RFID OK BUT SERVO NOT READY");

    }
    else if (boxUnlocked) {

      rfidStatus = "BOX ALREADY UNLOCKED";

    }
    else {

      rfidStatus = "ACCESS GRANTED";

      Serial.println("AUTHORISED CARD");

      unlockBox();
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  updateServoLock();
}

// =====================================================
// 22. WEB DASHBOARD
// =====================================================

String webpage() {

  return R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>MediRoute Dashboard</title>

<style>

* {
  box-sizing: border-box;
}

body {
  background: #101827;
  color: white;
  font-family: Arial, sans-serif;
  text-align: center;
  padding: 15px;
  margin: 0;
}

.container {
  max-width: 750px;
  margin: auto;
  background: #1d293b;
  padding: 22px;
  border-radius: 20px;
}

h1 {
  color: #38bdf8;
  font-size: 36px;
  margin-bottom: 5px;
}

.subtitle {
  font-size: 18px;
  margin-bottom: 22px;
}

.section {
  background: #334155;
  padding: 18px;
  border-radius: 13px;
  margin-top: 16px;
}

.section h3 {
  margin-top: 0;
  color: #38bdf8;
}

.grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 12px;
}

.sensor {
  background: #1e293b;
  padding: 18px 8px;
  border-radius: 12px;
}

.sensor h3 {
  color: white;
  font-size: 16px;
}

.value {
  font-size: 27px;
  font-weight: bold;
  margin: 10px 0;
  overflow-wrap: anywhere;
}

.label {
  color: #cbd5e1;
  font-size: 13px;
}

.statusValue {
  font-size: 17px;
  font-weight: bold;
  overflow-wrap: anywhere;
}

button {
  width: 100%;
  padding: 17px;
  margin-top: 13px;
  border: none;
  border-radius: 10px;
  font-size: 17px;
  font-weight: bold;
  cursor: pointer;
}

.start {
  background: #22c55e;
  color: white;
}

.stop {
  background: #ef4444;
  color: white;
}

.progressBackground {
  width: 100%;
  height: 14px;
  background: #0f172a;
  border-radius: 20px;
  overflow: hidden;
  margin-top: 12px;
}

.progressBar {
  height: 100%;
  width: 0%;
  background: #22c55e;
  transition: width 0.2s;
}

.good {
  color: #4ade80;
}

.bad {
  color: #f87171;
}

.warning {
  color: #facc15;
}

.neutral {
  color: #cbd5e1;
}

button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

@media(max-width: 420px) {

  h1 {
    font-size: 29px;
  }

  .value {
    font-size: 22px;
  }

  .container {
    padding: 14px;
  }
}

</style>

</head>

<body>

<div class="container">

<h1>MediRoute</h1>

<div class="subtitle">
Medicine Delivery Robot
</div>

<div class="section">

<h3>DELIVERY CONTROL</h3>

<p>Total delivery duration: 10 seconds</p>

<button class="start" id="startButton"
onclick="startRobot()">
START DELIVERY
</button>

<button class="stop" onclick="stopRobot()">
EMERGENCY STOP
</button>

</div>

<div class="section">

<h3>DELIVERY PROGRESS</h3>

<div class="value" id="progress">
0.0 / 10 seconds
</div>

<div class="progressBackground">
<div class="progressBar" id="progressBar"></div>
</div>

</div>

<div class="section">

<h3>ULTRASONIC SENSOR READINGS</h3>

<div class="grid">

<div class="sensor">

<h3>LEFT SENSOR</h3>

<div class="value" id="leftDistance">--</div>

<div class="label">Distance (cm)</div>

<div class="statusValue neutral" id="leftState">
WAITING
</div>

</div>

<div class="sensor">

<h3>RIGHT SENSOR</h3>

<div class="value" id="rightDistance">--</div>

<div class="label">Distance (cm)</div>

<div class="statusValue neutral" id="rightState">
WAITING
</div>

</div>

</div>

<p class="label">
Median-filtered readings. Safety checks use recent close readings.
</p>

</div>

<div class="section">

<h3>OBSTACLE DETECTION</h3>

<div class="statusValue neutral" id="obstacleStatus">
WAITING
</div>

</div>

<div class="section">

<h3>ROBOT STATUS</h3>

<div class="statusValue" id="robotStatus">
Ready
</div>

</div>

<div class="section">

<h3>RFID AUTHENTICATION</h3>

<div class="statusValue" id="rfidStatus">
Waiting for card
</div>

</div>

<div class="section">

<h3>MEDICINE BOX</h3>

<div class="statusValue good" id="boxStatus">
LOCKED
</div>

</div>

<div class="section">

<h3>SERVO STATUS</h3>

<div class="statusValue neutral" id="servoStatus">
Waiting for initialisation
</div>

<p class="label">
Command status only; no physical angle feedback sensor is installed.
</p>

</div>

<div class="section">

<h3>SAFETY STATUS</h3>

<div class="statusValue good" id="emergencyStatus">
NORMAL
</div>

</div>

</div>

<script>

let requestInProgress = false;

function startRobot() {

  fetch('/start')
    .then(r => r.text())
    .then(data => {
      alert(data);
      refreshStatus();
    })
    .catch(() => alert("ESP32 connection error"));
}

function stopRobot() {

  fetch('/stop')
    .then(r => r.text())
    .then(data => {
      alert(data);
      refreshStatus();
    })
    .catch(() => alert("ESP32 connection error"));
}

function setSensor(id, stateId, value, health) {

  const element = document.getElementById(id);
  const state = document.getElementById(stateId);

  if (value === -1) {

    element.innerText = "--";
    state.innerText = health;
    state.className = (health === "INVALID" || health === "NO ECHO") ?
                      "statusValue bad" : "statusValue neutral";

    return;
  }

  element.innerText = value;

  if (health === "INVALID") {

    state.innerText = "SENSOR FAULT";
    state.className = "statusValue bad";

  }
  else if (health === "ECHO MISSED") {

    state.innerText = "MISSED ECHO - LAST VALUE";
    state.className = "statusValue warning";

  }
  else if (value < 30) {

    state.innerText = "OBSTACLE";
    state.className = "statusValue bad";

  }
  else if (value < 40) {

    state.innerText = "CAUTION";
    state.className = "statusValue warning";

  }
  else {

    state.innerText = "CLEAR";
    state.className = "statusValue good";
  }
}

function refreshStatus() {

  if (requestInProgress) return;

  requestInProgress = true;

  fetch('/status')
    .then(r => r.json())
    .then(data => {

      document.getElementById('robotStatus').innerText =
        data.status;

      document.getElementById('progress').innerText =
        (data.progress / 1000).toFixed(1) +
        ' / 10 seconds';

      document.getElementById('progressBar').style.width =
        Math.min(100, data.progress / 100) + '%';

      setSensor(
        'leftDistance',
        'leftState',
        data.left,
        data.leftHealth
      );

      setSensor(
        'rightDistance',
        'rightState',
        data.right,
        data.rightHealth
      );

      let obstacleText;

      if (data.leftHealth === "INVALID" ||
          data.rightHealth === "INVALID") {

        obstacleText = "SENSOR FAULT - CHECK ROBOT";

      }
      else if (data.leftHealth === "WAITING" ||
               data.rightHealth === "WAITING") {

        obstacleText = "WAITING FOR READINGS";

      }
      else if (data.obstacle) {

        obstacleText = "OBSTACLE DETECTED";

      }
      else if (data.leftHealth === "ECHO MISSED" ||
               data.rightHealth === "ECHO MISSED") {

        obstacleText = "MONITORING - USING LAST VALID READING";

      }
      else {

        obstacleText = "NO OBSTACLE DETECTED";
      }

      document.getElementById('obstacleStatus').innerText =
        obstacleText;

      document.getElementById('obstacleStatus').className =
        (data.obstacle ||
         data.leftHealth === "INVALID" ||
         data.rightHealth === "INVALID") ?
        "statusValue bad" :
        (data.leftHealth === "ECHO MISSED" ||
         data.rightHealth === "ECHO MISSED") ?
        "statusValue warning" : "statusValue good";

      document.getElementById('rfidStatus').innerText =
        data.rfid;

      document.getElementById('boxStatus').innerText =
        data.box;

      document.getElementById('boxStatus').className =
        data.box === "LOCK COMMAND SENT" ?
        "statusValue good" :
        data.box === "UNLOCK COMMAND SENT" ?
        "statusValue warning" :
        "statusValue bad";

      document.getElementById('servoStatus').innerText =
        data.servo;

      document.getElementById('servoStatus').className =
        data.servo === "READY" ||
        data.servo === "LOCK COMMAND SENT" ?
        "statusValue good" :
        data.servo === "UNLOCK COMMAND SENT" ?
        "statusValue warning" :
        "statusValue bad";

      document.getElementById('emergencyStatus').innerText =
        data.emergency ? "EMERGENCY STOP ACTIVE" : "NORMAL";

      document.getElementById('emergencyStatus').className =
        data.emergency ?
        "statusValue bad" : "statusValue good";

      document.getElementById('startButton').disabled =
        data.emergency || data.running || data.complete;

    })
    .catch(() => {

      document.getElementById('robotStatus').innerText =
        "ESP32 CONNECTION LOST";

    })
    .finally(() => {

      requestInProgress = false;

    });
}

setInterval(refreshStatus, 500);

refreshStatus();

</script>

</body>
</html>
)rawliteral";
}

// =====================================================
// 23. WEB SERVER HANDLERS
// =====================================================

void handleRoot() {

  server.send(200, "text/html", webpage());
}

void handleStart() {

  if (emergencyStop) {

    server.send(403, "text/plain",
                "Emergency stop active. Restart ESP32.");

    return;
  }

  if (robotRunning) {

    server.send(200, "text/plain",
                "Delivery already running");

    return;
  }

  if (deliveryComplete) {

    server.send(403, "text/plain",
                "Delivery already completed. Restart ESP32.");

    return;
  }

  if (!servoReady) {

    server.send(503, "text/plain",
                "Servo initialisation failed. Check servo.");

    return;
  }

  if (!sensorsHealthy()) {

    robotStatus = "Waiting for healthy sensors";

    server.send(409, "text/plain",
                "Sensors not ready. Check dashboard readings.");

    return;
  }

  if (boxUnlocked) {
    lockBox();
  }

  deliveryComplete = false;

  deliveryElapsedMs = 0;
  deliveryStartTime = millis();

  robotRunning = true;

  rfidStatus = "Waiting for delivery completion";

  robotStatus = "Delivery started";

  Serial.println();
  Serial.println("============================");
  Serial.println("MEDIROUTE DELIVERY STARTED");
  Serial.println("TOTAL TIME: 10 SECONDS");
  Serial.println("============================");

  server.send(200, "text/plain",
              "10 second delivery started");
}

void handleStop() {

  emergencyStop = true;
  robotRunning = false;

  stopMotors();

  robotStatus = "EMERGENCY STOP";
  rfidStatus = "EMERGENCY STOP ACTIVE";

  Serial.println("EMERGENCY STOP ACTIVATED");

  server.send(200, "text/plain",
              "EMERGENCY STOP ACTIVATED");
}

// =====================================================
// 24. STATUS JSON
// =====================================================

void handleStatus() {

  updateDeliveryTime();

  String json = "{";

  json += "\"status\":\"" + robotStatus + "\",";
  json += "\"progress\":" + String(deliveryElapsedMs) + ",";

  json += "\"left\":" + String(leftSensorReading) + ",";
  json += "\"right\":" + String(rightSensorReading) + ",";

  json += "\"leftSafety\":" + String(leftSafetyDistance) + ",";
  json += "\"rightSafety\":" + String(rightSafetyDistance) + ",";

  json += "\"leftHealth\":\"" + leftSensorHealth + "\",";
  json += "\"rightHealth\":\"" + rightSensorHealth + "\",";

  json += "\"obstacle\":";
  json += obstacleDetected ? "true," : "false,";

  json += "\"rfid\":\"" + rfidStatus + "\",";
  json += "\"box\":\"" + boxStatus + "\",";
  json += "\"servo\":\"" + servoStatus + "\",";

  json += "\"running\":";
  json += robotRunning ? "true," : "false,";

  json += "\"complete\":";
  json += deliveryComplete ? "true," : "false,";

  json += "\"emergency\":";
  json += emergencyStop ? "true" : "false";

  json += "}";

  server.send(200, "application/json", json);
}

// =====================================================
// 25. SYSTEM SERVICE
// =====================================================

void serviceSystem() {

  server.handleClient();

  handleRFID();

  updateDeliveryTime();

  updateServoLock();

  updateSensorsIfDue();
}

// =====================================================
// 26. INTERRUPTIBLE MOTOR MOVEMENT
// =====================================================

bool motorStep(int leftSpeed,
               int rightSpeed,
               unsigned long duration) {

  if (emergencyStop ||
      !robotRunning ||
      deliveryTimeExpired()) {

    stopMotors();
    return false;
  }

  setMotor(leftSpeed, rightSpeed);

  unsigned long startTime = millis();

  while (millis() - startTime < duration) {

    serviceSystem();

    if (emergencyStop ||
        !robotRunning ||
        deliveryTimeExpired()) {

      stopMotors();
      return false;
    }

    delay(5);
  }

  stopMotors();

  return true;
}

// =====================================================
// 27. SAFE STOP
// =====================================================

void stopForUnsafePath(const char *message) {

  stopMotors();

  robotRunning = false;

  robotStatus = message;

  Serial.println();
  Serial.println("SAFE STOP");
  Serial.println(message);
}

// =====================================================
// 28. SENSOR-CHECKED OBSTACLE AVOIDANCE
// =====================================================

bool sensorsClear(int leftDistance, int rightDistance) {

  return leftDistance >= CLEAR_DISTANCE &&
         rightDistance >= CLEAR_DISTANCE;
}


bool avoidObstacle(int initialLeft,
                   int initialRight) {

  Serial.println("OBSTACLE DETECTED");

  robotStatus = "Avoiding obstacle";

  stopMotors();

  if (deliveryTimeExpired()) {
    return false;
  }

  // Reverse slightly.
  if (!motorStep(-AVOID_SPEED,
                 -AVOID_SPEED, 180)) {
    return false;
  }

  bool turnLeft = initialLeft > initialRight;

  int clearReadings = 0;
  bool foundClearance = false;

  // Turn in small pulses.
  for (int i = 0; i < 10; i++) {

    if (deliveryTimeExpired() ||
        emergencyStop || !robotRunning) {

      stopMotors();
      return false;
    }

    if (turnLeft) {

      if (!motorStep(-AVOID_SPEED,
                     AVOID_SPEED, 120)) {
        return false;
      }

    } else {

      if (!motorStep(AVOID_SPEED,
                     -AVOID_SPEED, 120)) {
        return false;
      }
    }

    serviceSystem();
    sampleSensorsNow(); // fresh pair: do not count the same old reading twice

    if (deliveryTimeExpired()) {
      stopMotors();
      return false;
    }

    if (!sensorsHealthy()) {

      stopForUnsafePath("Sensor fault during avoidance");
      return false;
    }

    int leftDistance = leftSafetyDistance;
    int rightDistance = rightSafetyDistance;

    if (sensorsClear(leftDistance, rightDistance)) {

      clearReadings++;

      if (clearReadings >= 2) {

        foundClearance = true;
        break;
      }

    } else {

      clearReadings = 0;
    }
  }

  if (!foundClearance) {

    stopForUnsafePath("No safe direction detected");
    return false;
  }

  // Carefully move forward while rechecking.
  for (int i = 0; i < 8; i++) {

    if (!motorStep(AVOID_SPEED,
                   AVOID_SPEED, 150)) {
      return false;
    }

    serviceSystem();
    sampleSensorsNow(); // check again using fresh readings after each forward pulse

    if (deliveryTimeExpired()) {
      stopMotors();
      return false;
    }

    if (!sensorsHealthy()) {

      stopForUnsafePath("Sensor fault during avoidance");
      return false;
    }

    int leftDistance = leftSafetyDistance;
    int rightDistance = rightSafetyDistance;

    if (leftDistance < OBSTACLE_DISTANCE ||
        rightDistance < OBSTACLE_DISTANCE) {

      stopMotors();

      robotStatus = "Reassessing obstacle";

      return true;
    }
  }

  robotStatus = "Delivery in progress";

  return true;
}

// =====================================================
// 29. DELIVERY COMPLETION
// =====================================================

void completeDelivery() {

  stopMotors();

  robotRunning = false;
  deliveryComplete = true;

  deliveryElapsedMs = TOTAL_DELIVERY_TIME;

  robotStatus = "Destination reached - Scan RFID";
  rfidStatus = "Ready for authorised card";

  Serial.println();
  Serial.println("============================");
  Serial.println("DELIVERY COMPLETED");
  Serial.println("TOTAL TIME: 10 SECONDS");
  Serial.println("MOTORS STOPPED");
  Serial.println("SCAN AUTHORISED RFID CARD");
  Serial.println("============================");
}

// =====================================================
// 30. AUTONOMOUS DELIVERY
// =====================================================

void autonomousLoop() {

  if (!robotRunning || emergencyStop) {
    return;
  }

  if (deliveryTimeExpired()) {

    completeDelivery();
    return;
  }

  if (!sensorsHealthy()) {

    stopForUnsafePath("Ultrasonic sensor reading invalid");

    return;
  }

  int leftDistance = leftSafetyDistance;
  int rightDistance = rightSafetyDistance;

  Serial.print("LEFT SAFETY: ");
  Serial.print(leftDistance);

  Serial.print(" cm | RIGHT SAFETY: ");
  Serial.print(rightDistance);

  Serial.print(" cm | ELAPSED: ");
  Serial.print(deliveryElapsedMs / 1000.0);

  Serial.println(" sec");

  // Both sensors detect an obstacle.
  if (leftDistance < OBSTACLE_DISTANCE &&
      rightDistance < OBSTACLE_DISTANCE) {

    stopMotors();

    robotStatus = "Obstacle ahead";

    avoidObstacle(leftDistance, rightDistance);

    return;
  }

  // Left obstacle: steer right.
  if (leftDistance < OBSTACLE_DISTANCE) {

    robotStatus = "Steering right";

    setMotor(LEFT_SPEED,
             RIGHT_SPEED - 35);

    return;
  }

  // Right obstacle: steer left.
  if (rightDistance < OBSTACLE_DISTANCE) {

    robotStatus = "Steering left";

    setMotor(LEFT_SPEED - 35,
             RIGHT_SPEED);

    return;
  }

  robotStatus = "Delivery in progress";

  moveForward();
}

// =====================================================
// 31. SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("       MEDIROUTE ROBOT");
  Serial.println("================================");

  // SERVO INITIALISATION FIRST
  // This reserves servo PWM resources before motor PWM.

  initialiseServo();

  // MOTOR SETUP

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STBY, OUTPUT);

  digitalWrite(STBY, HIGH);

  bool pwmAReady = ledcAttach(PWMA, 1000, 8);
  bool pwmBReady = ledcAttach(PWMB, 1000, 8);

  if (!pwmAReady || !pwmBReady) {

    Serial.println("ERROR: MOTOR PWM INITIALISATION FAILED");
  }

  stopMotors();

  Serial.println("Motor driver initialised");

  // ULTRASONIC SETUP

  pinMode(LEFT_TRIG, OUTPUT);
  pinMode(LEFT_ECHO, INPUT);

  pinMode(RIGHT_TRIG, OUTPUT);
  pinMode(RIGHT_ECHO, INPUT);

  digitalWrite(LEFT_TRIG, LOW);
  digitalWrite(RIGHT_TRIG, LOW);

  Serial.println("Ultrasonic sensors initialised");

  // RFID SETUP

  SPI.begin(18, 19, 23, RFID_SS);

  rfid.PCD_Init();

  Serial.println("RFID reader initialised");

  // WIFI SETUP

  WiFi.mode(WIFI_AP);

  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.println("WiFi Access Point started");

  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);

  Serial.print("Password: ");
  Serial.println(WIFI_PASSWORD);

  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());

  // WEB SERVER

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/status", handleStatus);

  server.begin();

  robotStatus = "Ready";

  Serial.println();
  Serial.println("MEDIROUTE READY");
  Serial.println("LIVE SENSOR MONITORING ENABLED");

  if (servoReady) {
    Serial.println("SERVO READY");
  } else {
    Serial.println("SERVO ERROR - CHECK CONNECTION");
  }
}

// =====================================================
// 32. MAIN LOOP
// =====================================================

void loop() {

  serviceSystem();

  if (emergencyStop) {

    stopMotors();

    delay(5);

    return;
  }

  if (robotRunning) {

    if (deliveryTimeExpired()) {

      completeDelivery();

    } else {

      autonomousLoop();
    }

  } else {

    stopMotors();

    delay(5);
  }
}
