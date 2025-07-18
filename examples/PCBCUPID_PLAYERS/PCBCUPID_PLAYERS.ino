#include <Wire.h>
#include "PCBCUPID_PLAYERS.h"


PCBCUPID_PLAYERS player(Wire, 0x21);  // Your custom I2C address

float vol = 0.5;  //volume can be set by the dev user


void setup() {
  Serial.begin(115200);

  delay(500);                // Small delay for Serial ready
  player.begin("mp3");       // dev user can change the extension to play the file: "mp3", "wav", "aac"
  player.setVolume(vol);     // 50% volume
  player.setAutoFade(true);  // Optional for dev user to set auto fade : enable or disable
  player.play();             //start playing at boot
}

void loop() {
  player.loop();

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'n') player.next();
    if (c == 's') player.stop();
    if (c == 'p') {
      if (player.wasJustStopped()) {
        player.play();  // force fresh play
      } else if (player.isPaused()) {
        player.play();  // resume
      } else {
        player.pause();  // pause
      }
    }

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
