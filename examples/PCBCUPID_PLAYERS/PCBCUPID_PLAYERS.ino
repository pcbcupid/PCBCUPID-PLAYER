#include <Wire.h>
#include "PCBCUPID_PLAYERS.h"

PCBCUPID_PLAYERS player(Wire, 0x21);  // Your custom I2C address

float vol = 0.5;

void setup() {
  Serial.begin(115200);
  delay(500);                // Small delay for Serial ready
  player.begin("mp3");       // dev user can the extension: "mp3", "wav", "aac"
  player.setVolume(vol);     // 50% volume
  player.setAutoFade(true);  // Optional for dev user to set auto fade : enable or disable
}

void loop() {
  player.loop();

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'n') player.next();
    if (c == 's') player.stop();
    if (c == 'p') player.play();
    if (c == 'r') player.previous();
    if (c == '+') {
      vol = min(1.0f, vol + 0.1f);
      player.setVolume(vol);
    }
    if (c == '-') {
      vol = max(0.0f, vol - 0.1f);
      player.setVolume(vol);
    }
  }
}


// #include <Wire.h>
// #include "PCBCUPID_PLAYERS.h"

// PCBCUPID_PLAYERS player(Wire, 0x21);

// void setup() {
//   Serial.begin(115200);
//   Wire.begin();  // <-- IMPORTANT: Ensure I2C bus is initialized
//   delay(100);    // Optional short delay
//   player.begin("aac");  // Choose "mp3", "wav", "ogg", "aac"
// }

// void loop() {
//   player.loop();
// }


