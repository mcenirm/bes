
# Automake file for dap-handler
#
# 5/17/2005 jhrg
#
# $Id: Makefile.am 12972 2006-01-05 15:41:07Z pwest $

AUTOMAKE_OPTIONS = foreign check-news

AM_CPPFLAGS = $(BES_CPPFLAGS) -I$(top_srcdir)/dap -I$(top_srcdir)/dispatch
ACLOCAL_AMFLAGS = -I conf
AM_CXXFLAGS = 

# These are not used by automake but are often useful for certain types of
# debugging. The best way to use these is to run configure as:
# ./configure --disable-shared CXXFLAGS=...
# or ./configure --enable-developer --disable-shared
# the --disable-shared is not required, but it seems to help with debuggers.
CXXFLAGS_DEBUG = -g3 -O0  -Wall -W -Wcast-align 
TEST_COV_FLAGS = -ftest-coverage -fprofile-arcs

SUBDIRS = . tests

lib_besdir=$(libdir)/bes
lib_bes_LTLIBRARIES = libdapreader_module.la

BES_SRCS = DapModule.cc DapRequestHandler.cc

BES_HDRS = DapModule.h DapRequestHandler.h 

# hack. This is probably not needed and is currently not used, but
# I'm leaving it in for now. If there are run-time issues with this
# module, add '$(BES_LIBS) $(OPENSSL_LIBS)' to LIBADD below.
# jhrg 10/13/14 
BES_LIBS = $(top_builddir)/xmlcommand/libbes_xml_command.la \
$(top_builddir)/ppt/libbes_ppt.la \
$(top_builddir)/dispatch/libbes_dispatch.la

libdapreader_module_la_SOURCES = $(BES_SRCS) $(BES_HDRS)
libdapreader_module_la_CPPFLAGS = $(DAP_CFLAGS) $(AM_CPPFLAGS)
libdapreader_module_la_LDFLAGS = -avoid-version -module 
libdapreader_module_la_LIBADD = $(DAP_SERVER_LIBS) $(DAP_CLIENT_LIBS) -ltest-types 

EXTRA_PROGRAMS = 

EXTRA_DIST = data dapreader.conf.in

CLEANFILES = *~ dapreader.conf

sample_datadir = $(datadir)/hyrax/data/dapreader
sample_data_DATA = data/dap2/fnoc1.das data/dap2/fnoc1.dds data/dap2/fnoc1.data

dap4_datadir = $(datadir)/hyrax/data/dapreader/dap4
dap4_data_DATA = data/dap4/dap4.html 

D4_xml_datadir = $(datadir)/hyrax/data/dapreader/dap4/D4-xml
D4_xml_data_DATA = data/dap4/D4-xml/DMR_0.1.xml			\
data/dap4/D4-xml/DMR_0.xml data/dap4/D4-xml/DMR_1.xml		\
data/dap4/D4-xml/DMR_2.1.xml data/dap4/D4-xml/DMR_2.xml		\
data/dap4/D4-xml/DMR_3.1.xml data/dap4/D4-xml/DMR_3.2.xml	\
data/dap4/D4-xml/DMR_3.3.xml data/dap4/D4-xml/DMR_3.4.xml	\
data/dap4/D4-xml/DMR_3.5.xml data/dap4/D4-xml/DMR_3.xml		\
data/dap4/D4-xml/DMR_4.1.xml data/dap4/D4-xml/DMR_4.xml		\
data/dap4/D4-xml/DMR_5.1.xml data/dap4/D4-xml/DMR_5.xml		\
data/dap4/D4-xml/DMR_6.1.xml data/dap4/D4-xml/DMR_6.2.xml	\
data/dap4/D4-xml/DMR_6.xml data/dap4/D4-xml/DMR_7.1.xml		\
data/dap4/D4-xml/DMR_7.2.xml data/dap4/D4-xml/DMR_7.3.xml	\
data/dap4/D4-xml/DMR_7.4.xml data/dap4/D4-xml/DMR_7.5.xml	\
data/dap4/D4-xml/DMR_7.6.xml data/dap4/D4-xml/DMR_7.7.xml	\
data/dap4/D4-xml/DMR_7.xml data/dap4/D4-xml/DMR_8.xml

dmr_testsuite_datadir = $(datadir)/hyrax/data/dapreader/dap4/dmr-testsuite
dmr_testsuite_data_DATA = data/dap4/dmr-testsuite/test_array_1.xml	\
data/dap4/dmr-testsuite/test_simple_1.dmr 				\
data/dap4/dmr-testsuite/test_array_10.xml				\
data/dap4/dmr-testsuite/test_array_11.xml				\
data/dap4/dmr-testsuite/test_array_2.xml				\
data/dap4/dmr-testsuite/test_array_3.xml				\
data/dap4/dmr-testsuite/test_array_4.xml				\
data/dap4/dmr-testsuite/test_array_5.xml				\
data/dap4/dmr-testsuite/test_array_6.xml				\
data/dap4/dmr-testsuite/test_array_7.xml				\
data/dap4/dmr-testsuite/test_array_8.xml				\
data/dap4/dmr-testsuite/test_escaped_paths.xml				\
data/dap4/dmr-testsuite/test_simple_1.xml				\
data/dap4/dmr-testsuite/test_simple_10.xml				\
data/dap4/dmr-testsuite/test_simple_2.xml				\
data/dap4/dmr-testsuite/test_simple_3.xml				\
data/dap4/dmr-testsuite/test_simple_3_error_1.xml			\
data/dap4/dmr-testsuite/test_simple_3_error_2.xml			\
data/dap4/dmr-testsuite/test_simple_3_error_3.xml			\
data/dap4/dmr-testsuite/test_simple_4.xml				\
data/dap4/dmr-testsuite/test_simple_5.xml				\
data/dap4/dmr-testsuite/test_simple_6.xml				\
data/dap4/dmr-testsuite/test_simple_7.xml				\
data/dap4/dmr-testsuite/test_simple_8.xml				\
data/dap4/dmr-testsuite/test_simple_9.1.xml				\
data/dap4/dmr-testsuite/test_simple_9.2.xml				\
data/dap4/dmr-testsuite/test_simple_9.xml


install-data-local: dapreader.conf
	@currdate=`date +"%y%m%d%H%M"`; \
	test -d $(DESTDIR)$(sysconfdir)/bes || $(MKDIR_P) $(DESTDIR)$(sysconfdir)/bes; \
	test -d $(DESTDIR)$(sysconfdir)/bes/modules || $(MKDIR_P) $(DESTDIR)$(sysconfdir)/bes/modules; \
	test -f $(DESTDIR)$(sysconfdir)/bes/modules/dapreader.conf \
	    && mv -f $(DESTDIR)$(sysconfdir)/bes/modules/dapreader.conf $(DESTDIR)$(sysconfdir)/bes/modules/dapreader.conf.$$currdate; \
	$(INSTALL_DATA) dapreader.conf $(DESTDIR)$(sysconfdir)/bes/modules/dapreader.conf

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/bes/modules/dapreader.conf

dapreader.conf: dapreader.conf.in $(top_builddir)/config.status
	sed -e "s%[@]bes_modules_dir[@]%${lib_besdir}%" $< > dapreader.conf

#reader.conf: reader.conf.in $(top_builddir)/config.status
#	sed -e "s%[@]pkgdatadir[@]%${pkgdatadir}%" \
#                -e "s%[@]sysconfdir[@]%${sysconfdir}%" \
#                -e "s%[@]pkglibdir[@]%${pkglibdir}%" \
#                -e "s%[@]datarootdir[@]%${datarootdir}%" \
#                -e "s%[@]datadir[@]%${datadir}%" \
#                -e "s%[@]bindir[@]%${bindir}%" \
#                -e "s%[@]bes_modules_dir[@]%${lib_besdir}%" $< > reader.conf

distclean-local:

C4_DIR=./cccc
.PHONY: cccc
cccc:	
	cccc --outdir=$(C4_DIR) $(libdapreader_module_la_SOURCES)

###########################################################################

# Fortify targets.

.PHONY: fortify
fortify:
	sourceanalyzer -b @PACKAGE@ $(MAKE)
	sourceanalyzer -b @PACKAGE@ -scan -f @PACKAGE@-@PACKAGE_VERSION@.fpr

# Use this to clean the fortify project.
.PHONY: fortifyclean
fortifyclean:
	sourceanalyzer -b @PACKAGE@ -clean

###########################################################################
#
# Build linux RPMs

srpm: dist
	rpmbuild -ts --clean $(RPM_OPTIONS) @PACKAGE@-@PACKAGE_VERSION@.tar.gz

rpm: dist
	rpmbuild -tb --clean $(RPM_OPTIONS) @PACKAGE@-@PACKAGE_VERSION@.tar.gz

###########################################################################

# Build OS/X Packages. The strange operations with configure and make
# are there so that the values built into bes-config will match the mac
# osx install dirs and not the temp directory used to build the packages

PACKAGEMAKER=/Developer/usr/bin/packagemaker
PKG_CONF_FLAGS=

clean-pkg:
	-rm -rf mac_osx @PACKAGE@-@PACKAGE_VERSION@.pkg @PACKAGE@-@PACKAGE_VERSION@

pkg-build: clean-pkg
	./configure --prefix=$(prefix) --disable-dependency-tracking $(PKG_CONF_FLAGS)
	make clean all
	DESTDIR=`pwd`/mac_osx make install

pkg-main: pkg-build
	./OSX_Resources/update_mac_package_contents.pl README
	cat ./OSX_Resources/InstallationCheck.proto | sed -e "s^_PREFIX_^$(prefix)^g" > foo
	mv foo ./OSX_Resources/InstallationCheck
	chmod 755 ./OSX_Resources/InstallationCheck
	cat ./OSX_Resources/Info.plist.proto | \
	   sed -e "s^_PREFIX_^$(prefix)^g" \
	       -e "s^_FULL_VERSION_^@PACKAGE_VERSION@^g" \
	       -e "s^_MAJOR_VERSION_^@PACKAGE_MAJOR_VERSION@^g" \
	       -e "s^_MINOR_VERSION_^@PACKAGE_MINOR_VERSION@^g" \
	       > foo
	mv foo ./OSX_Resources/Info.plist
	${PACKAGEMAKER} --root mac_osx --id org.opendap.@PACKAGE@ \
		--title "@PACKAGE@ @PACKAGE_VERSION@" --version @PACKAGE_VERSION@ \
		--out @PACKAGE@-@PACKAGE_VERSION@.pkg --resources OSX_Resources

pkg-dmg: pkg-main
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@.dmg
	mkdir @PACKAGE@-@PACKAGE_VERSION@
	cp -R @PACKAGE@-@PACKAGE_VERSION@.pkg @PACKAGE@-@PACKAGE_VERSION@
	cp README NEWS @PACKAGE@-@PACKAGE_VERSION@
	dropdmg -i --sanitize-for-servers --format zlib @PACKAGE@-@PACKAGE_VERSION@
	-rm -rf @PACKAGE@-@PACKAGE_VERSION@

pkg: pkg-main


