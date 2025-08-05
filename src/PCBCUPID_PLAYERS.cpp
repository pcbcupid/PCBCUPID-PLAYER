#include "PCBCUPID_PLAYERS.h"

PCBCUPID_PLAYERS::PCBCUPID_PLAYERS(TwoWire &w)
    : wire(w), nau8325_control(w), i2s(new audio_tools::I2SStream()) {}

PCBCUPID_PLAYERS::~PCBCUPID_PLAYERS() {
    // Clean up audio player
    if (player) {
        delete player;
        player = nullptr;
    }
    
    // Clean up audio source
    if (source) {
        delete source;
        source = nullptr;
    }
    
    // Clean up info handler
    if (infoHandler) {
        delete infoHandler;
        infoHandler = nullptr;
    }
}

/*Intialization*/

void PCBCUPID_PLAYERS::begin(const char *ext) {
    // Initialize logging
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
    delay(300);
    nau8325_control.powerOn();

    // Initialize SD card
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS, SPI)) {
        Serial.println("SD mount failed!");
        while (true); // Halt on SD card failure
    }

    // Build playlist
    buildPlaylist(ext);
    
    // Configure I2S
    auto cfg = i2s->defaultConfig(TX_MODE);
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    cfg.channels = 2;
    cfg.pin_bck = I2S_BCLK;
    cfg.pin_ws = I2S_WS;
    cfg.pin_data = I2S_DOUT;
    cfg.pin_mck = I2S_MCLK;
    i2s->begin(cfg);
    
    // Initialize audio source
    source = new AudioSourceSD(AUDIO_START_PATH, ext, SD_CS);
    
    // Initialize audio decoder based on file extension
    AudioDecoder *decoder = nullptr;
    String extStr = String(ext);
    extStr.toLowerCase();
    
    if (extStr == "mp3") {
        Serial.println("Initializing MP3 decoder...");
        decoder = &mp3;
    } else if (extStr == "wav") {
        Serial.println("Initializing WAV decoder...");
        decoder = &wav;
    } else if (extStr == "ogg") {
        Serial.println("Initializing OGG decoder...");
        decoder = &ogg;
    } else if (extStr == "aac" || extStr == "m4a") {
        Serial.println("Initializing AAC decoder...");
        decoder = &aac;
    } else {
        Serial.println("Unsupported file extension: " + String(ext));
    }

    if (!decoder) {
        Serial.println("No suitable decoder found for extension: " + String(ext));
        return;
    }
    
    // Initialize audio player
    player = new AudioPlayer(*source, *i2s, *decoder);
    infoHandler = new InfoHandler(nau8325, i2s, &nau8325_control);
    player->setMetadataCallback(printMetaData);
    player->addNotifyAudioChange(infoHandler);
    
    // Set initial volume
    player->setVolume(volume);
    
    // Begin playback
    player->begin();
    
    // Start playback
    playCurrentTrack();
}

/*Helper Method*/
void PCBCUPID_PLAYERS::buildPlaylist(const char *ext) {
    File dir = SD.open(AUDIO_START_PATH);
    if (!dir || !dir.isDirectory()) {
        Serial.println("Invalid audio directory!");
        return;
    }
    
    trackList.clear();
    File file;
    while ((file = dir.openNextFile())) {
        String fname = file.name();
        if (!file.isDirectory() && fname.endsWith("." + String(ext))) {
            trackList.push_back(String(AUDIO_START_PATH) + "/" + fname);
        }
        file.close();
    }
    dir.close();
}

AudioDecoder* PCBCUPID_PLAYERS::getAudioDecoder(const char *ext) {
    if (strcasecmp(ext, "mp3") == 0) return &mp3;
    if (strcasecmp(ext, "wav") == 0) return &wav;
    if (strcasecmp(ext, "ogg") == 0) return &ogg;
    if (strcasecmp(ext, "aac") == 0) return &aac;
    return nullptr;
}

/*Main Loop*/
void PCBCUPID_PLAYERS::loop() {
    if (player) {
        player->copy();
    }
}

/*Playback control*/
void PCBCUPID_PLAYERS::play() {
    if (!player) return;
    
    if (state == STOPPED) {
        // Start playing from the beginning if stopped
        playCurrentTrack();
        state = PLAYING;
        paused = false;
        stopped = false;
        lastCommandWasStop = false;
        return;
    }
    
    if (paused && state == PAUSED) {
        // Resume playback from pause
        unsigned long resumeTime = millis();
        totalPausedMillis += resumeTime - pauseStartMillis;
        player->play();
        paused = false;
        state = PLAYING;
    }
}

void PCBCUPID_PLAYERS::pause() {
    if (!player || paused) return;
    
    // Pause the current playback
    player->stop();
    paused = true;
    state = PAUSED;
    pauseStartMillis = millis();
}

void PCBCUPID_PLAYERS::stop() {
    if (player) {
        player->stop();
    }

    Serial.println("Playback stopped");

    // Reset playback state
    paused = false;
    stopped = true;
    state = STOPPED;
    lastCommandWasStop = true;
}


/*Track Navigation*/

void PCBCUPID_PLAYERS::next() {
    if (trackList.empty()) return;

    trackIndex = (trackIndex + 1) % trackList.size();
    playCurrentTrack();

    // Ensure correct state after new playback
    state = PLAYING;
    paused = false;
    stopped = false;
    lastCommandWasStop = false;
}


void PCBCUPID_PLAYERS::previous() {
    if (trackList.empty()) return;

    trackIndex = (trackIndex - 1 + trackList.size()) % trackList.size();
    playCurrentTrack();

    state = PLAYING;
    paused = false;
    stopped = false;
    lastCommandWasStop = false;
}



void PCBCUPID_PLAYERS::playTrackAtIndex(int index) {
    if (trackList.empty() || !player || !source)
        return;
    if (index < 0 || index >= (int)trackList.size())
        return;

    trackIndex = index;
    playCurrentTrack();

    state = PLAYING;
    paused = false;
    stopped = false;
    lastCommandWasStop = false;
}


bool PCBCUPID_PLAYERS::playTrack(int index) {
    // Check if the index is valid
    if (index < 0 || index >= (int)trackList.size()) {
        Serial.printf("Error: Invalid track index %d (valid range: 0-%d)\n", index, trackList.size() - 1);
        return false;
    }

    // Stop current playback if any
    if (player) {
        player->stop();
    }

    // Update the track index
    trackIndex = index;
  
    // Play the selected track
    playCurrentTrack();
  
    // Update the player state
    state = PLAYING;
    paused = false;
    stopped = false;
    lastCommandWasStop = false;
  
    return true;
}

/*Volume Control*/

void PCBCUPID_PLAYERS::setVolume(float vol) {
    volume = constrain(vol, 0.0f, 1.0f);
    if (player) {
        player->setVolume(volume);
    }
}

float PCBCUPID_PLAYERS::getVolume() const {
    return volume;
}

void PCBCUPID_PLAYERS::setAutoFade(bool enable) {
    if (player)
        player->setAutoFade(enable);
}

/*Status and Information*/
String PCBCUPID_PLAYERS::currentTrackName() const {
    return trackList.empty() ? "" : trackList[trackIndex];
}

bool PCBCUPID_PLAYERS::isPlaying() const {
    return player && player->isActive();
}

bool PCBCUPID_PLAYERS::isPaused() const {
    return state == PAUSED;
}

bool PCBCUPID_PLAYERS::isStopped() const {
    return state == STOPPED;
}

/*Playback Position and Duration*/
unsigned long PCBCUPID_PLAYERS::currentTrackElapsedMillis() const {
    if (state == STOPPED) return 0;
    if (state == PAUSED) return pauseStartMillis - playStartMillis - totalPausedMillis;
    return millis() - playStartMillis - totalPausedMillis;
}

unsigned long PCBCUPID_PLAYERS::currentTrackElapsedSeconds() const {
    return currentTrackElapsedMillis() / 1000;
}

unsigned long PCBCUPID_PLAYERS::currentTrackDurationMillis() const {
    return currentTrackDurationSeconds() * 1000;
}

unsigned long PCBCUPID_PLAYERS::currentTrackDurationSeconds() const {
    if (trackList.empty() || trackIndex < 0 || trackIndex >= trackList.size()) {
        return 0;
    }
    return calculateTrackDuration(trackList[trackIndex]);
}

/*Audio Information*/
unsigned long PCBCUPID_PLAYERS::currentTrackPositionBytes() const {
    if (!player) return 0;
    AudioInfo info = infoHandler->lastInfo;
    unsigned long elapsed = currentTrackElapsedSeconds();
    return (info.sample_rate * info.bits_per_sample * info.channels * elapsed) / 8;
}

unsigned long PCBCUPID_PLAYERS::currentTrackTotalBytes() const {
    String filename = currentTrackName();
    File f = SD.open(filename);
    if (!f) return 1;
    unsigned long size = f.size();
    f.close();
    return size;
}

audio_tools::AudioPlayer* PCBCUPID_PLAYERS::audioPlayer() {
    return player;
}

AudioInfo PCBCUPID_PLAYERS::audioInfo() const {
    return infoHandler ? infoHandler->lastInfo : AudioInfo();
}

void PCBCUPID_PLAYERS::resetPlayTime()
{
  playStartMillis = millis();
  totalPausedMillis = 0;
}

int PCBCUPID_PLAYERS::currentTrackIndex() const
{
  return trackIndex;
}

/*Track Playback (Internal)*/
void PCBCUPID_PLAYERS::playCurrentTrack() {
    if (trackList.empty() || trackIndex < 0 || trackIndex >= (int)trackList.size()) {
        Serial.println("Track index out of bounds.");
        return;
    }

    String filepath = trackList[trackIndex];
    if (!source || !player) {
        Serial.println("Player or source not initialized.");
        return;
    }

    Serial.print("Playing track: ");
    Serial.println(filepath);

    player->stop();

    source->setIndex(trackIndex);  // use index based source selection
    player->begin(trackIndex);     // begin playback from that index

    resetPlayTime();
    paused = false;
    stopped = false;
    state = PLAYING;

    unsigned long durationSec = calculateTrackDuration(filepath);
    if (durationSec == 0) {
        durationSec = 180;
    }

    AudioInfo info = player->audioInfo();
    nau8325_control.begin(info.sample_rate, info.bits_per_sample);
    nau8325_control.powerOn();

    player->play();
}

/*Audio Info Handlers*/
void PCBCUPID_PLAYERS::InfoHandler::setAudioInfo(AudioInfo info) {
    Serial.printf("Audio Changed: fs=%d, bits=%d\n", info.sample_rate, info.bits_per_sample);
    nau8325_control->begin(info.sample_rate, info.bits_per_sample);
    nau8325_control->powerOn();
    i2s->setAudioInfo(info);
    lastInfo = info;
}

void PCBCUPID_PLAYERS::printMetaData(MetaDataType type, const char *str, int len) {
    Serial.print("==> ");
    Serial.print(toStr(type));
    Serial.print(": ");
    Serial.println(str);
}

/*Duration Calculation*/

// Parses MP3 duration via ID3 TLEN or VBR header
unsigned long PCBCUPID_PLAYERS::calculateTrackDuration(const String &filename) const {
    File f = SD.open(filename);
    if (!f) return 0;
    
    size_t fileSize = f.size();
    unsigned long duration = 0;
    
    // Handle MP3 files
    if (filename.endsWith(".mp3")) {
        // Skip ID3v2 header if present
        uint8_t hdr[10];
        f.read(hdr, 10);
        if (hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3') {
            // Parse ID3v2 header size (stored as synchsafe integer)
            uint32_t sz = ((hdr[6] & 0x7F) << 21) | 
                         ((hdr[7] & 0x7F) << 14) | 
                         ((hdr[8] & 0x7F) << 7) | 
                         (hdr[9] & 0x7F);
            f.seek(10 + sz);
        } else {
            f.seek(0);
        }

        // Read MP3 frame header
        uint8_t frame[4];
        f.read(frame, 4);
        
        // Check for valid MP3 frame sync
        if (frame[0] == 0xFF && (frame[1] & 0xE0) == 0xE0) {
            int kbps = parseMP3Bitrate(frame);
            if (kbps > 0) {
                duration = (fileSize * 8UL) / (kbps * 1000UL);
            }
        }

        // Check for Xing/VBRI header for more accurate duration
        f.read(frame, 4);
        String tag = String((char *)frame);
        
        if (tag == "Xing" || tag == "Info") {
            // Seek and extract total frames/count for better duration
            f.read(frame, 4);
            uint32_t frames = (frame[0] << 24) | (frame[1] << 16) | (frame[2] << 8) | frame[3];
            if (frames > 0) {
                AudioInfo info = player->audioInfo();
                unsigned long spf = info.sample_rate / 1152; // samples per frame for MPEG
                duration = frames * spf / info.sample_rate;
            }
        } else if (tag == "VBRI") {
            f.seek(f.position() + 14);
            uint8_t ct[4];
            f.read(ct, 4);
            uint32_t frames = (ct[0] << 24) | (ct[1] << 16) | (ct[2] << 8) | ct[3];
            AudioInfo info = player->audioInfo();
            unsigned long spf = info.sample_rate / 1152;
            duration = frames * spf / info.sample_rate;
        }
    } 
    // Handle WAV files
    else if (filename.endsWith(".wav")) {
        duration = parseWAVDuration(f);
    }
    // Handle OGG files
    else if (filename.endsWith(".ogg")) {
        duration = parseOGGDuration(f);
    }
    // For other formats, use bitrate estimation
    else {
        duration = estimateBitrateBased(f, fileSize);
    }
    
    f.close();
    return duration;
}

/*MP3 Bitrate Parsing*/

/**
 * Parses MP3 bitrate from frame header
 * @param header Pointer to 4-byte MP3 frame header
 * @return Bitrate in kbps, or 0 if invalid
 */
int PCBCUPID_PLAYERS::parseMP3Bitrate(const uint8_t *header) const {
    // Bitrate lookup table [version][layer][bitrate_index]
    const int bitrateTable[2][3][16] = {
        {   // MPEG Version 2 & 2.5
            {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
            {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
            {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
        },
        {   // MPEG Version 1
            {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
            {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
            {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}
        }
    };

    // Extract header fields
    uint8_t versionBits = (header[1] >> 3) & 0x03;
    uint8_t layerBits = (header[1] >> 1) & 0x03;
    uint8_t bitrateBits = (header[2] >> 4) & 0x0F;

    // Determine version and layer indices
    int versionIndex = (versionBits == 3) ? 1 : 0; // 1 = MPEG1, 0 = MPEG2/2.5
    int layerIndex = (layerBits == 3) ? 0 : (3 - layerBits);

    return bitrateTable[versionIndex][layerIndex][bitrateBits]; // in kbps
}

/*WAV File Parsing*/

/**
 * Parses duration from WAV file header
 * @param file Reference to opened WAV file
 * @return Duration in seconds, or 0 if invalid
 */
unsigned long PCBCUPID_PLAYERS::parseWAVDuration(File &file) const {
    // Read sample rate
    file.seek(24);
    uint32_t sampleRate = 0;
    file.read((uint8_t *)&sampleRate, 4);
    if (sampleRate == 0) return 0;

    // Read number of channels
    file.seek(22);
    uint16_t channels = 0;
    file.read((uint8_t *)&channels, 2);

    // Read bits per sample
    file.seek(34);
    uint16_t bitsPerSample = 0;
    file.read((uint8_t *)&bitsPerSample, 2);

    // Read data size
    file.seek(40);
    uint32_t dataSize = 0;
    file.read((uint8_t *)&dataSize, 4);

    // Calculate duration
    uint32_t byteRate = sampleRate * (bitsPerSample / 8) * channels;
    return byteRate > 0 ? dataSize / byteRate : 0;
}

/*OGG File Parsing*/

/**
 * Estimates duration from OGG file by finding the last OggS page
 * @param file Reference to opened OGG file
 * @return Duration in seconds, or 0 if invalid
 */
unsigned long PCBCUPID_PLAYERS::parseOGGDuration(File &file) const {
    const size_t fileSize = file.size();
    const size_t scanSize = 2048; // How much of the end to scan for OggS
    const size_t minOffset = 32;   // Minimum possible OggS header size

    // Start scanning from the end of the file
    size_t startPos = (fileSize > scanSize) ? (fileSize - scanSize) : 0;
    
    for (size_t i = startPos; i < fileSize - minOffset; i++) {
        file.seek(i);
        
        // Look for OggS signature
        char sig[4];
        if (file.read((uint8_t *)sig, 4) != 4) continue;
        
        if (memcmp(sig, "OggS", 4) == 0) {
            // Found OggS header, read granule position
            file.seek(i + 6);
            uint64_t granule = 0;
            if (file.read((uint8_t *)&granule, 8) != 8) continue;
            
            // Use standard sample rate (most common is 44100)
            const uint32_t sampleRate = 44100;
            return (sampleRate > 0) ? (granule / sampleRate) : 0;
        }
    }
    
    return 0; // No valid OggS header found
}

/*Fallback Duration Estimation*/

/**
 * Estimates duration based on file size and average bitrate
 * @param file Reference to opened file
 * @param fileSize Size of the file in bytes
 * @return Estimated duration in seconds
 */
unsigned long PCBCUPID_PLAYERS::estimateBitrateBased(File &file, size_t fileSize) const {
    // Default to average bitrate of 192kbps for unknown formats
    const unsigned long avgBitrate = 192000; // 192kbps in bits per second
    
    // Calculate duration: (file_size_in_bits) / (bitrate_in_bits_per_second)
    unsigned long duration = (fileSize * 8) / (avgBitrate ? avgBitrate : 1);
    
    // Ensure minimum duration of 1 second
    return duration > 0 ? duration : 1;
}
