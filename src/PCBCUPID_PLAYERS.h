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
#include "config.h"
#include "Driver.h"

// Forward declarations
namespace audio_tools {
    class I2SStream;
    class AudioBoardStream;
}

enum PlayerState {
  STOPPED,
  PLAYING,
  PAUSED
};

class PCBCUPID_PLAYERS
{
public:
    PCBCUPID_PLAYERS(TwoWire &wire);
    ~PCBCUPID_PLAYERS();

    // Initialization and main loop
    void begin(const char *ext);
    void loop();

    // Playback controls
    void play();
    void pause();
    void stop();
    void next();
    void previous();

    // Volume control
    void setVolume(float volume);
    float getVolume() const;
    void setAutoFade(bool enable);

    // Track navigation
    bool playTrack(int index);
    int currentTrackIndex() const;
    int getTrackCount() const { return trackList.size(); }
    int trackCount() const { return trackList.size(); }

    // Player state
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    String currentTrackName() const;
    unsigned long currentTrackElapsedSeconds() const;
    unsigned long currentTrackDurationSeconds() const;

    // Audio info
    audio_tools::AudioPlayer* audioPlayer();
    AudioInfo audioInfo() const;
    AudioDecoder* getAudioDecoder(const char *ext);

private:
    // Internal state
    enum PlayerState { STOPPED, PLAYING, PAUSED };
    PlayerState state = STOPPED;
    float volume = 0.5f;  // Default volume (0.0 to 1.0)
    
    // Track management
    std::vector<String> trackList;
    int trackIndex = 0;

    // Timing
    unsigned long playStartMillis = 0;
    unsigned long pauseStartMillis = 0;
    unsigned long totalPausedMillis = 0;
    bool paused = false;
    bool stopped = true;
    bool lastCommandWasStop = false;

    // Audio components
    audio_tools::I2SStream* i2s = nullptr;
    audio_tools::AudioBoardStream* nau8325 = nullptr;
    AudioSourceSD* source = nullptr;
    audio_tools::AudioPlayer* player = nullptr;
    
    // Info handler
    class InfoHandler : public AudioInfoSupport {
        audio_tools::I2SStream* i2s = nullptr;
        audio_tools::AudioBoardStream* nau8325 = nullptr;
        PCBCUPID_NAU8325* nau8325_control = nullptr;
    public:
        AudioInfo lastInfo;
        InfoHandler(audio_tools::AudioBoardStream* a, audio_tools::I2SStream* i, PCBCUPID_NAU8325* control) 
            : nau8325(a), i2s(i), nau8325_control(control) {}
        void setAudioInfo(AudioInfo info) override;
        AudioInfo audioInfo() override { return lastInfo; }  
    } *infoHandler = nullptr;

    // Internal methods
    void buildPlaylist(const char *ext);
    void playTrackAtIndex(int index);
    void playCurrentTrack();
    void resetPlayTime();
    
    // Duration/bitrate helpers
    unsigned long calculateTrackDuration(const String &filename) const;
    unsigned long parseWAVDuration(File &file) const;
    unsigned long parseOGGDuration(File &file) const;
    unsigned long estimateBitrateBased(File &file, size_t fileSize) const;
    int parseMP3Bitrate(const uint8_t *header) const;
    
    // Internal timing methods
    unsigned long currentTrackElapsedMillis() const;
    unsigned long currentTrackDurationMillis() const;
    unsigned long currentTrackPositionBytes() const;
    unsigned long currentTrackTotalBytes() const;
    
    // Metadata callback
    static void printMetaData(MetaDataType type, const char *str, int len);
    
    TwoWire &wire;
    MP3DecoderHelix mp3;
    WAVDecoder wav;
    OpusOggDecoder ogg;
    AACDecoderHelix aac;
    PCBCUPID_NAU8325 nau8325_control;  

};
