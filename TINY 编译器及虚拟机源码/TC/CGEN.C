/****************************************************/
/* File: cgen.c                                     */
/* The code generator implementation                */
/* for the TINY compiler                            */
/* (generates code for the TM machine)              */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

#include "globals.h"
#include "symtab.h"
#include "code.h"
#include "cgen.h"

/* tmpOffset is the memory offset for temps
   It is decremented each time a temp is
   stored, and incremeted when loaded again
*/
static int tmpOffset = 0;

/* prototype for internal recursive code generator */
static void cGen (TreeNode * tree);

/* Procedure genStmt generates code at a statement node */
static void genStmt( TreeNode * tree)
{ TreeNode * p1, * p2, * p3;
  int savedLoc1,savedLoc2,currentLoc;
  int loc;
  switch (tree->kind.stmt) {

      case IfK :
         if (TraceCode) emitComment("-> if") ;
         p1 = tree->child[0] ;
         p2 = tree->child[1] ;
         p3 = tree->child[2] ;
         /* generate code for test expression */
         cGen(p1);
         savedLoc1 = emitSkip(1) ;
         emitComment("if: jump to else belongs here");
         /* recurse on then part */
         cGen(p2);
         savedLoc2 = emitSkip(1) ;
         emitComment("if: jump to end belongs here");
         currentLoc = emitSkip(0) ;
         emitBackup(savedLoc1) ;
         emitRM_Abs("JEQ",ac,currentLoc,"if: jmp to else");
         emitRestore() ;
         /* recurse on else part */
         cGen(p3);
         currentLoc = emitSkip(0) ;
         emitBackup(savedLoc2) ;
         emitRM_Abs("LDA",pc,currentLoc,"jmp to end") ;
         emitRestore() ;
         if (TraceCode)  emitComment("<- if") ;
         break; /* if_k */

      case RepeatK:
         if (TraceCode) emitComment("-> repeat") ;
         p1 = tree->child[0] ;
         p2 = tree->child[1] ;
         savedLoc1 = emitSkip(0);
         emitComment("repeat: jump after body comes back here");
         /* generate code for body */
         cGen(p1);
         /* generate code for test */
         cGen(p2);
         emitRM_Abs("JEQ",ac,savedLoc1,"repeat: jmp back to body");
         if (TraceCode)  emitComment("<- repeat") ;
         break; /* repeat */

      case AssignK:
         if (TraceCode) emitComment("-> assign") ;
         /* generate code for rhs */
         cGen(tree->child[0]);
         /* now store value */
         loc = st_lookup(tree->attr.name);
         if (st_lookup_type(tree->attr.name) == Float)
         { if (tree->child[0]->type == Integer)
             emitRO("ITOF",ac,ac,0,"promote int to float");
           emitRM("FST",ac,loc,gp,"assign: store float value");
         }
         else
           emitRM("ST",ac,loc,gp,"assign: store value");
         if (TraceCode)  emitComment("<- assign") ;
         break; /* assign_k */

      case ReadK:
         if (st_lookup_type(tree->attr.name) == Float)
         { emitRO("FIN",ac,0,0,"read float value");
           loc = st_lookup(tree->attr.name);
           emitRM("FST",ac,loc,gp,"read: store float value");
         }
         else
         { emitRO("IN",ac,0,0,"read integer value");
           loc = st_lookup(tree->attr.name);
           emitRM("ST",ac,loc,gp,"read: store value");
         }
         break;
      case WriteK:
         if (tree->child[0]->nodekind == ExpK &&
             tree->child[0]->kind.exp == ConstK &&
             tree->child[0]->type == Str)
         { /* string literal: emit OUTC per character */
           char *s = tree->child[0]->attr.strVal;
           if (s != NULL)
           { for (int i = 0; s[i] != '\0'; i++)
             { emitRM("LDC",ac,(int)(unsigned char)s[i],0,"load char");
               emitRO("OUTC",ac,0,0,"output char");
             }
           }
           emitRM("LDC",ac,'\n',0,"load newline");
           emitRO("OUTC",ac,0,0,"output newline");
         }
         else if (tree->child[0]->type == Float)
         { /* float expression */
           cGen(tree->child[0]);
           emitRO("FOUT",ac,0,0,"write float");
         }
         else
         { /* integer expression */
           cGen(tree->child[0]);
           emitRO("OUT",ac,0,0,"write ac");
         }
         break;
      case IntK:
      case FloatK:
         { int i;
           for (i=0; i < MAXCHILDREN; i++)
           { if (tree->child[i] != NULL)
             { if (tree->child[i]->nodekind == StmtK &&
                   tree->child[i]->kind.stmt == AssignK)
                 /* initialized variable: generate assignment code */
                 cGen(tree->child[i]);
               /* uninitialized variable: just allocate, analyzer did st_insert */
             }
           }
         }
         break;
      default:
         break;
    }
} /* genStmt */

/* Procedure genExp generates code at an expression node */
static void genExp( TreeNode * tree)
{ int loc;
  TreeNode * p1, * p2;
  switch (tree->kind.exp) {

    case ConstK :
      if (TraceCode) emitComment("-> Const") ;
      if (tree->type == Float)
      { float f = tree->attr.fval;
        int bits;
        memcpy(&bits, &f, sizeof(int));
        emitRM("FLDC",ac,bits,0,"load float const");
      }
      else if (tree->type == Str)
      { /* string constants handled in WriteK, no code needed here */ }
      else
        emitRM("LDC",ac,tree->attr.val,0,"load const");
      if (TraceCode)  emitComment("<- Const") ;
      break; /* ConstK */
    
    case IdK :
      if (TraceCode) emitComment("-> Id") ;
      loc = st_lookup(tree->attr.name);
      if (st_lookup_type(tree->attr.name) == Float)
        emitRM("FLD",ac,loc,gp,"load float id value");
      else
        emitRM("LD",ac,loc,gp,"load id value");
      if (TraceCode)  emitComment("<- Id") ;
      break; /* IdK */

    case OpK :
         if (TraceCode) emitComment("-> Op") ;
         p1 = tree->child[0];
         p2 = tree->child[1];
         if (p2 == NULL) {
           /* unary operator (e.g., unary minus) */
           cGen(p1);
           if (p1->type == Float)
           { /* float unary minus */
             switch (tree->attr.op) {
                case MINUS :
                   emitRM("FLDC",ac1,0,0,"load 0.0 for unary minus");
                   emitRO("FSUB",ac,ac1,ac,"op unary float -");
                   break;
                default:
                   emitComment("BUG: Unknown unary float operator");
                   break;
             }
           }
           else
           { /* integer unary minus */
             switch (tree->attr.op) {
                case MINUS :
                   emitRM("LDC",ac1,0,0,"load 0 for unary minus");
                   emitRO("SUB",ac,ac1,ac,"op unary -");
                   break;
                default:
                   emitComment("BUG: Unknown unary operator");
                   break;
             }
           }
         } else if (tree->type == Float) {
         /* binary float arithmetic */
         /* gen code for left arg */
         cGen(p1);
         if (p1->type == Integer) emitRO("ITOF",ac,ac,0,"promote left to float");
         /* push left float operand */
         emitRM("FST",ac,tmpOffset--,mp,"op: push float left");
         /* gen code for right operand */
         cGen(p2);
         if (p2->type == Integer) emitRO("ITOF",ac,ac,0,"promote right to float");
         /* now load left float operand */
         emitRM("FLD",ac1,++tmpOffset,mp,"op: load float left");
         switch (tree->attr.op) {
            case PLUS :
               emitRO("FADD",ac,ac1,ac,"op float +");
               break;
            case MINUS :
               emitRO("FSUB",ac,ac1,ac,"op float -");
               break;
            case TIMES :
               emitRO("FMUL",ac,ac1,ac,"op float *");
               break;
            case OVER :
               emitRO("FDIV",ac,ac1,ac,"op float /");
               break;
            default:
               emitComment("BUG: Unknown float operator");
               break;
         }
         } else {
         /* integer or comparison ops */
         int isFloatCmp = (p1->type == Float || p2->type == Float)
                          && tree->type == Boolean;
         if (isFloatCmp)
         { /* float comparison: evaluate both as float, get diff, convert to int */
           cGen(p1);
           if (p1->type == Integer) emitRO("ITOF",ac,ac,0,"promote left for cmp");
           emitRM("FST",ac,tmpOffset--,mp,"push float left for cmp");
           cGen(p2);
           if (p2->type == Integer) emitRO("ITOF",ac,ac,0,"promote right for cmp");
           emitRM("FLD",ac1,++tmpOffset,mp,"load float left for cmp");
           emitRO("FSUB",ac,ac1,ac,"float diff for cmp");
           emitRO("FTOI",ac,ac,0,"convert diff to int");
           /* now ac holds integer, use existing comparison logic */
           switch (tree->attr.op) {
              case LT :
                 emitRM("JLT",ac,2,pc,"br if true") ;
                 emitRM("LDC",ac,0,ac,"false case") ;
                 emitRM("LDA",pc,1,pc,"unconditional jmp") ;
                 emitRM("LDC",ac,1,ac,"true case") ;
                 break;
              case EQ :
                 emitRM("JEQ",ac,2,pc,"br if true");
                 emitRM("LDC",ac,0,ac,"false case") ;
                 emitRM("LDA",pc,1,pc,"unconditional jmp") ;
                 emitRM("LDC",ac,1,ac,"true case") ;
                 break;
              case GT:
                 emitRM("JGT",ac,2,pc,"br if true") ;
                 emitRM("LDC",ac,0,ac,"false case") ;
                 emitRM("LDA",pc,1,pc,"unconditional jmp") ;
                 emitRM("LDC",ac,1,ac,"true case") ;
                 break;
              case LTE:
                 emitRM("JLE",ac,2,pc,"br if true") ;
                 emitRM("LDC",ac,0,ac,"false case") ;
                 emitRM("LDA",pc,1,pc,"unconditional jmp") ;
                 emitRM("LDC",ac,1,ac,"true case") ;
                 break;
              case GTE:
                 emitRM("JGE",ac,2,pc,"br if true") ;
                 emitRM("LDC",ac,0,ac,"false case") ;
                 emitRM("LDA",pc,1,pc,"unconditional jmp") ;
                 emitRM("LDC",ac,1,ac,"true case") ;
                 break;
              default:
                 emitComment("BUG: Unknown float comparison");
                 break;
           }
         }
         else
         { /* integer binary ops (existing logic) */
         /* gen code for ac = left arg */
         cGen(p1);
         /* gen code to push left operand */
         emitRM("ST",ac,tmpOffset--,mp,"op: push left");
         /* gen code for ac = right operand */
         cGen(p2);
         /* now load left operand */
         emitRM("LD",ac1,++tmpOffset,mp,"op: load left");
         switch (tree->attr.op) {
            case PLUS :
               emitRO("ADD",ac,ac1,ac,"op +");
               break;
            case MINUS :
               emitRO("SUB",ac,ac1,ac,"op -");
               break;
            case TIMES :
               emitRO("MUL",ac,ac1,ac,"op *");
               break;
            case OVER :
               emitRO("DIV",ac,ac1,ac,"op /");
               break;
            case LT :
               emitRO("SUB",ac,ac1,ac,"op <") ;
               emitRM("JLT",ac,2,pc,"br if true") ;
               emitRM("LDC",ac,0,ac,"false case") ;
               emitRM("LDA",pc,1,pc,"unconditional jmp") ;
               emitRM("LDC",ac,1,ac,"true case") ;
               break;
            case EQ :
               emitRO("SUB",ac,ac1,ac,"op ==") ;
               emitRM("JEQ",ac,2,pc,"br if true");
               emitRM("LDC",ac,0,ac,"false case") ;
               emitRM("LDA",pc,1,pc,"unconditional jmp") ;
               emitRM("LDC",ac,1,ac,"true case") ;
               break;
            case GT:
               emitRO("SUB",ac,ac1,ac,"op >") ;
               emitRM("JGT",ac,2,pc,"br if true") ;
               emitRM("LDC",ac,0,ac,"false case") ;
               emitRM("LDA",pc,1,pc,"unconditional jmp") ;
               emitRM("LDC",ac,1,ac,"true case") ;
               break;
            case LTE:
               emitRO("SUB",ac,ac1,ac,"op <=") ;
               emitRM("JLE",ac,2,pc,"br if true") ;
               emitRM("LDC",ac,0,ac,"false case") ;
               emitRM("LDA",pc,1,pc,"unconditional jmp") ;
               emitRM("LDC",ac,1,ac,"true case") ;
               break;
            case GTE:
               emitRO("SUB",ac,ac1,ac,"op >=") ;
               emitRM("JGE",ac,2,pc,"br if true") ;
               emitRM("LDC",ac,0,ac,"false case") ;
               emitRM("LDA",pc,1,pc,"unconditional jmp") ;
               emitRM("LDC",ac,1,ac,"true case") ;
               break;
            default:
               emitComment("BUG: Unknown operator");
               break;
         } /* case op */
         } /* end integer binary */
         } /* end else binary op */
         if (TraceCode)  emitComment("<- Op") ;
         break; /* OpK */

    default:
      break;
  }
} /* genExp */

/* Procedure cGen recursively generates code by
 * tree traversal
 */
static void cGen( TreeNode * tree)
{ if (tree != NULL)
  { switch (tree->nodekind) {
      case StmtK:
        genStmt(tree);
        break;
      case ExpK:
        genExp(tree);
        break;
      default:
        break;
    }
    cGen(tree->sibling);
  }
}

/**********************************************/
/* the primary function of the code generator */
/**********************************************/
/* Procedure codeGen generates code to a code
 * file by traversal of the syntax tree. The
 * second parameter (codefile) is the file name
 * of the code file, and is used to print the
 * file name as a comment in the code file
 */
void codeGen(TreeNode * syntaxTree, char * codefile)
{  char * s = (char *)malloc(strlen(codefile)+7); // 20240329
   strcpy(s,"File: ");
   strcat(s,codefile);
   emitComment("TINY Compilation to TM Code");
   emitComment(s);
   /* generate standard prelude */
   emitComment("Standard prelude:");
   emitRM("LD",mp,0,ac,"load maxaddress from location 0");
   emitRM("ST",ac,0,ac,"clear location 0");
   emitComment("End of standard prelude.");
   /* generate code for TINY program */
   cGen(syntaxTree);
   /* finish */
   emitComment("End of execution.");
   emitRO("HALT",0,0,0,"");
}
