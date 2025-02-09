#include <linux/fs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/dcache.h>

#include <linux/kernel.h>
#include <linux/printk.h>

void ext2_log(const char *operation, const struct dentry *dentry, const struct dentry *old_dentry) {
    struct timespec64 ts;
    struct tm time_tm;
    char time_str[64];
    const int timezone_offset = 7 * 3600;

    char log_buf[512];
    struct file *file;
    loff_t pos = 0;
    ssize_t bytes_written;

    // Get current time and format the timestamp string
    ktime_get_real_ts64(&ts);
    ts.tv_sec += timezone_offset;
    time64_to_tm(ts.tv_sec, 0, &time_tm);
    snprintf(time_str, sizeof(time_str), "%02ld:%02ld-%02ld/%02ld/%04ld",
             (long)(time_tm.tm_hour),
             (long)(time_tm.tm_min),
             (long)(time_tm.tm_mday),
             (long)(time_tm.tm_mon + 1),
             (long)(time_tm.tm_year + 1900));

    // Validate the dentry pointer
    if (!dentry || dentry->d_name.len == 0) {
        pr_err("ext2_log: Invalid dentry\n");
        return;
    }

    // Get the absolute path of the dentry
    char abs_path[PATH_MAX] = {0};
    char *path = dentry_path_raw(dentry->d_parent, abs_path, sizeof(abs_path));
    if (IS_ERR(path)) {
        pr_err("ext2_log: Failed to get path for '%s'\n", dentry->d_name.name);
        return;
    }

    // Get the old path if old_dentry is not NULL
    char old_abs_path[PATH_MAX] = {0};
    char *old_path = NULL;
    if (old_dentry) {
        old_path = dentry_path_raw(old_dentry->d_parent, old_abs_path, sizeof(old_abs_path));
        if (IS_ERR(old_path)) {
            pr_err("ext2_log: Failed to get old path for '%s'\n", old_dentry->d_name.name);
            return;
        }
    }

    // Create the log content
    if (operation && strcmp(operation, "RENAME") == 0 && old_dentry) {
        snprintf(log_buf, sizeof(log_buf),
                 "[%s]: rename: '%s' to '%s' in directory '%s'\n",
                 time_str, old_dentry->d_name.name, dentry->d_name.name, path);
    } else if (operation && strcmp(operation, "MOVE") == 0 && old_dentry) {
        if (strcmp(old_dentry->d_name.name, dentry->d_name.name) != 0) {
            snprintf(log_buf, sizeof(log_buf),
                     "[%s]: move: '%s' (renamed to '%s') from '%s' to '%s'\n",
                     time_str, old_dentry->d_name.name, dentry->d_name.name, old_path, path);
        } else {
            snprintf(log_buf, sizeof(log_buf),
                     "[%s]: move: '%s' from '%s' to '%s'\n",
                     time_str, old_dentry->d_name.name, old_path, path);
        }
    }else {
        snprintf(log_buf, sizeof(log_buf),
                 "[%s]: %s: '%s' in directory '%s'\n",
                 time_str, operation, dentry->d_name.name, path);
    }

    // Write log to file
    file = filp_open("/var/log/ext2_log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(file)) {
        pr_err("ext2_log: Failed to open log file, error code: %ld\n", PTR_ERR(file));
        return;
    }

    bytes_written = kernel_write(file, log_buf, strlen(log_buf), &pos);
    if (bytes_written < 0) {
        pr_err("ext2_log: Failed to write to log file, error code: %ld\n", bytes_written);
    }

    // Close the log file
    filp_close(file, NULL);
}