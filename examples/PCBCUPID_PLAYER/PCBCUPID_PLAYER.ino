#pragma once

#include <Wire.h>
#include "PCBCUPID_PLAYER.h"

// Create configuration with default values
AudioPlayerConfig playerConfig = {
    // SD Card Configuration
    .sd_cs = 1,
    .sd_miso = 3,
    .sd_mosi = 2,
    .sd_sck = 11,
    
    // I2S Configuration
    .i2s_mclk = 22,
    .i2s_bclk = 25,
    .i2s_ws = 24,
    .i2s_dout = 23,
    
    // File path
    .audio_start_path = "/"
};

// Create player instance with configuration
AUDIO_PLAYER player(Wire, playerConfig);

float vol = 0.5;         // Initial volume
String fileext = "mp3";  // Default file extension (changeable e.g., wav, ogg, aac)

void printHelp()
{
  Serial.println("\n=== PCBCUPID Player Commands ===");
  Serial.println("n - Next track");
  Serial.println("r - Previous track");
  Serial.println("p - Play/Pause");
  Serial.println("s - Stop");
  Serial.println("l - List all tracks");
  Serial.println("0-50 - Play track by index");
  Serial.println("+ - Volume up");
  Serial.println("- - Volume down");
  Serial.println("===============================\n");
}

void setup()
{
  Serial.begin(115200);
  delay(500); // Wait for Serial monitor

  Serial.println("\nInitializing PCBCUPID Player...");
  player.begin(fileext.c_str());
  player.setVolume(vol);
  player.setAutoFade(true);
  // Ensure amplifier is powered on
  player.powerOnAmplifier();
  Serial.println("Waiting for user to press 'p' to start playback...");
  player.stop(); // Ensure player is stopped initially
  printHelp();
}

void loop()
{
  player.loop();

  if (Serial.available())
  {
    char c = Serial.read();

    switch (c)
    {
    case 'n':
      Serial.println("Next track");
      player.next();
      break;
    case 'r':
      Serial.println("Previous track");
      player.previous();
      break;
    case 's':
      Serial.println("Stopping playback");
      player.stop();
      break;
    case 'p':
      if (player.isStopped())
      {
        Serial.println("Starting playback");
        player.play();
      }
      else if (player.isPaused())
      {
        Serial.println("Resuming playback");
        player.play();
      }
      else
      {
        Serial.println("Pausing playback");
        player.pause();
      }
      break;
    case 'l':
      player.listTracks();
      break;
    case '+':
      vol = min(1.0f, vol + 0.1f);
      player.setVolume(vol);
      Serial.print("Volume: ");
      Serial.println(vol * 100);
      break;
    case '-':
      vol = max(0.0f, vol - 0.1f);
      player.setVolume(vol);
      Serial.print("Volume: ");
      Serial.println(vol * 100);
      break;
    case 'h':
      printHelp();
      break;
    default:
      if (c >= '0' && c <= '9') {
        // Read first digit
        int index = c - '0';
        
        // Check if there's a second digit available immediately
        if (Serial.available() > 0) {
          char nextChar = Serial.peek();
          if (nextChar >= '0' && nextChar <= '9') {
            // Read the second digit
            nextChar = Serial.read();
            int secondDigit = nextChar - '0';
            index = (index * 10) + secondDigit;
          }
        }
        
        // Limit index to maximum 50
        if (index <= 50) {
          Serial.print("Playing track index: ");
          Serial.println(index);
          player.playTrackIndex(index);
        } else {
          Serial.println("Error: Maximum track index is 50");
        }
      }
      else if (c != '\r' && c != '\n' && c != ' ')
      {
        Serial.print("Unknown command: ");
        Serial.println(c);
      }
      break;
    }
  }
}
