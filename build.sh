#!/bin/bash

echo "Building Cross-Processor Communication System"

# 创建build目录
mkdir -p build
cd build

# 编译A520内核模块
echo "Compiling A520 kernel module..."
make -C .. all

# 编译用户空间测试程序
echo "Compiling user space test program..."
gcc -o test_userspace ../test_userspace.c

echo "Build completed!"
echo ""
echo "To install the kernel module:"
echo "  sudo make install"
echo ""
echo "To run the test program:"
echo "  ./test_userspace"