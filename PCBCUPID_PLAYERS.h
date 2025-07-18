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

class PCBCUPID_PLAYERS {
public:
    PCBCUPID_PLAYERS(TwoWire& wire, uint8_t addr);
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


private:
    TwoWire& wire;
    uint8_t addr;

    I2SStream i2s;
    AudioPlayer* player = nullptr;
    AudioSourceSD* source = nullptr;

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
        AudioInfo audioInfo() override { return lastInfo; }
    };

    InfoHandler* infoHandler = nullptr;

    static void printMetaData(MetaDataType type, const char* str, int len);
};
