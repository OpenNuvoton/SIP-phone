AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/third_party/abseil-cpp -I. -Wall

bin_PROGRAMS=AEC3Demo

AEC3Demo_SOURCES=AEC3Demo.cc ALSA.c

AEC3Demo_LDADD = $(top_builddir)/src/.libs/libwebrtc_aec.a

AEC3Demo_LDFLAGS = -static-libtool-libs -L. -Wl,-Map=mapfile
LIBS= -lpthread -lm -lstdc++ -lasound
