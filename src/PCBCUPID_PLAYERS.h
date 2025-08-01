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


extern unsigned long trackStartTime;
extern unsigned long trackElapsed;
extern unsigned long trackDuration;
extern unsigned long totalPaused;
extern unsigned long pauseTime;

extern unsigned long pauseStart;
extern bool wasPaused; // remove UI related components

class PCBCUPID_PLAYERS
{
public:
  PCBCUPID_PLAYERS(TwoWire &wire);

  ~PCBCUPID_PLAYERS(); // Destructor

  unsigned long trackDurationSec = 0; // make private

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
  void playTrackIndex(int index); //play the song based on the File index
  int getTrackIndex();
  void playTrack(String TrackName); // play the song directly
  void playCurrentTrack(); //change the logic call the playcurrentFile directly from public
  String getCurrentTrackName();
  bool isTrackPlaying();
  void getCurrentTrackTime();
  void getTotalTrackTime();
  audio_tools::AudioPlayer *getAudioPlayer();
  AudioInfo audioInfo();
  
  // // remove UI files
  // unsigned long currentPositionFromSD(); // remove UI Functions
  // unsigned long getFileSize(); // getFile size
  // unsigned long pausedPlayedtime(); // use elapsed time
  // void resetPlayTime(); // make private 

  unsigned long trackElapsedSec()
  {
    return trackElapsed / 1000;
  }
  unsigned long getTrackElapsedMillis() const { return trackElapsed; }

  bool isPaused() const
  {
    return paused;
  }

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

  PCBCUPID_NAU8325 amp; //use nau8325 variable name

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

  unsigned long calculateTrackDuration(const String &filename);
  int parseMP3Bitrate(const uint8_t *header);
  unsigned long parseWAVDuration(File &file);
  unsigned long estimateBitrateBased(File &file, size_t fileSize);
  unsigned long parseOGGDuration(File &file);

  bool paused = false;
  bool lastCommandWasStop = false;
  unsigned long trackElapsed = 0; // track time in ms
  unsigned long playStartMillis = 0;
  unsigned long manualElapsedOverride = 0;

  void playCurrentFile(); // Helper to play file at currentFileIndex
};
