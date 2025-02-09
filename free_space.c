#include <linux/export.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/printk.h>
#include "ext2.h"

unsigned long ext2_check_free_space(struct super_block *sb)
{
    struct ext2_sb_info *sbi = EXT2_SB(sb);
    struct ext2_super_block *es = sbi->s_es;
    unsigned long free_blocks;
    unsigned long reserved_blocks;
    unsigned long available_blocks;

    free_blocks = le32_to_cpu(es->s_free_blocks_count);
    reserved_blocks = le32_to_cpu(es->s_r_blocks_count);

    if (free_blocks > reserved_blocks) {
        available_blocks = free_blocks - reserved_blocks;
    } else {
        available_blocks = 0;
    }

    // Trả về dung lượng trống tính bằng bytes
    return available_blocks * sb->s_blocksize;  // Đổi từ blocks sang bytes
}

EXPORT_SYMBOL(ext2_check_free_space);


MODULE_LICENSE("GPL");
