#include "PCBCUPID_PLAYERS.h"


PCBCUPID_PLAYERS::PCBCUPID_PLAYERS(TwoWire& w, uint8_t a)
  : wire(w), addr(a), amp(w, a) {}

PCBCUPID_PLAYERS::~PCBCUPID_PLAYERS() {
  if (player) {
    delete player;
    player = nullptr;
  }
  if (source) {
    delete source;
    source = nullptr;
  }
  if (infoHandler) {
    delete infoHandler;
    infoHandler = nullptr;
  }
}


void PCBCUPID_PLAYERS::begin(const char* ext) {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  delay(300);

  amp.powerOn();

  // --- Initialize SD ---
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("SD mount failed!");
    while (true)
      ;
  }

  // --- Setup I2S Output ---
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.sample_rate = 44100;
  cfg.bits_per_sample = 16;
  cfg.channels = 2;
  cfg.pin_bck = I2S_BCLK;
  cfg.pin_ws = I2S_WS;
  cfg.pin_data = I2S_DOUT;
  cfg.pin_mck = I2S_MCLK;
  i2s.begin(cfg);

  // --- Setup Source ---
  source = new AudioSourceSD(AUDIO_START_PATH, ext, SD_CS);
  source->begin();  // No check needed; it's void

  //Collect matching files manually
  File dir = SD.open(AUDIO_START_PATH);
  if (!dir || !dir.isDirectory()) {
    Serial.println("Invalid audio directory!");
    return;
  }

  fileList.clear();
  File file;
  while ((file = dir.openNextFile())) {
    String fname = file.name();
    if (!file.isDirectory() && fname.endsWith(ext)) {
      fileList.push_back(String(AUDIO_START_PATH) + "/" + fname);
    }
    file.close();
  }

  if (fileList.empty()) {
    Serial.println("No audio files found!");
    return;
  }

  currentFileIndex = 0;

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


void PCBCUPID_PLAYERS::pause() {
  if (!player) return;
  player->stop();  // simulate pause
  paused = true;
  currentState = PAUSED;
}

void PCBCUPID_PLAYERS::play() {
  if (!player) return;

  if (lastCommandWasStop) {
    // Force fresh start
    playCurrentFile();
    paused = false;
    currentState = PLAYING;
    lastCommandWasStop = false;  // reset flag
    return;
  }

  if (paused && currentState == PAUSED) {
    player->play();  // resume
    paused = false;
    currentState = PLAYING;
  } else if (currentState != PLAYING) {
    playCurrentFile();  // fallback
    paused = false;
    currentState = PLAYING;
  }
}

void PCBCUPID_PLAYERS::stop() {
  if (player) {
    player->stop();
  }
  paused = false;
  currentState = STOPPED;
  lastCommandWasStop = true; 
}

void PCBCUPID_PLAYERS::next() {
  if (!fileList.empty()) {
    currentFileIndex = (currentFileIndex + 1) % fileList.size();
    playCurrentFile();
  }
}

void PCBCUPID_PLAYERS::setVolume(float volume) {
  if (player) player->setVolume(volume);
}

float PCBCUPID_PLAYERS::getVolume() {
  return 0.0f;  // AudioPlayer does not support getVolume() currently
}

void PCBCUPID_PLAYERS::setAutoFade(bool enable) {
  if (player) player->setAutoFade(enable);
}

void PCBCUPID_PLAYERS::previous() {
  if (!fileList.empty()) {
    currentFileIndex = (currentFileIndex == 0)
                         ? fileList.size() - 1
                         : currentFileIndex - 1;
    playCurrentFile();
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

//Helper to play current file from fileList
void PCBCUPID_PLAYERS::playCurrentFile() {
  if (!fileList.empty() && player && source) {
    String filepath = fileList[currentFileIndex];
    Serial.printf("Playing: %s\n", filepath.c_str());

    source->setIndex(currentFileIndex);  // set current file index
    player->begin(currentFileIndex);     // begin player at index
    player->play();                      // start playback

    currentState = PLAYING;  
  }
}

