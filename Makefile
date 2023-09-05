mp4len: mp4len.c
	gcc -O3 $(getconf LFS_CFLAGS) -Wall mp4len.c -o mp4len

debug: mp4len.c
	gcc -Og -g $(getconf LFS_CFLAGS) -Wall mp4len.c -o mp4len
