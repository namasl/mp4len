# mp4len

`mp4len` is a command-line utility that takes in an MP4 (MPEG-4) video file as an argument and prints the length of the video in seconds.

It is meant to be fast and efficient, as its sole purpose is to extract MP4 time duration.  In my tests it has consistently been over an order of magnitude quicker than `ffprobe` for this purpose.

## Installation

To compile, simply run:

```bash
make
````

The resulting binary can be manually moved wherever required, such as `~/bin/` if only a single user will use `mp4len`, or `/usr/local/bin/` if all users need access.

## Usage

To obtain the length of a video, supply it as an argument to `mp4len`:

```bash
mp4len my_video.mp4
```

The length of the video will be printed to standard out.  If an error occurs, a message will be sent to standard error, and a non-zero error code will be returned.

## License

[Mozilla Public License Version 2.0](https://www.mozilla.org/en-US/MPL/2.0/)
