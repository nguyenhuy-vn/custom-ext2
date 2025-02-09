#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/printk.h>
#include "ext2.h"

// External function declaration
unsigned long ext2_check_free_space1(struct super_block *sb){
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
    return available_blocks * sb->s_blocksize; 
};

static int test_init(void)
{
    struct file_system_type *fs_type;
    struct super_block *sb;

    printk(KERN_ALERT "Test module loading...\n");

    // Lấy thông tin filesystem type
    fs_type = get_fs_type("ext2");
    if (!fs_type) {
        printk(KERN_ALERT "Error: ext2 filesystem not found\n");
        return -ENODEV;
    }
    printk(KERN_ALERT "ext2 filesystem type found\n");

    // Kiểm tra danh sách superblocks
    if (hlist_empty(&fs_type->fs_supers)) {
        printk(KERN_ALERT "Error: No mounted ext2 filesystems\n");
        return -ENODEV;
    }
    printk(KERN_ALERT "Mounted ext2 filesystems found\n");

    // Lấy superblock đầu tiên
    sb = hlist_entry(fs_type->fs_supers.first, struct super_block, s_instances);
    if (!sb) {
        printk(KERN_ALERT "Error: Unable to retrieve superblock\n");
        return -EINVAL;
    }
    printk(KERN_ALERT "Superblock retrieved successfully\n");

    // Gọi hàm kiểm tra dung lượng trống
    printk(KERN_ALERT "Calling ext2_check_free_space...\n");
    unsigned long free_space = ext2_check_free_space1(sb);
    printk(KERN_ALERT "Free space: %lu\n", free_space);

    return 0;
}

static void test_exit(void)
{
    printk(KERN_ALERT "Test module unloaded\n");
}

module_init(test_init);
module_exit(test_exit);
MODULE_LICENSE("GPL");
