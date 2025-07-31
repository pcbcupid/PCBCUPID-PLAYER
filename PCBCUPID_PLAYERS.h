#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSD.h>
#include <AudioTools/CoreAudio/AudioPlayer.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>
#include <AudioTools/AudioCodecs/CodecOpusOgg.h>
#include <AudioTools/AudioCodecs/CodecAACHelix.h>
#include <AudioTools/CoreAudio/AudioPlayer.h>

#include "config.h"
#include "Driver.h"

enum PlayerState
{
  STOPPED,
  PLAYING,
  PAUSED
};

extern unsigned long trackStartTime;
extern unsigned long trackElapsed;
extern unsigned long trackDuration;
extern unsigned long totalPaused;
extern unsigned long pauseTime;

extern unsigned long pauseStart;
extern bool wasPaused;

class PCBCUPID_PLAYERS
{
public:
  PCBCUPID_PLAYERS(TwoWire &wire, uint8_t addr); // remove address

  ~PCBCUPID_PLAYERS(); // Destructor

  void begin(const char *ext); // example: "mp3", "wav", "aac", "ogg"
  void loop();

  // New audio control APIs
  void play();
  void stop();
  void next();
  void setVolume(float volume);
  float getVolume(); // Returns maximum volume
  void setAutoFade(bool enable);
  void previous();
  void pause();
  void playCurrent();
  String getCurrentFileName();
  bool isPlaying();
  audio_tools::AudioPlayer *getAudioPlayer();
  AudioInfo audioInfo();
  unsigned long currentPosition(); // change the name to currentPositionFromSD
  unsigned long totalSize();
  void togglePlayPause(); // remove this function and add it on soundpod
  unsigned long trackDurationSec = 0;
  unsigned long secondsPlayed(); // change the name to pausedTime()
  unsigned long calculateTrackDuration(const String &filename);

  // private function
  int parseMP3Bitrate(const uint8_t *header);
  unsigned long parseWAVDuration(File &file);
  unsigned long estimateBitrateBased(File &file, size_t fileSize);
  unsigned long parseOGGDuration(File &file);

  unsigned long trackElapsedSec()
  {
    return trackElapsed / 1000;
  }
  unsigned long getTrackElapsedMillis() const { return trackElapsed; }

  bool isPaused() const
  {
    return paused;
  }

  //private
  bool wasJustStopped() const
  {
    return lastCommandWasStop;
  }

  std::vector<String> fileList; // List of matching audio files
  int currentFileIndex = 0;     // Index of the currently playing file

private:
  TwoWire &wire;
  uint8_t addr;

  I2SStream i2s;
  AudioPlayer *player = nullptr;
  AudioSourceSD *source = nullptr;
  PlayerState currentState = STOPPED;

  MP3DecoderHelix mp3;
  WAVDecoder wav;
  OpusOggDecoder ogg;
  AACDecoderHelix aac;

  PCBCUPID_NAU8325 amp;

  class InfoHandler : public AudioInfoSupport
  {
  public:
    PCBCUPID_NAU8325 &amp;
    I2SStream &i2s;
    AudioInfo lastInfo;

    InfoHandler(PCBCUPID_NAU8325 &amp, I2SStream &i2s)
        : amp(amp), i2s(i2s) {}

    void setAudioInfo(AudioInfo info) override;
    AudioInfo audioInfo() override
    {
      return lastInfo;
    }
  };

  InfoHandler *infoHandler = nullptr;

  static void printMetaData(MetaDataType type, const char *str, int len);
  bool paused = false;
  bool lastCommandWasStop = false;
  unsigned long trackElapsed = 0; // track time in ms

  void playCurrentFile(); // Helper to play file at currentFileIndex
};
