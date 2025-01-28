FROM ubuntu:22.04

# Set the working directory
WORKDIR /home/


# Non-interactive mode for apt-get
ARG DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update
RUN apt-get install -y build-essential

# ffmpeg , ffprobe
RUN apt-get install -y ffmpeg

COPY . /

RUN cc /src/*.c -lm -O3 -o /file2video

# Entry point is the executable, file2video. All args are passed to the executable
ENTRYPOINT [ "/file2video" ]