# 🛠 custom-ext2

A modified ext2 filesystem with additional functionalities:

  🟢 Free Block Checker – A module to check the number of free blocks.  
  📜 Event Logging – Logs file and folder events (creation, deletion, rename, move).  
  🕒 Extended Attribute (creation_time) – Stores file/directory creation timestamps.  

---

## 📦 Installation  

### 1️⃣ Build and Install the Modules  
Compile and load the modified ext2 kernel module:  

```bash
sudo make
```
Once compiled, load the modules into the kernel:

```
sudo insmod ext2.ko      # Load the modified ext2 filesystem module
sudo insmod test.ko      # Load the free space checker module
```
---
## ⚙️ Usage
### 🟢 Check Free Blocks
The free block checker module (test.ko) checks the number of free blocks only once when loaded into the kernel. The result is printed to the kernel log
```bash
sudo insmod test.ko
```
To view the output check the kernel log
```bash
dmesg | tail -n 10
```
💡 Note: Since this module does not run continuously, if you need to check again, you must reload it:
```bash
sudo rmmod test
sudo insmod test.ko
```
### 🕒 Creation_time Attribute
Whenever a file or folder is created, the creation_time is automatically set and it can't be overwritten.
To retrieve the creation timestamp of a file:
```bash
sudo getfattr -n user.creation_time /path/to/file_or_folder
```

### 📜 Event Logging
The logging module records file and folder events, including creation, deletion, renaming, and moving.
📖 Checking Logs
The logs are written to the file /var/log/ext2_log. You can view them using:
```bash
sudo cat /var/log/ext2_log
```
💡 Note: Since the log is based on the system's timestamp, the logging time may be biased compared to your local time.

## 🔄 Cleaning Up
To remove compiled files:
```bash
sudo make clean
```
To unload the kernel module:
```bash
sudo rmmod test
sudo rmmod ext2
```

## 📌Notes
Ensure your kernel supports ext2 and extended attributes (xattr).

If modules fail to load, check dmesg for error messages.

This project is experimental—test in a controlled environment before deployment.

## 📜 License
This project is licensed under MIT License.
