# RISC-V 32-bit 启动代码
# 用于 QEMU 用户模式下运行测试程序
#
# 说明：
# - QEMU 用户模式启动时会提供有效且 16 字节对齐的栈指针
# - 不需要额外分配栈空间，main 函数会管理自己的栈帧
# - 使用 Linux RISC-V 系统调用约定退出

    .text
    .globl _start

_start:
    # 直接调用 main 函数
    # QEMU 用户模式已提供有效的 sp，无需额外分配
    call    main
    
    # main 返回后，返回值在 a0 中
    # 使用 Linux RISC-V exit 系统调用
    # a0 = 退出码（已由 main 设置）
    # a7 = 系统调用号 (93 = exit)
    li      a7, 93
    ecall

    # ecall 后程序已终止，以下代码不会执行
    # 保留作为防御性编程
    j       .
