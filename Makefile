# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the linux ext2-filesystem routines.
#
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

# Tạo module ext2.ko từ các tệp nguồn ext2
obj-m += ext2.o

# Liệt kê các tệp nguồn cho ext2 module
ext2-y := balloc.o dir.o file.o ialloc.o inode.o \
	  ioctl.o ext2_log.o super.o symlink.o trace.o	\
	  namei.o

CFLAGS_trace.o := -I$(src)

ext2-$(CONFIG_EXT2_FS_XATTR)	 += xattr.o xattr_user.o xattr_trusted.o
ext2-$(CONFIG_EXT2_FS_POSIX_ACL) += acl.o
ext2-$(CONFIG_EXT2_FS_SECURITY)	 += xattr_security.o
# Tạo module check_free_space.ko từ free_space.o
obj-m += test.o
test-y := free_space.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

