# Debloating Analysis Tools
Tools for analyzing, repairing and augmenting debloated programs.

### Dependencies
```bash
# If version 15 is not in your system's apt sources list, run the command below.
wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh
sudo ./llvm.sh 15

sudo apt install python3 cmake ninja-build gdb libpcre3 libpcre3-dev
sudo apt install libclang-15-dev llvm-15 llvm-15-dev clang-15 lld-15 clang-format-15
```

# Usage
Install dependencies and git clone all the needed repositories.

Copy "env.default.sh" to "env.sh" and modify it to match your environment.

Directly use tools in "build/bin" directory or use them by running the scripts in "scripts" directory.
