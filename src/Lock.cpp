#include "HardwareSerial.h"
#include "Arduino.h"
#include "Print.h"
#include "WString.h"
#include "Lock.h"

const smartLockHardwareConfig* defaultConfig = new smartLockHardwareConfig {
  2, 3, 4, 5, 6, 7,
  8, 9,
  22, 24, 26, 28, 23, 25, 27, 29,
  30,
  36, 34, 35, 33,
  10, 11,
  12,
  40, 41
};

SmartLockHardware::SmartLockHardware(const smartLockHardwareConfig* config = defaultConfig):
  screen(config->LCD_RESET, config->LCD_ENABLE, config->LCD_DB4, config->LCD_DB5, config->LCD_DB6, config->LCD_DB7),
  scanner(config->RFID_SS, config->RFID_RST),
  keypad(
    new char[16]{'1','2','3','A','4','5','6','B','7','8','9','C','*','0','#','D'},
    new uint8_t[4]{config->KBD_ROW1, config->KBD_ROW2, config->KBD_ROW3, config->KBD_ROW4},
    new uint8_t[4]{config->KBD_COL1, config->KBD_COL2, config->KBD_COL3, config->KBD_COL4},
    4, 4),
  human(config->PIR_PIN),
  motor(360, config->MTR_PIN1, config->MTR_PIN2, config->MTR_PIN3, config->MTR_PIN4),
  distance(config->DST_ECHO, config->DST_TRIG),
  temperature(config->TMP_DATA),
  beep(config->BEEP_PIN),
  reset(config->BUTTON)
  {
  // screen = LiquidCrystal(cfg.LCD_RESET, cfg.LCD_ENABLE, cfg.LCD_DB4, cfg.LCD_DB5, cfg.LCD_DB6, cfg.LCD_DB7);
  screen.begin(16, 2);
  // scanner = MFRC522(cfg.RFID_SS, cfg.RFID_RST);
  SPI.begin();
  scanner.PCD_Init();
  motor.setSpeed(30);
  // keypad = Keypad(
  //   new char[]{'1','2','3','A','4','5','6','B','7','8','9','C','*','0','#','D'},
  //   new uint8_t[]{cfg.KBD_ROW1, cfg.KBD_ROW2, cfg.KBD_ROW3, cfg.KBD_ROW4},
  //   new uint8_t[]{cfg.KBD_COL1, cfg.KBD_COL2, cfg.KBD_COL3, cfg.KBD_COL4},
  //   4, 4);
  // human = HCSR501(cfg.PIR_PIN);
  // motor = Stepper(360, cfg.MTR_PIN1, cfg.MTR_PIN2, cfg.MTR_PIN3, cfg.MTR_PIN4);
  // distance = SR04(cfg.DST_ECHO, cfg.DST_TRIG);
  // temperature = DHT11(cfg.TMP_DATA);
  // beep = Beep(cfg.BEEP_PIN);
  // reset = Button(cfg.BUTTON);
}

void SmartLockHardware::gotoL1() { screen.setCursor(0, 0); }
void SmartLockHardware::gotoL2() { screen.setCursor(0, 1); }
void SmartLockHardware::clearL1() { gotoL1(); screen.write("                "); gotoL1(); }
void SmartLockHardware::clearL2() { gotoL2(); screen.write("                "); gotoL2(); }
void SmartLockHardware::clearScreen() { screen.clear(); }
void SmartLockHardware::println1(const char* str) { clearL1(); screen.write(str); }
void SmartLockHardware::println1(String str) { clearL1(); screen.write(str.c_str()); }
void SmartLockHardware::println2(const char* str) { clearL2(); screen.write(str); }
void SmartLockHardware::println2(String str) { clearL2(); screen.write(str.c_str()); }
void SmartLockHardware::printf2(char c) { screen.print(c); }

String* SmartLockHardware::scanCard() {
  if(!scanner.PICC_IsNewCardPresent()) return nullptr;
  if(!scanner.PICC_ReadCardSerial()) return nullptr;
  MFRC522::PICC_Type piccType = scanner.PICC_GetType(scanner.uid.sak);
  if(piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K)
    return nullptr;
  String* result = new String();
  for (byte i = 0; i < scanner.uid.size; i++) result->concat(scanner.uid.uidByte[i]);
  return result;
}

char SmartLockHardware::getChar() { return keypad.getKey(); }
bool SmartLockHardware::isHumanPresent() { return human.isHuman(); }

void SmartLockHardware::lock() { motor.step(360); }
void SmartLockHardware::unlock() { motor.step(-360); }

long SmartLockHardware::getDistance() { return distance.Distance(); }
bool SmartLockHardware::isDistance() { long d = getDistance(); return 15 <= d && d <= 100? true : false; }

uint8_t SmartLockHardware::getTemperature() { return temperature.getTemperature(); }
bool SmartLockHardware::isTemperature() { long t = getTemperature(); return 20 <= t && t <= 36? true : false; }

void SmartLockHardware::beepOnce() { beep.beepOnce(); }
void SmartLockHardware::beepTwice() { beep.beepTwice(); }
bool SmartLockHardware::isResetPressed() { return reset.isPressed(); }

SmartLock::SmartLock(Authorizer* authorizer): hardware(defaultConfig) { auth = authorizer; state = SmartLockState::INITIAL; }
SmartLock::SmartLock(Authorizer* authorizer, const smartLockHardwareConfig* config = defaultConfig): hardware(config) { auth = authorizer; state = SmartLockState::INITIAL; }
void SmartLock::run() {
  switch (state) {
    case SmartLockState::INITIAL: {
      hardware.println1("2FA By Group 14");
      delay(5000);
      hardware.clearScreen(); state = SmartLockState::WAITING;
      return;
    }
    case SmartLockState::WAITING: {
      if(hardware.isHumanPresent())
        if(hardware.isDistance()) onHumanDetected();
      return;
    }
    case SmartLockState::SCANNING_CARD: {
      String* s = hardware.scanCard();
      if(s) onCardScanned();
      return;
    }
    case SmartLockState::SCANNING_CODE: {
      char c = hardware.getChar();
      if(c) {
        hardware.beepOnce();
        auth->code.concat(c);
        hardware.printf2('X');
        if(auth->code.length() == 4) onCodeScanned();
      }
      return;
    }
    case SmartLockState::PROCESSING: {
      if(auth->run()) onAuthSuccess();
      else onAuthFailure();
      return;
    }
    case SmartLockState::AUTHSUCCESS: {
      if(hardware.isResetPressed()) onResetPress();
      return;
    }
  }
}

void SmartLock::onHumanDetected() {
  hardware.beepOnce();
  hardware.clearL1();
  hardware.println1("Place your Key");
  state = SmartLockState::SCANNING_CARD;
}
void SmartLock::onCardScanned() {
  hardware.beepOnce();
  auth->card = *s;
  // hardware.println2(auth->card);
  hardware.clearScreen();
  hardware.println1("Enter your Code");
  hardware.gotoL2();
  state = SmartLockState::SCANNING_CODE;
}
void SmartLock::onCodeScanned() {
  hardware.clearScreen();
  hardware.println1(" Authenticating ");
  state = SmartLockState::PROCESSING;
}
void SmartLock::onAuthSuccess() {
  hardware.beepTwice();
  hardware.println1("Welcome!");
  hardware.unlock();
  hardware.beepOnce();
  state = SmartLockState::AUTHSUCCESS;
}
void SmartLock::onAuthFailure() {
  hardware.beepOnce();
  hardware.println1("Goodbye!");
  delay(5000);
  hardware.clearScreen();
  auth->reset();
  state = SmartLockState::WAITING;
}
void SmartLock::onResetPress() {
  hardware.beepTwice();
  hardware.lock();
  hardware.beepOnce();
  hardware.clearScreen();
  auth->reset();
  state = SmartLockState::WAITING;
}
