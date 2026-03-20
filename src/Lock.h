#include "Arduino.h"
#ifndef _SMART_LOCK_H_
#define _SMART_LOCK_H_
#include <stdint.h>
#include <LiquidCrystal.h>
#include <Keypad.h>
#include <Stepper.h>
#include <SR04.h>
#include <dht11.h>
#include <SPI.h>
#include <MFRC522.h>
#include "WString.h"

class HCSR501 {
  private: uint8_t pin;
  public:
    HCSR501(uint8_t p) { pin = p; pinMode(pin, INPUT); }
    bool isHuman() { return digitalRead(pin) == HIGH ? true : false; }
};

class Beep {
  private: uint8_t pin;
  public:
    Beep(uint8_t p) { pin = p; pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    void beepOnce() { digitalWrite(pin, HIGH); delay(100); digitalWrite(pin, LOW); }
    void beepTwice() { digitalWrite(pin, HIGH); delay(100); digitalWrite(pin, LOW); delay(60); digitalWrite(pin, HIGH); delay(100); digitalWrite(pin, LOW); }
};

class DHT11: public dht11 {
  private: uint8_t pin;
  public:
    DHT11(uint8_t p): dht11(), pin(p) {}
    uint8_t getTemperature() { read(pin); return temperature; }
    uint8_t getHumidity() { read(pin); return humidity; }
};

class Button {
  private: uint8_t pin;
  public:
    Button(uint8_t p) { pin = p; pinMode(pin, INPUT_PULLUP); }
    bool isPressed() { return digitalRead(pin) == LOW ? true: false; }
};

struct smartLockHardwareConfig {
  uint8_t LCD_RESET; uint8_t LCD_ENABLE; uint8_t LCD_DB4; uint8_t LCD_DB5; uint8_t LCD_DB6; uint8_t LCD_DB7; // Screen
  uint8_t RFID_RST; uint RFID_SS; // Card Scanner
  uint8_t KBD_ROW1; uint8_t KBD_ROW2; uint8_t KBD_ROW3; uint8_t KBD_ROW4; // Keyboard
  uint8_t KBD_COL1; uint8_t KBD_COL2; uint8_t KBD_COL3; uint8_t KBD_COL4; // Keyboard
  uint8_t PIR_PIN; // HC-SR501 : Presence Sensor
  uint8_t MTR_PIN1; uint8_t MTR_PIN2; uint8_t MTR_PIN3; uint8_t MTR_PIN4; // Stepper Motor
  uint8_t DST_ECHO; uint8_t DST_TRIG; // Distance Sensor
  uint8_t TMP_DATA; // Temperature Sensor
  uint8_t BEEP_PIN; // Passive Buzzer
  uint8_t BUTTON; // Reset Button
};

class SmartLockHardware {
  protected:
    LiquidCrystal screen;
    MFRC522 scanner;
    Keypad keypad;
    HCSR501 human;
    Stepper motor;
    SR04 distance;
    DHT11 temperature;
    Beep beep;
    Button reset;
  
  public:
    SmartLockHardware(const smartLockHardwareConfig*);
    // Screen
    void gotoL1();
    void gotoL2();
    void clearL1();
    void clearL2();
    void clearScreen();
    void println1(const char*);
    void println1(String);
    void println2(const char*);
    void println2(String);
    void printf2(char);
    // Scanner
    String* scanCard();
    // Keyboard
    char getChar();
    // Presence Detection
    bool isHumanPresent();
    // Motor
    void lock();
    void unlock();
    // Distance Sensor
    long getDistance();
    bool isDistance();
    // Temperature
    uint8_t getTemperature();
    bool isTemperature();
    // Beep
    void beepOnce();
    void beepTwice();
    // Reset
    bool isResetPressed();
};

enum SmartLockState {
  INITIAL, WAITING, SCANNING_CARD, SCANNING_CODE, PROCESSING, AUTHSUCCESS
};

struct Authorizer {
  String card; String code;
  bool (*runnable)(String, String);
  void reset() { card = ""; code = ""; }
  bool run() { return (*runnable)(card, code); }
};

class SmartLock {
  protected:
    SmartLockHardware hardware;
    SmartLockState state;
    Authorizer* auth;
  public:
    SmartLock(Authorizer*);
    SmartLock(Authorizer*, const smartLockHardwareConfig*);
    void run();
    
    void onHumanDetected();
    void onCardScanned();
    void onCodeScanned();
    void onAuthSuccess();
    void onAuthFailure();
    void onResetPress();
};

#endif