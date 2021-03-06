# Prebuilt toolchains don't build the way most code does.
INHIBIT_DEFAULT_DEPS = "1"

# License applies to this recipe code, not the toolchain itself
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/LICENSE;md5=3f40d7994397109285ec7b81fdeb3b58 \
                    file://${COREBASE}/meta/COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"

PV = "${CSL_VER_MAIN}"
PR = "r2"

CSL_VER_MAIN ??= ""

python () {
    if not d.getVar("CSL_VER_MAIN"):
        raise bb.parse.SkipPackage("External CSL toolchain not configured (CSL_VER_MAIN not set).")
}

# The prefix which goes on package names; either "" or ${BBEXTENDVARIANT}-
# (including the hyphen).
EXTERNAL_SOURCERY_TOOLCHAIN_VARIANT = "${@['', (d.getVar("BBEXTENDVARIANT", True) or "") + '-'][(d.getVar("BBEXTENDVARIANT", True) or "") != ""]}"
# There's a fancy bit of setup code that copies/extracts relevant hunks of
# the prebuilts, including multilib-specific binaries. A single target covers
# it so sstate can save us time.
EXTERNAL_SOURCERY_TOOLCHAIN_SETUP = "${EXTERNAL_SOURCERY_TOOLCHAIN_VARIANT}sourcery-tc-prebuilt:do_setup_shared"

# We keep needing this function.

# cp -a, only "do what I mean".
#
# Specifically, do NOT try to preserve ownership or ACLs; note that
# ACLs would be implied by "mode", but the default behavior will still
# preserve regular permission bits.
#
# It seems that this can be done with cp, because
#   cp -R dirname/. targetname
# works whether or not targetname already exists.
#
# If $warn_or_fail is set to "true", doesn't error out on
# missing files.
tccp() {
        cp -dR --preserve=timestamps "$@" || ${warn_or_fail:-false}
}

# Fancy (shared) code to set up the symlink from a multilib subdirectory
# back to the sysroot directory.

# Postprocessing, which needs to be in both the base recipe and
# the cross-canadian (used for SDK). Accommodates the multilib-friendly
# toolchain's model that sysroot/<multilib-derived-name> is the "real"
# sysroot by symlinking that back to the sysroot.

SYSROOT_PREPROCESS_FUNCS += "external_toolchain_sysroot_adjust"

external_toolchain_sysroot_adjust() {
        # Pick up any symlinks in the top of the sysroot
        external_toolchain_links ${SYSROOT_DESTDIR}

        if [ "${TUNE_PKGARCH}" = "i586" ]; then
                rm -f ${SYSROOT_DESTDIR}/system32
                ln -s . ${SYSROOT_DESTDIR}/system32
        fi
}

external_toolchain_links() {
        # Can't use the wrapper here in the cross-canadian case;
        # base gcc + $CCARGS should work in both cases.
        dest_sysroot="${CSL_TARGET_CORE}"
        if [ -n "$dest_sysroot" ] ; then
                dirs=$dest_sysroot
                target=
                while [ "${dirs%/*}" != "$dirs" ] ; do
                        dirs=${dirs%/*}
                        target=..${target:+/$target}
                done
                rm -f $1/$dest_sysroot
                ln -s "${target:-.}" "$1/$dest_sysroot"
        fi
        # If the usr/lib directory doesn't exist, the toolchain fails to even
        # try to find crti.o in a completely different directory (usr/lib64)
        install -d $1/usr/lib
}

# Create links using tuning-specific names.
external_toolchain_additional_links() {
	if [ -n "$1" ]; then
		bindir="$1"
	else
		bindir="${TOOLCHAIN_SHARED_BINDIR}"
	fi
	target_prefix="${TARGET_PREFIX}"
	binary_prefix="${TOOLCHAIN_BINARY_PREFIX}"
	echo "Creating toolchain symlinks for $target_prefix."

	echo "looking in $bindir"
	for file in $(cd $bindir; echo "$binary_prefix"*); do
		linkname="$target_prefix${file#$binary_prefix}"
		echo "linking $linkname => $file"
		ln -s "$file" "$bindir/$linkname" || true
		case $binary_prefix in
		powerpc*)
			# add SPE variants in case we need them.
			linkname="$(echo $linkname | sed -e s/-gnu-/-gnuspe-/)"
			ln -s "$file" "$linkname" || true
			;;
		esac
	done
}

# Create links out to the external toolchain.
external_toolchain_binary_links() {
	# Experimental: Also do the symlinking here.
	external="$(readlink -f "${EXTERNAL_TOOLCHAIN}")"
	bindir="${TOOLCHAIN_SHARED_BINDIR}"
	host_prefix="${HOST_PREFIX}"
	binary_prefix="${TOOLCHAIN_BINARY_PREFIX}"
	sysroot_dir="${STAGING_DIR_HOST}"

	all_files=""

	echo "Creating base toolchain symlinks."
	for dir in bin libexec sbin; do
		files=$(cd $external; echo $dir/$binary_prefix*)
		case $files in
		*\**)	echo "No $dir files.";;
		*)	all_files="$all_files $files";;
		esac
	done

	mkdir -p $bindir
	mkdir -p $sysroot_dir

	# Try to symlink these to their CS-specific names
	for file in prelink mklibs mklibs-readelf prelink-rtld; do
		ln -s "$binary_prefix$file" "$bindir/$file" || true
	done

	for path in $all_files; do
		dir=${path%/*}
		file=${path##*/}
		ln -s "$external/$path" "$bindir/$file" || true
	done
	# We only want the extra links if HOST_PREFIX is set, otherwise
	# they're going to cause problems.
	if [ -n "${HOST_PREFIX}" ]; then
		external_toolchain_additional_links
	fi
	ln -s "${host_prefix}ld" "$bindir/${host_prefix}ld.bfd" || true
}

# There's some files which are typically part of the CS sysroot, and
# which should be installed by our prebuilts package, or by the kernel.
# And they should NOT be installed by eglibc.
LIKELY_KERNEL_HEADERS = "${includedir}/asm* \
	${includedir}/drm \
	${includedir}/linux \
	${includedir}/mtd \
	${includedir}/rdma \
	${includedir}/scsi \
	${includedir}/sound \
	${includedir}/uapi \
	${includedir}/video \
	${includedir}/xen \
"

# So, in aarch64, everything is in /lib64... EXCEPT ld-linux.so.
EXTERNAL_SOURCERY_EXTRA_SYSROOT_FILES_append_aarch64 = " lib/ld-linux-aarch64.so.1"
