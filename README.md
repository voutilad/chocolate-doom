# Chocolate Doom w/ Telemetry

This is an experimental fork of Chocolate Doom with the aim to
introduce telemetry output for observing the behavior of both player
and enemies during a game.

The [original Chocolate Doom README](./README.Original.md) is
preserved under a different name: `README.Original.md`.

## Building

In general, you can hack your way to a functional build system pretty
easily, but the best place to get a list of the 3rd party dependencies
is in the "Building Chocolate Doom on Windows" wiki page:

https://www.chocolate-doom.org/wiki/index.php/Building_Chocolate_Doom_on_Windows

I personally build and test on the following platforms:

- macOS v10.15.3
- Ubuntu 18.04 LTS
- Windows 10 w/ msys2-x86_64 v20190524
- OpenBSD-current

I do this primarily using the automake/autoconf tooling and the
resulting Makefiles, so if you have the proper dependencies (python,
SDL2, SDL_Net, SDL_Mixer, etc.) it should just work in building local
executables.

For instance, run the autogen script:

```bash
$ ./autogen.sh
```

> Note: if building on Win10 64-bit, I recommend running `./autogen
> --host=i686-w64-mingw32`

Then just build:

```bash
$ make
```

The resulting executable will be in `./src/` as `chocolate-doom` or
`chocolate-doom.exe` (on Windows). You don't need to build the install
package or bundled app, but if you do follow the official Chocolate
Doom docs from their wiki.

## Running

You should first configure Choclate Doom, specifically the telemetry
stuff I've build. After building the project per the above
instructions, run: `chocolate-doom` or `chocolate-doom.exe`.

Configure the display settings as you see fit...personally I recommend
turning off full-screen mode if you want to hack on things.

In the `Configure Telemetry` section, I've provided a few default
settings, but see the [TELEMETRY.md](/TELEMETRY.md) file for
documentation on configuration settings. (You can also hit `F1` in
that setup screen to pop a web browser to the latest documentation.)

## Telemetry
**TODO: DOCUMENT TELEMETRY SCHEMA**

Sample output (produced by the test binary):
```json
{"level":{"episode":69,"level":69,"difficulty":1},"type":"start_level","frame":{"millis":0,"tic":0}}
{"armor_type":69,"type":"pickup_armor","frame":{"millis":0,"tic":0},"actor":{"position":{"x":12,"y":13,"z":0,"angle":180,"subsector":4484865928},"type":"player","id":140732730996144}}
{"weapon_type":2,"type":"pickup_weapon","frame":{"millis":0,"tic":0},"actor":{"position":{"x":12,"y":13,"z":0,"angle":180,"subsector":4484865928},"type":"player","id":140732730996144}}
{"type":"move","frame":{"millis":0,"tic":0},"actor":{"position":{"x":10,"y":20,"z":0,"angle":180,"subsector":4484865928},"type":"shotgun_soldier","id":140732730996592}}
{"type":"targeted","frame":{"millis":0,"tic":0},"actor":{"position":{"x":10,"y":20,"z":0,"angle":180,"subsector":4484865928},"type":"shotgun_soldier","id":140732730996592},"target":{"type":"barrel","id":140732730996368}}
{"type":"killed","frame":{"millis":0,"tic":0},"actor":{"position":{"x":10,"y":20,"z":0,"angle":180,"subsector":4484865928},"type":"shotgun_soldier","id":140732730996592}}
{"type":"move","frame":{"millis":0,"tic":0},"actor":{"position":{"x":12,"y":13,"z":0,"angle":180,"subsector":4484865928},"type":"player","id":140732730996144}}
{"type":"killed","frame":{"millis":0,"tic":0},"actor":{"position":{"x":10,"y":20,"z":0,"angle":180,"subsector":4484865928},"type":"shotgun_soldier","id":140732730996592},"target":{"type":"player","id":140732730996144}}
```

## Testing

Right now I have a hacky manual test that I also use for Valgrind (on
Linux) mostly. To build it (and run it at the same time) run:

```bash
$ make check
```

You can either use `make check` to run the test code or run the
resulting executable directly:

```bash
$ ./src/doom/test_events
```

You should see some console output:

```
----- TESTING MODE 1 -----
X_InitTelemetry: initialized filesystem logger writing to 'doom-1586357873.log'
X_InitTelemetry: enabled telemetry mode (1)
...log start
...log armor
...log weapon
...log enemy move
...log targeted
...log enemy killed
...log player move
...log player died
...stop
X_StopTelemetry: shut down telemetry service
----- TESTING MODE 2 -----
X_InitTelemetry: starting udp logger using SDL_Net v2.0.1
X_InitTelemetry: initialized udp logger to localhost:10666
X_InitTelemetry: enabled telemetry mode (2)
...log start
...log armor
...log weapon
...log enemy move
...log targeted
...log enemy killed
...log player move
...log player died
...stop
X_StopTelemetry: shut down telemetry service
```

If you want to use Valgrind to check for leaks, I suggest you run it
on the `test_events` binary to focus testing primarily on my changes
from the original chocolate-doom source.

## Copyright and License

This fork contains works from other authors in addition to
myself. Chocolate Doom is made available under the GPLv2. A copy is
provided in [COPYING.md](/COPYING.md).

Happy Hacking.
