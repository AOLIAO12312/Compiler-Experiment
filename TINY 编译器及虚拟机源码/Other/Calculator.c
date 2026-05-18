/* Simple integer arithmetic calculator according to the EBNF:
  <exp>   → <term> <addop> <exp> | <term>
  <addop> → + | -
  <term>  → <factor> <mulop> <term> | <factor>
  <mulop> → * | /
  <factor> → ( <exp> ) | <unaryop> <factor> | Number
  <unaryop>→ + | -
  Inputs a line of text from stdin.
  Outputs "error" or the result.
*/

#include <stdio.h>
#include <stdlib.h>

char  token; /* global token variable */
/*function prototype for recursive calls*/
int exp(void);
int term(void);
int factor(void);

void error(void)
{
	fprintf(stderr, "error\n");
	exit(1);
}

void match(char expectedToken)
{
	if (token==expectedToken)  token=getchar();
	else error();
}

int main()
{
	int result;

	token=getchar(); /*load token with first character for lookahead*/
	result=exp();
	if (token=='\n')     /*check for end of line*/
		printf("Result = %d\n", result);
	else error();        /*extraneous chars on line*/

	return 0;
}

int exp(void)
{
	int temp=term();
	if ( (token=='+') || (token=='-') )
		switch (token)
		{
		case '+':  match('+');
		           temp+=exp();
		           break;
		case '-':  match('-');
		           temp-=exp();
		           break;
		}
	return temp;
}

int term(void)
{
	int temp=factor();
	if ( (token=='*') || (token=='/') )
	{
		switch (token)
		{
		case '*':  match('*');
		           temp*=term();
		           break;
		case '/':  match('/');
		           temp/=term();
		           break;
		}
	}
	return temp;
}

int factor(void)
{
	int temp;

	if (token=='(')
	{
		match('(');   temp = exp();   match(')');
	}
	else if (token=='+')
	{
		match('+');
		temp = factor();
	}
	else if (token=='-')
	{
		match('-');
		temp = -factor();
	}
	else if (token>=48 && token<=57)  // if (isdigit(token))
	{
		ungetc(token,stdin);
		scanf("%d",&temp);
		token = getchar();
	}
	else error();

	return temp;
}
