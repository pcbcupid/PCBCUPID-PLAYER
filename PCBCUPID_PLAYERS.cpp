#include "PCBCUPID_PLAYERS.h"

// unsigned long PCBCUPID_PLAYERS::trackDurationSec = 0;

unsigned long trackStartTime = 0;
unsigned long trackElapsed = 0;
unsigned long trackDuration = 0;
unsigned long totalPaused = 0;
unsigned long pauseTime = 0;

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
  pauseTime = millis(); // Capture when we paused
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
    // Add paused duration to totalPaused
    unsigned long resumeTime = millis();
    totalPaused += resumeTime - pauseTime;
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

  // if (strncmp(str, "TLEN=", 5) == 0)
  // {
  //   int durationMs = atoi(str + 5); // Skip "TLEN="
  //   trackDurationSec = durationMs / 1000;
  //   Serial.printf("Parsed TLEN duration: %lu seconds (from ID3 TLEN tag)\n", trackDurationSec);
  // }
}

// Helper to play current file from fileList

void PCBCUPID_PLAYERS::playCurrentFile()
{
  if (fileList.empty() || !player || !source)
    return;

  String filepath = fileList[currentFileIndex];
  source->setIndex(currentFileIndex);
  player->begin(currentFileIndex);

  trackStartTime = millis();
  totalPaused = 0;
  paused = false;
  currentState = PLAYING;

  this->trackDurationSec = calculateTrackDuration(filepath);
  if (trackDurationSec == 0)
    trackDurationSec = 180; // fallback only if not parsed

  AudioInfo info = player->audioInfo();
  amp.begin(info.sample_rate, info.bits_per_sample);
  amp.powerOn();

  player->play();
}
// void PCBCUPID_PLAYERS::playCurrentFile()
// {
//   if (!fileList.empty() && player && source)
//   {
//     String filepath = fileList[currentFileIndex];
//     Serial.printf("Playing: %s\n", filepath.c_str());

//     source->setIndex(currentFileIndex); // set current file index
//     player->begin(currentFileIndex);    // begin player at index
//     // amp.powerOn();
//     Serial.println("Amp power ON");
//     // Estimate duration based on source size and bitrate
//     // Estimate duration based on source size and bitrate
//     AudioInfo info = player->audioInfo();
//     int bitrate = info.sample_rate * info.bits_per_sample * info.channels; // bits per second
//     if (bitrate > 0 && source)
//     {
//       size_t fileSize = source->size(); // bytes
//       trackDurationSec = (fileSize * 8) / bitrate;
//     }
//     else
//     {
//       trackDurationSec = 240; // fallback
//     }
//     trackElapsed = 0;

//     player->play();                          // start playback
//     unsigned long trackStartTime = millis(); // use the global variable, don't redeclare
//     trackDurationSec = 0;                    // reset before each new track
//     unsigned long pauseStart = 0;
//     unsigned long totalPaused = 0;
//     bool wasPaused = false;

//     currentState = PLAYING;
//   }
// }

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

unsigned long PCBCUPID_PLAYERS::secondsPlayed()
{
  if (currentState == STOPPED)
    return 0;

  if (currentState == PAUSED)
  {
    // When paused, return elapsed up to when it was paused
    return (pauseTime - trackStartTime - totalPaused) / 1000;
  }
  else
  {
    // While playing, use current millis
    return (millis() - trackStartTime - totalPaused) / 1000;
  }
}

// Parses MP3 duration via ID3 TLEN or VBR header
unsigned long PCBCUPID_PLAYERS::calculateTrackDuration(const String &filename)
{
  File f = SD.open(filename);
  if (!f)
    return 0;
  size_t fileSize = f.size();

  unsigned long duration = 0;
  if (filename.endsWith(".mp3"))
  {
    // Skip ID3v2
    uint8_t hdr[10];
    f.read(hdr, 10);
    if (hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3')
    {
      uint32_t sz = ((hdr[6] & 0x7F) << 21) | ((hdr[7] & 0x7F) << 14) | ((hdr[8] & 0x7F) << 7) | (hdr[9] & 0x7F);
      f.seek(10 + sz);
    }
    else
    {
      f.seek(0);
    }

    uint8_t frame[4];
    f.read(frame, 4);
    if (frame[0] == 0xFF && (frame[1] & 0xE0) == 0xE0)
    {
      int kbps = parseMP3Bitrate(frame);
      if (kbps > 0)
      {
        duration = (fileSize * 8UL) / (kbps * 1000UL);
      }
    }

    // Check for Xing/VBRI header
    f.read(frame, 4);
    String tag = String((char *)frame);
    if (tag == "Xing" || tag == "Info")
    {
      // Seek and extract total frames/count for better duration
      f.read(frame, 4);
      uint32_t frames = (frame[0] << 24) | (frame[1] << 16) | (frame[2] << 8) | frame[3];
      if (frames > 0)
      {
        AudioInfo info = player->audioInfo();
        unsigned long spf = info.sample_rate / 1152; // samples per frame for MPEG
        duration = frames * spf / info.channels;
      }
    }
    else if (tag == "VBRI")
    {
      f.seek(f.position() + 14);
      uint8_t ct[4];
      f.read(ct, 4);
      uint32_t frames = (ct[0] << 24) | (ct[1] << 16) | (ct[2] << 8) | ct[3];
      AudioInfo info = player->audioInfo();
      unsigned long spf = info.sample_rate / 1152;
      duration = frames * spf / info.channels;
    }
  }
  else if (filename.endsWith(".wav"))
  {
    duration = parseWAVDuration(f);
  }
  else if (filename.endsWith(".ogg"))
  {
    duration = parseOGGDuration(f);
  }
  else
  {
    duration = estimateBitrateBased(f, fileSize);
  }
  f.close();
  return duration;
}

int PCBCUPID_PLAYERS::parseMP3Bitrate(const uint8_t *header)
{
  const int bitrateTable[2][3][16] = {
      {// MPEG Version 2 & 2.5
       {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
       {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
       {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}},
      {// MPEG Version 1
       {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
       {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
       {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}}};

  uint8_t versionBits = (header[1] >> 3) & 0x03;
  uint8_t layerBits = (header[1] >> 1) & 0x03;
  uint8_t bitrateBits = (header[2] >> 4) & 0x0F;

  int versionIndex = (versionBits == 3) ? 1 : 0; // 1 = MPEG1, 0 = MPEG2/2.5
  int layerIndex = (layerBits == 3) ? 0 : (3 - layerBits);

  return bitrateTable[versionIndex][layerIndex][bitrateBits]; // in kbps
}

unsigned long PCBCUPID_PLAYERS::parseWAVDuration(File &file)
{
  file.seek(24);
  uint32_t sampleRate = 0;
  file.read((uint8_t *)&sampleRate, 4);

  file.seek(22);
  uint16_t channels = 0;
  file.read((uint8_t *)&channels, 2);

  file.seek(34);
  uint16_t bitsPerSample = 0;
  file.read((uint8_t *)&bitsPerSample, 2);

  file.seek(40);
  uint32_t dataSize = 0;
  file.read((uint8_t *)&dataSize, 4);

  uint32_t byteRate = sampleRate * bitsPerSample * channels / 8;
  return byteRate > 0 ? dataSize / byteRate : 0;
}

unsigned long PCBCUPID_PLAYERS::estimateBitrateBased(File &file, size_t fileSize)
{
  file.seek(0);
  unsigned long avgBitrate = 128000; // assume 128kbps default
  return (fileSize * 8UL) / avgBitrate;
}

unsigned long PCBCUPID_PLAYERS::parseOGGDuration(File &file)
{
  // Heuristic: look for "OggS" at end of file and extract granule position
  size_t fileSize = file.size();
  const int scanSize = 2048;

  for (int i = fileSize - scanSize; i < (int)fileSize - 10; i++)
  {
    file.seek(i);
    char sig[4];
    file.read((uint8_t *)sig, 4);
    if (memcmp(sig, "OggS", 4) == 0)
    {
      file.seek(i + 6);
      uint64_t granule = 0;
      file.read((uint8_t *)&granule, 8);
      uint32_t sampleRate = 44100; // most common
      return granule / sampleRate;
    }
  }
  return 0;
}
