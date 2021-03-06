#
# Copyright (C) 2012, 2013 Wind River Systems, Inc.
#
include sourcery-tc-shared.inc

INHIBIT_DEFAULT_DEPS = "1"

SRC_URI = "file://prelink.conf"

# The following is needed for bitbake to find files when building
# for a multilib.
FILESEXTRAPATHS_prepend := "${THISDIR}/sourcery-tc:"

do_configure[depends] += "${EXTERNAL_SOURCERY_TOOLCHAIN_SETUP}"
SSTATEPREINSTFUNCS += " external_toolchain_binary_links"

# In fact, mklibs and prelink are really cross tools; they run on
# the host, and affect target binaries. But because they are in
# principle sort of architecture-neutral, they're considered -native.
PROVIDES = "\
             ${@base_conditional('PREFERRED_PROVIDER_mklibs-native', PN, 'mklibs-native', '', d)} \
             ${@base_conditional('PREFERRED_PROVIDER_prelink-native', PN, 'prelink-native', '', d)} \
	     "

# We don't actually need this, but if it isn't around, the target
# gcc build can fail because the configure run to figure out how to
# build host binaries fails because there's no host gmp available.
DEPENDS += "mpfr-native gmp-native libmpc-native zlib-native"

# These packages are "provided", but the recipe doesn't do anything
# for them. They're copied in from the prebuilts by the tcmode file,
# conf/distro/include/tcmode-external-sourcery.inc. We just need to indicate
# somewhere that they have been provided.
PROVIDES += "\
	virtual/${TARGET_PREFIX}gcc \
	virtual/${TARGET_PREFIX}g++ \
	virtual/${TARGET_PREFIX}gcc-initial \
	virtual/${TARGET_PREFIX}gcc-intermediate \
	virtual/${TARGET_PREFIX}binutils \
	virtual/${TARGET_PREFIX}compilerlibs \
	binutils-cross \
	gcc-cross \
	gdb-cross \
"

FILES_mklibs-native = "${bindir}/*mklibs"
FILES_prelink-native = "${bindir}/*prelink \
        ${bindir}/*prelink-rtld \
        ${sysconfdir}/prelink.conf"

inherit cross

# Suppress the usr/whatever/host-triplet behavior, because the CS
# utilities have some predefined rules, like prelink calling
# ../libexec/PREFIX-prelink-rtld. Note that we also need a
# plain prelink-rtld in ${bindir} for Yocto components.
bindir = "${exec_prefix}/bin"
sbindir = "${exec_prefix}/sbin"
libexecdir = "${exec_prefix}/libexec"

DO_MKLIBS = "${@base_conditional('PREFERRED_PROVIDER_mklibs-native', PN, 'true', 'false', d)}"
DO_PRELINK = "${@base_conditional('PREFERRED_PROVIDER_prelink-native', PN, 'true', 'false', d)}"

do_install() {
	install -d ${D}${bindir}
	if ${DO_MKLIBS}; then
		sed -e s/${CSL_TARGET_SYS}-// < ${TOOLCHAIN_SHARED_BINDIR}/mklibs \
			> ${D}${bindir}/mklibs
		chmod a+x ${D}${bindir}/mklibs
		tccp -L ${TOOLCHAIN_SHARED_BINDIR}/mklibs-readelf ${D}${bindir}
	fi
	if ${DO_PRELINK}; then
		install -d ${D}${sysconfdir}
		install -d ${D}${sbindir}
		install -d ${D}${libexecdir}
		install -m 0644 ${WORKDIR}/prelink.conf ${D}${sysconfdir}/prelink.conf
		# The CS prelink looks for prelink-rtld in libexec, not in
		# $PATH, but some other Yocto components look for it in $PATH.
		ln -sf ${TOOLCHAIN_SHARED_BINDIR}/${CSL_TARGET_SYS}-prelink-rtld ${D}${libexecdir}/.
		ln -sf ${TOOLCHAIN_SHARED_BINDIR}/${CSL_TARGET_SYS}-prelink-rtld ${D}${sbindir}/.
		# And make sure there's links without the prefix.
		ln -sf ${CSL_TARGET_SYS}-prelink-rtld ${D}${libexecdir}/prelink-rtld
		ln -sf ${CSL_TARGET_SYS}-prelink-rtld ${D}${sbindir}/prelink-rtld

		# Prelink is typically installed in sbin by Yocto.
		tccp -L ${TOOLCHAIN_SHARED_BINDIR}/prelink ${D}${sbindir}/prelink
	fi
}
