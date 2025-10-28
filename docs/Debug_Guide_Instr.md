wsl clang -S -target riscv32-unknown-elf -march=rv32i -mabi=ilp32 examples/single_func/test_if.c -o test_if_clang.s

g++ -std=c++17 -o toyc.exe src/*.cpp

./toyc.exe examples/single_func/test_if.c --mode asm > test_if_toyc.s

wsl clang -S -target riscv32 -mabi=ilp32 -march=rv32i test_if_toyc.ll -o test_if_LL-by-toyC_ASM-by-clang.s

./toyc.exe examples/single_func/test_if.c --mode ir --output test_if_toyc.ll

wsl clang -S -target riscv32 -mabi=ilp32 -march=rv32i test_if_toyc.ll -o test_if_LL-by-toyC_ASM-by-clang.s

// 使用Clang无优化的将c文件编译为ll后缀文件的指令
wsl clang -S -emit-llvm -O0 examples/single_func/test_if.c -o test_if_clang.ll

// 使用Clang无优化的将c文件编译为s后缀文件的指令
wsl clang -S -target riscv32 -mabi=ilp32 -march=rv32i test_if_c_llvm_ir.ll -o test_if_c_llvm_ir.s


// 使用Clang无优化的将ll后缀文件编译为s后缀文件的指令
wsl clang -S -target riscv32 -march=rv32i -mabi=ilp32 test_if_toyc.ll -o test_if_LL-by-toyC_ASM-by-clang.s






