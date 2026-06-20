SUBDIRS = lib

bin_PROGRAMS = fbterm-mod

fbterm_mod_SOURCES = fbconfig.cpp fbconfig.h fbio.cpp fbio.h fbshell.cpp fbshell.h fbshellman.cpp fbshellman.h fbterm.cpp \
	fbterm.h font.cpp font.h input.cpp input.h input_key.h mouse.cpp mouse.h screen.cpp screen.h improxy.cpp improxy.h \
	screen_render.cpp screen_sixel.cpp fbdev.cpp fbdev.h vesadev.cpp vesadev.h vbe.h
EXTRA_fbterm_mod_SOURCES = signalfd.h

fbterm_mod_CXXFLAGS = -fno-exceptions -fno-rtti -Ilib @FT2_CFLAGS@ @FC_CFLAGS@
fbterm_mod_LDADD = lib/libshell.a @FT2_LIBS@ @FC_LIBS@ @X86_LIBS@ -lutil

if HAVE_ARM_NEON
fbterm_mod-screen_sixel.$(OBJEXT): CXXFLAGS += @NEON_CFLAGS@
fbterm_mod-screen_render.$(OBJEXT): CXXFLAGS += @NEON_CFLAGS@
endif
