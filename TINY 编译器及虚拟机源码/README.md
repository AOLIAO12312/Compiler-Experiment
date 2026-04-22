# TINY 编译器及虚拟机

TINY 语言编译器（TC）与 TM 虚拟机（TM）的完整实现，来自 Kenneth C. Louden 的经典教材《编译原理及实践》（*Compiler Construction: Principles and Practice*）。

---

## 项目结构

```
TINY 编译器及虚拟机源码/
├── TC/                  TINY 编译器
│   ├── MAIN.C           主入口，控制各编译阶段
│   ├── GLOBALS.H        全局类型定义（Token、AST 节点等）
│   ├── SCAN.C/H         词法分析器（Scanner）
│   ├── PARSE.C/H        语法分析器（Parser，递归下降）
│   ├── SYMTAB.C/H       符号表（哈希链表实现）
│   ├── ANALYZE.C/H      语义分析（符号表构建 + 类型检查）
│   ├── CODE.C/H         TM 指令发射工具
│   ├── CGEN.C/H         代码生成器（生成 TM 汇编）
│   ├── UTIL.C/H         辅助工具（AST 打印等）
│   ├── MAKEDOT.C/H      AST 可视化（Graphviz dot 格式）
│   ├── SAMPLE.TNY       示例源程序（计算阶乘）
│   └── MAKEFILE         原始 Makefile（Borland C/Windows 专用）
└── TM/                  TM 虚拟机
    ├── tm.c             虚拟机实现（单文件）
    └── sample.tm        示例 TM 汇编文件
```

---

## 编译方法

> 原始 `MAKEFILE` 为 **Borland C（Windows）** 所写，macOS / Linux 请使用以下命令。

### 环境要求

- `gcc`（或 `clang`）
- macOS / Linux

### 编译 TINY 编译器

```bash
cd TC

gcc -w -x c -std=c89 -o tiny \
    MAIN.C UTIL.C SCAN.C PARSE.C SYMTAB.C ANALYZE.C CODE.C CGEN.C
```

| 参数 | 说明 |
|------|------|
| `-w` | 关闭警告（原代码为老式 C89 写法，会产生大量无害警告）|
| `-x c` | 强制将文件视为 C 源码（文件名全大写 `.C`，gcc 可能误判）|
| `-std=c89` | 使用 C89 标准（`main` 无返回类型声明是 C89 合法写法）|
| `-o tiny` | 输出可执行文件名为 `tiny` |

### 编译 TM 虚拟机

```bash
cd TM

gcc -w -x c -std=c89 -o tm tm.c
```

---

## 使用方法

### 第一步：编译 TINY 源程序（.tny → .tm）

```bash
cd TC
./tiny SAMPLE.TNY
```

成功后在当前目录生成 `SAMPLE.tm`，即 TM 虚拟机可执行的目标代码。

### 第二步：在 TM 虚拟机上运行

```bash
cd TM
./tm ../TC/SAMPLE.tm
```

TM 是交互式模拟器，进入后按如下操作：

```
TM  simulation (enter h for help)...
Enter command: g                      ← 输入 g，开始运行
Enter value for IN instruction: 5     ← 程序要求输入，输入整数
OUT instruction prints: 120           ← 输出结果（5! = 120）
HALT: 0,0,0
Halted
Enter command: q                      ← 输入 q，退出
```

### 完整流程示意

```
SAMPLE.TNY
    │
    │  ./tiny SAMPLE.TNY
    ▼
SAMPLE.tm
    │
    │  ./tm SAMPLE.tm
    ▼
TM 虚拟机交互执行
    │  输入: 5
    ▼
    输出: 120
```

---

## TM 虚拟机调试命令

| 命令 | 功能 |
|------|------|
| `g` | 运行到 HALT |
| `s` | 单步执行 1 条指令 |
| `s <n>` | 单步执行 n 条指令 |
| `r` | 查看所有寄存器的值 |
| `i <b> <n>` | 查看从地址 b 开始的 n 条指令 |
| `d <b> <n>` | 查看从地址 b 开始的 n 个数据内存单元 |
| `t` | 开关指令跟踪（每步打印当前指令）|
| `p` | 开关执行指令计数（`g` 模式下）|
| `c` | 重置模拟器，重新运行 |
| `h` | 显示帮助 |
| `q` | 退出 |

---

## TINY 语言语法

### 保留字

```
if   then   else   end   repeat   until   read   write
```

### 文法（BNF）

```
program        → stmt_sequence

stmt_sequence  → statement { ; statement }

statement      → if_stmt
               | repeat_stmt
               | assign_stmt
               | read_stmt
               | write_stmt

if_stmt        → if exp then stmt_sequence
                 [ else stmt_sequence ]
                 end

repeat_stmt    → repeat stmt_sequence until exp

assign_stmt    → ID := exp

read_stmt      → read ID

write_stmt     → write exp

exp            → simple_exp [ ( < | = ) simple_exp ]

simple_exp     → term { ( + | - ) term }

term           → factor { ( * | / ) factor }

factor         → ( exp )
               | NUM
               | ID
```

### 运算符优先级（从低到高）

| 优先级 | 运算符 |
|--------|--------|
| 最低 | `<`  `=`（比较）|
| 中 | `+`  `-` |
| 高 | `*`  `/` |
| 最高 | `( )` 括号 |

### 语言特性说明

- **变量**：无需声明，首次赋值即定义；变量名只能由字母组成
- **类型**：只有整数（Integer）和布尔值（Boolean），布尔值仅用于条件判断
- **注释**：`{ 注释内容 }`，不可嵌套
- **语句分隔符**：`;` 是语句之间的**分隔符**（非结尾符），最后一条语句不加分号
- **作用域**：只有全局作用域
- **repeat-until**：先执行后判断（do-while），条件为**真**时退出

### 示例程序（SAMPLE.TNY）

```
{ 计算阶乘 }
read x;
if 0 < x then
  fact := 1;
  repeat
    fact := fact * x;
    x := x - 1
  until x = 0;
  write fact
end
```

输入 `5`，输出 `120`（即 5! = 120）。

---

## 编译器架构

```
.tny 源文件
    │
    ▼  SCAN.C — 词法分析
    │  将源码转为 Token 流
    ▼  PARSE.C — 语法分析（递归下降）
    │  构建抽象语法树（AST）
    ▼  ANALYZE.C — 语义分析
    │  ├── buildSymtab()  构建符号表
    │  └── typeCheck()    类型检查
    ▼  CGEN.C — 代码生成
    │  遍历 AST，发射 TM 指令
    ▼
  .tm 目标文件
```

## TM 虚拟机规格

| 项目 | 规格 |
|------|------|
| 指令内存 | 1024 条 |
| 数据内存 | 1024 个整数单元 |
| 寄存器 | 8 个（reg[0..7]），reg[7] 为 PC |
| 指令格式 | RR 类 / RM 类 / RA 类 |

### 指令集

```
RR 类：HALT  IN   OUT   ADD  SUB  MUL  DIV
RM 类：LD    ST
RA 类：LDA   LDC  JLT  JLE  JGT  JGE  JEQ  JNE
```
