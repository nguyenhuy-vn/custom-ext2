#ifndef EXT2_LOG_H
#define EXT2_LOG_H

#include <linux/dcache.h>

void ext2_log(const char *operation, const struct dentry *new_dentry, const struct dentry *old_dentry);

#endif /* EXT2_LOG_H */
