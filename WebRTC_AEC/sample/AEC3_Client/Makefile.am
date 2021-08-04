AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/third_party/abseil-cpp -I. -Wall

bin_PROGRAMS=AEC3Client

AEC3Client_SOURCES=AEC3Client.cc ALSA.c

AEC3Client_LDADD = $(top_builddir)/src/.libs/libwebrtc_aec.a

AEC3Client_LDFLAGS = -static-libtool-libs -L. -Wl,-Map=mapfile
LIBS= -lpthread -lm -lstdc++ -lasound
