/* mp4len
   Takes an MP4 file in as a command line argument and returns the time
   duration of the video in seconds.

   Usage:
   mp4len VIDEO_FILE

   Nicholas A. Masluk
   nick@randombytes.net
   Copyright 2023
   Mozilla Public License Version 2.0
*/

#include <stdio.h>
#include <stdlib.h>

#define VERSION "2023-09-05"
#define MIN_SIZE 51 // minimum file size
#define BLOCK_SIZE 16384 // number of bytes to read from file at once

// Make sure the file contains an MP4 magic number at offset of 4 bytes:
//   "ftypisom" for ISO base media file MPEG-4, MP4
//   66 74 79 70 69 73 6F 6D
//
//   "ftypmp42" for QuickTime MPEG-4, M4V
//   66 74 79 70 6D 70 34 32
//
// Return 0 for not mp4
// Return 1 for is mp4
int has_mp4_magic(FILE *fptr)
{
    union {
        unsigned long long ull;
        char c[8];
    } magic;

    // read in 8 bytes where the magic number should be
    if (fseek(fptr, 4L, SEEK_SET)) {
        fputs("Problem accessing file\n", stderr);
        exit(10);
    }
    if (fread(magic.c, 1, 8, fptr) != 8) {
        // we did not complete a full read
        fputs("Problem reading file\n", stderr);
        exit(11);
    }

    // At this point our char array is essentially a long long, and
    // x86 is little endian so our magic number looks like
    // 6D 6F 73 69 70 79 74 66 for ISO base media file MPEG-4
    // 32 34 70 6D 70 79 74 66 for QuickTime MPEG-4

    // check magic number
    if (   (magic.ull == 0x6D6F736970797466LL)
        || (magic.ull == 0x3234706D70797466LL)) {
        // mp4 magic number is present
        return 1;
    }
    // not a valid mp4 file
    return 0;
}

// Move file position to just after mvhd header.
// Return 0 if successful.
// Return 1 if header could not be found.
int move_to_header(FILE *fptr, long long fsize)
{
    char *buf;
    long n_blocks;

    buf = (char*)malloc(BLOCK_SIZE * sizeof(char));
    if (buf == NULL) {
        fputs("Could not allocate memory.\n", stderr);
        exit(20);
    }

    // number of blocks to cover file
    n_blocks = fsize / BLOCK_SIZE;
    if (fsize % BLOCK_SIZE > 0) {
        n_blocks += 1;
    }

    // Search file for "mvhd" header atom string in blocks.  Since header will
    // be at the beginning or end of the file, we will search alternately in
    // both directions.  Even iterations at beginning of file, odd at end, and
    // work our way inwards.
    int buf_len;
    unsigned long ii;
    for (unsigned long xx = 0; xx < n_blocks; xx++) {
        // move file position to start of current block
        if (xx % 2) {
            // odd iteration, work at end of file
            ii = n_blocks - 1 - (xx - 1) / 2;
            if (fseek(fptr, (ii - n_blocks) * BLOCK_SIZE, SEEK_END)) {
                fputs("Problem accessing file\n", stderr);
                exit(21);
            }
        }
        else {
            // even iteration, work at beginning of file
            ii = xx / 2;
            if (fseek(fptr, ii * BLOCK_SIZE, SEEK_SET)) {
                fputs("Problem accessing file\n", stderr);
                exit(22);
            }
        }

        // read in block
        buf_len = fread(buf, 1, BLOCK_SIZE, fptr);
        if ((buf_len < BLOCK_SIZE) && (buf_len < fsize)) {
            // we did not complete a full read
            fputs("Problem reading file\n", stderr);
            exit(23);
        }

        // search the block for header
        char hdr[4] = {'m', 'v', 'h', 'd'}; // header to find
        int chars_matching = 0; // streak of characters matching header
        for (int jj = 0; jj < buf_len; jj++) {
            if (buf[jj] == hdr[chars_matching]) {
                chars_matching += 1;
                if (chars_matching == 4) {
                    // we found 'mvhd', move file position right after it
                    if (fseek(fptr, -buf_len + jj + 1, SEEK_CUR)) {
                        fputs("Problem accessing file\n", stderr);
                        exit(24);
                    }
                    free(buf);
                    return 0;
                }
            }
            else {
                // no match
                chars_matching = 0;
            }
        }

        if (chars_matching > 0) {
            // we might have a match that flows into neighboring block
            // check remaining characters
            while ((chars_matching != 0) && (chars_matching != 4)) {
                if (fgetc(fptr) == hdr[chars_matching]) {
                    chars_matching += 1;
                }
                else {
                    chars_matching = 0;
                    break;
                }
                // If chars_matching == 4 at this point, loop will end with
                // fptr indexed right after the header.
            }
        }
        if (chars_matching == 4) {
            // we found the header, and fptr is indexed right after it
            free(buf);
            return 0;
        }
        // no match, try next block
    }
    // no match found in file
    free(buf);
    return 1;
}

// Get the time duration of video file in seconds.
double get_mp4_len(FILE *fptr, long long fsize) 
{
    int version;
    unsigned char buf[8];
    unsigned long unit_per_sec = 0; // units per second
    unsigned long long len_unit = 0; // time length in units
    double len_sec; // time length in seconds

    // move file positon to just after movie header atom "mvhd"
    if (move_to_header(fptr, fsize)) {
        fputs("Could not find header.\n", stderr);
        exit(30);
    }

    // get version (if version 1, date and duration values are 8 bytes, not 4)
    version = fgetc(fptr);

    // we must now skip over:
    //   3 bytes of hex flags
    //   4 bytes creation date, (8 bytes if version 1)
    //   4 bytes modified date, (8 bytes if version 1)
    if (version == 1) {
        if (fseek(fptr, 19L, SEEK_CUR)) {
            fputs("Problem accessing file\n", stderr);
            exit(31);
        }
    }
    else {
        if (fseek(fptr, 11L, SEEK_CUR)) {
            fputs("Problem accessing file\n", stderr);
            exit(32);
        }
    }

    // read units per second, a big endian unsigned long
    if (fread(buf, 1, 4, fptr) != 4) {
        // we did not complete a full read
        fputs("Problem reading file\n", stderr);
        exit(33);
    }
    // move buffer into long int
    unit_per_sec = buf[3] + (buf[2]<<8) + (buf[1]<<16) + (buf[0]<<24);

    // read time length
    if (version == 1) {
        // unsigned long long
        if (fread(buf, 1, 8, fptr) != 8) {
            // we did not complete a full read
            fputs("Problem reading file\n", stderr);
            exit(34);
        }
    }
    else {
        // unsigned long, but fill buffer as if a long long
        if (fread(buf + 4, 1, 4, fptr) != 4) {
            // we did not complete a full read
            fputs("Problem reading file\n", stderr);
            exit(34);
        }
        // fill start of buffer with zeros
        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0;
        buf[3] = 0;
    }
    // move buffer into long long int, length in "units"
    len_unit = buf[7] + (buf[6]<<8) + (buf[5]<<16) + (buf[4]<<24)
             + ((unsigned long long)buf[3]<<32)
             + ((unsigned long long)buf[2]<<40)
             + ((unsigned long long)buf[1]<<48)
             + ((unsigned long long)buf[0]<<56);

    // get length in seconds
    len_sec = (double)len_unit / (float)unit_per_sec;
    return len_sec;
}

int main (int argc, char *argv[])
{
    FILE *fptr;
    long long fsize;
    double vlen;

    if (argc < 2) {
        fprintf(stderr, "%s: missing argument\n", argv[0]);
        fputs("\n", stderr);
        fputs("Prints the length of an mp4 video in seconds.\n", stderr);
        fputs("mp4len version "VERSION"\n", stderr);
        fputs("Copyright (C) 2023 Nicholas A. Masluk, nick@randombytes.net\n",
              stderr);
        return 1;
    }

    // open video file for reading
    fptr = fopen(argv[1], "rb");
    if (!fptr) {
        fprintf(stderr, "%s: %s: no such file\n", argv[0], argv[1]);
        return 2;
    }

    // get file size
    if (fseek(fptr, 0L, SEEK_END)) {
        fprintf(stderr, "%s: %s: problem accessing file\n", argv[0], argv[1]);
    }
    fsize = ftell(fptr);
    // check file size
    if (fsize < MIN_SIZE) {
        fclose(fptr);
        fprintf(stderr, "%s: %s: file size too small, %lld bytes\n",
                argv[0], argv[1], fsize);
        return 3;
    }

    // check for magic number matching MP4 file
    if (!has_mp4_magic(fptr)) {
        fclose(fptr);
        fprintf(stderr, "%s: %s: MP4 file format not valid\n", argv[0],
                argv[1]);
        return 4;
    }

    // get video length
    vlen = get_mp4_len(fptr, fsize);
    // print length
    printf("%f\n", vlen);
    // close file
    fclose(fptr);
    return 0;
}
