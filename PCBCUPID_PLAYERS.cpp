#include "PCBCUPID_PLAYERS.h"

PCBCUPID_PLAYERS::PCBCUPID_PLAYERS(TwoWire& w, uint8_t a)
  : wire(w), addr(a), amp(w, a) {}

void PCBCUPID_PLAYERS::begin(const char* ext) {
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

    delay(300);

    amp.powerOn();

    // --- Initialize SD ---
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS, SPI)) {
        Serial.println("SD mount failed!");
        while (true);
    }

    // --- Setup I2S Output ---
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    cfg.channels = 2;
    cfg.pin_bck = I2S_BCLK;
    cfg.pin_ws  = I2S_WS;
    cfg.pin_data = I2S_DOUT;
    cfg.pin_mck  = I2S_MCLK;
    i2s.begin(cfg);

    // --- Setup Source ---
    source = new AudioSourceSD(AUDIO_START_PATH, ext, SD_CS);
    source->begin();  // No check needed; it's void

    // --- Decoder selection ---
    AudioDecoder* decoder = nullptr;
    if (strcasecmp(ext, "mp3") == 0) {
        decoder = &mp3;
    } else if (strcasecmp(ext, "wav") == 0) {
        decoder = &wav;
    } else if (strcasecmp(ext, "ogg") == 0) {
        decoder = &ogg;
    } else if (strcasecmp(ext, "aac") == 0) {
        decoder = &aac;
    } else {
        Serial.printf("Unsupported extension: %s\n", ext);
        return;
    }

    // --- Player Setup ---
    player = new AudioPlayer(*source, i2s, *decoder);

    infoHandler = new InfoHandler(amp, i2s);
    player->setMetadataCallback(printMetaData);
    player->addNotifyAudioChange(infoHandler);

    player->begin();
}

void PCBCUPID_PLAYERS::loop() {
    if (player) {
        player->copy();
    }
}

void PCBCUPID_PLAYERS::InfoHandler::setAudioInfo(AudioInfo info) {
    Serial.printf("Audio Changed: fs=%d, bits=%d\n", info.sample_rate, info.bits_per_sample);
    amp.begin(info.sample_rate, info.bits_per_sample);
    i2s.setAudioInfo(info);
    lastInfo = info;
}

void PCBCUPID_PLAYERS::printMetaData(MetaDataType type, const char* str, int len) {
    Serial.print("==> ");
    Serial.print(toStr(type));
    Serial.print(": ");
    Serial.println(str);
}



// #include "PCBCUPID_PLAYERS.h"

// PCBCUPID_PLAYERS::PCBCUPID_PLAYERS(TwoWire& w, uint8_t a)
//   : wire(w), addr(a), amp(w, a) {}

// void PCBCUPID_PLAYERS::begin(const char* ext) {
//     AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

//     delay(300);

//     amp.powerOn(); 

//     // --- Initialize SD ---
//     SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
//     if (!SD.begin(SD_CS, SPI)) {
//         Serial.println("SD mount failed!");
//         while (true);
//     }

//     // --- Setup I2S Output ---
//     auto cfg = i2s.defaultConfig(TX_MODE);
//     cfg.sample_rate = 44100;       // placeholder
//     cfg.bits_per_sample = 16;      // placeholder
//     cfg.channels = 2;
//     cfg.pin_bck = I2S_BCLK;
//     cfg.pin_ws  = I2S_WS;
//     cfg.pin_data = I2S_DOUT;
//     cfg.pin_mck  = I2S_MCLK;
//     i2s.begin(cfg);

//     // --- Setup Source ---
//     source = new AudioSourceSD(AUDIO_START_PATH, ext);

//     // --- Use appropriate decoder without Decoder* ---
//     if (strcasecmp(ext, "mp3") == 0) {
//         player = new AudioPlayer(*source, i2s, mp3);
//     } else if (strcasecmp(ext, "wav") == 0) {
//         player = new AudioPlayer(*source, i2s, wav);
//     } else if (strcasecmp(ext, "ogg") == 0) {
//         player = new AudioPlayer(*source, i2s, ogg);
//     } else if (strcasecmp(ext, "aac") == 0) {
//         player = new AudioPlayer(*source, i2s, aac);
//     } else {
//         Serial.printf("Unsupported extension: %s\n", ext);
//         return;
//     }

//     // --- Metadata + Audio Info Callback ---
//     infoHandler = new InfoHandler(amp, i2s);
//     player->setMetadataCallback(printMetaData);
//     player->addNotifyAudioChange(infoHandler);

//     player->begin();
// }

// void PCBCUPID_PLAYERS::loop() {
//     if (player) {
//         player->copy();
//     }
// }

// void PCBCUPID_PLAYERS::InfoHandler::setAudioInfo(AudioInfo info) {
//     Serial.printf("Audio Changed: fs=%d, bits=%d\n", info.sample_rate, info.bits_per_sample);
//     amp.begin(info.sample_rate, info.bits_per_sample);
//     i2s.setAudioInfo(info);
//     lastInfo = info;
// }

// void PCBCUPID_PLAYERS::printMetaData(MetaDataType type, const char* str, int len) {
//     Serial.print("==> ");
//     Serial.print(toStr(type));
//     Serial.print(": ");
//     Serial.println(str);
// }
