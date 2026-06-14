/****************************************************/
/* File: tm.c                                       */
/* The TM ("Tiny Machine") computer                 */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/******* const *******/
#define   IADDR_SIZE  1024 /* increase for large programs */
#define   DADDR_SIZE  1024 /* increase for large programs */
#define   NO_REGS 8
#define   PC_REG  7

#define   LINESIZE  121
#define   WORDSIZE  20

/******* type  *******/

typedef enum {
   opclRR,     /* reg operands r,s,t */
   opclRM,     /* reg r, mem d+s */
   opclRA      /* reg r, int d+s */
   } OPCLASS;

typedef enum {
   /* RR instructions */
   opHALT,    /* RR     halt, operands are ignored */
   opIN,      /* RR     read integer into reg(r) */
   opOUT,     /* RR     write integer from reg(r) */
   opADD,     /* RR     reg(r) = reg(s)+reg(t) */
   opSUB,     /* RR     reg(r) = reg(s)-reg(t) */
   opMUL,     /* RR     reg(r) = reg(s)*reg(t) */
   opDIV,     /* RR     reg(r) = reg(s)/reg(t) */
   opFADD,    /* RR     freg(r) = freg(s)+freg(t) */
   opFSUB,    /* RR     freg(r) = freg(s)-freg(t) */
   opFMUL,    /* RR     freg(r) = freg(s)*freg(t) */
   opFDIV,    /* RR     freg(r) = freg(s)/freg(t) */
   opFOUT,    /* RR     write float from freg(r) */
   opFIN,     /* RR     read float into freg(r) */
   opOUTC,    /* RR     write char from reg(r) */
   opITOF,    /* RR     freg(r) = (float)reg(s) */
   opFTOI,    /* RR     reg(r) = (int)freg(s) */
   opRRLim,   /* limit of RR opcodes */

   /* RM instructions */
   opLD,      /* RM     reg(r) = dMem[d+reg(s)] */
   opST,      /* RM     dMem[d+reg(s)] = reg(r) */
   opFLD,     /* RM     freg(r) = fMem[d+reg(s)] */
   opFST,     /* RM     fMem[d+reg(s)] = freg(r) */
   opRMLim,   /* Limit of RM opcodes */

   /* RA instructions */
   opLDA,     /* RA     reg(r) = d+reg(s) */
   opLDC,     /* RA     reg(r) = d ; reg(s) is ignored */
   opJLT,     /* RA     if reg(r)<0 then reg(7) = d+reg(s) */
   opJLE,     /* RA     if reg(r)<=0 then reg(7) = d+reg(s) */
   opJGT,     /* RA     if reg(r)>0 then reg(7) = d+reg(s) */
   opJGE,     /* RA     if reg(r)>=0 then reg(7) = d+reg(s) */
   opJEQ,     /* RA     if reg(r)==0 then reg(7) = d+reg(s) */
   opJNE,     /* RA     if reg(r)!=0 then reg(7) = d+reg(s) */
   opFLDC,    /* RA     freg(r) = bitcast of d */
   opRALim    /* Limit of RA opcodes */
   } OPCODE;

typedef enum {
   srOKAY,
   srHALT,
   srIMEM_ERR,
   srDMEM_ERR,
   srZERODIVIDE
   } STEPRESULT;

typedef struct {
      int iop  ;
      int iarg1  ;
      int iarg2  ;
      int iarg3  ;
   } INSTRUCTION;

/******** vars ********/
int iloc = 0 ;
int dloc = 0 ;
int traceflag = FALSE;
int icountflag = FALSE;

INSTRUCTION iMem [IADDR_SIZE];
int dMem [DADDR_SIZE];
int reg [NO_REGS];
float freg[NO_REGS];
float fMem[DADDR_SIZE];
float fnum;

char * opCodeTab[]
        = {"HALT","IN","OUT","ADD","SUB","MUL","DIV",
           "FADD","FSUB","FMUL","FDIV","FOUT","FIN","OUTC","ITOF",
           "FTOI","????",
            /* RR opcodes */
           "LD","ST","FLD","FST","????", /* RM opcodes */
           "LDA","LDC","JLT","JLE","JGT","JGE","JEQ","JNE",
           "FLDC","????"
           /* RA opcodes */
          };

char * stepResultTab[]
        = {"OK","Halted","Instruction Memory Fault",
           "Data Memory Fault","Division by 0"
          };

char pgmName[20];
FILE *pgm  ;

char in_Line[LINESIZE] ;
int lineLen ;
int inCol  ;
int num  ;
char word[WORDSIZE] ;
char ch  ;
int done  ;

/********************************************/
int opClass( int c )
{ if      ( c <= opRRLim) return ( opclRR );
  else if ( c <= opRMLim) return ( opclRM );
  else                    return ( opclRA );
} /* opClass */

/********************************************/
void writeInstruction ( int loc )
{ printf( "%5d: ", loc) ;
  if ( (loc >= 0) && (loc < IADDR_SIZE) )
  { printf("%6s%3d,", opCodeTab[iMem[loc].iop], iMem[loc].iarg1);
    switch ( opClass(iMem[loc].iop) )
    { case opclRR: printf("%1d,%1d", iMem[loc].iarg2, iMem[loc].iarg3);
                   break;
      case opclRM:
      case opclRA: printf("%3d(%1d)", iMem[loc].iarg2, iMem[loc].iarg3);
                   break;
    }
    printf ("\n") ;
  }
} /* writeInstruction */

/********************************************/
void getCh (void)
{ if (++inCol < lineLen)
  ch = in_Line[inCol] ;
  else ch = ' ' ;
} /* getCh */

/********************************************/
int nonBlank (void)
{ while ((inCol < lineLen)
         && (in_Line[inCol] == ' ') )
    inCol++ ;
  if (inCol < lineLen)
  { ch = in_Line[inCol] ;
    return TRUE ; }
  else
  { ch = ' ' ;
    return FALSE ; }
} /* nonBlank */

/********************************************/
int getNum (void)
{ int sign;
  int term;
  int temp = FALSE;
  num = 0 ;
  do
  { sign = 1;
    while ( nonBlank() && ((ch == '+') || (ch == '-')) )
    { temp = FALSE ;
      if (ch == '-')  sign = - sign ;
      getCh();
    }
    term = 0 ;
    nonBlank();
    while (isdigit(ch))
    { temp = TRUE ;
      term = term * 10 + ( ch - '0' ) ;
      getCh();
    }
    num = num + (term * sign) ;
  } while ( (nonBlank()) && ((ch == '+') || (ch == '-')) ) ;
  return temp;
} /* getNum */

/********************************************/
int getFloat (void)
{ int sign;
  float term;
  int temp = FALSE;
  fnum = 0.0 ;
  sign = 1;
  while ( nonBlank() && ((ch == '+') || (ch == '-')) )
  { temp = FALSE ;
    if (ch == '-')  sign = - sign ;
    getCh();
  }
  term = 0.0 ;
  nonBlank();
  while (isdigit(ch))
  { temp = TRUE ;
    term = term * 10.0f + (float)(ch - '0') ;
    getCh();
  }
  if (ch == '.')
  { getCh();
    float frac = 0.1f;
    while (isdigit(ch))
    { temp = TRUE ;
      term = term + frac * (float)(ch - '0') ;
      frac *= 0.1f ;
      getCh();
    }
  }
  if (ch == 'e' || ch == 'E')
  { getCh();
    int expSign = 1;
    if (ch == '+' || ch == '-')
    { if (ch == '-') expSign = -1;
      getCh();
    }
    int expVal = 0;
    while (isdigit(ch))
    { expVal = expVal * 10 + (ch - '0');
      getCh();
    }
    if (expSign < 0)
      for (int i = 0; i < expVal; i++) term /= 10.0f;
    else
      for (int i = 0; i < expVal; i++) term *= 10.0f;
  }
  fnum = term * (float)sign ;
  return temp;
} /* getFloat */

/********************************************/
int getWord (void)
{ int temp = FALSE;
  int length = 0;
  if (nonBlank ())
  { while (isalnum(ch))
    { if (length < WORDSIZE-1) word [length++] =  ch ;
      getCh() ;
    }
    word[length] = '\0';
    temp = (length != 0);
  }
  return temp;
} /* getWord */

/********************************************/
int skipCh ( char c  )
{ int temp = FALSE;
  if ( nonBlank() && (ch == c) )
  { getCh();
    temp = TRUE;
  }
  return temp;
} /* skipCh */

/********************************************/
int atEOL(void)
{ return ( ! nonBlank ());
} /* atEOL */

/********************************************/
int error( char * msg, int lineNo, int instNo)
{ printf("Line %d",lineNo);
  if (instNo >= 0) printf(" (Instruction %d)",instNo);
  printf("   %s\n",msg);
  return FALSE;
} /* error */

/********************************************/
int readInstructions (void)
{ OPCODE op;
  int arg1, arg2, arg3;
  int loc, regNo, lineNo;
  for (regNo = 0 ; regNo < NO_REGS ; regNo++)
  { reg[regNo] = 0 ;
    freg[regNo] = 0.0 ;
  }
  dMem[0] = DADDR_SIZE - 1 ;
  fMem[0] = 0.0 ;
  for (loc = 1 ; loc < DADDR_SIZE ; loc++)
  { dMem[loc] = 0 ;
    fMem[loc] = 0.0 ;
  }
  for (loc = 0 ; loc < IADDR_SIZE ; loc++)
  { iMem[loc].iop = opHALT ;
    iMem[loc].iarg1 = 0 ;
    iMem[loc].iarg2 = 0 ;
    iMem[loc].iarg3 = 0 ;
  }
  lineNo = 0 ;
  while (! feof(pgm))
  { fgets( in_Line, LINESIZE-2, pgm  ) ;
    inCol = 0 ; 
    lineNo++;
    lineLen = strlen(in_Line)-1 ;
    if (in_Line[lineLen]=='\n') in_Line[lineLen] = '\0' ;
    else in_Line[++lineLen] = '\0';
    if ( (nonBlank()) && (in_Line[inCol] != '*') )
    { if (! getNum())
        return error("Bad location", lineNo,-1);
      loc = num;
      if (loc > IADDR_SIZE)
        return error("Location too large",lineNo,loc);
      if (! skipCh(':'))
        return error("Missing colon", lineNo,loc);
      if (! getWord ())
        return error("Missing opcode", lineNo,loc);
      op = opHALT ;
      while ((op < opRALim)
             && (strncmp(opCodeTab[op], word, 4) != 0) )
          op++ ;
      if (strncmp(opCodeTab[op], word, 4) != 0)
          return error("Illegal opcode", lineNo,loc);
      switch ( opClass(op) )
      { case opclRR :
        /***********************************/
        if ( (! getNum ()) || (num < 0) || (num >= NO_REGS) )
            return error("Bad first register", lineNo,loc);
        arg1 = num;
        if ( ! skipCh(','))
            return error("Missing comma", lineNo, loc);
        if ( (! getNum ()) || (num < 0) || (num >= NO_REGS) )
            return error("Bad second register", lineNo, loc);
        arg2 = num;
        if ( ! skipCh(',')) 
            return error("Missing comma", lineNo,loc);
        if ( (! getNum ()) || (num < 0) || (num >= NO_REGS) )
            return error("Bad third register", lineNo,loc);
        arg3 = num;
        break;

        case opclRM :
        case opclRA :
        /***********************************/
        if ( (! getNum ()) || (num < 0) || (num >= NO_REGS) )
            return error("Bad first register", lineNo,loc);
        arg1 = num;
        if ( ! skipCh(','))
            return error("Missing comma", lineNo,loc);
        if (! getNum ())
            return error("Bad displacement", lineNo,loc);
        arg2 = num;
        if ( ! skipCh('(') && ! skipCh(',') )
            return error("Missing LParen", lineNo,loc);
        if ( (! getNum ()) || (num < 0) || (num >= NO_REGS))
            return error("Bad second register", lineNo,loc);
        arg3 = num;
        break;
        }
      iMem[loc].iop = op;
      iMem[loc].iarg1 = arg1;
      iMem[loc].iarg2 = arg2;
      iMem[loc].iarg3 = arg3;
    }
  }
  return TRUE;
} /* readInstructions */


/********************************************/
STEPRESULT stepTM (void)
{ INSTRUCTION currentinstruction  ;
  int pc  ;
  int r,s,t,m  ;
  int ok ;

  pc = reg[PC_REG] ;
  if ( (pc < 0) || (pc > IADDR_SIZE)  )
      return srIMEM_ERR ;
  reg[PC_REG] = pc + 1 ;
  currentinstruction = iMem[ pc ] ;
  switch (opClass(currentinstruction.iop) )
  { case opclRR :
    /***********************************/
      r = currentinstruction.iarg1 ;
      s = currentinstruction.iarg2 ;
      t = currentinstruction.iarg3 ;
      break;

    case opclRM :
    /***********************************/
      r = currentinstruction.iarg1 ;
      s = currentinstruction.iarg3 ;
      m = currentinstruction.iarg2 + reg[s] ;
      if ( (m < 0) || (m > DADDR_SIZE))
         return srDMEM_ERR ;
      break;

    case opclRA :
    /***********************************/
      r = currentinstruction.iarg1 ;
      s = currentinstruction.iarg3 ;
      m = currentinstruction.iarg2 + reg[s] ;
      break;
  } /* case */

  switch ( currentinstruction.iop)
  { /* RR instructions */
    case opHALT :
    /***********************************/
      printf("HALT: %1d,%1d,%1d\n",r,s,t);
      return srHALT ;
      /* break; */

    case opIN :
    /***********************************/
      do
      { printf("Enter value for IN instruction: ") ;
        fflush (stdin);
        fflush (stdout);
        gets(in_Line);
        lineLen = strlen(in_Line) ;
        inCol = 0;
        ok = getNum();
        if ( ! ok ) printf ("Illegal value\n");
        else reg[r] = num;
      }
      while (! ok);
      break;

    case opOUT :  
      printf ("OUT instruction prints: %d\n", reg[r] ) ;
      break;
    case opADD :  reg[r] = reg[s] + reg[t] ;  break;
    case opSUB :  reg[r] = reg[s] - reg[t] ;  break;
    case opMUL :  reg[r] = reg[s] * reg[t] ;  break;

    case opDIV :
    /***********************************/
      if ( reg[t] != 0 ) reg[r] = reg[s] / reg[t];
      else return srZERODIVIDE ;
      break;

    case opFADD :  freg[r] = freg[s] + freg[t] ;  break;
    case opFSUB :  freg[r] = freg[s] - freg[t] ;  break;
    case opFMUL :  freg[r] = freg[s] * freg[t] ;  break;

    case opFDIV :
    /***********************************/
      if ( freg[t] != 0.0 ) freg[r] = freg[s] / freg[t];
      else return srZERODIVIDE ;
      break;

    case opFOUT :
      printf ("FOUT instruction prints: %g\n", freg[r] ) ;
      break;

    case opFIN :
    /***********************************/
      do
      { printf("Enter float value for FIN instruction: ") ;
        fflush (stdin);
        fflush (stdout);
        gets(in_Line);
        lineLen = strlen(in_Line) ;
        inCol = 0;
        ok = getFloat();
        if ( ! ok ) printf ("Illegal value\n");
        else freg[r] = fnum;
      }
      while (! ok);
      break;

    case opOUTC :
      printf("%c", (char)reg[r]) ;
      break;

    case opITOF :
      freg[r] = (float)reg[s] ;
      break;

    case opFTOI :
      reg[r] = (int)freg[s] ;
      break;

    /*************** RM instructions ********************/
    case opLD :    reg[r] = dMem[m] ;  break;
    case opST :    dMem[m] = reg[r] ;  break;

    case opFLD :
      if ( (m < 0) || (m >= DADDR_SIZE)) return srDMEM_ERR ;
      freg[r] = fMem[m] ;
      break;

    case opFST :
      if ( (m < 0) || (m >= DADDR_SIZE)) return srDMEM_ERR ;
      fMem[m] = freg[r] ;
      break;

    /*************** RA instructions ********************/
    case opLDA :    reg[r] = m ; break;
    case opLDC :    reg[r] = currentinstruction.iarg2 ;   break;
    case opJLT :    if ( reg[r] <  0 ) reg[PC_REG] = m ; break;
    case opJLE :    if ( reg[r] <=  0 ) reg[PC_REG] = m ; break;
    case opJGT :    if ( reg[r] >  0 ) reg[PC_REG] = m ; break;
    case opJGE :    if ( reg[r] >=  0 ) reg[PC_REG] = m ; break;
    case opJEQ :    if ( reg[r] == 0 ) reg[PC_REG] = m ; break;
    case opJNE :    if ( reg[r] != 0 ) reg[PC_REG] = m ; break;

    case opFLDC :
    /***********************************/
      { int bits = currentinstruction.iarg2;
        float f;
        memcpy(&f, &bits, sizeof(float));
        freg[r] = f;
      }
      break;

    /* end of legal instructions */
  } /* case */
  return srOKAY ;
} /* stepTM */

/********************************************/
int doCommand (void)
{ char cmd;
  int stepcnt=0, i;
  int printcnt;
  int stepResult;
  int regNo, loc;
  do
  { printf ("Enter command: ");
    fflush (stdin);
    fflush (stdout);
    gets(in_Line);
    lineLen = strlen(in_Line);
    inCol = 0;
  }
  while (! getWord ());

  cmd = word[0] ;
  switch ( cmd )
  { case 't' :
    /***********************************/
      traceflag = ! traceflag ;
      printf("Tracing now ");
      if ( traceflag ) printf("on.\n"); else printf("off.\n");
      break;

    case 'h' :
    /***********************************/
      printf("Commands are:\n");
      printf("   s(tep <n>      "\
             "Execute n (default 1) TM instructions\n");
      printf("   g(o            "\
             "Execute TM instructions until HALT\n");
      printf("   r(egs          "\
             "Print the contents of the registers\n");
      printf("   i(Mem <b <n>>  "\
             "Print n iMem locations starting at b\n");
      printf("   d(Mem <b <n>>  "\
             "Print n dMem locations starting at b\n");
      printf("   f(Mem <b <n>>  "\
             "Print n fMem (float) locations starting at b\n");
      printf("   t(race         "\
             "Toggle instruction trace\n");
      printf("   p(rint         "\
             "Toggle print of total instructions executed"\
             " ('go' only)\n");
      printf("   c(lear         "\
             "Reset simulator for new execution of program\n");
      printf("   h(elp          "\
             "Cause this list of commands to be printed\n");
      printf("   q(uit          "\
             "Terminate the simulation\n");
      break;

    case 'p' :
    /***********************************/
      icountflag = ! icountflag ;
      printf("Printing instruction count now ");
      if ( icountflag ) printf("on.\n"); else printf("off.\n");
      break;

    case 's' :
    /***********************************/
      if ( atEOL ())  stepcnt = 1;
      else if ( getNum ())  stepcnt = abs(num);
      else   printf("Step count?\n");
      break;

    case 'g' :   stepcnt = 1 ;     break;

    case 'r' :
    /***********************************/
      printf("Integer registers:\n");
      for (i = 0; i < NO_REGS; i++)
      { printf("%1d: %4d    ", i,reg[i]);
        if ( (i % 4) == 3 ) printf ("\n");
      }
      printf("\nFloat registers:\n");
      for (i = 0; i < NO_REGS; i++)
      { printf("%1d: %10.6g ", i,freg[i]);
        if ( (i % 4) == 3 ) printf ("\n");
      }
      printf("\n");
      break;

    case 'i' :
    /***********************************/
      printcnt = 1 ;
      if ( getNum ())
      { iloc = num ;
        if ( getNum ()) printcnt = num ;
      }
      if ( ! atEOL ())
        printf ("Instruction locations?\n");
      else
      { while ((iloc >= 0) && (iloc < IADDR_SIZE)
                && (printcnt > 0) )
        { writeInstruction(iloc);
          iloc++ ;
          printcnt-- ;
        }
      }
      break;

    case 'd' :
    /***********************************/
      printcnt = 1 ;
      if ( getNum  ())
      { dloc = num ;
        if ( getNum ()) printcnt = num ;
      }
      if ( ! atEOL ())
        printf("Data locations?\n");
      else
      { while ((dloc >= 0) && (dloc < DADDR_SIZE)
                  && (printcnt > 0))
        { printf("%5d: %5d\n",dloc,dMem[dloc]);
          dloc++;
          printcnt--;
        }
      }
      break;

    case 'f' :
    /***********************************/
      printcnt = 1 ;
      if ( getNum  ())
      { dloc = num ;
        if ( getNum ()) printcnt = num ;
      }
      if ( ! atEOL ())
        printf("Float data locations?\n");
      else
      { while ((dloc >= 0) && (dloc < DADDR_SIZE)
                  && (printcnt > 0))
        { printf("%5d: %10.6g\n",dloc,fMem[dloc]);
          dloc++;
          printcnt--;
        }
      }
      break;

    case 'c' :
    /***********************************/
      iloc = 0;
      dloc = 0;
      stepcnt = 0;
      for (regNo = 0;  regNo < NO_REGS ; regNo++)
      { reg[regNo] = 0 ;
        freg[regNo] = 0.0 ;
      }
      dMem[0] = DADDR_SIZE - 1 ;
      fMem[0] = 0.0 ;
      for (loc = 1 ; loc < DADDR_SIZE ; loc++)
      { dMem[loc] = 0 ;
        fMem[loc] = 0.0 ;
      }
      break;

    case 'q' : return FALSE;  /* break; */

    default : printf("Command %c unknown.\n", cmd); break;
  }  /* case */
  stepResult = srOKAY;
  if ( stepcnt > 0 )
  { if ( cmd == 'g' )
    { stepcnt = 0;
      while (stepResult == srOKAY)
      { iloc = reg[PC_REG] ;
        if ( traceflag ) writeInstruction( iloc ) ;
        stepResult = stepTM ();
        stepcnt++;
      }
      if ( icountflag )
        printf("Number of instructions executed = %d\n",stepcnt);
    }
    else
    { while ((stepcnt > 0) && (stepResult == srOKAY))
      { iloc = reg[PC_REG] ;
        if ( traceflag ) writeInstruction( iloc ) ;
        stepResult = stepTM ();
        stepcnt-- ;
      }
    }
    printf( "%s\n",stepResultTab[stepResult] );
  }
  return TRUE;
} /* doCommand */


/********************************************/
/* E X E C U T I O N   B E G I N S   H E R E */
/********************************************/

int main( int argc, char * argv[] )
{ if (argc != 2)
  { printf("usage: %s <filename>\n",argv[0]);
    exit(1);
  }
  strcpy(pgmName,argv[1]) ;
  if (strchr (pgmName, '.') == NULL)
     strcat(pgmName,".tm");
  pgm = fopen(pgmName,"r");
  if (pgm == NULL)
  { printf("file '%s' not found\n",pgmName);
    exit(1);
  }

  /* read the program */
  if ( ! readInstructions ())
         exit(1) ;
  /* switch input file to terminal */
  /* reset( input ); */
  /* read-eval-print */
  printf("TM  simulation (enter h for help)...\n");
  do
     done = ! doCommand ();
  while (! done );
  printf("Simulation done.\n");
  return 0;
}
