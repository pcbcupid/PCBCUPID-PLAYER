#include "PCBCUPID_PLAYERS.h"

unsigned long PCBCUPID_PLAYERS::trackDurationSec = 0;

unsigned long trackStartTime = 0;
unsigned long trackElapsed = 0;
unsigned long trackDuration = 0;
unsigned long totalPaused = 0;



PCBCUPID_PLAYERS::PCBCUPID_PLAYERS(TwoWire &w, uint8_t a)
    : wire(w), addr(a), amp(w, a) {}

PCBCUPID_PLAYERS::~PCBCUPID_PLAYERS()
{
  if (player)
  {
    delete player;
    player = nullptr;
  }
  if (source)
  {
    delete source;
    source = nullptr;
  }
  if (infoHandler)
  {
    delete infoHandler;
    infoHandler = nullptr;
  }
}

void PCBCUPID_PLAYERS::begin(const char *ext)
{
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  delay(300);

  amp.powerOn();

  // --- Initialize SD ---
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS, SPI))
  {
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
  source->begin(); // No check needed; it's void

  // Collect matching files manually
  File dir = SD.open(AUDIO_START_PATH);
  if (!dir || !dir.isDirectory())
  {
    Serial.println("Invalid audio directory!");
    return;
  }

  fileList.clear();
  File file;
  while ((file = dir.openNextFile()))
  {
    String fname = file.name();
    if (!file.isDirectory() && fname.endsWith(ext))
    {
      fileList.push_back(String(AUDIO_START_PATH) + "/" + fname);
    }
    file.close();
  }

  if (fileList.empty())
  {
    Serial.println("No audio files found!");
    return;
  }

  currentFileIndex = 0;

  // --- Decoder selection ---
  AudioDecoder *decoder = nullptr;
  if (strcasecmp(ext, "mp3") == 0)
  {
    decoder = &mp3;
  }
  else if (strcasecmp(ext, "wav") == 0)
  {
    decoder = &wav;
  }
  else if (strcasecmp(ext, "ogg") == 0)
  {
    decoder = &ogg;
  }
  else if (strcasecmp(ext, "aac") == 0)
  {
    decoder = &aac;
  }
  else
  {
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

void PCBCUPID_PLAYERS::loop()
{
  if (player)
  {
    player->copy();

    if (player->isActive())
    {
       trackElapsed = millis() - trackStartTime;
    }
  }
}

void PCBCUPID_PLAYERS::pause()
{
  if (!player)
    return;
  player->stop(); // simulate pause
  paused = true;
  currentState = PAUSED;
}

void PCBCUPID_PLAYERS::play()
{
  if (!player)
    return;

  if (lastCommandWasStop)
  {
    // Force fresh start
    playCurrentFile();
    paused = false;
    currentState = PLAYING;
    lastCommandWasStop = false; // reset flag
    return;
  }

  if (paused && currentState == PAUSED)
  {
    player->play(); // resume
    paused = false;
    currentState = PLAYING;
  }
  else if (currentState != PLAYING)
  {
    playCurrentFile(); // fallback
    paused = false;
    currentState = PLAYING;
  }
}

void PCBCUPID_PLAYERS::stop()
{
  if (player)
  {
    player->stop();
  }
  paused = false;
  currentState = STOPPED;
  lastCommandWasStop = true;
}

void PCBCUPID_PLAYERS::next()
{
  if (!fileList.empty())
  {
    currentFileIndex = (currentFileIndex + 1) % fileList.size();
    playCurrentFile();
  }
}

void PCBCUPID_PLAYERS::setVolume(float volume)
{
  if (player)
    player->setVolume(volume);
}

float PCBCUPID_PLAYERS::getVolume()
{
  return 0.0f; // AudioPlayer does not support getVolume() currently
}

void PCBCUPID_PLAYERS::setAutoFade(bool enable)
{
  if (player)
    player->setAutoFade(enable);
}

void PCBCUPID_PLAYERS::previous()
{
  if (!fileList.empty())
  {
    currentFileIndex = (currentFileIndex == 0)
                           ? fileList.size() - 1
                           : currentFileIndex - 1;
    playCurrentFile();
  }
}

void PCBCUPID_PLAYERS::InfoHandler::setAudioInfo(AudioInfo info)
{
  Serial.printf("Audio Changed: fs=%d, bits=%d\n", info.sample_rate, info.bits_per_sample);
  amp.begin(info.sample_rate, info.bits_per_sample);
  amp.powerOn();
  i2s.setAudioInfo(info);
  lastInfo = info;
}

void PCBCUPID_PLAYERS::printMetaData(MetaDataType type, const char *str, int len)
{
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);

  if (strncmp(str, "TLEN=", 5) == 0)
  {
    int durationMs = atoi(str + 5); // Skip "TLEN="
    trackDurationSec = durationMs / 1000;
    Serial.printf("Parsed TLEN duration: %lu seconds (from ID3 TLEN tag)\n", trackDurationSec);
  }
}

// Helper to play current file from fileList
void PCBCUPID_PLAYERS::playCurrentFile()
{
  if (!fileList.empty() && player && source)
  {
    String filepath = fileList[currentFileIndex];
    Serial.printf("Playing: %s\n", filepath.c_str());

    source->setIndex(currentFileIndex); // set current file index
    player->begin(currentFileIndex);    // begin player at index
    // amp.powerOn();
    Serial.println("Amp power ON");
    // Estimate duration based on source size and bitrate
    // Estimate duration based on source size and bitrate
    AudioInfo info = player->audioInfo();
    int bitrate = info.sample_rate * info.bits_per_sample * info.channels; // bits per second
    if (bitrate > 0 && source)
    {
      size_t fileSize = source->size(); // bytes
      trackDurationSec = (fileSize * 8) / bitrate;
    }
    else
    {
      trackDurationSec = 240; // fallback
    }
    trackElapsed = 0;

    player->play();                          // start playback
    unsigned long trackStartTime = millis(); // use the global variable, don't redeclare
    trackDurationSec = 0;                    // reset before each new track
    unsigned long pauseStart = 0;
    unsigned long totalPaused = 0;
    bool wasPaused = false;

    currentState = PLAYING;
  }
}

void PCBCUPID_PLAYERS::playCurrent()
{
  if (fileList.empty())
    return;
  playCurrentFile(); // existing player call
}

String PCBCUPID_PLAYERS::getCurrentFileName()
{
  if (fileList.empty())
    return "";
  return fileList[currentFileIndex];
}

bool PCBCUPID_PLAYERS::isPlaying()
{
  return player && player->isActive();
}

audio_tools::AudioPlayer *PCBCUPID_PLAYERS::getAudioPlayer()
{
  return player;
}

AudioInfo PCBCUPID_PLAYERS::audioInfo()
{
  return infoHandler ? infoHandler->lastInfo : AudioInfo();
}

unsigned long PCBCUPID_PLAYERS::currentPosition()
{
  if (!player)
    return 0;

  AudioInfo info = infoHandler->lastInfo;
  unsigned long elapsed = (millis() - trackStartTime) / 1000;

  unsigned long bytesPerSecond = (info.sample_rate * info.bits_per_sample * info.channels) / 8;
  return bytesPerSecond * elapsed;
}

unsigned long PCBCUPID_PLAYERS::totalSize()
{
  String filename = getCurrentFileName();
  File f = SD.open(filename);
  if (!f)
    return 1; // avoid divide-by-zero
  unsigned long size = f.size();
  f.close();
  return size;
}

void PCBCUPID_PLAYERS::togglePlayPause()
{
  if (!player)
    return;

  if (player->isActive())
  {
    pause();
  }
  else
  {
    play();
  }
}

// unsigned long PCBCUPID_PLAYERS::getCurrentTrackDuration()
// {
//   if (!player || fileList.empty())
//     return 0;

//   AudioInfo info = player->audioInfo();
//   if (info.bits_per_sample == 0 || info.sample_rate == 0)
//     return 0;

//   String filePath = "/" + fileList[currentFileIndex];
//   File file = SD.open(filePath.c_str());
//   if (!file)
//     return 0;

//   size_t fileSize = file.size();
//   file.close();

//   int bitrate = info.sample_rate * info.bits_per_sample * info.channels;

//   if (bitrate == 0)
//     return 0;

//   unsigned long durationSeconds = (fileSize * 8UL) / bitrate;
//   return durationSeconds;
// }
