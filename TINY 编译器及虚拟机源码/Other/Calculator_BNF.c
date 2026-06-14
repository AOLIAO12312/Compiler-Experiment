/* 路径1：基于BNF文法的计算器
 * 去除左递归，处理继承属性与综合属性
 *
 * 原始BNF文法（含左递归）：
 *   exp    → exp addop term | term
 *   addop  → + | -
 *   term   → term mulop factor | factor
 *   mulop  → * | /
 *   factor → ( exp ) | Number
 *
 * 去除左递归后：
 *   exp     → term exp'
 *   exp'    → addop term exp' | ε
 *   addop   → + | -
 *   term    → factor term'
 *   term'   → mulop factor term' | ε
 *   mulop   → * | /
 *   factor  → ( exp ) | Number
 *
 * 属性说明：
 *   exp'.inh  — 继承属性，从左侧传入的累积值
 *   exp'.syn  — 综合属性，最终计算结果
 *   term'.inh — 继承属性
 *   term'.syn — 综合属性
 */

#include <stdio.h>
#include <stdlib.h>

char token;

int expr(void);
int exp_prime(int inh);
int term(void);
int term_prime(int inh);
int factor(void);

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

/* exp → term exp'
 * exp.val = exp'(term.val).syn
 */
int expr(void)
{
    int temp = term();
    return exp_prime(temp);
}

/* exp' → addop term exp' | ε
 * 继承属性 inh 为左侧已累积的值
 * 综合属性 syn 为最终结果
 *
 * 引线产生式：exp' → ε 时，syn = inh（将继承属性直接传递为综合属性）
 */
int exp_prime(int inh)
{
    if (token == '+') {
        match('+');
        int t = term();
        return exp_prime(inh + t);
    } else if (token == '-') {
        match('-');
        int t = term();
        return exp_prime(inh - t);
    }
    /* ε产生式：syn = inh */
    return inh;
}

/* term → factor term'
 * term.val = term'(factor.val).syn
 */
int term(void)
{
    int temp = factor();
    return term_prime(temp);
}

/* term' → mulop factor term' | ε
 * 继承属性 inh 为左侧已累积的值
 * 引线产生式：term' → ε 时，syn = inh
 */
int term_prime(int inh)
{
    if (token == '*') {
        match('*');
        int f = factor();
        return term_prime(inh * f);
    } else if (token == '/') {
        match('/');
        int f = factor();
        if (f == 0) {
            fprintf(stderr, "error: division by zero\n");
            exit(1);
        }
        return term_prime(inh / f);
    }
    /* ε产生式：syn = inh */
    return inh;
}

/* factor → ( exp ) | Number */
int factor(void)
{
    int temp = 0;

    if (token == '(') {
        match('(');
        temp = expr();
        match(')');
    } else if (token >= '0' && token <= '9') {
        ungetc(token, stdin);
        scanf("%d", &temp);
        token = getchar();
    } else {
        error();
    }

    return temp;
}

int main(void)
{
    int result;

    printf("BNF Calculator (left-recursion removed, inherited+synthesized attributes)\n");
    printf("Enter expression: ");

    token = getchar();
    result = expr();

    if (token == '\n')
        printf("Result = %d\n", result);
    else
        error();

    return 0;
}
