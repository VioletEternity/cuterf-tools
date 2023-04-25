# nanovna-tools

Tools for working with a [NanoVNA](https://nanovna.com/) connected to a Windows PC.

Only NanoVNA-H 4 is supported.

## nanovna_screenshot.exe

```
Usage: nanovna_screenshot.exe [options] [filename.png]

Writes a screen capture to a PNG file. The PNG file includes all acquired data
in Touchstone format for a 2-port network in the "Touchstone" tEXt chunk.

Options:
        /?              Show program usage.
        /scale:N, /xN   Enlarge image by factor of N (1 <= N <= 4).
```

## nanovna_data.exe

```
Usage: nanovna_data.exe [options] [filename.s1p,s2p]

Writes a capture of the data displayed on screen to a Touchstone format file,
without initiating a sweep.

Options:
        /?              Show program usage.
        /s1p            Save measurements of 1-port network.
        /s2p            Save measurements of 2-port network. Default if no filename given.
```

## nanovna_extract.exe

```
Usage: nanovna_extract.exe [options] [filename.png] [filename.s2p]

Extracts Touchstone data from a PNG file. The Touchstone file name may be
omitted.

Options:
        /?              Show program usage.
```
