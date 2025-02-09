# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the linux ext2-filesystem routines.
#

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

# Build the ext2.ko module from ext2 source files
obj-m += ext2.o

# List of source files for the ext2 module
ext2-y := balloc.o dir.o file.o ialloc.o inode.o \
	  ioctl.o ext2_log.o super.o symlink.o trace.o	\
	  namei.o

CFLAGS_trace.o := -I$(src)

# Include extended attribute, POSIX ACL, and security-related source files if enabled
ext2-$(CONFIG_EXT2_FS_XATTR)	 += xattr.o xattr_user.o xattr_trusted.o
ext2-$(CONFIG_EXT2_FS_POSIX_ACL) += acl.o
ext2-$(CONFIG_EXT2_FS_SECURITY)	 += xattr_security.o
# Build the test.ko module from free_space.o
obj-m += test.o
test-y := free_space.o


# Compile all modules
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean build artifacts
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

