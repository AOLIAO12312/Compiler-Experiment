/****************************************************/
/* File: analyze.c                                  */
/* Semantic analyzer implementation                 */
/* for the TINY compiler                            */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

// 语义分析器：包含两个主要阶段 —
//   1. 构建符号表（前序遍历，收集所有变量定义）
//   2. 类型检查（后序遍历，自底向上推导表达式类型并检查语义错误）

#include "globals.h"
#include "symtab.h"
#include "analyze.h"

static int location = 0;   // 变量内存地址分配计数器

// ============================================================
// traverse — 通用递归树遍历框架
// preProc：前序遍历时对每个节点执行的操作
// postProc：后序遍历时对每个节点执行的操作
// 遍历顺序：preProc(当前) → 递归子节点 → postProc(当前) → 递归兄弟节点
// ============================================================
static void traverse( TreeNode * t,
               void (* preProc) (TreeNode *),
               void (* postProc) (TreeNode *) )
{ if (t != NULL)
  { preProc(t);                                          // 前序回调
    { int i;
      for (i=0; i < MAXCHILDREN; i++)
        traverse(t->child[i],preProc,postProc);          // 递归遍历子节点
    }
    postProc(t);                                         // 后序回调
    traverse(t->sibling,preProc,postProc);                // 遍历兄弟节点
  }
}

// ============================================================
// nullProc — 空操作，用于只需要前序或只需要后序的遍历
// ============================================================
static void nullProc(TreeNode * t)
{ if (t==NULL) return;
  else return;
}

// ============================================================
// insertNode — 前序回调：将节点中的标识符插入符号表
// 处理三种情况：
//   - 赋值/read 语句中的变量（首次出现则分配地址）
//   - int/float 声明中的变量列表
//   - 表达式中引用的标识符
// ============================================================
static void insertNode( TreeNode * t)
{ switch (t->nodekind)
  { case StmtK:
      switch (t->kind.stmt)
      { case AssignK:                                     // 赋值语句的目标变量
        case ReadK:                                       // read 的目标变量
          if (st_lookup(t->attr.name) == -1)
            // 首次出现：分配新地址
            st_insert(t->attr.name,t->lineno,location++,Integer);
          else
            // 已声明：仅记录引用行号，不分配新地址
            st_insert(t->attr.name,t->lineno,0,Integer);
          break;
        case IntK:                                        // int 声明：遍历逗号分隔的变量列表
          { int i;
            for (i=0; i < MAXCHILDREN; i++)
            { if (t->child[i] != NULL)
              { char * name = NULL;
                if (t->child[i]->nodekind == ExpK && t->child[i]->kind.exp == IdK)
                  name = t->child[i]->attr.name;          // 仅声明
                else if (t->child[i]->nodekind == StmtK && t->child[i]->kind.stmt == AssignK)
                  name = t->child[i]->attr.name;          // 带初始化的声明
                if (name != NULL && st_lookup(name) == -1)
                  st_insert(name,t->lineno,location++,Integer);
              }
            }
          }
          break;
        case FloatK:                                      // float 声明（逻辑同 int）
          { int i;
            for (i=0; i < MAXCHILDREN; i++)
            { if (t->child[i] != NULL)
              { char * name = NULL;
                if (t->child[i]->nodekind == ExpK && t->child[i]->kind.exp == IdK)
                  name = t->child[i]->attr.name;
                else if (t->child[i]->nodekind == StmtK && t->child[i]->kind.stmt == AssignK)
                  name = t->child[i]->attr.name;
                if (name != NULL && st_lookup(name) == -1)
                  st_insert(name,t->lineno,location++,Float);
              }
            }
          }
          break;
        default:
          break;
      }
      break;
    case ExpK:
      switch (t->kind.exp)
      { case IdK:                                         // 表达式中引用的标识符
          if (st_lookup(t->attr.name) == -1)
            st_insert(t->attr.name,t->lineno,location++,Integer);
          else
            st_insert(t->attr.name,t->lineno,0,Integer);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

// ============================================================
// buildSymtab — 构建符号表：前序遍历语法树，收集所有标识符定义
// ============================================================
void buildSymtab(TreeNode * syntaxTree)
{ traverse(syntaxTree,insertNode,nullProc);                // 仅前序，后序为空
  if (TraceAnalyze)
  { fprintf(listing,"\nSymbol table:\n\n");
    printSymTab(listing);
  }
}

// ============================================================
// typeError — 报告类型错误
// ============================================================
static void typeError(TreeNode * t, char * message)
{ fprintf(listing,"Type error at line %d: %s\n",t->lineno,message);
  Error = TRUE;
}

// ============================================================
// checkNode — 后序回调：对单个节点进行类型检查与推导
// 表达式节点：自底向上推导结果类型
// 语句节点：检查语义约束（条件需布尔、赋值类型匹配等）
// ============================================================
static void checkNode(TreeNode * t)
{ switch (t->nodekind)
  { case ExpK:
      switch (t->kind.exp)
      { case OpK:
          if (t->child[1] == NULL) {
            // 一元运算符（仅有负号 -）：操作数需为数值类型
            if (t->child[0]->type != Integer && t->child[0]->type != Float)
              typeError(t,"Op applied to non-integer/non-float");
            t->type = t->child[0]->type;
          } else {
            // 二元运算符
            ExpType leftType = t->child[0]->type;
            ExpType rightType = t->child[1]->type;
            if ((t->attr.op == EQ) || (t->attr.op == LT) || (t->attr.op == LTE) ||
                (t->attr.op == GT) || (t->attr.op == GTE))
            { // 比较运算符：操作数需为数值，结果为 Boolean
              if ((leftType != Integer && leftType != Float) ||
                  (rightType != Integer && rightType != Float))
                typeError(t,"comparison applied to non-numeric type");
              t->type = Boolean;
            }
            else
            { // 算术运算符：操作数需为数值，任一 Float 则结果为 Float
              if ((leftType != Integer && leftType != Float) ||
                  (rightType != Integer && rightType != Float))
                typeError(t,"Op applied to non-numeric type");
              if (leftType == Float || rightType == Float)
                t->type = Float;
              else
                t->type = Integer;
            }
          }
          break;
        case ConstK:
          if (t->type == Void) t->type = Integer;         // 未标注类型的常量默认为 Integer
          break;
        case IdK:
          t->type = st_lookup_type(t->attr.name);          // 从符号表查询标识符类型
          break;
        default:
          break;
      }
      break;
    case StmtK:
      switch (t->kind.stmt)
      { case IfK:                                          // if 条件必须为 Boolean
          if (t->child[0]->type == Integer || t->child[0]->type == Float)
            typeError(t->child[0],"if test is not Boolean");
          break;
        case AssignK:                                      // 赋值右值需为数值
          { ExpType lhsType = st_lookup_type(t->attr.name);
            ExpType rhsType = t->child[0]->type;
            if (rhsType != Integer && rhsType != Float)
              typeError(t->child[0],"assignment of non-numeric value");
          }
          break;
        case WriteK:                                       // write 只能输出数值或字符串
          if (t->child[0]->type != Integer &&
              t->child[0]->type != Float &&
              t->child[0]->type != Str)
            typeError(t->child[0],"write of invalid type");
          break;
        case RepeatK:                                      // repeat 条件需为 Boolean
          if (t->child[1]->type == Integer || t->child[1]->type == Float)
            typeError(t->child[1],"repeat test is not Boolean");
          break;
        default:
          break;
      }
      break;
    default:
      break;

  }
}

// ============================================================
// typeCheck — 类型检查入口：后序遍历语法树，自底向上推导类型并检查
// ============================================================
void typeCheck(TreeNode * syntaxTree)
{ traverse(syntaxTree,nullProc,checkNode);                 // 仅后序，前序为空
}
