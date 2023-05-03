# Chocolate Doom w/ Telemetry
![Compile and Check](https://github.com/voutilad/chocolate-doom/workflows/Compile%20and%20Check/badge.svg?branch=personal)

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

- Ubuntu 20.04 LTS
- Windows 10 w/ msys2-x86_64 v20190524 (see notes for WSL2 below)
- OpenBSD-current
- macOS v10.15 (but only occasionally...ymmv)

I do this primarily using the automake/autoconf tooling and the
resulting Makefiles, so if you have the proper dependencies (python,
SDL2, SDL_Net, SDL_Mixer, etc.) it should just work in building local
executables.

For instance, run the autogen script:

```bash
$ ./autogen.sh
```

> Note: if building on Win10 64-bit, I recommend running:
> `./autogen.sh --host=i686-w64-mingw32`

Then just build:

```bash
$ make
```

The resulting executable will be in `./src/` as `chocolate-doom` or
`chocolate-doom.exe` (on Windows). You don't need to build the install
package or bundled app, but if you do follow the official Chocolate
Doom docs from their wiki.

### Optional 3rd Party Dependencies
#### Kafka
If you'd like to add in Kafka support:
- build/install [librdkafka](https://github.com/edenhill/librdkafka)
- make sure you've followed the `librdkafka` docs for SASL2 support
- make sure it's accessible via `pkg-config`

If you'd like to disable building with `librdkafka`, pass
`--without-librdkafka` to `./configure`.

#### WebSockets
Right now, websocket support requires `libtls` from the
[LibreSSL](https://libressl.org) project.

### Leveraging WSL2 on Windows 10
Msys2/mingw can be challenging to get working if you're trying to use
`librdkafka` as a telemetry destination.

Using WSL2 can vastly simplify things, but you need to also do the
following (assuming you use VcXsrv):

- enable `X11Forwarding` in `/etc/ssh/sshd_config`
- disable "Native OpenGL" in VcXsrv session
- set `LIBGL_ALWAYS_INDIRECT=0` in your enviornment
- disable authentication in VcXsrv or learn how to get it working

Without the above, SDL2 seems to not work (at least as of Win10 v2004,
build 19041.508).

> Note: the above assumes using Ubuntu as the WSL2 environment

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
The event structure takes the following shape and is emitted in JSON:

```json
{
  "damage": 10,
  "counter": 479,
  "session": "7372d46f7d83f353d9e1c18f",
  "type": "hit",
  "frame": {
    "millis": 11829,
    "tic": 414
  },
  "actor": {
    "position": {
      "x": 82759579,
      "y": -186153129,
      "z": 0,
      "angle": 3292528640,
      "subsector": 140365466405016
    },
    "type": "player",
    "health": 100,
    "armor": 0,
    "id": 140365466484344
  },
  "target": {
    "position": {
      "x": 85983232,
      "y": -213909504,
      "z": -1048576,
      "angle": 1073741824,
      "subsector": 140365466404840
    },
    "type": "barrel",
    "health": 10,
    "id": 140365466491112
  }
}
```

The `target` is optional.

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
