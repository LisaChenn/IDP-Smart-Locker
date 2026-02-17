//no external libraries
//SMART LOCKER STATE MACHINE

//current: user able to reset unlock/lock, # key need hold time?

#include <avr/io.h>
#include <Arduino.h>

#define EE_PW_ADDR     0
#define EE_MAGIC_ADDR  10
#define EE_MAGIC_VALUE 0xA5


//ENUMS AND STATES
enum RowState {NONE, ROW1, ROW2, ROW3, ROW4, UNKNOWN};


bool pwMatches();
void pw_save_4digits(const char* pw);

enum SystemState {
  LOCKED_IDLE,      //System locked, waiting for input
  ENTERING_PW,      //User entering password to unlock
  CHECK_PW,         //Verifying entered password
  UNLOCKED,         //System unlocked
  RESET_PW,         //Resetting/changing password
  CONFIRM_NEW_PW    //Confirming new password entry, UI: additional state to double check the new pwd user wants to keep as lock code
};

// PIN DEFINITIONS
const int COL_PINS[] = {2, 3, 4};
const int analogPin = A0;
const int SERVO_PIN = 9;

const int BUZZER_PIN = 10; //for later additional features

// KEYPAD LAYOUT ARRAY
const char KEYS[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

// PASSWORD SETTINGS VARIABLES
const int MAX_PASSWORD_LENGTH = 4;

// KEYPAD DEBOUNCING
char lastKey = '\0';
char currentKey = '\0';
unsigned long lastKeyTime = 0;
const unsigned long DEBOUNCE_TIME = 300;

// NON-BLOCKING KEYPAD SCANNING
int currentCol = 0;
unsigned long colSwitchTime = 0;
const unsigned long COL_SETTLE_TIME = 2;

// STATE MACHINE
SystemState state = LOCKED_IDLE;
unsigned long stateStartTime = 0;

// FUNCTION DECLARATIONS
RowState classifyRow(int adc);
char scanKeypadNonBlocking();
void handleKeyPress(char key);
void changeState(SystemState newState);
static void eeprom_write_byte_11(uint16_t addr, uint8_t data);
static uint8_t eeprom_read_byte_11(uint16_t);
void pw_load_default_if_missing();
void clearEntered();

//COUNTER FOR #
uint8_t hashCount = 0;
unsigned long firstHashTime = 0;
const unsigned long HASH_WINDOW_MS = 2000;

//SETTING PW LENGTH
const uint8_t PW_LEN = 4;
char entered[PW_LEN + 1]; // char array
uint8_t enteredLen = 0;

char stored[PW_LEN + 1];


bool isUnlocked = false;


// SETUP (placeholder)
void setup() {
  Serial.begin(9600);
  pinMode(analogPin, INPUT);
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pw_load_default_if_missing();
  
  for (int i = 0; i < 3; i++) {
    pinMode(COL_PINS[i], OUTPUT);
    digitalWrite(COL_PINS[i], LOW);
  }
    
  Serial.println("=== Smart Locker System ===");
  Serial.println("State: LOCKED_IDLE");
  Serial.println("Enter password to unlock");
 
}

//MAIN LOOP
void loop() {
  char raw = scanKeypadNonBlocking();
  static char latched = '\0';
  static unsigned long releaseStart = 0;
  const unsigned long RELEASE_MS = 80;   

  if (latched == '\0') {
    if (raw != '\0') {
      latched = raw;
      Serial.print("Key Pressed: ");
      Serial.println(latched);
      handleKeyPress(latched);
    }
    return;
  }

  if (raw == '\0') {
    if (releaseStart == 0) releaseStart = millis();
    if (millis() - releaseStart >= RELEASE_MS) {
      latched = '\0';
      releaseStart = 0;
    }
  } else {
    // still seeing some key (maybe bouncing), reset release timer
    releaseStart = 0;
  }
}


// HANDLE KEY PRESS
void handleKeyPress(char k) {
  unsigned long now = millis();

  switch (state) {
    case LOCKED_IDLE: 
      Serial.println("LOCKED_IDLE");
      
      if (hashCount > 0 && (now - firstHashTime) > HASH_WINDOW_MS) {
        hashCount = 0;
      }

      if (k == '#') {
        if (hashCount == 0) firstHashTime = now;
        hashCount++;

        if (hashCount >= 4) {
          hashCount = 0;
          clearEntered();
          state = RESET_PW;
          break;
        }
      } 
      else if (k >= '0' && k <= '9') {
        hashCount = 0;
        clearEntered();
        entered[enteredLen++] = k;
        entered[enteredLen] = '\0';
        state = ENTERING_PW;
      }


     break;


    case ENTERING_PW:
      Serial.println("ENTERING_PW");

      if (k >= '0' && k <= '9') {
        if (enteredLen < PW_LEN) {
          entered[enteredLen++] = k;
          entered[enteredLen] = '\0';

          if (enteredLen == PW_LEN) {
            state = CHECK_PW;
          }
        }
      }
      else if (k == '*') {
        clearEntered();
        Serial.println("Password Cleared");
        state = LOCKED_IDLE;
      }
      else if (k == '#') {
        if (enteredLen == PW_LEN) {
          
          state = CHECK_PW;
          }
      }
    break;



    case CHECK_PW:
      Serial.println("CHECK_PW");
      if (pwMatches()) {
        Serial.println("Correct Password");
        Serial.println(">>> UNLOCKED <<<");
        isUnlocked = true;
        clearEntered();
        state = UNLOCKED;
      } else {
        Serial.println("Incorrect Password");
        delay(1000);
        clearEntered();
        state = LOCKED_IDLE;
      }
      break;

    case UNLOCKED: 
      if (k == '*' && isUnlocked){
        isUnlocked = false;
        state = LOCKED_IDLE;
      }
     break;


    case RESET_PW: 
      Serial.println("RESET_PW");
      // collect exactly 4 digits
      if (k >= '0' && k <= '9'){
        if (enteredLen < PW_LEN){
          entered[enteredLen++] = k;
          entered[enteredLen] = '\0';
        }
      } else if (k == '*'){
        hashCount = 0;
        clearEntered();
        state = LOCKED_IDLE;
      } else if (k == '#'){
        // confirm/save only if 4 digits entered
        if (enteredLen == PW_LEN){
          pw_save_4digits(entered);
          clearEntered();
          state = LOCKED_IDLE;
        }
      }
    break;
    
  }
}

// CHANGE STATE
void changeState(SystemState newState) {
  state = newState;
  stateStartTime = millis();
  
  Serial.print("State: ");
  switch (newState) {
    case LOCKED_IDLE: Serial.println("LOCKED_IDLE"); break;
    case UNLOCKED: Serial.println("UNLOCKED"); break;
    case ENTERING_PW: Serial.println("ENTERING_PW"); break;
    case CHECK_PW: Serial.println("CHECK_PW"); break;
    case RESET_PW: Serial.println("RESET_PW - Enter new 4-digit password"); break;
    case CONFIRM_NEW_PW: Serial.println("CONFIRM_NEW_PW"); break;
  }
}
// CLASSIFY ROW (from previous keypad recognition)
RowState classifyRow(int adc) {
  if (adc >= 620 && adc <= 750) return ROW1;
  if (adc >= 450 && adc <= 560) return ROW2;
  if (adc >= 270 && adc <= 360) return ROW3;
  if (adc >= 130 && adc <= 230) return ROW4;
  if (adc >= 0   && adc <= 50)  return NONE;
  return UNKNOWN;
}

// NON-BLOCKING KEYPAD SCAN
char scanKeypadNonBlocking() {
  static enum ScanState {WAITING, READING} scanState = WAITING;
  
  switch (scanState) {
    case WAITING:
      digitalWrite(COL_PINS[currentCol], HIGH);
      colSwitchTime = millis();
      scanState = READING;
      return '\0';
      
    case READING:
      if (millis() - colSwitchTime >= COL_SETTLE_TIME) {
        int adc = analogRead(analogPin);
        RowState currentRow = classifyRow(adc);
        
        char detectedKey = '\0';
        
        if (currentRow >= ROW1 && currentRow <= ROW4) {
          int rowIndex = currentRow - 1;
          detectedKey = KEYS[rowIndex][currentCol];
        }
        
        digitalWrite(COL_PINS[currentCol], LOW);
        currentCol = (currentCol + 1) % 3;
        scanState = WAITING;
        
        return detectedKey;
      }
      return '\0';
  }
  
  return '\0';
}

bool pwMatches() {
  // must have exactly 4 digits
  if (enteredLen != PW_LEN) return false;

  for (uint8_t i = 0; i < PW_LEN; i++) {
    if (entered[i] != stored[i]) return false;
  }
  return true;
}
void pw_save_4digits(const char* pw) {

  for (uint8_t i = 0; i < PW_LEN; i++) {
    eeprom_write_byte_ll(EE_PW_ADDR + i, (uint8_t)pw[i]);
  }
  eeprom_write_byte_ll(EE_MAGIC_ADDR, EE_MAGIC_VALUE);

  for (uint8_t i = 0; i < PW_LEN; i++) stored[i] = pw[i];
  stored[PW_LEN] = '\0';
  Serial.print("New password saved: ");
  Serial.println(stored);
}


//EPROM STUFF
static void eeprom_wait_ready() {
  while (EECR & (1 << EEPE)) { /* wait */ }
}

static void eeprom_write_byte_ll(uint16_t addr, uint8_t data) {
  eeprom_wait_ready();
  EEAR = addr;
  EEDR = data;
  EECR |= (1 << EEMPE);
  EECR |= (1 << EEPE);
}

static uint8_t eeprom_read_byte_ll(uint16_t addr) {
  eeprom_wait_ready();
  EEAR = addr;
  EECR |= (1 << EERE);
  return EEDR;
}

void pw_load_default_if_missing() {
  if (eeprom_read_byte_ll(EE_MAGIC_ADDR) != EE_MAGIC_VALUE) {
    const char defaultPw[PW_LEN + 1] = "1234";  
    for (uint8_t i = 0; i < PW_LEN; i++) {
      eeprom_write_byte_ll(EE_PW_ADDR + i, (uint8_t)defaultPw[i]);
    }
    eeprom_write_byte_ll(EE_MAGIC_ADDR, EE_MAGIC_VALUE);
  }

  // Load stored PW into RAM buffer
  for (uint8_t i = 0; i < PW_LEN; i++) {
    stored[i] = (char)eeprom_read_byte_ll(EE_PW_ADDR + i);
  }
  stored[PW_LEN] = '\0';
}

// WHEN PRESSING THE STAR, THIS METHOD CLEARS ARR
void clearEntered() {
  enteredLen = 0;
  entered[0] = '\0';
}
