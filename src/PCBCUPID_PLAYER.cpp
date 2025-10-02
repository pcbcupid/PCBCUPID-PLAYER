#include "PCBCUPID_PLAYER.h"

AUDIO_PLAYER::AUDIO_PLAYER(TwoWire &w, const AudioPlayerConfig &cfg)
    : wire(w), config(cfg), nau8325_control(w), i2s(new audio_tools::I2SStream()) {}

AUDIO_PLAYER::~AUDIO_PLAYER()
{
    // Clean up audio player
    if (player)
    {
        delete player;
        player = nullptr;
    }

    // Clean up audio source
    if (source)
    {
        delete source;
        source = nullptr;
    }

    // Clean up info handler
    if (infoHandler)
    {
        delete infoHandler;
        infoHandler = nullptr;
    }
}

/*Intialization*/

void AUDIO_PLAYER::begin(const char *ext)
{
    // Initialize logging
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
    delay(300);

    // Initialize I2S first
    auto cfg = i2s->defaultConfig(TX_MODE);
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    cfg.channels = 2;
    cfg.pin_bck = config.i2s_bclk;
    cfg.pin_ws = config.i2s_ws;
    cfg.pin_data = config.i2s_dout;
    cfg.pin_mck = config.i2s_mclk;
    i2s->begin(cfg);

    // Power on the amplifier
    nau8325_control.powerOn();

    // Initialize SD card
    SPI.begin(config.sd_sck, config.sd_miso, config.sd_mosi);
    if (!SD.begin(config.sd_cs, SPI))
    {
        Serial.println("SD mount failed!");
        while (true)
            ;
    }

    // Initialize audio source before building playlist
    source = new AudioSourceSD(config.audio_start_path, ext, config.sd_cs);
    if (!source)
    {
        Serial.println("Failed to create audio source!");
        return;
    }

    // Initialize decoder
    AudioDecoder *decoder = nullptr;
    String extStr = String(ext);
    extStr.toLowerCase();

    if (extStr == "mp3")
    {
        decoder = &mp3;
    }
    else if (extStr == "wav")
    {
        decoder = &wav;
    }
    else if (extStr == "ogg")
    {
        decoder = &ogg;
    }
    else if (extStr == "aac" || extStr == "m4a")
    {
        decoder = &aac;
    }

    if (!decoder)
    {
        Serial.println("Unsupported file extension: " + String(ext));
        return;
    }

    // Create and configure the player
    player = new AudioPlayer(*source, *i2s, *decoder);
    if (!player)
    {
        Serial.println("Failed to create audio player!");
        return;
    }

    infoHandler = new InfoHandler(nau8325, i2s, &nau8325_control);
    player->setMetadataCallback(printMetaData);
    player->addNotifyAudioChange(infoHandler);
    player->setVolume(volume);

    // Build playlist after everything is initialized
    buildPlaylist(ext);
    listTracks();

    // Start playback if there are tracks
    if (trackList.size() > 0)
    {
        player->begin();
    }
    else
    {
        Serial.println("No tracks found with extension: " + String(ext));
    }
}

/*Helper Method*/
void AUDIO_PLAYER::buildPlaylist(const char *ext)
{
    File dir = SD.open(config.audio_start_path);
    if (!dir || !dir.isDirectory())
    {
        Serial.println("Invalid audio directory!");
        return;
    }

    trackList.clear();
    File file;
    while ((file = dir.openNextFile()))
    {
        String fname = file.name();
        if (!file.isDirectory() && fname.endsWith("." + String(ext)))
        {
            // Strip leading "//" if present
            if (fname.startsWith("//"))
            {
                fname = fname.substring(2); // remove the first two characters
            }
            // Add to track list
            String fullPath = String(config.audio_start_path) + "/" + fname;
            // Validate the file by checking if it can be opened and has content
            File testFile = SD.open(fullPath.c_str());
            if (!testFile || testFile.isDirectory())
            {
                Serial.println("Invalid audio file!");
                return;
            }
            if (testFile && testFile.size() > 0)
            {
                trackList.push_back(fullPath);
                Serial.print("Added to the playList");
                Serial.println(fname);
            }
            else
            {
                Serial.print("SkippingInvalid/corrupted audio file: ");
                Serial.println(fname);
            }
            testFile.close();
        }
        file.close();
    }
    dir.close();

    // List all available tracks after building playlist
    listTracks();
}

AudioDecoder *AUDIO_PLAYER::getAudioDecoder(const char *ext)
{
    if (strcasecmp(ext, "mp3") == 0)
        return &mp3;
    if (strcasecmp(ext, "wav") == 0)
        return &wav;
    if (strcasecmp(ext, "ogg") == 0)
        return &ogg;
    if (strcasecmp(ext, "aac") == 0)
        return &aac;
    return nullptr;
}

/*Main Loop*/
void AUDIO_PLAYER::loop()
{
    if (player)
    {
        player->copy();
    }
}

/*Playback control*/
void AUDIO_PLAYER::play()
{
    if (!player)
        return;

    if (state == STOPPED)
    {
        // Start playing from the beginning if stopped
        playCurrentTrack();
        state = PLAYING;
        paused = false;
        stopped = false;
        lastCommandWasStop = false;
        return;
    }

    if (paused && state == PAUSED)
    {
        // Resume playback from pause
        unsigned long resumeTime = millis();
        totalPausedMillis += resumeTime - pauseStartMillis;
        player->play();
        paused = false;
        state = PLAYING;
    }
}

void AUDIO_PLAYER::pause()
{
    if (!player || paused)
        return;

    // Pause the current playback
    player->stop();
    paused = true;
    state = PAUSED;
    pauseStartMillis = millis();
}

void AUDIO_PLAYER::stop()
{
    if (player)
    {
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

void AUDIO_PLAYER::next()
{
    if (trackList.empty())
        return;

    trackIndex = (trackIndex + 1) % trackList.size();
    playCurrentTrack();

    // Ensure correct state after new playback
    state = PLAYING;
    paused = false;
    stopped = false;
    lastCommandWasStop = false;

    /*notify UI track change*/
    notifyTrackChange();
}

void AUDIO_PLAYER::previous()
{
    if (trackList.empty())
        return;

    trackIndex = (trackIndex - 1 + trackList.size()) % trackList.size();
    playCurrentTrack();

    state = PLAYING;
    paused = false;
    stopped = false;
    lastCommandWasStop = false;
    /*notify UI track change*/
    notifyTrackChange();
}

bool AUDIO_PLAYER::playTrackIndex(int index)
{
    // Check if the index is valid
    if (trackList.empty() || index < 0 || index >= (int)trackList.size())
    {
        Serial.printf("Error: Invalid track index %d (valid range: 0-%d)\n", index, trackList.size() - 1);
        return false;
    }

    if (!player || !source)
    {
        Serial.println("Player or source not initialized.");
        return false;
    }

    // Stop current playback if any
    if (player)
    {
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

      // Notify UI of track change
    notifyTrackChange();

    return true;
}

/*Volume Control*/

void AUDIO_PLAYER::setVolume(float vol)
{
    volume = constrain(vol, 0.0f, 1.0f);
    if (player)
    {
        player->setVolume(volume);
    }
}

// int AUDIO_PLAYER::getVolume() const
// {
//     return int(volume * 100.0f); // Return volume as an integer percentage
// }

void AUDIO_PLAYER::setAutoFade(bool enable)
{
    if (player)
        player->setAutoFade(enable);
}

/*Status and Information*/
String AUDIO_PLAYER::currentTrackName() const
{
    return trackList.empty() ? "" : trackList[trackIndex];
}

bool AUDIO_PLAYER::isPlaying() const
{
    return player && player->isActive();
}

bool AUDIO_PLAYER::isPaused() const
{
    return state == PAUSED;
}

bool AUDIO_PLAYER::isStopped() const
{
    return state == STOPPED;
}

/*Playback Position and Duration*/
unsigned long AUDIO_PLAYER::currentTrackElapsedMillis() const
{
    if (state == STOPPED)
        return 0;
    if (state == PAUSED)
        return pauseStartMillis - playStartMillis - totalPausedMillis;
    return millis() - playStartMillis - totalPausedMillis;
}

unsigned long AUDIO_PLAYER::currentTrackElapsedSeconds() const
{
    return currentTrackElapsedMillis() / 1000;
}

unsigned long AUDIO_PLAYER::currentTrackDurationMillis() const
{
    return currentTrackDurationSeconds() * 1000;
}

unsigned long AUDIO_PLAYER::currentTrackDurationSeconds() const
{
    if (trackList.empty() || trackIndex < 0 || trackIndex >= trackList.size())
    {
        return 0;
    }
    return calculateTrackDuration(trackList[trackIndex]);
}

/*Audio Information*/
unsigned long AUDIO_PLAYER::currentTrackPositionBytes() const
{
    if (!player)
        return 0;
    AudioInfo info = infoHandler->lastInfo;
    unsigned long elapsed = currentTrackElapsedSeconds();
    return (info.sample_rate * info.bits_per_sample * info.channels * elapsed) / 8;
}

unsigned long AUDIO_PLAYER::currentTrackTotalBytes() const
{
    String filename = currentTrackName();
    File f = SD.open(filename);
    if (!f)
        return 1;
    unsigned long size = f.size();
    f.close();
    return size;
}

audio_tools::AudioPlayer *AUDIO_PLAYER::audioPlayer()
{
    return player;
}

AudioInfo AUDIO_PLAYER::audioInfo() const
{
    return infoHandler ? infoHandler->lastInfo : AudioInfo();
}

void AUDIO_PLAYER::resetPlayTime()
{
    playStartMillis = millis();
    totalPausedMillis = 0;
}

// int AUDIO_PLAYER::currentTrackIndex() const
// {
//     return trackIndex;
// }

/*Track Playback (Internal)*/
void AUDIO_PLAYER::playCurrentTrack()
{
    if (trackList.empty() || trackIndex < 0 || trackIndex >= (int)trackList.size())
    {
        Serial.println("Track index out of bounds.");
        return;
    }

    String filepath = trackList[trackIndex];
    if (!source || !player)
    {
        Serial.println("Player or source not initialized.");
        return;
    }

    Serial.print("Playing track: ");
    Serial.println(filepath);

    player->stop();

    // Update source index to the selected track
    source->setIndex(trackIndex);

    // player->begin(); // begin playback

    resetPlayTime();
    paused = false;
    stopped = false;
    state = PLAYING;

    unsigned long durationSec = calculateTrackDuration(filepath);
    if (durationSec == 0)
    {
        durationSec = 180;
    }

    AudioInfo info = player->audioInfo();
    nau8325_control.begin(info.sample_rate, info.bits_per_sample);
    nau8325_control.powerOn();

    player->play();
}

/*Audio Info Handlers*/
void AUDIO_PLAYER::InfoHandler::setAudioInfo(AudioInfo info)
{
    Serial.printf("Audio Changed: fs=%d, bits=%d\n", info.sample_rate, info.bits_per_sample);
    nau8325_control->begin(info.sample_rate, info.bits_per_sample);
    nau8325_control->powerOn();
    i2s->setAudioInfo(info);
    lastInfo = info;
}

void AUDIO_PLAYER::printMetaData(MetaDataType type, const char *str, int len)
{
    Serial.print("==> ");
    Serial.print(toStr(type));
    Serial.print(": ");
    Serial.println(str);
}

/*Duration Calculation*/

// Parses MP3 duration via ID3 TLEN or VBR header
unsigned long AUDIO_PLAYER::calculateTrackDuration(const String &filename) const
{
    File f = SD.open(filename);
    if (!f)
        return 0;

    size_t fileSize = f.size();
    unsigned long duration = 0;

    // Handle MP3 files
    if (filename.endsWith(".mp3"))
    {
        // Skip ID3v2 header if present
        uint8_t hdr[10];
        f.read(hdr, 10);
        if (hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3')
        {
            // Parse ID3v2 header size (stored as synchsafe integer)
            uint32_t sz = ((hdr[6] & 0x7F) << 21) |
                          ((hdr[7] & 0x7F) << 14) |
                          ((hdr[8] & 0x7F) << 7) |
                          (hdr[9] & 0x7F);
            f.seek(10 + sz);
        }
        else
        {
            f.seek(0);
        }

        // Read MP3 frame header
        uint8_t frame[4];
        f.read(frame, 4);

        // Check for valid MP3 frame sync
        if (frame[0] == 0xFF && (frame[1] & 0xE0) == 0xE0)
        {
            int kbps = parseMP3Bitrate(frame);
            if (kbps > 0)
            {
                duration = (fileSize * 8UL) / (kbps * 1000UL);
            }
        }

        // Check for Xing/VBRI header for more accurate duration
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
                duration = frames * spf / info.sample_rate;
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
            duration = frames * spf / info.sample_rate;
        }
    }
    // Handle WAV files
    else if (filename.endsWith(".wav"))
    {
        duration = parseWAVDuration(f);
    }
    // Handle OGG files
    else if (filename.endsWith(".ogg"))
    {
        duration = parseOGGDuration(f);
    }
    // For other formats, use bitrate estimation
    else
    {
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
int AUDIO_PLAYER::parseMP3Bitrate(const uint8_t *header) const
{
    // Bitrate lookup table [version][layer][bitrate_index]
    const int bitrateTable[2][3][16] = {
        {// MPEG Version 2 & 2.5
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}},
        {// MPEG Version 1
         {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
         {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
         {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}}};

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
unsigned long AUDIO_PLAYER::parseWAVDuration(File &file) const
{
    // Read sample rate
    file.seek(24);
    uint32_t sampleRate = 0;
    file.read((uint8_t *)&sampleRate, 4);
    if (sampleRate == 0)
        return 0;

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
unsigned long AUDIO_PLAYER::parseOGGDuration(File &file) const
{
    const size_t fileSize = file.size();
    const size_t scanSize = 2048; // How much of the end to scan for OggS
    const size_t minOffset = 32;  // Minimum possible OggS header size

    // Start scanning from the end of the file
    size_t startPos = (fileSize > scanSize) ? (fileSize - scanSize) : 0;

    for (size_t i = startPos; i < fileSize - minOffset; i++)
    {
        file.seek(i);

        // Look for OggS signature
        char sig[4];
        if (file.read((uint8_t *)sig, 4) != 4)
            continue;

        if (memcmp(sig, "OggS", 4) == 0)
        {
            // Found OggS header, read granule position
            file.seek(i + 6);
            uint64_t granule = 0;
            if (file.read((uint8_t *)&granule, 8) != 8)
                continue;

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
unsigned long AUDIO_PLAYER::estimateBitrateBased(File &file, size_t fileSize) const
{
    // Default to average bitrate of 192kbps for unknown formats
    const unsigned long avgBitrate = 192000; // 192kbps in bits per second

    // Calculate duration: (file_size_in_bits) / (bitrate_in_bits_per_second)
    unsigned long duration = (fileSize * 8) / (avgBitrate ? avgBitrate : 1);

    // Ensure minimum duration of 1 second
    return duration > 0 ? duration : 1;
}

/* List all available tracks */
String AUDIO_PLAYER::listTracks()
{
    String result = "";

    if (trackList.empty())
    {
        result = "No tracks found in the playlist.\n";
        Serial.print(result);
        return result;
    }

    result += "\nAvailable Tracks:\n";
    result += "----------------\n";

    for (size_t i = 0; i < trackList.size(); i++)
    {
        // Extract just the filename from the full path
        String fullPath = trackList[i];
        int lastSlash = fullPath.lastIndexOf('/');
        String filename = (lastSlash != -1) ? fullPath.substring(lastSlash + 1) : fullPath;

        // Format: " 1: filename.mp3\n"
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "%2d: %s\n", i + 1, filename.c_str());
        result += buffer;
    }
    result += "\n";

    // Also print to Serial for backward compatibility
    Serial.print(result);
    return result;
}

void AUDIO_PLAYER::setTrackChangeCallback(TrackChangeCallback cb)
{
    trackChangeCb = cb;
}

void AUDIO_PLAYER::notifyTrackChange()
{
    if (trackChangeCb)
    {
        String title = currentTrackName();
        uint32_t duration = currentTrackDurationSeconds();
        trackChangeCb(title.c_str(), duration);
    }
}

void AUDIO_PLAYER::update()
{
    // Check if playback finished naturally
    if (state == PLAYING && player && !player->isActive())
    {
        Serial.println("Track finished, moving to next...");
        next();  // auto-advance + notify UI
    }
}


