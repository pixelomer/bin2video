# bin2video

A C program for encoding anything as a video file. Inspired by [Infinite-Storage-Glitch](https://github.com/DvorakDwarf/Infinite-Storage-Glitch).

## Building

```bash
cc src/*.c -lm -O3 -o bin2video
```

## Dependencies

You must have `ffmpeg` and `ffprobe` in your PATH to use this program.

## Usage

Run the program with no arguments for more usage details.

```bash
# Encode archive.zip as a video
./bin2video -e -i archive.zip -o archive.zip.mp4

# Encode archive.zip as a video, storing 3 bits in each pixel
./bin2video -e -b 3 -i archive.zip -o archive.zip.mp4

# Encode archive.zip as a video at 1920x1080 resolution
./bin2video -e -h 1920 -w 1080 -i archive.zip -o archive.zip.mp4

# Extract archive.zip from the video
./bin2video -d -i archive.zip.mp4 -o archive.zip
```