AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libheif.la

libheif_ladir = \
        $(includedir)/libheif

libheif_la_CPPFLAGS =
libheif_la_CFLAGS = \
  $(CFLAG_VISIBILITY)
libheif_la_CXXFLAGS = \
  $(CFLAG_VISIBILITY)

# libheif_la_LDFLAGS = -version-info $(LIBDE265_CURRENT):$(LIBDE265_REVISION):$(LIBDE265_AGE)

libheif_la_SOURCES = \
  box.h \
  box.cc






bin_PROGRAMS = heif

# AM_CPPFLAGS = -I../libde265

heif_DEPENDENCIES = libheif.la # ../libde265/libde265.la
heif_CXXFLAGS =
heif_LDFLAGS =
heif_LDADD = libheif.la -lstdc++ # ../libde265/libde265.la -lstdc++
heif_SOURCES = heif.cc
