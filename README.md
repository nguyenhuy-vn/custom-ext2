# 🛠 custom-ext2  

A modified ext2 filesystem with additional functionalities:  

- 🟢 **Free Block Checker** – A module to check the number of free blocks.  
- 📜 **Event Logging** – Logs file and folder events (creation, deletion, rename, move).  
- 🕒 **Extended Attribute (`creation_time`)** – Stores file/directory creation timestamps.  

---

## 📦 Installation  

### 1️⃣ Build and Install the Modules  
Compile and load the modified ext2 kernel module:  

```bash
sudo make
sudo insmod ext2.ko

### 2️⃣ Build and Load Free Block Checker

sudo 
