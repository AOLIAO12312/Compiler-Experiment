/****************************************************/
/* File: scan.c                                     */
/* The scanner implementation for the TINY compiler */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

#include "globals.h"
#include "util.h"
#include "scan.h"

/* states in scanner DFA */
typedef enum
   { START,INASSIGN,INCOMMENT,INNUM,INID,DONE,
     INFLOAT,INEXP,INEXPSIGN,INEXPNUM,
     INCOMMENT2,INCOMMENT2_STAR
   }
   StateType;

/* lexeme of identifier or reserved word */
char tokenString[MAXTOKENLEN+1];

/* BUFLEN = length of the input buffer for
   source code lines */
#define BUFLEN 256

static char lineBuf[BUFLEN]; /* holds the current line */
static int linepos = 0; /* current position in LineBuf */
static int bufsize = 0; /* current size of buffer string */
static int EOF_flag = FALSE; /* corrects ungetNextChar behavior on EOF */

/* getNextChar fetches the next non-blank character
   from lineBuf, reading in a new line if lineBuf is
   exhausted */
static int getNextChar(void)
{ if (!(linepos < bufsize))
  { lineno++;
    if (fgets(lineBuf,BUFLEN-1,source))
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

/* ungetNextChar backtracks one character
   in lineBuf */
static void ungetNextChar(void)
{ if (!EOF_flag) linepos-- ;}

/* lookup table of reserved words */
static struct
    { char* str;
      TokenType tok;
    } reservedWords[MAXRESERVED]
   = {{"if",IF},{"then",THEN},{"else",ELSE},{"end",END},
      {"repeat",REPEAT},{"until",UNTIL},{"read",READ},
      {"write",WRITE}};

/* lookup an identifier to see if it is a reserved word */
/* uses linear search */
static TokenType reservedLookup (char * s)
{ int i;
  for (i=0;i<MAXRESERVED;i++)
    if (!strcmp(s,reservedWords[i].str))
      return reservedWords[i].tok;
  return ID;
}

/****************************************/
/* the primary function of the scanner  */
/****************************************/
/* function getToken returns the 
 * next token in source file
 */
TokenType getToken(void)
{  /* index for storing into tokenString */
   int tokenStringIndex = 0;
   /* holds current token to be returned */
   TokenType currentToken;
   /* current state - always begins at START */
   StateType state = START;
   /* flag to indicate save to tokenString */
   int save;
   while (state != DONE)
   { int c = getNextChar();
     save = TRUE;
     switch (state)
     { case START:
         if (isdigit(c))
           state = INNUM;
         else if (isalpha(c))
           state = INID;
         else if (c == ':')
           state = INASSIGN;
         else if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'))
           save = FALSE;
         else if (c == '{')
         { save = FALSE;
           state = INCOMMENT;
         }
         else if (c == '/')
         { /* peek: is next char '*'? */
           int next = getNextChar();
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
         else
         { state = DONE;
           switch (c)
           { case EOF:
               save = FALSE;
               currentToken = ENDFILE;
               break;
             case '=':
               currentToken = EQ;
               break;
             case '<':
               { int next = getNextChar();
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
             case '>':
               { int next = getNextChar();
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
       /* original { } comment */
       case INCOMMENT:
         save = FALSE;
         if (c == EOF)
         { state = DONE;
           currentToken = ENDFILE;
         }
         else if (c == '}') state = START;
         break;
       /* C-style multi-line comment: waiting for * */
       case INCOMMENT2:
         save = FALSE;
         if (c == EOF)
         { state = DONE;
           fprintf(listing,"ERROR at line %d: unterminated comment\n",lineno);
           currentToken = ERROR;
         }
         else if (c == '*')
           state = INCOMMENT2_STAR;
         /* else stay in INCOMMENT2 */
         break;
       /* C-style multi-line comment: saw *, waiting for / */
       case INCOMMENT2_STAR:
         save = FALSE;
         if (c == EOF)
         { state = DONE;
           fprintf(listing,"ERROR at line %d: unterminated comment\n",lineno);
           currentToken = ERROR;
         }
         else if (c == '/')
           state = START;   /* comment closed */
         else if (c == '*')
           /* stay: another * seen */;
         else
           state = INCOMMENT2;
         break;
       case INASSIGN:
         state = DONE;
         if (c == '=')
           currentToken = ASSIGN;
         else
         { /* backup in the input */
           ungetNextChar();
           save = FALSE;
           currentToken = ERROR;
         }
         break;
       /* integer part of a number */
       case INNUM:
         if (c == '.')
         { /* peek next char to decide float vs error */
           int next = getNextChar();
           ungetNextChar(); /* always put next back; '.' is still current c */
           if (isdigit(next) || next == 'E' || next == 'e')
           { /* valid float start: keep '.' in tokenString, go to INFLOAT */
             state = INFLOAT;
           }
           else
           { /* e.g. "3." with no digit/E after -> just NUM, put '.' back */
             ungetNextChar(); /* put back '.' itself */
             save = FALSE;
             state = DONE;
             currentToken = NUM;
           }
         }
         else if (c == 'E' || c == 'e')
           state = INEXP;
         else if (isalpha(c))
         { /* e.g. 2n -> error */
           save = FALSE;
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal token '%s%c'\n",
                   lineno,tokenString,(char)c);
           currentToken = ERROR;
         }
         else if (!isdigit(c))
         { ungetNextChar();
           save = FALSE;
           state = DONE;
           currentToken = NUM;
         }
         break;
       /* fractional digits after '.' */
       case INFLOAT:
         if (c == 'E' || c == 'e')
           state = INEXP;
         else if (c == '.')
         { /* second dot -> error, e.g. 1.2.3 */
           save = FALSE;
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
         { ungetNextChar();
           save = FALSE;
           state = DONE;
           currentToken = FLOAT;
         }
         break;
       /* seen E/e, expect optional sign or digit */
       case INEXP:
         if (c == '+' || c == '-')
           state = INEXPSIGN;
         else if (isdigit(c))
           state = INEXPNUM;
         else
         { /* E not followed by sign or digit -> error */
           save = FALSE;
           state = DONE;
           tokenString[tokenStringIndex] = '\0';
           fprintf(listing,"ERROR at line %d: illegal exponent in '%s'\n",
                   lineno,tokenString);
           currentToken = ERROR;
         }
         break;
       /* seen E+ or E-, must have digit next */
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
       /* digits of exponent */
       case INEXPNUM:
         if (c == '.')
         { /* e.g. 100.E1.2 -> error */
           save = FALSE;
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
         { ungetNextChar();
           save = FALSE;
           state = DONE;
           currentToken = FLOAT;
         }
         break;
       case INID:
         if (!isalpha(c))
         { /* backup in the input */
           ungetNextChar();
           save = FALSE;
           state = DONE;
           currentToken = ID;
         }
         break;
       case DONE:
       default: /* should never happen */
         fprintf(listing,"Scanner Bug: state= %d\n",state);
         state = DONE;
         currentToken = ERROR;
         break;
     }
     if ((save) && (tokenStringIndex <= MAXTOKENLEN))
       tokenString[tokenStringIndex++] = (char) c;
     if (state == DONE)
     { tokenString[tokenStringIndex] = '\0';
       if (currentToken == ID)
         currentToken = reservedLookup(tokenString);
     }
   }
   if (TraceScan) {
     fprintf(listing,"\t%d: ",lineno);
     printToken(currentToken,tokenString);
   }
   return currentToken;
} /* end getToken */

