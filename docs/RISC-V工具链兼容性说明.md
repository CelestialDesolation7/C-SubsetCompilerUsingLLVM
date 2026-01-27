# RISC-V 工具链兼容性说明

## 问题背景

在 Ubuntu/Debian 系统中，官方仓库提供的 RISC-V 工具链包名为 `gcc-riscv64-unknown-elf`，安装后提供的命令是 `riscv64-unknown-elf-gcc`，而不是 `riscv32-unknown-elf-gcc`。

## 解决方案

验证脚本 `scripts/verify_output.sh` 已更新，支持**自动检测**并使用以下任一工具链：

### 支持的 RISC-V GCC
1. `riscv32-unknown-elf-gcc` (优先)
2. `riscv64-unknown-elf-gcc` (备选)

### 支持的 QEMU
1. `qemu-riscv32` (优先)
2. `qemu-riscv32-static`
3. `qemu-riscv64` (备选)
4. `qemu-riscv64-static`

## 为什么 riscv64 工具链可以编译 RV32？

RISC-V 64 位工具链**完全支持**编译 32 位代码，只需指定正确的架构参数：

```bash
riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 file.s -o output
```

参数说明：
- `-march=rv32im`: 指定 RV32I + M 扩展指令集
- `-mabi=ilp32`: 指定 32 位 ABI（int/long/pointer 都是 32 位）

## 安装推荐

### Ubuntu/Debian
```bash
# 推荐：安装官方仓库的 64 位工具链（兼容 32 位）
sudo apt install gcc-riscv64-unknown-elf qemu-user
```

### 手动编译（可选）
如果需要原生 32 位工具链：
```bash
git clone https://github.com/riscv/riscv-gnu-toolchain
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv32 --with-arch=rv32im --with-abi=ilp32
make
```

## 验证脚本行为

### 检测流程
1. 首先尝试查找 `riscv32-unknown-elf-gcc`
2. 如果不存在，尝试 `riscv64-unknown-elf-gcc`
3. 显示使用的工具链信息
4. 使用相应的编译参数

### 输出示例

#### 使用 riscv64 工具链时：
```
Note: Using riscv64-unknown-elf-gcc for RV32 compilation
=========================================
  ToyC Compiler Output Verification
=========================================
Source directory: examples/compiler_inputs
Assembly directory: test/asm
RISC-V GCC: riscv64-unknown-elf-gcc
QEMU command: qemu-riscv64
```

#### 使用 riscv32 工具链时：
```
=========================================
  ToyC Compiler Output Verification
=========================================
Source directory: examples/compiler_inputs
Assembly directory: test/asm
RISC-V GCC: riscv32-unknown-elf-gcc
QEMU command: qemu-riscv32
```

## 常见问题

### Q1: 为什么我安装了 gcc-riscv64-unknown-elf 但找不到 riscv32-unknown-elf-gcc？

**A:** 这是正常的。Ubuntu 官方包只提供 64 位工具链，但它可以编译 32 位代码。验证脚本会自动使用 `riscv64-unknown-elf-gcc`。

### Q2: 使用 riscv64 工具链会影响测试结果吗？

**A:** 不会。只要指定了正确的 `-march=rv32im -mabi=ilp32` 参数，riscv64 工具链生成的 32 位代码与 riscv32 工具链完全相同。

### Q3: 我需要同时安装两个工具链吗？

**A:** 不需要。安装 `gcc-riscv64-unknown-elf` 就足够了。

### Q4: QEMU 也要同时支持 32 和 64 位吗？

**A:** 不需要。`qemu-user` 包会同时提供 `qemu-riscv32` 和 `qemu-riscv64`。验证脚本优先使用 32 位模拟器，如果不存在会回退到 64 位。

### Q5: 如何确认我的工具链正常工作？

**A:** 运行验证命令：
```bash
# 检查编译器
which riscv64-unknown-elf-gcc
riscv64-unknown-elf-gcc --version

# 检查 QEMU
which qemu-riscv32
qemu-riscv32 --version

# 运行测试
make verify
```

## 技术细节

### RV32 和 RV64 的关系
- RV64 是 RV32 的**超集**
- RV64 工具链包含完整的 RV32 支持
- 通过 `-march` 和 `-mabi` 参数切换目标架构

### ABI 对应关系
| 架构 | ABI | int | long | pointer |
|------|-----|-----|------|---------|
| RV32 | ilp32 | 32-bit | 32-bit | 32-bit |
| RV64 | lp64 | 32-bit | 64-bit | 64-bit |

### 指令集说明
- `rv32i`: 基础整数指令集（32 位）
- `rv32im`: 基础 + 乘除法扩展
- `rv32gc`: 通用配置（包含多个扩展）

## 总结

✅ **无需担心工具链版本问题**
- 验证脚本自动检测可用工具
- riscv64 工具链完全支持 RV32
- 测试结果准确可靠

✅ **推荐安装方式**
```bash
sudo apt install gcc-riscv64-unknown-elf qemu-user
```

✅ **验证安装**
```bash
make verify
```

如遇到其他问题，请检查工具链版本和路径配置。
