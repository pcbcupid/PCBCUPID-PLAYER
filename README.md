# PCBCUPID_PLAYERS

A robust Arduino/ESP32 audio player library for SD card playback with I2S output and NAU8325 amplifier control.

---

## Overview

**PCBCUPID_PLAYERS** is a high-level audio player library designed for ESP32 and Arduino-compatible boards. It enables seamless playback of MP3, WAV, OGG, and AAC files from an SD card, with I2S digital audio output and integrated control of the NAU8325 Codec with Class-D audio amplifier via I2C.

- Simple API for playback control (play, pause, stop, next, previous)
- Automatic file discovery and playlist management
- Volume and auto-fade support
- Metadata and audio info reporting
- Designed for custom hardware (tested on PCBCUPID GLYPHC3, GLYPHH2, GLYPHC6)

---

## Key Features

- **Multi-format support:** MP3, WAV, OGG, AAC
- **SD card file browsing:** Automatic playlist creation from a folder
- **I2S audio output:** High-quality digital audio
- **NAU8325 amplifier control:** Power, volume, and audio path management
- **Playback state tracking:** Elapsed time, paused time, and file position
- **Metadata callback:** Display track info (title, artist, etc.)
- **Easy integration:** Just provide your `TwoWire` instance and call `begin()`

---

## Hardware Requirements

- **ESP32** (all variants supported)
- **SD card module** (SPI interface)
- **NAU8325 Codec with Class-D amplifier** (I2C)
- **I2S DAC or amplifier**
- **Speaker or headphones**

---

## Getting Started

### 1. Clone the Repository

```sh
git clone https://github.com/pcbcupid/PCBCUPID-PLAYER.git
```

### 2. Install Dependencies

This library requires the following libraries:

### Option 1: Install from ZIP files

Download the latest release ZIP files and install them through the Arduino IDE:

**Sketch → Include Library → Add .ZIP Library...**

- [AudioTools](https://github.com/pschatzmann/arduino-audio-tools)
- [Arduino-Audio-Driver](https://github.com/pcbcupid/arduino-audio-driver.git)
- [SD](https://github.com/arduino-libraries/SD)
- [Wire](https://www.arduino.cc/en/Reference/Wire)

### Option 2: Clone using Git

```bash
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/arduino-audio-tools.git
```
```bash
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/arduino-audio-driver.git
```

Place the downloaded libraries in your Arduino `libraries` folder and restart the IDE.

> [!IMPORTANT]
> Make sure all required libraries are installed before compiling.
> Missing dependencies may result in build errors.

### 3. Wiring

Connect your ESP32 to the SD card, NAU8325 codec with amplifier, and I2S output as per your hardware.  
Update `config.h` for your pin assignments.

### 4. Example Usage

```cpp
#include "PCBCUPID_PLAYERS.h"

TwoWire wire = TwoWire(0);
PCBCUPID_PLAYERS player(wire);

void setup() {
  Serial.begin(115200);
  player.begin("mp3"); // or "wav", "aac", "ogg"
}

void loop() {
  player.loop();
  // Add your playback control logic here
}
```

---

## API Highlights

- `void begin(const char *ext);` — Initialize player and scan for files
- `void play();` — Start or resume playback
- `void pause();` — Pause playback
- `void stop();` — Stop playback
- `void next();` / `void previous();` — Navigate playlist
- `void setVolume(float volume);` — Set playback volume
- `bool isPlaying();` — Query playback state
- `String getCurrentFileName();` — Get current track name

---

## Example Projects

See the `examples/` directory for ready-to-use sketches demonstrating SD card playback, I2S output, and amplifier control.

---

## Documentation & Resources

- **NAU8325 Datasheet:** [Nuvoton NAU8325](https://www.nuvoton.com/export/resource-files/en-us--DS_NAU8325_DataSheet_EN_Rev2.5.pdf)
- **AudioTools Library:** [AudioTools](https://github.com/pschatzmann/arduino-audio-tools)
- **PCBCUPID Forum:** [forum.pcbcupid.com](https://forum.pcbcupid.com/)

---

## Credits & Attribution

This library was developed and maintained by **PCBCUPID**, based on the NAU8325 codec specifications and the I2C protocol defined by Nuvoton Technology Corporation.  
Special thanks to the open-source community for foundational research and NAU8325 references.

**Original Author:**

- **Karthik Elumalai** – @PCBCUPID
- **Version**: 1.0.0
- **License**: MIT License (refer to the LICENSE file for terms)

---

## Contributing

We welcome contributions and feedback from the community! You can:

- Report issues or suggest improvements
- Submit pull requests with enhancements or fixes
- Share example projects using this library

> Please refer to the [Contributing Guidelines](CONTRIBUTING.md) before submitting changes.

---

## License

This library is licensed under the **MIT License**. See the `LICENSE` file for full details.

---

**Made with 🎵 by PCBCUPID**
