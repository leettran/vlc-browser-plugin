ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = \
	1.11 \
	-Wall \
	check-news \
	dist-xz \
	no-dist-gzip

DIST_SUBDIRS= share common activex
SUBDIRS = common
if BUILD_ACTIVEX
SUBDIRS += activex
endif

EXTRA_DIST = \
	autogen.sh
