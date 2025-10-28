# 1 编译链接生成编译器
g++ -std=c++20 -o toyc src/*.cpp

# 2 生成asm文件
# 2.1 toyc生成asm
./toyc.exe ./examples/compiler_inputs/03_assignment.c --mode asm --output test_assignment_toyc.s
# 2.2 clang生成asm
clang -S -target riscv32-unknown-elf -march=rv32i -mabi=ilp32 ./examples/compiler_inputs/02_assignment.c -o test_assignment_clang.s
# 2.3 riscv生成汇编
riscv32-unknown-elf-gcc -march=rv32i -mabi=ilp32 -Wall -S ./examples/compiler_inputs/02_assignment.c -o test_assignment_risc.s


# 3 asm或c编译成elf文件
# 3.1 c编译成elf文件
riscv32-unknown-elf-gcc ./examples/compiler_inputs/01_minimal.c -o test_mini_c.elf
# 3.2 asm编译成elf文件
clang \
  --target=riscv32-unknown-elf \
  -march=rv32i -mabi=ilp32 \
  test_mini_clang.s -o test_mini_c123.elf

# 4 运行elf文件
qemu-riscv32 -strace test_mini_c123.elf
# 4.1 打印返回值
echo $?
# 4.2 显示调用过程
qemu-riscv32 -strace test_mini_c.elf



clang \  
  --target=riscv32-unknown-elf \
  -march=rv32i      \   # 只用最基本的 rv32i
  -mabi=ilp32       \   # 不用 d/f 扩展的 ilp32 ABI
  -fno-addrsig      \   # 如果你的 clang 支持这个选项，用来禁止 .addrsig
  -S test.c -o test_mini_clang.s