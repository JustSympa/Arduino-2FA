#include "Lock.h"
bool Authorize(String card, String code) {
  Serial.println(card);
  if((card.equals("2011853142") || card.equals("201190199177")) && code.equals("1234")) return true;
  else return false;
}
SmartLock* lock;
Authorizer* auth = new Authorizer{"", "", Authorize};

void setup() {
  Serial.begin(9600);
  lock = new SmartLock(auth);
}

void loop() {
  lock->run();
}
