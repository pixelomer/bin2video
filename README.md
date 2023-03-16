# bin2video

A C program for encoding anything as a video file. Inspired by [Infinite-Storage-Glitch](https://github.com/DvorakDwarf/Infinite-Storage-Glitch). Supports Windows, macOS and Linux.

## Building

**Windows:** bin2video can be compiled with [w64devkit](https://github.com/skeeto/w64devkit).  
**macOS:** Make sure command line tools are installed with `xcode-select --install`.  
**Linux:** Make sure `build-essential` or equivalent is installed.

```bash
cc src/*.c -lm -O3 -o bin2video
```

## Dependencies

You must have `ffmpeg` and `ffprobe` in your PATH to use this program.

## Usage

```
USAGE:
  ./bin2video [options] -e data.bin video.mp4
  ./bin2video [options] -d video.mp4 data.bin

OPTIONS:
  -e          Encode mode. Takes an input binary file and produces
              a video file.
  -d          Decode mode. Takes an input video file and produces
              the original binary file.
  -i          Input file. Defaults to stdin.
  -o          Output file. Defaults to stdout.
  -t          Allows writing output to a tty.
  -f <rate>   Framerate. Defaults to 10. Set to -1 to let FFmpeg
              decide.
  -c <n>      Write every frame n times. Defaults to 1. Cannot be
              used with -I.  -b <bits>   Bits per pixel. Defaults to 1 (black and white).
  -w <width>  Video width. Defaults to 1280.
  -h <height> Video height. Defaults to 720.
  -H <height> Data height. Set this to a value less than the video
              height to limit the data blocks to a region on top of
              the video. The bottom of the region will be black.
              A value of -1 disables the data height. Defaults to -1.
              Cannot be used with -I.
  -s <size>   Size of each block. Defaults to 5.
  -I          Infinite-Storage-Glitch compatibility mode.
  -E          End the output with a black frame. Cannot be used with
              -I.

ADVANCED OPTIONS:
  -S <size>   Sets the size of each block for the initial frame.
              Defaults to 10. Do not change this unless you have
              a good reason to do so. If you specify this flag
              while encoding, you will also need to do it while
              decoding. When -I is used, this value defaults to 5
              and cannot be changed.
  -F <args>   Space separated options for encoding with FFmpeg.
              Defaults to "-c:v libx264 -pix_fmt yuv420p".
```

## Usage Examples

```bash
# Encode archive.zip as a video
./bin2video -e -i archive.zip -o archive.zip.mp4

# Encode archive.zip as a video, storing 3 bits in each pixel
./bin2video -e -b 3 -i archive.zip -o archive.zip.mp4

# Encode archive.zip as a video at 1920x1080 resolution
./bin2video -e -h 1920 -w 1080 -i archive.zip -o archive.zip.mp4

# Extract archive.zip from the video
./bin2video -d -i archive.zip.mp4 -o archive.zip

# Decode video encoded with Infinite-Storage-Glitch
# (Example video taken from Infinite-Storage-Glitch README.md)
yt-dlp -f 247 -o isg-video.webm 'https://www.youtube.com/watch?v=8I4fd_Sap-g'
./bin2video -I -d -i isg-video.webm -o archive.zip
```