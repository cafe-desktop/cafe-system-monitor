## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPROCMAN_DATADIR=\""$(datadir)/procman/"\" \
	-DCAFELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DPKGLIBEXECDIR=\""$(pkglibexecdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	@PROCMAN_CFLAGS@ \
	@SYSTEMD_CFLAGS@

bin_PROGRAMS = cafe-system-monitor

cafe_system_monitor_cpp_files = \
	argv.cpp \
	procman.cpp \
	interface.cpp \
	callbacks.cpp \
	load-graph.cpp \
	proctable.cpp \
	prettytable.cpp \
	util.cpp \
	procactions.cpp \
	procdialogs.cpp \
	memmaps.cpp \
	openfiles.cpp \
	procproperties.cpp \
	smooth_refresh.cpp \
	disks.cpp \
	selinux.cpp \
	cgroups.cpp \
	procman_gksu.cpp \
	procman_pkexec.cpp \
	sysinfo.cpp \
	lsof.cpp \
	selection.cpp \
	settings-keys.cpp \
	iconthemewrapper.cpp

cafe_system_monitor_c_files = \
	gsm_color_button.c

cafe_system_monitor_h_files = \
	$(cafe_system_monitor_cpp_files:.cpp=.h) \
	$(cafe_system_monitor_c_files:.c=.h) \
	defaulttable.h

cafe_system_monitor_SOURCES = \
	$(cafe_system_monitor_h_files) \
	$(cafe_system_monitor_cpp_files) \
	$(cafe_system_monitor_c_files)

cafe_system_monitor_CXXFLAGS = \
	$(WARN_CXXFLAGS)

cafe_system_monitor_CFLAGS = \
	$(WARN_CFLAGS)

cafe_system_monitor_LDADD = @PROCMAN_LIBS@ @SYSTEMD_LIBS@ libbacon.la


noinst_LTLIBRARIES = libbacon.la
libbacon_la_SOURCES = \
	bacon-message-connection.c \
	bacon-message-connection.h
libbacon_la_CFLAGS = \
	$(WARN_CFLAGS)

specdir = $(datadir)/procman


gsettings_ENUM_NAMESPACE = org.cafe.system-monitor
gsettings_ENUM_FILES = $(top_srcdir)/src/*.h

gsettingsschema_in_files = org.cafe.system-monitor.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)

@GSETTINGS_RULES@

dist_noinst_DATA = \
	$(gsettings_in_file)

CLEANFILES = \
	$(gsettings_SCHEMAS)

EXTRA_DIST = \
	$(gsettingsschema_in_files)

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)
