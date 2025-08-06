#include <Wire.h>
#include "PCBCUPID_PLAYERS.h"


PCBCUPID_PLAYERS player(Wire);  // Your custom I2Cbus

float vol = 0.5;  //volume can be set between 1 - 0.5


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
    Serial.print("Received input: ");
    Serial.println(c);
    
    if (c >= '0' && c <= '9') {
      int trackIndex = c - '0';
      if (trackIndex < player.getTrackCount()) {
        if (player.playTrack(trackIndex)) {
          Serial.print("Playing track: ");
          Serial.println(player.currentTrackName());
        } else {
          Serial.println("Failed to play track");
        }
      } else {
        Serial.print("Invalid track index. Please use 0-");
        Serial.println(player.getTrackCount() - 1);
      }
    }
    else if (c == 'n') {
      Serial.println("Next track");
      player.next();
    }
    else if (c == 'r') {
      Serial.println("Previous track");
      player.previous();
    }
    else if (c == 's') {
      Serial.println("Stopping playback");
      player.stop();
    }
    else if (c == 'p') {
      if (player.isStopped()) {
        Serial.println("Restarting playback");
        player.play();
      } else if (player.isPaused()) {
        Serial.println("Resuming playback");
        player.play();
      } else {
        Serial.println("Pausing playback");
        player.pause();
      }
    }
    else if (c == '+') {
      vol = min(1.0f, vol + 0.1f);
      player.setVolume(vol);
      Serial.print("Volume: ");
      Serial.println(vol * 100);
    }
    else if (c == '-') {
      vol = max(0.0f, vol - 0.1f);
      player.setVolume(vol);
      Serial.print("Volume: ");
      Serial.println(vol * 100);
    }
  }
}
