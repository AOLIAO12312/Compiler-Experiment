/****************************************************/
/* File: scan.c                                     */
/* The scanner implementation for the TINY compiler */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

// 词法分析器（Scanner）：将源代码字符流切分为 token 序列
// 核心是 DFA（确定性有限自动机），从 START 状态出发逐字符驱动状态转移

#include "globals.h"
#include "util.h"
#include "scan.h"

// DFA 状态枚举：每个状态对应词法分析器所处的不同识别阶段
typedef enum
   { START,              // 初始状态，等待下一个 token 的首字符
     INASSIGN,           // 已读 ':'，等待 '=' 构成赋值符 :=
     INCOMMENT,          // 花括号注释 {...} 内
     INNUM,              // 整数部分
     INID,               // 标识符或关键字
     DONE,               // 当前 token 识别完毕，返回
     INFLOAT,            // 小数点后的浮点小数部分
     INEXP,              // 已读 E/e，等待指数部分
     INEXPSIGN,          // 已读指数符号 + 或 -
     INEXPNUM,           // 指数部分的数字
     INCOMMENT2,         // C 风格注释 /* ... */ 内
     INCOMMENT2_STAR,    // C 风格注释中读到 *，等待 / 结束
     INSTRING            // 字符串字面量 "..." 内
   }
   StateType;

// 存储当前 token 的词素（标识符名 / 数字字符串 / 字符串内容）
char tokenString[MAXTOKENLEN+1];

// 行缓冲区长度
#define BUFLEN 256

static char lineBuf[BUFLEN];   // 当前行的字符缓冲区
static int linepos = 0;        // 当前读取位置（缓冲区下标）
static int bufsize = 0;        // 缓冲区有效字符数
static int EOF_flag = FALSE;   // EOF 标记，防止 ungetNextChar 在文件结束后回退

// ============================================================
// getNextChar — 从行缓冲区取下一个非空字符
// 若当前行已读完则从源文件读取新行
// ============================================================
static int getNextChar(void)
{ if (!(linepos < bufsize))
  { lineno++;                                             // 行号递增
    if (fgets(lineBuf,BUFLEN-1,source))                   // 读取新行
    { if (EchoSource) fprintf(listing,"%4d: %s",lineno,lineBuf);
      bufsize = strlen(lineBuf);
      linepos = 0;
      return lineBuf[linepos++];
    }
    else
    { EOF_flag = TRUE;
      return EOF;
    }
  }
  else return lineBuf[linepos++];
}

// ungetNextChar — 回退一个字符，使下次 getNextChar 重新读到该字符
static void ungetNextChar(void)
{ if (!EOF_flag) linepos-- ;}

// 关键字查找表：TINY 语言共 8 个关键字
static struct
    { char* str;
      TokenType tok;
    } reservedWords[MAXRESERVED]
   = {{"if",IF},{"then",THEN},{"else",ELSE},{"end",END},
      {"repeat",REPEAT},{"until",UNTIL},{"read",READ},
      {"write",WRITE},{"int",INT},{"float",FLOATKW}};

// ============================================================
// reservedLookup — 在关键字表中线性查找，判断标识符是否为关键字
// 若匹配则返回对应的 TokenType，否则返回 ID
// ============================================================
static TokenType reservedLookup (char * s)
{ int i;
  for (i=0;i<MAXRESERVED;i++)
    if (!strcmp(s,reservedWords[i].str))
      return reservedWords[i].tok;
  return ID;
}

/****************************************/
/* 词法分析器主函数                      */
/****************************************/
// getToken — 每次调用返回源代码中的下一个 token（DFA 驱动）
TokenType getToken(void)
{  int tokenStringIndex = 0;       // tokenString 写入位置
   TokenType currentToken;         // 当前识别的 token 类型
   StateType state = START;        // DFA 初始状态
   int save;                       // 当前字符是否需要存入 tokenString

   while (state != DONE)           // DFA 主循环，直到识别完毕
   { int c = getNextChar();
     save = TRUE;

     switch (state)
     { // ========== START：根据首字符跳转到对应状态 ==========
       case START:
         if (isdigit(c))
           state = INNUM;                          // 数字 → 进入整数状态
         else if (isalpha(c))
           state = INID;                           // 字母 → 进入标识符状态
         else if (c == ':')
           state = INASSIGN;                       // 冒号 → 等待赋值符
         else if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'))
           save = FALSE;                           // 空白字符跳过，不保存
         else if (c == '"')
         { save = FALSE;
           state = INSTRING;                       // 引号 → 进入字符串状态
         }
         else if (c == '{')
         { save = FALSE;
           state = INCOMMENT;                      // { → 进入花括号注释
         }
         else if (c == '/')
         { int next = getNextChar();               // 向前看一个字符
           if (next == '*')
           { save = FALSE;
             state = INCOMMENT2;                   // /* → 进入 C 风格注释
           }
           else
           { ungetNextChar();
             state = DONE;
             currentToken = OVER;                  // 单独的 / → 除法运算符
           }
         }
         else
         { state = DONE;                           // 单字符 token 直接完成
           switch (c)
           { case EOF:
               save = FALSE;
               currentToken = ENDFILE;
               break;
             case '=':
               currentToken = EQ;
               break;
             case '<':
               { int next = getNextChar();         // 向前看是否为 <=
                 if (next == '=')
                   currentToken = LTE;
                 else
                 { ungetNextChar();
                   currentToken = LT;
                 }
               }
               break;
             case '+':
               currentToken = PLUS;
               break;
             case '-':
               currentToken = MINUS;
               break;
             case '*':
               currentToken = TIMES;
               break;
             case '(':
               currentToken = LPAREN;
               break;
             case ')':
               currentToken = RPAREN;
               break;
             case ';':
               currentToken = SEMI;
               break;
             case ',':
               currentToken = COMMA;
               break;
             case '>':
               { int next = getNextChar();         // 向前看是否为 >=
                 if (next == '=')
                   currentToken = GTE;
                 else
                 { ungetNextChar();
                   currentToken = GT;
                 }
               }
               break;
             default:
               currentToken = ERROR;
               break;
           }
         }
         break;

       // ========== 花括号注释 {...}：跳过直到 } ==========
       case INCOMMENT:
         save = FALSE;
         if (c == EOF)
         { state = DONE;
           currentToken = ENDFILE;
         }
         else if (c == '}') state = START;          // 注释结束，回到 START
         break;

       // ========== C 风格注释 /* ... */：等待 * ==========
       case INCOMMENT2:
         save = FALSE;
         if (c == EOF)
         { state = DONE;
           fprintf(listing,"ERROR at line %d: unterminated comment\n",lineno);
           currentToken = ERROR;
         }
         else if (c == '*')
           state = INCOMMENT2_STAR;                  // 可能即将结束
         break;

       // ========== C 风格注释读到 *：等待 / 闭合 ==========
       case INCOMMENT2_STAR:
         save = FALSE;
         if (c == EOF)
         { state = DONE;
           fprintf(listing,"ERROR at line %d: unterminated comment\n",lineno);
           currentToken = ERROR;
         }
         else if (c == '/')
           state = START;                            // */ → 注释结束
         else if (c == '*')
           /* 连续 *，继续等待 / */;
         else
           state = INCOMMENT2;                       // 回到注释体内
         break;

       // ========== 字符串字面量 "..." ==========
       case INSTRING:
         save = TRUE;
         if (c == EOF)
         { state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: unterminated string literal\n",lineno);
           currentToken = ERROR;
         }
         else if (c == '\n' || c == '\r')
         { state = DONE;                             // 字符串内不允许换行
           save = FALSE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: newline in string literal\n",lineno);
           currentToken = ERROR;
         }
         else if (c == '"')
         { save = FALSE;
           state = DONE;
           currentToken = STRING;                    // 配对引号 → 字符串 token
         }
         break;

       // ========== 赋值符 := ==========
       case INASSIGN:
         state = DONE;
         if (c == '=')
           currentToken = ASSIGN;                    // := 识别成功
         else
         { ungetNextChar();                          // 回退，报错
           save = FALSE;
           currentToken = ERROR;
         }
         break;

       // ========== 整数部分 ==========
       case INNUM:
         if (c == '.')
         { int next = getNextChar();                 // 窥视下个字符
           ungetNextChar();
           if (isdigit(next) || next == 'E' || next == 'e')
           { state = INFLOAT;                        // 合法浮点起点 → 进入浮点状态
           }
           else
           { ungetNextChar();                        // "3." 后无内容 → 回退，整数结束
             save = FALSE;
             state = DONE;
             currentToken = NUM;
           }
         }
         else if (c == 'E' || c == 'e')
           state = INEXP;                            // 科学计数法 → 指数部分
         else if (isalpha(c))
         { save = FALSE;                             // 如 "2n" 非法 token
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal token '%s%c'\n",
                   lineno,tokenString,(char)c);
           currentToken = ERROR;
         }
         else if (!isdigit(c))
         { ungetNextChar();                          // 非数字 → 整数识别完毕
           save = FALSE;
           state = DONE;
           currentToken = NUM;
         }
         break;

       // ========== 浮点小数部分 ==========
       case INFLOAT:
         if (c == 'E' || c == 'e')
           state = INEXP;                            // → 指数部分
         else if (c == '.')
         { save = FALSE;                             // 第二个小数点，非法
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal float '%s.'\n",
                   lineno,tokenString);
           currentToken = ERROR;
         }
         else if (isalpha(c))
         { save = FALSE;
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal float '%s%c'\n",
                   lineno,tokenString,(char)c);
           currentToken = ERROR;
         }
         else if (!isdigit(c))
         { ungetNextChar();                          // 非数字 → 浮点识别完毕
           save = FALSE;
           state = DONE;
           currentToken = FLOAT;
         }
         break;

       // ========== 指数符号 E/e 之后，等待符号位或数字 ==========
       case INEXP:
         if (c == '+' || c == '-')
           state = INEXPSIGN;                        // E+ 或 E-
         else if (isdigit(c))
           state = INEXPNUM;
         else
         { save = FALSE;                             // E 后无符号或数字 → 错误
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal exponent in '%s'\n",
                   lineno,tokenString);
           currentToken = ERROR;
         }
         break;

       // ========== 指数符号位之后，必须接数字 ==========
       case INEXPSIGN:
         if (isdigit(c))
           state = INEXPNUM;
         else
         { save = FALSE;
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal exponent in '%s'\n",
                   lineno,tokenString);
           currentToken = ERROR;
         }
         break;

       // ========== 指数部分数字 ==========
       case INEXPNUM:
         if (c == '.')
         { save = FALSE;                             // 指数中不能有小数点
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal float '%s.'\n",
                   lineno,tokenString);
           currentToken = ERROR;
         }
         else if (isalpha(c))
         { save = FALSE;
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal float '%s%c'\n",
                   lineno,tokenString,(char)c);
           currentToken = ERROR;
         }
         else if (!isdigit(c))
         { ungetNextChar();                          // 非数字 → 浮点识别完毕
           save = FALSE;
           state = DONE;
           currentToken = FLOAT;
         }
         break;

       // ========== 标识符 / 关键字 ==========
       case INID:
         if (!isalpha(c))
         { ungetNextChar();                          // 非字母 → 标识符结束
           save = FALSE;
           state = DONE;
           currentToken = ID;
         }
         break;

       case DONE:
       default:                                      // 不应到达此处
         fprintf(listing,"Scanner Bug: state= %d\n",state);
         state = DONE;
         currentToken = ERROR;
         break;
     }

     if ((save) && (tokenStringIndex <= MAXTOKENLEN))
       tokenString[tokenStringIndex++] = (char) c;   // 将字符存入 tokenString

     if (state == DONE)
     { tokenString[tokenStringIndex] = '\0';          // 字符串结尾
       if (currentToken == ID)
         currentToken = reservedLookup(tokenString);  // 区分关键字和标识符
     }
   }

   // 若开启词法跟踪，输出每个 token 的信息
   if (TraceScan) {
     fprintf(listing,"\t%d: ",lineno);
     printToken(currentToken,tokenString);
   }
   return currentToken;
} /* end getToken */
