#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <SPI.h>
#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSD.h>
#include <AudioTools/CoreAudio/AudioPlayer.h>
#include <AudioTools/AudioCodecs/CodecMP3Helix.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>
#include <AudioTools/AudioCodecs/CodecOpusOgg.h>
#include <AudioTools/AudioCodecs/CodecAACHelix.h>
#include "Driver.h"

namespace audio_tools {}  // Forward declare the namespace
using namespace ::audio_tools;  // Use the ROOT namespace

// Configuration structure for pins
struct AudioPlayerConfig
{
    // SD Card SPI Configuration
    int8_t sd_cs = 1;
    int8_t sd_miso = 3;
    int8_t sd_mosi = 2;
    int8_t sd_sck = 11;

    // I2S Pins for NAU8325
    int8_t i2s_mclk = 22;
    int8_t i2s_bclk = 25;
    int8_t i2s_ws = 24;
    int8_t i2s_dout = 23;

    // Default File Path
    const char *audio_start_path = "/";
};

// Forward declarations
namespace audio_tools
{
    class I2SStream;
    class AudioBoardStream;
}

enum PlayerState
{
    STOPPED,
    PLAYING,
    PAUSED
};

typedef void (*TrackChangeCallback)(const char *title, uint32_t duration);

class AUDIO_PLAYER
{
public:
    AUDIO_PLAYER(TwoWire &wire, const AudioPlayerConfig &cfg = AudioPlayerConfig());
    ~AUDIO_PLAYER();

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
    int getVolume() const { return int(volume * 100.0f); }
    void setAutoFade(bool enable);

    // Track listing
    String listTracks();

    // Track navigation
    bool playTrackIndex(int index);
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
    AudioPlayer *audioPlayer();
    AudioInfo audioInfo() const;
    AudioDecoder *getAudioDecoder(const char *ext);

    // Amplifier control
    void powerOnAmplifier() { nau8325_control.powerOn(); }
    void powerOffAmplifier() { nau8325_control.powerOff(); }

    // Track management
    std::vector<String> trackList;
    int trackIndex = 0;

    int currentTrackIndex() const { return trackIndex; } // UI uses it

    /*to retrieve track name by index*/
    String getTrackNameAt(int index) const
    {
        if (index >= 0 && index < (int)trackList.size())
        {
            return trackList[index];
        }
        return "";
    }

    void setTrackChangeCallback(TrackChangeCallback cb);
    void update();

private:
    // Internal state
    enum PlayerState
    {
        STOPPED,
        PLAYING,
        PAUSED
    };
    PlayerState state = STOPPED;
    float volume = 0.5f; // Default volume (0.0 to 1.0)

    // Timing
    unsigned long playStartMillis = 0;
    unsigned long pauseStartMillis = 0;
    unsigned long totalPausedMillis = 0;
    bool paused = false;
    bool stopped = true;
    bool lastCommandWasStop = false;

    // Audio components
    I2SStream *i2s = nullptr;
    AudioBoardStream *nau8325 = nullptr;
    AudioSourceSD *source = nullptr;
    AudioPlayer *player = nullptr;

    // Info handler
    class InfoHandler : public AudioInfoSupport
    {
        I2SStream *i2s = nullptr;
        AudioBoardStream *nau8325 = nullptr;
        PCBCUPID_NAU8325 *nau8325_control = nullptr;

    public:
        AudioInfo lastInfo;
        InfoHandler(AudioBoardStream *a, I2SStream *i, PCBCUPID_NAU8325 *control)
            : nau8325(a), i2s(i), nau8325_control(control) {}
        void setAudioInfo(AudioInfo info) override;
        AudioInfo audioInfo() override { return lastInfo; }
    } *infoHandler = nullptr;

    // Internal methods
    void buildPlaylist(const char *ext);
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

    TrackChangeCallback trackChangeCb = nullptr;
    void notifyTrackChange();

    AudioPlayerConfig config;
    TwoWire &wire;
    MP3DecoderHelix mp3;
    WAVDecoder wav;
    OpusOggDecoder ogg;
    AACDecoderHelix aac;
    PCBCUPID_NAU8325 nau8325_control;
};
