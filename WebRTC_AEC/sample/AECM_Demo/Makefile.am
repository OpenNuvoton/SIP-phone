AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/inc -I. -Wall

bin_PROGRAMS=AECMDemo

AECMDemo_SOURCES=AECMDemo.cc ALSA.c

AECMDemo_LDADD = $(top_builddir)/src/.libs/libwebrtc_aec.a

AECMDemo_LDFLAGS = -static-libtool-libs -L. -Wl,-Map=mapfile
LIBS= -lpthread -lm -lstdc++ -lasound
