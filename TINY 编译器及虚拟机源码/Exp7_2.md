# 实验二：TINY 词法分析器扩展

## 实验目的

在 TINY 词法分析器（`SCAN.C`）的基础上，扩展以下功能：

1. 对浮点数（含科学记数法）的识别
2. 对多行注释 `/* */` 的识别
3. 出错处理：`2n`、`100.E1.2`、未闭合多行注释等

前置条件：`MAIN.C` 中将宏 `NO_PARSE` 设置为 `TRUE`，使编译器仅运行词法分析阶段；同时将 `TraceScan` 设置为 `TRUE` 以打印每个 Token 的识别结果。

---

## 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `GLOBALS.H` | 在 `TokenType` 枚举中新增 `FLOAT` |
| `UTIL.C` | 在 `printToken()` 中新增 `FLOAT` 的打印分支 |
| `SCAN.C` | 扩展 DFA 状态机，新增 6 个状态，实现浮点数识别、多行注释和出错处理 |
| `MAIN.C` | `NO_PARSE=TRUE`，`TraceScan=TRUE` |

---

## 设计说明

### 1. 新增 Token 类型

在 `GLOBALS.H` 的 `TokenType` 枚举中，在 `NUM` 之后添加：

```c
ID, NUM, FLOAT,
```

在 `UTIL.C` 的 `printToken()` 中添加打印分支：

```c
case FLOAT:
    fprintf(listing, "FLOAT, val= %s\n", tokenString);
    break;
```

---

### 2. 扩展 DFA 状态

原始 DFA 状态：

```
START, INASSIGN, INCOMMENT, INNUM, INID, DONE
```

新增 6 个状态：

```
INFLOAT      — 识别小数点后的数字部分（如 3.14 中的 14）
INEXP        — 识别 E/e（指数符号）
INEXPSIGN    — 识别指数的正负号（E+ / E-）
INEXPNUM     — 识别指数的数字部分
INCOMMENT2       — 识别 /* */ 注释，等待 *
INCOMMENT2_STAR  — 已见 *，等待 / 关闭注释
```

### 3. 浮点数识别 DFA 转换图

```
         digit          digit
START ──────► INNUM ──────► INNUM
                │                │
               '.'             E/e
                │                │
                ▼                ▼
            INFLOAT ──E/e──► INEXP ──digit──► INEXPNUM ──► FLOAT
                │                │   +/-              │
              digit            +/- ──► INEXPSIGN ─────┘
                │
              FLOAT（非数字时退出）
```

整数 `100E2` 路径：`INNUM` → 读 `E` → `INEXP` → 读 `2` → `INEXPNUM` → 非数字 → `FLOAT`

浮点 `3.14` 路径：`INNUM` → 读 `.`（偷看下一字符为数字）→ 保留 `.` 进 `INFLOAT` → 读 `1`,`4` → 非数字 → `FLOAT`

科学计数 `1.5E3` 路径：`INNUM` → `.` → `INFLOAT` → `E` → `INEXP` → `3` → `INEXPNUM` → 非数字 → `FLOAT`

### 4. 多行注释识别

在 `START` 状态读到 `/` 时，向前偷看一个字符：
- 若下一字符是 `*`：进入 `INCOMMENT2`（吃掉注释内容）
- 否则：回退，返回 `OVER` token（除法运算符）

`INCOMMENT2` 状态机：
```
INCOMMENT2 ──*──► INCOMMENT2_STAR ──/──► START（注释结束）
     │                  │
    EOF               EOF/非*非/
     ▼                  ▼
   ERROR             回到 INCOMMENT2
```

### 5. 出错处理

| 错误情形 | 触发条件 | 处理方式 |
|----------|----------|----------|
| `2n`（数字后跟字母）| INNUM 状态读到 `isalpha(c)` | 报错并返回 ERROR token |
| `100.E1.2`（指数中出现小数点）| INEXPNUM 状态读到 `'.'` | 报错并返回 ERROR token |
| 未闭合多行注释 | INCOMMENT2 / INCOMMENT2_STAR 状态读到 EOF | 报错并返回 ERROR token |

所有错误消息格式均为：

```
ERROR at line <行号>: <错误描述>
```

**注意**：在出错的 `fprintf` 之前必须手动执行 `tokenString[tokenStringIndex] = '\0'`，否则因为循环末尾的 null 终止在 `fprintf` 之后执行，会导致错误消息中打印出残留字符（例如显示 `'2=n'` 而非 `'2n'`）。

---

## 测试程序（test_exp2.tny）

```
{ test case 1: normal float numbers }
x := 3.14;
y := 0.5;
z := 100.0;

{ test case 2: scientific notation }
a := 1.5E3;
b := 2.0e-4;
c := 100E2;
d := 6.02E+23;

/* test case 3: multi-line comment
   this comment spans
   multiple lines */
read n;
write n;

{ test case 4: error - digit followed by letter }
bad1 := 2n;

{ test case 5: error - double dot in float }
bad2 := 100.E1.2;

/* test case 6: unterminated comment - no closing
```

---

## 运行结果

编译命令：

```bash
cd TC
gcc -w -x c -std=c89 -o tiny \
    MAIN.C UTIL.C SCAN.C PARSE.C SYMTAB.C ANALYZE.C CODE.C CGEN.C
./tiny test_exp2.tny
```

实际输出：

```
TINY COMPILATION: test_exp2.tny
	2: ID, name= x
	2: :=
	2: FLOAT, val= 3.14
	2: ;
	3: ID, name= y
	3: :=
	3: FLOAT, val= 0.5
	3: ;
	4: ID, name= z
	4: :=
	4: FLOAT, val= 100.0
	4: ;
	7: ID, name= a
	7: :=
	7: FLOAT, val= 1.5E3
	7: ;
	8: ID, name= b
	8: :=
	8: FLOAT, val= 2.0e-4
	8: ;
	9: ID, name= c
	9: :=
	9: FLOAT, val= 100E2
	9: ;
	10: ID, name= d
	10: :=
	10: FLOAT, val= 6.02E+23
	10: ;
	15: reserved word: read
	15: ID, name= n
	15: ;
	16: reserved word: write
	16: ID, name= n
	16: ;
	19: ID, name= bad
	19: NUM, val= 1
	19: :=
ERROR at line 19: illegal token '2n'
	19: ERROR: 2
	19: ;
	22: ID, name= bad
	22: NUM, val= 2
	22: :=
ERROR at line 22: illegal float '100.E1.'
	22: ERROR: 100.E1
	22: NUM, val= 2
	22: ;
ERROR at line 25: unterminated comment
	25: ERROR: 
	26: EOF
```

---

## 结果分析

### 功能一：浮点数识别

| 测试输入 | 识别结果 | 说明 |
|----------|----------|------|
| `3.14` | `FLOAT, val= 3.14` | 普通小数 |
| `0.5` | `FLOAT, val= 0.5` | 小于 1 的小数 |
| `100.0` | `FLOAT, val= 100.0` | 整数部分较大 |
| `1.5E3` | `FLOAT, val= 1.5E3` | 正指数 |
| `2.0e-4` | `FLOAT, val= 2.0e-4` | 负指数，小写 e |
| `100E2` | `FLOAT, val= 100E2` | 无小数点的科学计数 |
| `6.02E+23` | `FLOAT, val= 6.02E+23` | 显式正号指数 |

全部正确识别为 `FLOAT` token。

### 功能二：多行注释识别

第 11-14 行（`/* test case 3: ... */`）和第 6 行 `{ }` 注释均被正确跳过，不产生任何 token，第 15-16 行的 `read n` 和 `write n` 被正常识别。

### 功能三：出错处理

| 错误输入 | 错误消息 | 说明 |
|----------|----------|------|
| `2n` | `ERROR at line 19: illegal token '2n'` | 数字后跟字母 |
| `100.E1.2` | `ERROR at line 22: illegal float '100.E1.'` | 指数中出现第二个小数点 |
| 未闭合注释 | `ERROR at line 25: unterminated comment` | 到达文件末尾仍未关闭注释 |

错误发生后扫描器继续运行，后续 token 仍可正常识别（错误恢复）。

### 补充说明：`bad1 := 2n` 的词法分解

TINY 语言标识符只能由字母构成（`isalpha` 规则），因此 `bad1` 被分解为：
- `bad`（ID）
- `1`（NUM）

这是词法分析的正确行为，并非错误。`2n` 才是真正的非法 token。

---

## 关键实现细节

### 偷看字符（Lookahead）

处理 `3.14` 时，在 INNUM 读到 `.` 后必须偷看下一个字符才能判断是浮点数还是运算符：

```c
case INNUM:
    if (c == '.')
    { int next = getNextChar();
      ungetNextChar();
      if (isdigit(next) || next == 'E' || next == 'e')
          state = INFLOAT;   /* 保留 '.' 进入浮点状态 */
      else
      { ungetNextChar();     /* 退回 '.' */
        state = DONE;
        currentToken = NUM;
      }
    }
```

### `/*` 识别的双字符前瞻

在 START 读到 `/` 时需要偷看下一字符以区分除法和注释开始：

```c
else if (c == '/')
{ int next = getNextChar();
  if (next == '*')
  { save = FALSE;
    state = INCOMMENT2;
  }
  else
  { ungetNextChar();
    state = DONE;
    currentToken = OVER;
  }
}
```

### fprintf 前提前终止 tokenString

错误处理中，`fprintf` 必须在循环内部 `tokenString[tokenStringIndex]='\0'` 之前执行，若不提前终止，会打印出上一个 token 残留的字符。解决方案：在每处错误 `fprintf` 前手动设置 null 终止符。

```c
tokenString[tokenStringIndex] = '\0';   /* 必须在 fprintf 之前 */
fprintf(listing, "ERROR at line %d: illegal token '%s%c'\n",
        lineno, tokenString, (char)c);
```
