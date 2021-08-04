AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/inc -I. -Wall

bin_PROGRAMS=AECMClient

AECMClient_SOURCES=AECMClient.cc ALSA.c

AECMClient_LDADD = $(top_builddir)/src/.libs/libwebrtc_aec.a

AECMClient_LDFLAGS = -static-libtool-libs -L. -Wl,-Map=mapfile
LIBS= -lpthread -lm -lstdc++ -lasound
