
AM_CFLAGS = @SDL_CFLAGS@                                           \
            @SDLMIXER_CFLAGS@                                      \
            -I$(top_srcdir)/textscreen -I$(top_srcdir)/src

noinst_LIBRARIES = libsetup.a

libsetup_a_SOURCES =                            \
    compatibility.c   compatibility.h           \
    display.c         display.h                 \
    joystick.c        joystick.h                \
    keyboard.c        keyboard.h                \
    mainmenu.c                                  \
    mode.c            mode.h                    \
    mouse.c           mouse.h                   \
    multiplayer.c     multiplayer.h             \
    sound.c           sound.h                   \
    execute.c         execute.h                 \
    txt_joyaxis.c     txt_joyaxis.h             \
    txt_joybinput.c   txt_joybinput.h           \
    txt_keyinput.c    txt_keyinput.h            \
    txt_mouseinput.c  txt_mouseinput.h          \
    telemetry.c       telemetry.h

EXTRA_DIST=                                     \
    CMakeLists.txt                              \
    setup_icon.c

appdir = $(prefix)/share/applications
app_DATA = @PACKAGE_RDNS@.Setup.desktop

CLEANFILES = $(app_DATA)

@PACKAGE_RDNS@.Setup.desktop : Setup.desktop
	cp Setup.desktop $@

if HAVE_ICONS

setup_icon.c : $(top_builddir)/data/setup.png
	$(top_builddir)/data/convert-icon $(top_builddir)/data/setup.png $@

endif
