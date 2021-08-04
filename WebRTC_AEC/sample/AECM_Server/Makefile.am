AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/inc -I. -Wall

bin_PROGRAMS=AECMServer

AECMServer_SOURCES=AECMServer.cc ALSA.c

AECMServer_LDADD = $(top_builddir)/src/.libs/libwebrtc_aec.a

AECMServer_LDFLAGS = -static-libtool-libs -L. -Wl,-Map=mapfile
LIBS= -lpthread -lm -lstdc++ -lasound
