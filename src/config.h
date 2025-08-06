#pragma once

/*SD Card SPI Configuration*/
#define SD_CS 1
#define SD_MISO 3
#define SD_MOSI 2
#define SD_SCK 11

/*I2S Pins for NAU8325*/
#define I2S_MCLK 22
#define I2S_BCLK 25
#define I2S_WS 24
#define I2S_DOUT 23

/*Default File Path & Extension*/
#define AUDIO_START_PATH "/"
#define AUDIO_FILE_EXTENSION "mp3"  // Changeable: "mp3", "wav", "aac", "ogg"
