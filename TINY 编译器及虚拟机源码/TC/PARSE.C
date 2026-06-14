/****************************************************/
/* File: parse.c                                    */
/* The parser implementation for the TINY compiler  */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

// 语法分析器（Parser）：采用递归下降法，按 TINY 文法逐个解析语句和表达式
// 每个非终结符对应一个解析函数，token 消费由 match() 统一完成

#include "globals.h"
#include "util.h"
#include "scan.h"
#include "parse.h"

static TokenType token;          // 当前待处理的 token

// ============================================================
// 递归下降解析函数声明（每个非终结符对应一个函数）
// ============================================================
static TreeNode * stmt_sequence(void);   // 语句序列
static TreeNode * statement(void);       // 单条语句（分发器）
static TreeNode * if_stmt(void);         // if 语句
static TreeNode * repeat_stmt(void);     // repeat 语句
static TreeNode * assign_stmt(void);     // 赋值语句
static TreeNode * read_stmt(void);       // read 语句
static TreeNode * write_stmt(void);      // write 语句
static TreeNode * int_stmt(void);        // int 声明
static TreeNode * float_stmt(void);      // float 声明
static TreeNode * exp(void);             // 比较表达式
static TreeNode * simple_exp(void);      // 加减表达式
static TreeNode * term(void);            // 乘除表达式
static TreeNode * factor(void);          // 基本因子（字面量/标识符/括号/负号）

// ============================================================
// syntaxError — 输出语法错误信息，并置全局 Error 标志
// ============================================================
static void syntaxError(char * message)
{ fprintf(listing,"\n>>> ");
  fprintf(listing,"Syntax error at line %d: %s",lineno,message);
  Error = TRUE;
}

// ============================================================
// match — 检查当前 token 是否为期望类型，是则消费并前进一步，否则报错
// ============================================================
static void match(TokenType expected)
{ if (token == expected) token = getToken();
  else {
    syntaxError("unexpected token -> ");
    printToken(token,tokenString);
    fprintf(listing,"      ");
  }
}

// ============================================================
// stmt_sequence — 解析语句序列：statement { ; statement }
// 用 sibling 指针将多条语句串联为链表
// ============================================================
TreeNode * stmt_sequence(void)
{ TreeNode * t = statement();            // 解析第一条语句
  TreeNode * p = t;                      // p 指向链表末尾，便于追加
  while ((token!=ENDFILE) && (token!=END) &&
         (token!=ELSE) && (token!=UNTIL))
  { TreeNode * q;
    match(SEMI);                         // 消费分号
    q = statement();                     // 解析下一条语句
    if (q!=NULL) {
      if (t==NULL) t = p = q;            // 为首个非空节点
      else
      { p->sibling = q;                  // 追加到 sibling 链表
        p = q;
      }
    }
  }
  return t;                              // 返回链表头
}

// ============================================================
// statement — 根据当前 token 分发到对应的语句解析函数
// ============================================================
TreeNode * statement(void)
{ TreeNode * t = NULL;
  switch (token) {
    case IF : t = if_stmt(); break;
    case REPEAT : t = repeat_stmt(); break;
    case ID : t = assign_stmt(); break;
    case READ : t = read_stmt(); break;
    case WRITE : t = write_stmt(); break;
    case INT : t = int_stmt(); break;
    case FLOATKW : t = float_stmt(); break;
    default : syntaxError("unexpected token -> ");
              printToken(token,tokenString);
              token = getToken();
              break;
  }
  return t;
}

// ============================================================
// if_stmt — 解析 if 语句：if exp then stmt_sequence [else stmt_sequence] end
// 结构：child[0]=条件, child[1]=then分支, child[2]=else分支(可选)
// ============================================================
TreeNode * if_stmt(void)
{ TreeNode * t = newStmtNode(IfK);
  match(IF);
  if (t!=NULL) t->child[0] = exp();        // 解析条件表达式
  match(THEN);
  if (t!=NULL) t->child[1] = stmt_sequence(); // 解析 then 分支
  if (token==ELSE) {
    match(ELSE);
    if (t!=NULL) t->child[2] = stmt_sequence(); // 解析 else 分支
  }
  match(END);
  return t;
}

// ============================================================
// repeat_stmt — 解析 repeat 语句：repeat stmt_sequence until exp
// 结构：child[0]=循环体, child[1]=条件
// ============================================================
TreeNode * repeat_stmt(void)
{ TreeNode * t = newStmtNode(RepeatK);
  match(REPEAT);
  if (t!=NULL) t->child[0] = stmt_sequence(); // 循环体
  match(UNTIL);
  if (t!=NULL) t->child[1] = exp();           // 循环条件
  return t;
}

// ============================================================
// assign_stmt — 解析赋值语句：ID := exp
// 结构：属性 name=变量名, child[0]=表达式
// ============================================================
TreeNode * assign_stmt(void)
{ TreeNode * t = newStmtNode(AssignK);
  if ((t!=NULL) && (token==ID))
    t->attr.name = copyString(tokenString);   // 保存变量名
  match(ID);
  match(ASSIGN);
  if (t!=NULL) t->child[0] = exp();           // 解析右值表达式
  return t;
}

// ============================================================
// read_stmt — 解析 read 语句：read ID
// ============================================================
TreeNode * read_stmt(void)
{ TreeNode * t = newStmtNode(ReadK);
  match(READ);
  if ((t!=NULL) && (token==ID))
    t->attr.name = copyString(tokenString);
  match(ID);
  return t;
}

// ============================================================
// write_stmt — 解析 write 语句：write exp
// 结构：child[0]=要输出的表达式
// ============================================================
TreeNode * write_stmt(void)
{ TreeNode * t = newStmtNode(WriteK);
  match(WRITE);
  if (t!=NULL) t->child[0] = exp();
  return t;
}

// ============================================================
// int_stmt — 解析 int 声明语句：int ID [:= exp] { , ID [:= exp] }
// 支持逗号分隔的多变量声明，可选带初始化表达式
// 每个变量作为 AssignK（有初始化）或 IdK（无初始化）子节点
// ============================================================
TreeNode * int_stmt(void)
{ TreeNode * t = newStmtNode(IntK);
  match(INT);
  int child_idx = 0;

  // 解析第一个变量
  if ((t!=NULL) && (token==ID))
  { char * name = copyString(tokenString);
    match(ID);
    if (token==ASSIGN)                                    // 带初始化 int x := expr
    { match(ASSIGN);
      TreeNode * assign = newStmtNode(AssignK);
      if (assign!=NULL)
      { assign->attr.name = name;
        assign->child[0] = exp();
      }
      if (child_idx < MAXCHILDREN) t->child[child_idx] = assign;
    }
    else                                                  // 仅声明 int x
    { TreeNode * id_node = newExpNode(IdK);
      if (id_node!=NULL) id_node->attr.name = name;
      if (child_idx < MAXCHILDREN) t->child[child_idx] = id_node;
    }
    child_idx++;
  }

  // 解析逗号分隔的后续变量
  while (token==COMMA && child_idx < MAXCHILDREN)
  { match(COMMA);
    if (token==ID)
    { char * name = copyString(tokenString);
      match(ID);
      if (token==ASSIGN)
      { match(ASSIGN);
        TreeNode * assign = newStmtNode(AssignK);
        if (assign!=NULL)
        { assign->attr.name = name;
          assign->child[0] = exp();
        }
        if (child_idx < MAXCHILDREN) t->child[child_idx] = assign;
      }
      else
      { TreeNode * id_node = newExpNode(IdK);
        if (id_node!=NULL) id_node->attr.name = name;
        if (child_idx < MAXCHILDREN) t->child[child_idx] = id_node;
      }
      child_idx++;
    }
  }
  // SEMI 由 stmt_sequence 负责匹配
  return t;
}

// ============================================================
// float_stmt — 解析 float 声明语句（结构同 int 声明）
// ============================================================
TreeNode * float_stmt(void)
{ TreeNode * t = newStmtNode(FloatK);
  match(FLOATKW);
  int child_idx = 0;

  // 解析第一个变量
  if ((t!=NULL) && (token==ID))
  { char * name = copyString(tokenString);
    match(ID);
    if (token==ASSIGN)
    { match(ASSIGN);
      TreeNode * assign = newStmtNode(AssignK);
      if (assign!=NULL)
      { assign->attr.name = name;
        assign->child[0] = exp();
      }
      if (child_idx < MAXCHILDREN) t->child[child_idx] = assign;
    }
    else
    { TreeNode * id_node = newExpNode(IdK);
      if (id_node!=NULL) id_node->attr.name = name;
      if (child_idx < MAXCHILDREN) t->child[child_idx] = id_node;
    }
    child_idx++;
  }

  // 解析逗号分隔的后续变量
  while (token==COMMA && child_idx < MAXCHILDREN)
  { match(COMMA);
    if (token==ID)
    { char * name = copyString(tokenString);
      match(ID);
      if (token==ASSIGN)
      { match(ASSIGN);
        TreeNode * assign = newStmtNode(AssignK);
        if (assign!=NULL)
        { assign->attr.name = name;
          assign->child[0] = exp();
        }
        if (child_idx < MAXCHILDREN) t->child[child_idx] = assign;
      }
      else
      { TreeNode * id_node = newExpNode(IdK);
        if (id_node!=NULL) id_node->attr.name = name;
        if (child_idx < MAXCHILDREN) t->child[child_idx] = id_node;
      }
      child_idx++;
    }
  }
  // SEMI 由 stmt_sequence 负责匹配
  return t;
}

// ============================================================
// exp — 解析比较表达式：simple_exp [比较运算符 simple_exp]
// 比较运算符：= < <= > >=
// ============================================================
TreeNode * exp(void)
{ TreeNode * t = simple_exp();
  if ((token==LT)||(token==LTE)||(token==GT)||(token==GTE)||(token==EQ)) {
    TreeNode * p = newExpNode(OpK);
    if (p!=NULL) {
      p->child[0] = t;              // 左操作数
      p->attr.op = token;           // 比较运算符类型
      t = p;
    }
    match(token);
    if (t!=NULL)
      t->child[1] = simple_exp();   // 右操作数
  }
  return t;
}

// ============================================================
// simple_exp — 解析加减表达式：term { (+|-) term }
// 左结合，用循环处理连续运算
// ============================================================
TreeNode * simple_exp(void)
{ TreeNode * t = term();
  while ((token==PLUS)||(token==MINUS))
  { TreeNode * p = newExpNode(OpK);
    if (p!=NULL) {
      p->child[0] = t;
      p->attr.op = token;
      t = p;
      match(token);
      t->child[1] = term();
    }
  }
  return t;
}

// ============================================================
// term — 解析乘除表达式：factor { (*|/) factor }
// 左结合，优先级高于加减
// ============================================================
TreeNode * term(void)
{ TreeNode * t = factor();
  while ((token==TIMES)||(token==OVER))
  { TreeNode * p = newExpNode(OpK);
    if (p!=NULL) {
      p->child[0] = t;
      p->attr.op = token;
      t = p;
      match(token);
      p->child[1] = factor();
    }
  }
  return t;
}

// ============================================================
// factor — 解析基本因子：NUM | FLOAT | STRING | ID | (exp) | -factor
// 这是表达式解析的最底层
// ============================================================
TreeNode * factor(void)
{ TreeNode * t = NULL;
  switch (token) {
    case NUM :
      t = newExpNode(ConstK);
      if ((t!=NULL) && (token==NUM))
      { t->attr.val = atoi(tokenString);                // 整数常量
        t->type = Integer;
      }
      match(NUM);
      break;
    case FLOAT :
      t = newExpNode(ConstK);
      if ((t!=NULL) && (token==FLOAT))
      { t->attr.fval = (float)atof(tokenString);        // 浮点常量
        t->type = Float;
      }
      match(FLOAT);
      break;
    case STRING :
      t = newExpNode(ConstK);
      if ((t!=NULL) && (token==STRING))
      { t->attr.strVal = copyString(tokenString);       // 字符串常量
        t->type = Str;
      }
      match(STRING);
      break;
    case ID :
      t = newExpNode(IdK);
      if ((t!=NULL) && (token==ID))
        t->attr.name = copyString(tokenString);          // 标识符引用
      match(ID);
      break;
    case LPAREN :
      match(LPAREN);
      t = exp();                                         // 括号内子表达式
      match(RPAREN);
      break;
    case MINUS :
      match(MINUS);
      t = newExpNode(OpK);
      if (t!=NULL)
      { t->attr.op = MINUS;                              // 一元负号
        t->child[0] = factor();                          // 只取右操作数
      }
      break;
    default:
      syntaxError("unexpected token -> ");
      printToken(token,tokenString);
      token = getToken();
      break;
    }
  return t;
}

/****************************************/
/* 语法分析器主函数                      */
/****************************************/
// parse — 启动语法分析，返回构建好的语法树
TreeNode * parse(void)
{ TreeNode * t;
  token = getToken();                // 获取第一个 token
  t = stmt_sequence();               // 从文法起始符号开始解析
  if (token!=ENDFILE)
    syntaxError("Code ends before file\n");
  return t;
}
