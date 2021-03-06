noinst_LIBRARIES = libpassword.a
auth_moduledir = $(moduledir)/auth

# automake seems to force making this unconditional..
NOPLUGIN_LDFLAGS =

if GSSAPI_PLUGIN
GSSAPI_LIB = libmech_gssapi.la
endif

if LDAP_PLUGIN
LDAP_LIB = libauthdb_ldap.la
endif

auth_module_LTLIBRARIES = \
	$(GSSAPI_LIB) \
	$(LDAP_LIB) \
	libauthdb_imap.la

pkglibexecdir = $(libexecdir)/dovecot

pkglibexec_PROGRAMS = auth checkpassword-reply

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/lib \
	-I$(top_srcdir)/src/lib-auth \
	-I$(top_srcdir)/src/lib-test \
	-I$(top_srcdir)/src/lib-dict \
	-I$(top_srcdir)/src/lib-dns \
	-I$(top_srcdir)/src/lib-sql \
	-I$(top_srcdir)/src/lib-settings \
	-I$(top_srcdir)/src/lib-stats \
	-I$(top_srcdir)/src/lib-ntlm \
	-I$(top_srcdir)/src/lib-otp \
	-I$(top_srcdir)/src/lib-master \
	-DAUTH_MODULE_DIR=\""$(auth_moduledir)"\" \
	-DPKG_LIBEXECDIR=\""$(pkglibexecdir)"\" \
	-DPKG_RUNDIR=\""$(rundir)"\" \
	$(AUTH_CFLAGS)

auth_LDFLAGS = -export-dynamic

libpassword_a_SOURCES = \
	mycrypt.c \
	password-scheme.c \
	password-scheme-crypt.c \
	password-scheme-md5crypt.c \
	password-scheme-scram.c \
	password-scheme-otp.c \
	password-scheme-rpa.c

auth_libs = \
	libpassword.a \
	../lib-ntlm/libntlm.a \
	../lib-otp/libotp.a \
	$(LIBDOVECOT_SQL)

auth_LDADD = $(auth_libs) $(LIBDOVECOT) $(AUTH_LIBS)
auth_DEPENDENCIES = $(auth_libs) $(LIBDOVECOT_DEPS)

ldap_sources = db-ldap.c passdb-ldap.c userdb-ldap.c

auth_SOURCES = \
	auth.c \
	auth-cache.c \
	auth-client-connection.c \
	auth-master-connection.c \
	auth-postfix-connection.c \
	mech-otp-skey-common.c \
	mech-plain-common.c \
	auth-penalty.c \
	auth-request.c \
	auth-request-handler.c \
	auth-request-stats.c \
	auth-request-var-expand.c \
	auth-settings.c \
	auth-stats.c \
	auth-fields.c \
	auth-token.c \
	auth-worker-client.c \
	auth-worker-server.c \
	db-checkpassword.c \
	db-dict.c \
	db-dict-cache-key.c \
	db-sql.c \
	db-passwd-file.c \
	main.c \
	mech.c \
	mech-anonymous.c \
	mech-plain.c \
	mech-login.c \
	mech-cram-md5.c \
	mech-digest-md5.c \
	mech-external.c \
	mech-gssapi.c \
	mech-ntlm.c \
	mech-otp.c \
	mech-scram-sha1.c \
	mech-skey.c \
	mech-rpa.c \
	mech-apop.c \
	mech-winbind.c \
	mech-dovecot-token.c \
	passdb.c \
	passdb-blocking.c \
	passdb-bsdauth.c \
	passdb-cache.c \
	passdb-checkpassword.c \
	passdb-dict.c \
	passdb-passwd.c \
	passdb-passwd-file.c \
	passdb-pam.c \
	passdb-shadow.c \
	passdb-sia.c \
	passdb-vpopmail.c \
	passdb-sql.c \
	passdb-static.c \
	passdb-template.c \
	userdb.c \
	userdb-blocking.c \
	userdb-checkpassword.c \
	userdb-dict.c \
	userdb-nss.c \
	userdb-passwd.c \
	userdb-passwd-file.c \
	userdb-prefetch.c \
	userdb-static.c \
	userdb-vpopmail.c \
	userdb-sql.c \
	userdb-template.c \
	$(ldap_sources)

headers = \
	auth.h \
	auth-cache.h \
	auth-client-connection.h \
	auth-common.h \
	auth-master-connection.h \
	auth-postfix-connection.h \
	mech-otp-skey-common.h \
	mech-plain-common.h \
	auth-penalty.h \
	auth-request.h \
	auth-request-handler.h \
	auth-request-stats.h \
	auth-request-var-expand.h \
	auth-settings.h \
	auth-stats.h \
	auth-fields.h \
	auth-token.h \
	auth-worker-client.h \
	auth-worker-server.h \
	db-dict.h \
	db-ldap.h \
	db-sql.h \
	db-passwd-file.h \
	db-checkpassword.h \
	mech.h \
	mycrypt.h \
	passdb.h \
	passdb-blocking.h \
	passdb-cache.h \
	passdb-template.h \
	password-scheme.h \
	userdb.h \
	userdb-blocking.h \
	userdb-template.h \
	userdb-vpopmail.h

if GSSAPI_PLUGIN
libmech_gssapi_la_LDFLAGS = -module -avoid-version
libmech_gssapi_la_LIBADD = $(KRB5_LIBS)
libmech_gssapi_la_CPPFLAGS = $(AM_CPPFLAGS) $(KRB5_CFLAGS) -DPLUGIN_BUILD
libmech_gssapi_la_SOURCES = mech-gssapi.c
endif

if LDAP_PLUGIN
libauthdb_ldap_la_LDFLAGS = -module -avoid-version
libauthdb_ldap_la_LIBADD = $(LDAP_LIBS)
libauthdb_ldap_la_CPPFLAGS = $(AM_CPPFLAGS) -DPLUGIN_BUILD
libauthdb_ldap_la_SOURCES = $(ldap_sources)
endif

libauthdb_imap_la_LDFLAGS = -module -avoid-version
libauthdb_imap_la_LIBADD = \
	../lib-imap-client/libimap_client.la \
	$(LIBDOVECOT)
libauthdb_imap_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src/lib-imap \
	-I$(top_srcdir)/src/lib-imap-client
libauthdb_imap_la_SOURCES = passdb-imap.c

pkginc_libdir=$(pkgincludedir)
pkginc_lib_HEADERS = $(headers)

checkpassword_reply_LDADD = $(LIBDOVECOT)
checkpassword_reply_DEPENDENCIES = $(LIBDOVECOT_DEPS)

checkpassword_reply_sources = \
	checkpassword-reply.c

stats_moduledir = $(moduledir)/stats
stats_module_LTLIBRARIES = libstats_auth.la

libstats_auth_la_LDFLAGS = -module -avoid-version
libstats_auth_la_LIBADD = auth-stats.lo $(LIBDOVECOT)
libstats_auth_la_DEPENDENCIES = auth-stats.lo
libstats_auth_la_SOURCES =

test_programs = \
	test-auth-cache \
	test-auth-request-var-expand \
	test-db-dict

noinst_PROGRAMS = $(test_programs)

test_libs = \
	../lib-test/libtest.la \
	../lib/liblib.la

test_auth_cache_SOURCES = test-auth-cache.c
test_auth_cache_LDADD = auth-cache.o $(test_libs)
test_auth_cache_DEPENDENCIES = $(pkglibexec_PROGRAMS) $(test_libs)

test_auth_request_var_expand_SOURCES = test-auth-request-var-expand.c
test_auth_request_var_expand_LDADD = auth-request-var-expand.o auth-fields.o $(test_libs)
test_auth_request_var_expand_DEPENDENCIES = $(pkglibexec_PROGRAMS) $(test_libs)

test_db_dict_SOURCES = test-db-dict.c
test_db_dict_LDADD = db-dict-cache-key.o $(test_libs)
test_db_dict_DEPENDENCIES = $(pkglibexec_PROGRAMS) $(test_libs)

check: check-am check-test
check-test: all-am
	for bin in $(test_programs); do \
	  if ! $(RUN_TEST) ./$$bin; then exit 1; fi; \
	done
