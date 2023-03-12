# bin2video

A C program for encoding anything as a video file. Inspired by [Infinite-Storage-Glitch](https://github.com/DvorakDwarf/Infinite-Storage-Glitch).

## Building

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
  -f <rate>   Framerate. Defaults to 10.
  -b <bits>   Bits per pixel. Defaults to 1 (black and white).
  -w <width>  Sets video width. Defaults to 1280.
  -h <height> Sets video height. Defaults to 720.
  -s <size>   Sets the size of each block. Defaults to 10.
  -I          Infinite-Storage-Glitch compatibility mode.

ADVANCED OPTIONS:
  -S <size>   Sets the size of each block for the initial frame.
              Defaults to 10. Do not change this unless you have
              a good reason to do so. If you specify this flag
              while encoding, you will also need to do it while
              decoding. Cannot be used with -I.
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