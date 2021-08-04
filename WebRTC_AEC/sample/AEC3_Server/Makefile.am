AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/third_party/abseil-cpp -I. -Wall

bin_PROGRAMS=AEC3Server

AEC3Server_SOURCES=AEC3Server.cc ALSA.c

AEC3Server_LDADD = $(top_builddir)/src/.libs/libwebrtc_aec.a

AEC3Server_LDFLAGS = -static-libtool-libs -L. -Wl,-Map=mapfile
LIBS= -lpthread -lm -lstdc++ -lasound
