# nanovna-tools

Tools for working with a [NanoVNA](https://nanovna.com/) connected to a Windows PC.

Only NanoVNA-H 4 is supported.

## nanovna_screenshot.exe

Saves a copy of the screen.

```
Usage: nanovna_screenshot.exe [options] [filename.png]
Options:
        /?              Show program usage.
        /scale:N, /xN   Enlarge image by factor of N (1 <= N <= 4).
```

## nanovna_data.exe

Saves a copy of data currently displayed on screen. Does not start a sweep.

```
Usage: nanovna_data.exe [options] [filename.s1p,s2p]
Options:
        /?              Show program usage.
        /s1p            Save measurements of 1-port network.
        /s2p            Save measurements of 2-port network. Default if no filename given.
```