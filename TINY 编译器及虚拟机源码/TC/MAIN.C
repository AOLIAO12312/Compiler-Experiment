/****************************************************/
/* File: main.c                                     */
/* Main program for TINY compiler                   */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

// TINY 编译器入口：按顺序执行 词法分析→语法分析→语义分析→目标代码生成
#include "globals.h"

// ============================================================
// 编译阶段开关：可按需裁剪流水线，方便分阶段调试
// ============================================================

// 为 TRUE 时只做词法分析（扫描全部 token）
#define NO_PARSE FALSE
// 为 TRUE 时只做词法+语法分析（生成语法树后停止）
#define NO_ANALYZE FALSE
// 为 TRUE 时不做目标代码生成（跳过 cgen）
#define NO_CODE FALSE

// 按编译开关决定引入哪些阶段的头文件，未启用的阶段不参与编译
#include "util.h"
#if NO_PARSE
#include "scan.h"
#else
#include "parse.h"
#include "makedot.h"
#if !NO_ANALYZE
#include "analyze.h"
#if !NO_CODE
#include "cgen.h"
#endif
#endif
#endif

// ============================================================
// 全局变量
// ============================================================

int lineno = 0;       // 当前行号
FILE * source;        // 源代码文件指针
FILE * listing;       // 输出列表文件指针（默认 stdout）
FILE * code;          // 目标代码文件指针（.tm）

// 跟踪调试开关：控制各阶段是否输出中间信息
int EchoSource   = TRUE;   // 回显源代码并标注行号
int TraceScan    = TRUE;   // 输出词法单元信息
int TraceParse   = TRUE;   // 输出语法树结构
int TraceAnalyze = FALSE;  // 输出符号表构建与类型检查信息
int TraceCode    = FALSE;  // 在目标代码中写入注释

int Error = FALSE;   // 错误标志，任一阶段出错则跳过后续阶段

// ============================================================
// 主函数：串联完整编译流水线
// ============================================================
int main( int argc, char * argv[] )
{ TreeNode * syntaxTree;        // 语法树根节点
  char pgm[120];                // 源码文件名（可缺省 .tny 后缀）

  // ---------- 1. 参数校验 ----------
  if (argc != 2)
    { fprintf(stderr,"usage: %s <filename>\n",argv[0]);
      exit(1);
    }

  // ---------- 2. 打开源文件 ----------
  strcpy(pgm,argv[1]) ;
  if (strchr (pgm, '.') == NULL)   // 无后缀时自动补 .tny
     strcat(pgm,".tny");
  source = fopen(pgm,"r");
  if (source==NULL)
  { fprintf(stderr,"File %s not found\n",pgm);
    exit(1);
  }
  listing = stdout;                // 编译信息输出到屏幕
  fprintf(listing,"\nTINY COMPILATION: %s\n",pgm);

#if NO_PARSE
  // ---- 仅词法分析：逐个扫描 token 直至文件结束 ----
  while (getToken()!=ENDFILE);
#else
  // ---------- 3. 语法分析：生成语法树 ----------
  syntaxTree = parse();
  if (TraceParse) {
    fprintf(listing,"\nSyntax tree:\n");
    printTree(syntaxTree);
  }

  // ---------- 4. 导出 Graphviz 格式的 AST（用于可视化） ----------
  if (!Error)
  { char * dotfile;
    int fnlen = strcspn(pgm,".");        // 取文件名（不含后缀）
    dotfile = (char *) calloc(fnlen+5, sizeof(char));
    strncpy(dotfile,pgm,fnlen);
    dotfile[fnlen] = '\0';
    strcat(dotfile,".dot");              // 生成 .dot 文件供 Graphviz 渲染
    outputGraphvizFormat(dotfile,syntaxTree);
    fprintf(listing,"\nAST Graphviz file generated: %s\n",dotfile);
    free(dotfile);
  }

#if !NO_ANALYZE
  // ---------- 5. 语义分析：构建符号表并进行类型检查 ----------
  if (! Error)
  { if (TraceAnalyze) fprintf(listing,"\nBuilding Symbol Table...\n");
    buildSymtab(syntaxTree);             // 前序遍历，建立符号表
    if (TraceAnalyze) fprintf(listing,"\nChecking Types...\n");
    typeCheck(syntaxTree);               // 后序遍历，检查类型是否匹配
    if (TraceAnalyze) fprintf(listing,"\nType Checking Finished\n");
  }

#if !NO_CODE
  // ---------- 6. 代码生成：遍历语法树，输出 TM 虚拟机指令 ----------
  if (! Error)
  { char * codefile;
    int fnlen = strcspn(pgm,".");
    codefile = (char *) calloc(fnlen+4, sizeof(char));
    strncpy(codefile,pgm,fnlen);
    strcat(codefile,".tm");              // 目标代码文件与源文件同名，后缀 .tm
    code = fopen(codefile,"w");
    if (code == NULL)
    { printf("Unable to open %s\n",codefile);
      exit(1);
    }
    codeGen(syntaxTree,codefile);        // 递归遍历语法树生成 TM 指令
    fclose(code);
  }
#endif
#endif
#endif

  fclose(source);
  return 0;
}

