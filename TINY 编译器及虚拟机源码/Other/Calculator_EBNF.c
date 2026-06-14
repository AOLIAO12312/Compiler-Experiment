/* 路径2：基于EBNF文法的计算器
 * 仅使用综合属性，结合中缀转后缀实现
 *
 * EBNF文法：
 *   exp    → term { addop term }
 *   addop  → + | -
 *   term   → factor { mulop factor }
 *   mulop  → * | /
 *   factor → ( exp ) | Number
 *
 * 实现方式：
 *   1. 解析阶段：将中缀表达式转换为后缀（逆波兰）表达式
 *   2. 求值阶段：使用栈对后缀表达式求值
 *   所有属性均为综合属性（自底向上传递）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS 256
#define MAX_STACK  128

/* 后缀表达式的token类型 */
typedef enum { TK_NUM, TK_OP } TokenType;

typedef struct {
    TokenType type;
    int value;    /* TK_NUM时为数值，TK_OP时为运算符字符 */
} PostfixToken;

PostfixToken postfix[MAX_TOKENS];
int postfix_count = 0;

char token;

void error(void)
{
    fprintf(stderr, "error\n");
    exit(1);
}

void match(char expectedToken)
{
    if (token == expectedToken)
        token = getchar();
    else
        error();
}

void emit_number(int num)
{
    if (postfix_count >= MAX_TOKENS) error();
    postfix[postfix_count].type = TK_NUM;
    postfix[postfix_count].value = num;
    postfix_count++;
}

void emit_op(char op)
{
    if (postfix_count >= MAX_TOKENS) error();
    postfix[postfix_count].type = TK_OP;
    postfix[postfix_count].value = op;
    postfix_count++;
}

/* 前向声明 */
void parse_exp(void);
void parse_term(void);
void parse_factor(void);

/* exp → term { addop term }
 * EBNF的重复部分用while循环实现
 * 综合属性：每次循环产生一个运算符token到后缀序列
 */
void parse_exp(void)
{
    parse_term();
    while (token == '+' || token == '-') {
        char op = token;
        match(token);
        parse_term();
        emit_op(op);  /* 综合：运算符在两个操作数之后输出 */
    }
}

/* term → factor { mulop factor }
 * 综合属性：乘除运算符在操作数之后输出
 */
void parse_term(void)
{
    parse_factor();
    while (token == '*' || token == '/') {
        char op = token;
        match(token);
        parse_factor();
        emit_op(op);
    }
}

/* factor → ( exp ) | Number
 * 综合属性：数字直接输出到后缀序列
 */
void parse_factor(void)
{
    if (token == '(') {
        match('(');
        parse_exp();
        match(')');
    } else if (token >= '0' && token <= '9') {
        int num;
        ungetc(token, stdin);
        scanf("%d", &num);
        token = getchar();
        emit_number(num);
    } else {
        error();
    }
}

/* 后缀表达式求值（栈计算） */
int evaluate_postfix(void)
{
    int stack[MAX_STACK];
    int top = -1;

    for (int i = 0; i < postfix_count; i++) {
        if (postfix[i].type == TK_NUM) {
            stack[++top] = postfix[i].value;
        } else {
            if (top < 1) error();
            int right = stack[top--];
            int left  = stack[top--];
            switch (postfix[i].value) {
                case '+': stack[++top] = left + right; break;
                case '-': stack[++top] = left - right; break;
                case '*': stack[++top] = left * right; break;
                case '/':
                    if (right == 0) {
                        fprintf(stderr, "error: division by zero\n");
                        exit(1);
                    }
                    stack[++top] = left / right;
                    break;
            }
        }
    }

    if (top != 0) error();
    return stack[0];
}

/* 打印后缀表达式（用于展示中缀转后缀的结果） */
void print_postfix(void)
{
    printf("Postfix: ");
    for (int i = 0; i < postfix_count; i++) {
        if (postfix[i].type == TK_NUM)
            printf("%d ", postfix[i].value);
        else
            printf("%c ", postfix[i].value);
    }
    printf("\n");
}

int main(void)
{
    int result;

    printf("EBNF Calculator (synthesized attributes + infix-to-postfix)\n");
    printf("Enter expression: ");

    token = getchar();
    parse_exp();

    if (token != '\n')
        error();

    print_postfix();
    result = evaluate_postfix();
    printf("Result = %d\n", result);

    return 0;
}
