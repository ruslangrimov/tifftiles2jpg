# tifftiles2jpg
Program to extract tiles from tiff container

Before compilling install libtiff http://www.libtiff.org/

To compile use gcc
```
gcc -g tifftiles2jpg.c -o tifftiles2jpg -lm -Wl,/opt/conda/lib/libtiff.so.5
```
