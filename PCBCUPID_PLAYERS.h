#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <vector>

#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSD.h>
#include <AudioTools/CoreAudio/AudioPlayer.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>
#include <AudioTools/AudioCodecs/CodecOpusOgg.h>
#include <AudioTools/AudioCodecs/CodecAACHelix.h>

#include "config.h"
#include "Driver.h"

enum PlayerState { STOPPED,
                   PLAYING,
                   PAUSED };

class PCBCUPID_PLAYERS {
public:
  PCBCUPID_PLAYERS(TwoWire& wire, uint8_t addr);

  ~PCBCUPID_PLAYERS();  // Destructor

  void begin(const char* ext);  // example: "mp3", "wav", "aac", "ogg"
  void loop();


  // New audio control APIs
  void play();
  void stop();
  void next();
  void setVolume(float volume);
  float getVolume();
  void setAutoFade(bool enable);
  void previous();
  void pause();

  bool isPaused() const {
    return paused;
  }

bool wasJustStopped() const {
  return lastCommandWasStop;
}




private:
  TwoWire& wire;
  uint8_t addr;


  I2SStream i2s;
  AudioPlayer* player = nullptr;
  AudioSourceSD* source = nullptr;
  PlayerState currentState = STOPPED;

  MP3DecoderHelix mp3;
  WAVDecoder wav;
  OpusOggDecoder ogg;
  AACDecoderHelix aac;

  PCBCUPID_NAU8325 amp;

  class InfoHandler : public AudioInfoSupport {
  public:
    PCBCUPID_NAU8325& amp;
    I2SStream& i2s;
    AudioInfo lastInfo;

    InfoHandler(PCBCUPID_NAU8325& amp, I2SStream& i2s)
      : amp(amp), i2s(i2s) {}

    void setAudioInfo(AudioInfo info) override;
    AudioInfo audioInfo() override {
      return lastInfo;
    }
  };

  InfoHandler* infoHandler = nullptr;

  static void printMetaData(MetaDataType type, const char* str, int len);
  std::vector<String> fileList;  // List of matching audio files
  int currentFileIndex = 0;      // Index of the currently playing file

  bool paused = false;
  bool lastCommandWasStop = false;

  void playCurrentFile();  // Helper to play file at currentFileIndex
};

