// SPDX-License-Identifier: GPL-2.0
/*
 * linux/fs/ext2/namei.c
 *
 * Rewrite to pagecache. Almost all code had been changed, so blame me
 * if the things go wrong. Please, send bug reports to
 * viro@parcelfarce.linux.theplanet.co.uk
 *
 * Stuff here is basically a glue between the VFS and generic UNIXish
 * filesystem that keeps everything in pagecache. All knowledge of the
 * directory layout is in fs/ext2/dir.c - it turned out to be easily separatable
 * and it's easier to debug that way. In principle we might want to
 * generalize that a bit and turn it into a library. Or not.
 *
 * The only non-static object here is ext2_dir_inode_operations.
 *
 * TODO: get rid of kmap() use, add readahead.
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/namei.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

#include <linux/pagemap.h>
#include <linux/quotaops.h>
#include <linux/fs.h>
#include <linux/list.h>
//#include <linux/module.h>
#include "ext2.h"
#include "xattr.h"
#include "acl.h"
#include "ext2_log.h"
#include <linux/ktime.h>
#include <linux/string.h>

// rename function
static inline char to_upper(char c)
{
	if (c >= 'a' &&	c <= 'z')
		return c - 'a' + 'A';
	return c;
}
static inline char to_lower(char c)
{
	if (c >= 'A' &&	c <= 'Z')
		return c + 'a' - 'A';
	return c; 
}

static inline void set_creation_time(struct inode *inode){
	int err;
	struct timespec64 ts;
	struct tm tm;
	char time_str[17];
	const int timezone_offset = 7 * 3600; // UTC + 7

	// Get the current time
	ktime_get_real_ts64(&ts);

	// Adjust for UTC+7 timezone
	ts.tv_sec += timezone_offset;

	// Convert time to tm structure
	time64_to_tm(ts.tv_sec, 0, &tm);

	// Format time string as "HH:MM DD/MM/YYYY"
	snprintf(time_str, sizeof(time_str), "%02d:%02d %02d/%02d/%04d", 
         tm.tm_hour, tm.tm_min, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);

	// Calculate the length of the time string
	size_t len = strnlen(time_str, sizeof(time_str));

	// Set the extended attribute 'creation_time' for the inode
	err = ext2_xattr_set(inode, EXT2_XATTR_INDEX_USER, "creation_time",
                         time_str, len + 1, 0);

	if (err) {
    	printk(KERN_ERR "Failed to set xattr: %d\n", err);
	}
}

static inline int ext2_add_nondir(struct dentry *dentry, struct inode *inode)
{
	int err = ext2_add_link(dentry, inode);
	if (!err) {
		d_instantiate_new(dentry, inode);
		return 0;
	}
	inode_dec_link_count(inode);
	discard_new_inode(inode);
	return err;
}

/*
 * Methods themselves.
 */

static struct dentry *ext2_lookup(struct inode * dir, struct dentry *dentry, unsigned int flags)
{
	struct inode * inode;
	ino_t ino;
	int res;
	
	if (dentry->d_name.len > EXT2_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	res = ext2_inode_by_name(dir, &dentry->d_name, &ino);
	if (res) {
		if (res != -ENOENT)
			return ERR_PTR(res);
		inode = NULL;
	} else {
		inode = ext2_iget(dir->i_sb, ino);
		if (inode == ERR_PTR(-ESTALE)) {
			ext2_error(dir->i_sb, __func__,
					"deleted inode referenced: %lu",
					(unsigned long) ino);
			return ERR_PTR(-EIO);
		}
	}
	return d_splice_alias(inode, dentry);
}

struct dentry *ext2_get_parent(struct dentry *child)
{
	ino_t ino;
	int res;

	res = ext2_inode_by_name(d_inode(child), &dotdot_name, &ino);
	if (res)
		return ERR_PTR(res);

	return d_obtain_alias(ext2_iget(child->d_sb, ino));
} 

/*
 * By the time this is called, we already have created
 * the directory cache entry for the new file, but it
 * is so far negative - it has no inode.
 *
 * If the create succeeds, we fill in the inode information
 * with d_instantiate(). 
 */
static int ext2_create (struct mnt_idmap * idmap,
			struct inode * dir, struct dentry * dentry,
			umode_t mode, bool excl)
{
	struct inode *inode;
	int err;
	
	char *name = dentry->d_name.name;
	
	err = dquot_initialize(dir);
	if (err)
		return err;
	
	inode = ext2_new_inode(dir, mode, &dentry->d_name);
	if (IS_ERR(inode)){
		return PTR_ERR(inode);
	}
	
	for (int i=0; name[i]; i++){
		name[i] = to_upper(name[i]);
	}	

	ext2_log("Create file", dentry, NULL);

	ext2_set_file_ops(inode);
	mark_inode_dirty(inode);
	
	struct file_system_type *fs_type;
	struct super_block *sb;
    fs_type = get_fs_type("ext2");
    if (fs_type) {
		if (hlist_empty(&fs_type->fs_supers)) {
        	printk(KERN_ALERT "Error: No mounted ext2 filesystems\n");
			return ext2_add_nondir(dentry, inode);
		}
		sb = hlist_entry(fs_type->fs_supers.first, struct super_block, s_instances);
	    unsigned long free_space = ext2_check_free_space1(sb);
		printk(KERN_ALERT "Free space: %lu\n", free_space);
	}

	set_creation_time(inode);
	
	return ext2_add_nondir(dentry, inode);
}

static int ext2_tmpfile(struct mnt_idmap *idmap, struct inode *dir,
			struct file *file, umode_t mode)
{
	struct inode *inode = ext2_new_inode(dir, mode, NULL);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	ext2_set_file_ops(inode);
	mark_inode_dirty(inode);
	d_tmpfile(file, inode);
	unlock_new_inode(inode);
	return finish_open_simple(file, 0);
}

static int ext2_mknod (struct mnt_idmap * idmap, struct inode * dir,
	struct dentry *dentry, umode_t mode, dev_t rdev)
{
	struct inode * inode;
	int err;

	err = dquot_initialize(dir);
	if (err)
		return err;

	inode = ext2_new_inode (dir, mode, &dentry->d_name);
	err = PTR_ERR(inode);
	if (!IS_ERR(inode)) {
		init_special_inode(inode, inode->i_mode, rdev);
		inode->i_op = &ext2_special_inode_operations;
		mark_inode_dirty(inode);
		err = ext2_add_nondir(dentry, inode);
	}
	return err;
}

static int ext2_symlink (struct mnt_idmap * idmap, struct inode * dir,
	struct dentry * dentry, const char * symname)
{
	struct super_block * sb = dir->i_sb;
	int err = -ENAMETOOLONG;
	unsigned l = strlen(symname)+1;
	struct inode * inode;

	if (l > sb->s_blocksize)
		goto out;

	err = dquot_initialize(dir);
	if (err)
		goto out;

	inode = ext2_new_inode (dir, S_IFLNK | S_IRWXUGO, &dentry->d_name);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out;

	if (l > sizeof (EXT2_I(inode)->i_data)) {
		/* slow symlink */
		inode->i_op = &ext2_symlink_inode_operations;
		inode_nohighmem(inode);
		inode->i_mapping->a_ops = &ext2_aops;
		err = page_symlink(inode, symname, l);
		if (err)
			goto out_fail;
	} else {
		/* fast symlink */
		inode->i_op = &ext2_fast_symlink_inode_operations;
		inode->i_link = (char*)EXT2_I(inode)->i_data;
		memcpy(inode->i_link, symname, l);
		inode->i_size = l-1;
	}
	mark_inode_dirty(inode);

	err = ext2_add_nondir(dentry, inode);
out:
	return err;

out_fail:
	inode_dec_link_count(inode);
	discard_new_inode(inode);
	goto out;
}

static int ext2_link (struct dentry * old_dentry, struct inode * dir,
	struct dentry *dentry)
{
	struct inode *inode = d_inode(old_dentry);
	int err;

	err = dquot_initialize(dir);
	if (err)
		return err;

	inode_set_ctime_current(inode);
	inode_inc_link_count(inode);
	ihold(inode);

	err = ext2_add_link(dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}
	inode_dec_link_count(inode);
	iput(inode);
	return err;
}

static int ext2_mkdir(struct mnt_idmap * idmap,
	struct inode * dir, struct dentry * dentry, umode_t mode)
{	
	struct inode * inode;
	int err;
	
	char *name = dentry->d_name.name;
	
	err = dquot_initialize(dir);
	if (err)
		return err;
    
	inode_inc_link_count(dir);

	inode = ext2_new_inode(dir, S_IFDIR | mode, &dentry->d_name);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_dir;
	for (int i = 0; name[i]; i++){
		name[i] = to_lower(name[i]);
	}

	ext2_log("Create directory", dentry, NULL);

	inode->i_op = &ext2_dir_inode_operations;
	inode->i_fop = &ext2_dir_operations;
	inode->i_mapping->a_ops = &ext2_aops;

	inode_inc_link_count(inode);

	err = ext2_make_empty(inode, dir);
	if (err)
		goto out_fail;

	err = ext2_add_link(dentry, inode);
	if (err)
		goto out_fail;

	d_instantiate_new(dentry, inode);
	set_creation_time(inode);
out:
	return err;

out_fail:
	inode_dec_link_count(inode);
	inode_dec_link_count(inode);
	discard_new_inode(inode);
out_dir:
	inode_dec_link_count(dir);
	goto out;
}

static int ext2_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct ext2_dir_entry_2 *de;
	struct folio *folio;
	int err;

	// Ghi log xóa file nếu không phải thư mục
    if (!S_ISDIR(inode->i_mode)) {
        ext2_log("Remove file", dentry, NULL);
    }

	err = dquot_initialize(dir);
	if (err)
		goto out;

	de = ext2_find_entry(dir, &dentry->d_name, &folio);
	if (IS_ERR(de)) {
		err = PTR_ERR(de);
		goto out;
	}

	err = ext2_delete_entry(de, folio);
	folio_release_kmap(folio, de);
	if (err)
		goto out;

	inode_set_ctime_to_ts(inode, inode_get_ctime(dir));
	inode_dec_link_count(inode);
	err = 0;
out:
	return err;
}

static int ext2_rmdir (struct inode * dir, struct dentry *dentry)
{
	struct inode * inode = d_inode(dentry);
	int err = -ENOTEMPTY;


	if (ext2_empty_dir(inode)) {
		ext2_log("Remove directory", dentry, NULL);
		err = ext2_unlink(dir, dentry);
		if (!err) {
			inode->i_size = 0;
			inode_dec_link_count(inode);
			inode_dec_link_count(dir);
		}
	}
	return err;
}

static int ext2_rename (struct mnt_idmap * idmap,
			struct inode * old_dir, struct dentry * old_dentry,
			struct inode * new_dir, struct dentry * new_dentry,
			unsigned int flags)
{
	struct inode * old_inode = d_inode(old_dentry);
	struct inode * new_inode = d_inode(new_dentry);
	struct folio *dir_folio = NULL;
	struct ext2_dir_entry_2 * dir_de = NULL;
	struct folio * old_folio;
	struct ext2_dir_entry_2 * old_de;
	bool old_is_dir = S_ISDIR(old_inode->i_mode);
	int err;

	if (flags & ~RENAME_NOREPLACE)
		return -EINVAL;

	err = dquot_initialize(old_dir);
	if (err)
		return err;

	err = dquot_initialize(new_dir);
	if (err)
		return err;

	old_de = ext2_find_entry(old_dir, &old_dentry->d_name, &old_folio);
	if (IS_ERR(old_de))
		return PTR_ERR(old_de);
	
	char *name = new_dentry->d_name.name;
	if (old_is_dir) {
		for (int i=0; name[i]; i++){
			name[i] = to_lower(name[i]);
		}
	}
	else {
		for (int i=0; name[i]; i++){
			name[i] = to_upper(name[i]);
		}
	}

    // Kiểm tra loại thao tác
    bool is_move = (old_dir != new_dir);

    // Ghi log trước khi thực hiện thao tác
    if (is_move) {
        ext2_log("MOVE", new_dentry, old_dentry);
    } else{
        ext2_log("RENAME", new_dentry, old_dentry);
    }

	if (old_is_dir && old_dir != new_dir) {
		err = -EIO;
		dir_de = ext2_dotdot(old_inode, &dir_folio);
		if (!dir_de)
			goto out_old;
	}
	
	if (new_inode) {
		struct folio *new_folio;
		struct ext2_dir_entry_2 *new_de;

		err = -ENOTEMPTY;
		if (old_is_dir && !ext2_empty_dir(new_inode))
			goto out_dir;

		new_de = ext2_find_entry(new_dir, &new_dentry->d_name,
					 &new_folio);
		if (IS_ERR(new_de)) {
			err = PTR_ERR(new_de);
			goto out_dir;
		}
		err = ext2_set_link(new_dir, new_de, new_folio, old_inode, true);
		folio_release_kmap(new_folio, new_de);
		if (err)
			goto out_dir;
		inode_set_ctime_current(new_inode);
		if (old_is_dir)
			drop_nlink(new_inode);
		inode_dec_link_count(new_inode);
	} else {
		err = ext2_add_link(new_dentry, old_inode);
		if (err)
			goto out_dir;
		if (old_is_dir)
			inode_inc_link_count(new_dir);
	}

	/*
	 * Like most other Unix systems, set the ctime for inodes on a
 	 * rename.
	 */
	inode_set_ctime_current(old_inode);
	mark_inode_dirty(old_inode);

	err = ext2_delete_entry(old_de, old_folio);
	if (!err && old_is_dir) {
		if (old_dir != new_dir)
			err = ext2_set_link(old_inode, dir_de, dir_folio,
					    new_dir, false);

		inode_dec_link_count(old_dir);
	}
out_dir:
	if (dir_de)
		folio_release_kmap(dir_folio, dir_de);
out_old:
	folio_release_kmap(old_folio, old_de);
	return err;
}

const struct inode_operations ext2_dir_inode_operations = {
	.create		= ext2_create,
	.lookup		= ext2_lookup,
	.link		= ext2_link,
	.unlink		= ext2_unlink,
	.symlink	= ext2_symlink,
	.mkdir		= ext2_mkdir,
	.rmdir		= ext2_rmdir,
	.mknod		= ext2_mknod,
	.rename		= ext2_rename,
	.listxattr	= ext2_listxattr,
	.getattr	= ext2_getattr,
	.setattr	= ext2_setattr,
	.get_inode_acl	= ext2_get_acl,
	.set_acl	= ext2_set_acl,
	.tmpfile	= ext2_tmpfile,
	.fileattr_get	= ext2_fileattr_get,
	.fileattr_set	= ext2_fileattr_set,
};

const struct inode_operations ext2_special_inode_operations = {
	.listxattr	= ext2_listxattr,
	.getattr	= ext2_getattr,
	.setattr	= ext2_setattr,
	.get_inode_acl	= ext2_get_acl,
	.set_acl	= ext2_set_acl,
};
