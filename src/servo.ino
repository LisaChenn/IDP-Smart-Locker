const unsigned long FRAME = 20000UL;   // 20,000 us = 50 Hz frame
const int PWM_ZERO = 600;             // us pulse for 0 deg
const int PWM_NINETY = 1500;          // us pulse for 90 deg 
const int PWM_ONE_EIGHTY = 2400;      // us pulse for 180 deg
const int PIN_SERVO = 9;

unsigned long startFrame = 0;
unsigned long startPulse = 0;

int pulseWidth = PWM_NINETY;

bool servoOn = false;
bool servoPinHigh = false;

static int clampInt(int x, int lo, int hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

void servoEnable() {
  servoOn = true;
  servoPinHigh = false;
  digitalWrite(PIN_SERVO, LOW);
  startFrame = micros();
  startPulse = startFrame;
}

void setAngle(int angleDeg) {
  angleDeg = clampInt(angleDeg, 0, 180);
  pulseWidth = PWM_ZERO + (angleDeg * (PWM_ONE_EIGHTY - PWM_ZERO)) / 180;
}

void servoDisable() {
  servoOn = false;
  servoPinHigh = false;
  digitalWrite(PIN_SERVO, LOW);
}

void setup() {
  pinMode(PIN_SERVO, OUTPUT);
  digitalWrite(PIN_SERVO, LOW);

  servoEnable();
  setAngle(130);
}

void loop() {
  if (!servoOn) return;

  unsigned long now = micros();

  if (!servoPinHigh && (now - startFrame) >= FRAME) {
    startFrame += FRAME;        
    digitalWrite(PIN_SERVO, HIGH);
    startPulse = now;
    servoPinHigh = true;
    return;
  }

  if (servoPinHigh && (now - startPulse) >= (unsigned long)pulseWidth) {
    digitalWrite(PIN_SERVO, LOW);
    servoPinHigh = false;
    return;
  }
}





