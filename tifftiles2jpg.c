#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tiffio.h"

#define PATH_LENGTH 8192

static char fname[PATH_LENGTH];

static int save_tiles(TIFF*, int, char*);

int
main(int argc, char* argv[])
{
    if (argc < 3) {
        fprintf(stderr, "%s\n\n", TIFFGetVersion());
        fprintf(stderr, "usage: tifftiles2jpg input.tif dir_number path\n");
        return (-3);
    }

    struct stat st = {0};
    char* fname = argv[1];
    int tiff_dir = atoi(argv[2]);
    char* path = argv[3];

    mkdir(path, 0777);

    if (stat(path, &st) == -1) {
        fprintf(stderr, "tifftiles2jpg: can't create output directory %s\n", path);
        return (-1);
    }

    TIFF* in = TIFFOpen(fname, "r");
    if (in == NULL) {
        fprintf(stderr, "tifftiles2jpg: can't open file %s\n", fname);
        return (-2);
    }

    int res = save_tiles(in, tiff_dir, path);

    TIFFClose(in);

    return res;
}


/* Return the first occurrence of NEEDLE in HAYSTACK.  */
void *
memmem (const void *haystack, size_t haystack_len, const void *needle,
size_t needle_len)
{
    const char *begin;
    const char *const last_possible
        = (const char *) haystack + haystack_len - needle_len;

    if (needle_len == 0)
    /* The first occurrence of the empty string is deemed to occur at
    the beginning of the string.  */
    return (void *) haystack;

    /* Sanity check, otherwise the loop might search through the whole
    memory.  */
    if (__builtin_expect (haystack_len < needle_len, 0))
        return NULL;

    for (begin = (const char *) haystack; begin <= last_possible; ++begin)
        if (begin[0] == ((const char *) needle)[0] &&
            !memcmp ((const void *) &begin[1],
            (const void *) ((const char *) needle + 1),
            needle_len - 1))
                return (void *) begin;

    return NULL;
}

static int
save_tiles(TIFF* in, int tiff_dir, char* path)
{
    uint16 bitspersample, samplesperpixel, compression, photometric;
    uint32 img_w, img_l, tile_w, tile_l;
    float floatv;
    char *stringv;
    
    int res = TIFFSetDirectory(in, tiff_dir);
    if (res != 1) {
        fprintf(stderr, "wrong directory number %d\n", tiff_dir);
        return (-7);
    }
    
    TIFFGetField(in, TIFFTAG_TILEWIDTH, &tile_w);
    TIFFGetField(in, TIFFTAG_TILELENGTH, &tile_l);
    TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &img_w);
    TIFFGetField(in, TIFFTAG_IMAGELENGTH, &img_l);
    TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
    TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
    TIFFGetField(in, TIFFTAG_COMPRESSION, &compression);
    TIFFGetField(in, TIFFTAG_PHOTOMETRIC, &photometric);

    int t_img_w = ceil((double)img_w / (double)tile_w);
    int t_img_l = ceil((double)img_l / (double)tile_l);

    printf("Image width: %d, Image lenght: %d\n", img_w, img_l);
    printf("Tile width: %d, Tile lenght: %d\n", tile_w, tile_l);
    printf("Tiles in width: %d, Tiles in lenght: %d\n", t_img_w, t_img_l);
    printf("BitsPerSample: %d\n", bitspersample);
    printf("SamplesPerPixel: %d\n", samplesperpixel);
    printf("Compression: %d\n", compression);
    printf("Photometric: %d\n", photometric);

    if (compression != COMPRESSION_JPEG) {
        fprintf(stderr, "tifftiles2jpg: compression must be jpeg\n");
        return (-3);
    }

    uint32 t_count = 0;
    unsigned char *table = NULL;

    if (TIFFGetField(in, TIFFTAG_JPEGTABLES, &t_count, &table)
        && t_count > 0 && table) {
        printf("JPEGTables size %d\n", t_count);
    }

    if (t_count <= 4) {
        t_count = 0;
    }

    tmsize_t bufsize = TIFFTileSize(in);
    unsigned char *buf = (unsigned char *)_TIFFmalloc(bufsize);
    if (buf) {
        ttile_t t, nt = TIFFNumberOfTiles(in);
        uint64 *bytecounts;

        if (!TIFFGetField(in, TIFFTAG_TILEBYTECOUNTS, &bytecounts)) {
            fprintf(stderr, "tifftiles2jpg: tile byte counts are missing\n");
            _TIFFfree(buf);
            return (-4);
        }

        printf("Number of tiles: %d\n", nt);
        printf("_");

        for (t = 0; t < nt; t++) {
            if (bytecounts[t] > (uint64) bufsize) {
                buf = (unsigned char *)_TIFFrealloc(buf, (tmsize_t)bytecounts[t]);
                if (!buf)
                    return (-5);
                bufsize = (tmsize_t)bytecounts[t];
            }

            tmsize_t ts = TIFFReadRawTile(in, t, buf, (tmsize_t)bytecounts[t]);
            // printf("ts %d\n", ts);
            if (ts < 0) {
                _TIFFfree(buf);
                return (-6);
            }

            if (photometric == PHOTOMETRIC_RGB) {
                const unsigned char FFC0[] = {0xFF, 0xC0};
                const unsigned char FFDA[] = {0xFF, 0xDA};
                unsigned char * bs = NULL;

                // Find and fix channels ids
                bs = (unsigned char*)memmem(buf, ts, FFC0, 2);
                if (bs) {
                    bs[10] = 'R';
                    bs[13] = 'G';
                    bs[16] = 'B';
                }
                bs = (unsigned char*)memmem(buf, ts, FFDA, 2);
                if (bs) {
                    bs[5] = 'R';
                    bs[7] = 'G';
                    bs[9] = 'B';
                }
            }

            int y = t / t_img_w;
            int x = t % t_img_w;
            snprintf(fname, PATH_LENGTH, "%s/%d_%d.jpeg", path, y, x);

            FILE* f = fopen(fname, "wb+");
            if (f) {
                fwrite(buf, 2, 1, f);
                if (t_count > 0) {
                    fwrite(&table[2], t_count - 4, 1, f);
                }
                fwrite(&buf[2], ts - 2, 1, f);
                fclose(f);
            } else {
                fprintf(stderr, "can't write to %s\n", fname);
            }

            if (t % 100 == 0) {
                printf("\rtile: %d", t);
            }
        }

        printf("\rfinished..............\n");

        _TIFFfree(buf);
        return (0);
    }
}
