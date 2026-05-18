#include "makedot.h"
/*
用于递归输出Graphviz(DOT语言)文件
输入：输出流、语法树“根节点”、递归深度（仅用作打印记号）
输出：指定路径的Graphviz(DOT语言)文件
*/
char *optab[] = { "EOF","ERR",
                    "IF","THEN","ELSE","END","REPEAT","UNTIL","READ","WRITE","INT",
                    "ID","NUM","FLOAT",
                    ":=","=","<","<=",">",">=","+","-","*","/","(",")",";",","
                  };

const int N = 100;
TreeNode * drawn_opnode[N] = {NULL};
int op_index = 0;
int isdrawn(TreeNode* t){// 判断当前op节点是否已经绘制
    int res = FALSE;
    if (t != NULL){
        for (int i = 0; i < N; i++){
            if (drawn_opnode[i] == NULL) break;
            if (drawn_opnode[i] == t){// 指向相同的op节点
                res = TRUE;
                break;
            }
        }
    }else {
        printf("treenode id null\n");
    }
    return res;
}

void CreateGraphvizFormat(FILE* pf, TreeNode* syntaxtree, unsigned depth)
{
    //
    if (syntaxtree->nodekind == StmtK){
        switch (syntaxtree->kind.stmt)
        {
        case IfK:
            fprintf(pf, "\"%d\"[label = \"[IfK]\"];\n", syntaxtree);
            CreateGraphvizFormat(pf, syntaxtree->child[0], depth+1);//exp
            CreateGraphvizFormat(pf, syntaxtree->child[1], depth+1);//stmt_seq
            fprintf(pf, "\"%d\"->{\"%d\"\"%d\"", syntaxtree, syntaxtree->child[0], syntaxtree->child[1]);
            if (syntaxtree->child[2]) {
                CreateGraphvizFormat(pf, syntaxtree->child[0], depth+1);//stmt_seq
                fprintf(pf, "\"%d\"", syntaxtree->child[2]);
            }
            fprintf(pf, "};\n");
            break;
        case RepeatK:
            fprintf(pf, "\"%d\"[label = \"[RepeatK]\"];\n", syntaxtree);
            CreateGraphvizFormat(pf, syntaxtree->child[0], depth+1);//stmt_seq
            CreateGraphvizFormat(pf, syntaxtree->child[1], depth+1);//exp
            fprintf(pf, "\"%d\"->{\"%d\"\"%d\"};\n", syntaxtree, syntaxtree->child[0], syntaxtree->child[1]);
            break;
        case AssignK:
            fprintf(pf, "\"%d\"[label = \"[AssignK:%s]\"];\n", syntaxtree, syntaxtree->attr.name);
            CreateGraphvizFormat(pf, syntaxtree->child[0], depth+1);//exp
            fprintf(pf, "\"%d\"->{\"%d\"};\n", syntaxtree, syntaxtree->child[0]);
            break;
        case ReadK:
            fprintf(pf, "\"%d\"[label = \"[ReadK:%s]\"];\n", syntaxtree, syntaxtree->attr.name);
            break;
        case WriteK:
            fprintf(pf, "\"%d\"[label = \"[WriteK]\"];\n", syntaxtree);
            CreateGraphvizFormat(pf, syntaxtree->child[0], depth+1);//exp
            fprintf(pf, "\"%d\"->{\"%d\"};\n", syntaxtree, syntaxtree->child[0]);
            break;
        case IntK:
            fprintf(pf, "\"%d\"[label = \"[IntK]\"];\n", syntaxtree);
            for (int i = 0; i < MAXCHILDREN; i++){
                if (syntaxtree->child[i]){
                    CreateGraphvizFormat(pf, syntaxtree->child[i], depth+1);//identifier or expression
                    fprintf(pf, "\"%d\"->{\"%d\"};\n", syntaxtree, syntaxtree->child[i]);
                }else break;
            }
            break;
        default:
            break;
        }
        if (syntaxtree->sibling) {
            CreateGraphvizFormat(pf, syntaxtree->sibling, depth+1);//stmt
            fprintf(pf, "\"%d\"->{\"%d\"};\n", syntaxtree, syntaxtree->sibling);
        }
    }
    //
    else if (syntaxtree->nodekind == ExpK){
        switch (syntaxtree->kind.exp)
        {
        case OpK:
            if (!isdrawn(syntaxtree)){// 当前op节点尚未绘制
                fprintf(pf, "\"%d\"[label = \"[OpK:%s]\"];\n", syntaxtree, optab[syntaxtree->attr.op]);
                if (syntaxtree->child[1] == NULL){// 单目运算符只有一个孩子节点
                    CreateGraphvizFormat(pf, syntaxtree->child[0], depth+1);
                    fprintf(pf, "\"%d\"->\"%d\";\n", syntaxtree, syntaxtree->child[0]);
                }else{
                    CreateGraphvizFormat(pf, syntaxtree->child[0], depth+1);
                    CreateGraphvizFormat(pf, syntaxtree->child[1], depth+1);
                    fprintf(pf, "\"%d\"->\"%d\"[label = \"L\"];\n", syntaxtree, syntaxtree->child[0]);
                    fprintf(pf, "\"%d\"->\"%d\"[label = \"R\"];\n", syntaxtree, syntaxtree->child[1]);
                }
                

                drawn_opnode[op_index++] = syntaxtree;
            }
            break;
        case ConstK:
            if ((int)syntaxtree->attr.val == syntaxtree->attr.val){
                fprintf(pf, "\"%d\"[label = \"[ConstK:%d]\"];\n", syntaxtree, (int)syntaxtree->attr.val);
            }else{
                fprintf(pf, "\"%d\"[label = \"[ConstK:%f]\"];\n", syntaxtree, syntaxtree->attr.val);
            }
            break;
        case IdK:
            fprintf(pf, "\"%d\"[label = \"[IdK:%s]\"];\n", syntaxtree, syntaxtree->attr.name);
            break;
        default:
            break;
        }
    }
    else {
        printf("makedot wrong\n");
    }
}

/*
输出Graphviz(DOT语言)文件
输入：输出文件路径
输出：指定路径的Graphviz(DOT语言)文件
*/
void outputGraphvizFormat(const char* outputFilePath, TreeNode* syntaxtree)
{
    FILE *pf = fopen(outputFilePath, "w");
    if (pf == NULL) {
      printf("Unable to open %s\n",outputFilePath);
      exit(1);
    }
    fprintf(pf, "digraph SyntaxTree {\n");
    CreateGraphvizFormat(pf, syntaxtree, 1);
    fprintf(pf, "}");
}