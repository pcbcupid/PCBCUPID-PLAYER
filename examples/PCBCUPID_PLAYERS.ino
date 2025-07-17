#include <Wire.h>
#include "PCBCUPID_PLAYERS.h"

PCBCUPID_PLAYERS player(Wire, 0x21);

void setup() {
  Serial.begin(115200);
  Wire.begin();  
  delay(100);    
  player.begin("mp3");  // Choose "mp3", "wav", "aac"
}

void loop() {
  player.loop();
}
