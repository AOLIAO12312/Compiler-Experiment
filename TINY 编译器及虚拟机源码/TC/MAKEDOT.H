#include "globals.h"

/*
用于递归输出Graphviz(DOT语言)文件
输入：输出流、语法树“根节点”、递归深度（仅用作打印记号）
输出：指定路径的Graphviz(DOT语言)文件
*/
void CreateGraphvizFormat(FILE* pf, treeNode* syntaxtree, unsigned depth);

/*
输出Graphviz(DOT语言)文件
输入：输出文件路径
输出：指定路径的Graphviz(DOT语言)文件
*/
void outputGraphvizFormat(const char* outputFilePath, treeNode* syntaxtree);