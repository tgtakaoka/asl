/* asmpars.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Verwaltung von Symbolen und das ganze Drumherum...                        */
/*                                                                           */
/* Historie:  5. 5.1996 Grundsteinlegung                                     */
/*            4. 1.1997 Umstellung wg. case-sensitiv                         */
/*           24. 9.1997 Registersymbole                                      */
/*           26. 6.1998 Codepages                                            */
/*            7. 7.1998 Fix Zugriffe auf CharTransTable wg. signed chars     */
/*           17. 7.1998 Korrektur Maskentabellen                             */
/*           16. 8.1998 NoICE-Symbolausgabe                                  */
/*           18. 8.1998 Benutzung RadixBase                                  */
/*           19. 8.1998 == als Alias fuer = - Operator                       */
/*            1. 9.1998 RefList nicht initialisiert bei Symbolen             */
/*                      ACOT korrigiert                                      */
/*            6.12.1998 UInt14                                               */
/*           30. 1.1999 Formate maschinenunabhaengig gemacht                 */
/*           12. 2.1999 Compilerwarnungen beseitigt                          */
/*           17. 4.1999 Abfrage auf PCSymbol gegen Nullzeigerzugriff ge-     */
/*                      schuetzt.                                            */
/*           30. 5.1999 OutRadixBase beruecksichtigt                         */
/*           12. 7.1999 angefangen mit externen Symbolen                     */
/*           14. 7.1999 Relocs im Parser beruecksichtigt                     */
/*            1. 8.1999 Relocs im Formelparser durch                         */
/*            8. 8.1999 Relocs in EvalIntExpression beruecksichtigt          */
/*            8. 3.2000 'ambigious else'-Warnungen beseitigt                 */
/*           21. 5.2000 added TmpSymCounter                                  */
/*            1. 6.2000 dump symbols explicitly as hex for NoICE             */
/*           26. 6.2000 GetIntSymbol sets FirstPassUnknown                   */
/*           14. 1.2001 silenced warnings about unused parameters            */
/*           25. 5.2001 added UInt21                                         */
/*            3. 8.2001 added SInt6                                          */
/*           2001-10-04 better check for ASCII-like integer consts           */
/*           2001-10-20 added UInt23                                         */
/*                                                                           */
/*****************************************************************************/
/* $Id: asmpars.c,v 1.36 2017/02/26 16:57:48 alfred Exp $                     */
/*****************************************************************************
 * $Log: asmpars.c,v $
 * Revision 1.36  2017/02/26 16:57:48  alfred
 * - make some arguments const
 *
 * Revision 1.35  2016/10/07 20:03:03  alfred
 * - make some arguments const
 *
 * Revision 1.34  2016/09/29 16:43:36  alfred
 * - introduce common DecodeDATA/DecodeRES functions
 *
 * Revision 1.33  2016/08/17 21:26:45  alfred
 * - fix some errors and warnings detected by clang
 *
 * Revision 1.32  2015/10/25 20:47:18  alfred
 * - correct two-column printout indentation of symbol list
 *
 * Revision 1.31  2015/08/28 17:22:26  alfred
 * - add special handling for labels following BSR
 *
 * Revision 1.30  2015/07/05 17:23:39  alfred
 * - disallow float constants starting with 0x
 *
 * Revision 1.29  2014/12/14 17:58:46  alfred
 * - remove static variables in strutil.c
 *
 * Revision 1.28  2014/12/07 19:13:58  alfred
 * - silence a couple of Borland C related warnings and errors
 *
 * Revision 1.27  2014/12/05 11:09:10  alfred
 * - eliminate Nil
 *
 * Revision 1.26  2014/12/03 19:01:00  alfred
 * - remove static return value
 *
 * Revision 1.25  2014/11/30 10:09:54  alfred
 * - rework to current style
 *
 * Revision 1.24  2014/04/15 05:57:03  alfred
 * - remove overlapping strcpy() when parsing function arguments
 *
 * Revision 1.23  2014/03/08 21:06:35  alfred
 * - rework ASSUME framework
 *
 * Revision 1.22  2014/03/08 17:26:14  alfred
 * - print out declaration position for unresolved forwards
 *
 * Revision 1.21  2013/12/21 19:46:51  alfred
 * - dynamically resize code buffer
 *
 * Revision 1.20  2013-03-09 16:15:08  alfred
 * - add NEC 75xx
 *
 * Revision 1.19  2010/04/17 13:14:19  alfred
 * - address overlapping strcpy()
 *
 * Revision 1.18  2010/03/07 11:16:53  alfred
 * - allow DC.(float) on string operands
 *
 * Revision 1.17  2009/06/07 09:32:25  alfred
 * - add named temporary symbols
 *
 * Revision 1.16  2009/04/10 08:58:30  alfred
 * - correct address ranges for AVRs
 *
 * Revision 1.15  2008/11/23 10:39:15  alfred
 * - allow strings with NUL characters
 *
 * Revision 1.14  2008/10/21 16:33:04  alfred
 * - added charfromstr() function
 *
 * Revision 1.13  2007/11/24 22:48:02  alfred
 * - some NetBSD changes
 *
 * Revision 1.12  2007/09/24 17:39:02  alfred
 * - correct handling of '-' operator
 *
 * Revision 1.11  2007/04/30 18:37:51  alfred
 * - add weird integer coding
 *
 * Revision 1.10  2006/10/10 10:41:41  alfred
 * - free up space in data segment
 *
 * Revision 1.9  2005/12/13 19:28:37  alfred
 * - correct format strings for 16-bit platforms
 *
 * Revision 1.8  2005/10/30 13:24:28  alfred
 * - allow strings as int constants
 *
 * Revision 1.7  2005/10/02 10:00:43  alfred
 * - ConstLongInt gets default base, correct length check on KCPSM3 registers
 *
 * Revision 1.6  2004/05/31 12:47:40  alfred
 * - clean up operator handling
 *
 * Revision 1.5  2004/05/30 20:51:42  alfred
 * - major cleanups in Const... functions
 *
 * Revision 1.4  2004/05/28 16:12:07  alfred
 * - added some const definitions
 *
 * Revision 1.3  2004/01/17 16:18:38  alfred
 * - fix some more GCC 3.3 quarrel
 *
 * Revision 1.2  2004/01/17 16:12:49  alfred
 * - some quirks for GCC 3.3
 *
 * Revision 1.1  2003/11/06 02:49:18  alfred
 * - recreated
 *
 * Revision 1.17  2003/10/04 15:38:46  alfred
 * - differentiate constant/variable messages
 *
 * Revision 1.16  2003/05/20 17:45:02  alfred
 * - StrSym with length spec
 *
 * Revision 1.15  2003/05/02 21:23:08  alfred
 * - strlen() updates
 *
 * Revision 1.14  2003/02/26 19:18:25  alfred
 * - add/use EvalIntDisplacement()
 *
 * Revision 1.13  2003/02/02 12:15:21  alfred
 * - added exptype() function
 *
 * Revision 1.12  2002/11/10 15:08:34  alfred
 * - use tree functions
 *
 * Revision 1.11  2002/11/10 09:43:07  alfred
 * - relocated symbol node type
 *
 * Revision 1.10  2002/11/04 19:04:26  alfred
 * - prevent modification of constants with SET
 *
 * Revision 1.9  2002/10/10 17:11:33  alfred
 * - repaired $$ temp symbols
 *
 * Revision 1.8  2002/10/07 20:25:01  alfred
 * - added '/' nameless temporary symbols
 *
 * Revision 1.7  2002/09/29 17:05:40  alfred
 * - ass +/- temporary symbols
 *
 * Revision 1.6  2002/05/25 21:15:20  alfred
 * - Fix array definition
 *
 * Revision 1.5  2002/05/19 13:44:52  alfred
 * - added ClearSectionUsage()
 *
 * Revision 1.4  2002/05/18 16:10:14  alfred
 * - optimize search via Find(Loc)Node
 *
 * Revision 1.3  2002/05/13 18:14:09  alfred
 * - use error msg 2010
 *
 * Revision 1.2  2002/03/10 11:55:42  alfred
 * - state which operand type was expected/got
 *
 *****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "endian.h"
#include "bpemu.h"
#include "nls.h"
#include "nlmessages.h"
#include "as.rsc"
#include "strutil.h"

#include "asmdef.h"
#include "asmsub.h"
#include "asmfnums.h"
#include "asmrelocs.h"
#include "chunks.h"
#include "trees.h"

#include "asmpars.h"

#define LOCSYMSIGHT 3       /* max. sight for nameless temporary symbols */

tIntTypeDef IntTypeDefs[IntTypeCnt] =
{
  { 0x00000001l,          0l,          1l }, /* UInt1 */
  { 0x00000003l,          0l,          3l }, /* UInt2 */
  { 0x00000007l,          0l,          7l }, /* UInt3 */
  { 0x00000007l,         -8l,          7l }, /* SInt4 */
  { 0x0000000fl,          0l,         15l }, /* UInt4 */
  { 0x0000000fl,         -8l,         15l }, /* Int4 */
  { 0x0000000fl,        -16l,         15l }, /* SInt5 */
  { 0x0000001fl,          0l,         31l }, /* UInt5 */
  { 0x0000001fl,        -16l,         31l }, /* Int5 */
  { 0x0000001fl,        -32l,         31l }, /* SInt6 */
  { 0x0000003fl,          0l,         63l }, /* UInt6 */
  { 0x0000003fl,        -64l,         63l }, /* SInt7 */
  { 0x0000007fl,          0l,        127l }, /* UInt7 */
  { 0x0000007fl,       -128l,        127l }, /* SInt8 */
  { 0x000000ffl,          0l,        255l }, /* UInt8 */
  { 0x000000ffl,       -128l,        255l }, /* Int8 */
  { 0x000001ffl,          0l,        511l }, /* UInt9 */
  { 0x000003ffl,          0l,       1023l }, /* UInt10 */
  { 0x000003ffl,       -512l,       1023l }, /* Int10 */
  { 0x000007ffl,          0l,       2047l }, /* UInt11 */
  { 0x00000fffl,          0l,       4095l }, /* UInt12 */
  { 0x00000fffl,      -2047l,       4095l }, /* Int12 */
  { 0x00001fffl,          0l,       8191l }, /* UInt13 */
  { 0x00003fffl,          0l,      16383l }, /* UInt14 */
  { 0x00003fffl,      -8192l,      16383l }, /* Int14 */
  { 0x00007fffl,          0l,      32767l }, /* UInt15 */
  { 0x00007fffl,     -32768l,      32767l }, /* SInt16 */
  { 0x0000ffffl,          0l,      65535l }, /* UInt16 */
  { 0x0000ffffl,     -32768l,      65535l }, /* Int16 */
  { 0x0001ffffl,          0l,     131071l }, /* UInt17 */
  { 0x0003ffffl,          0l,     262143l }, /* UInt18 */
  { 0x0007ffffl,    -524288l,     524287l }, /* SInt20 */
  { 0x000fffffl,          0l,    1048575l }, /* UInt20 */
  { 0x000fffffl,    -524288l,    1048575l }, /* Int20 */
  { 0x001fffffl,          0l,    2097151l }, /* UInt21 */
  { 0x003fffffl,          0l,    4194303l }, /* UInt22 */
  { 0x007fffffl,          0l,    8388608l }, /* UInt23 */
  { 0x007fffffl,   -8388608l,    8388607l }, /* SInt24 */
  { 0x00ffffffl,          0l,   16777215l }, /* UInt24 */
  { 0x00ffffffl,   -8388608l,   16777215l }, /* Int24 */
  { 0xffffffffl, -2147483647l, 2147483647l }, /* SInt32 */
  { 0xffffffffl,          0l, 4294967295ul }, /* UInt32 */
  { 0xffffffffl, -2147483647l, 4294967295ul }, /* Int32 */
#ifdef HAS64
  { 0xffffffffffffffffll, -9223372036854775807ll, 9223372036854775807ll }, /* Int64 */
#endif
};

typedef struct
{
  Boolean Back;
  LongInt Counter;
} TTmpSymLog;

Boolean FirstPassUnknown;      /* Hinweisflag: evtl. im ersten Pass unbe-
                                  kanntes Symbol, Ausdruck nicht ausgewertet */
Boolean SymbolQuestionable;    /* Hinweisflag:  Dadurch, dass Phasenfehler
                                  aufgetreten sind, ist dieser Symbolwert evtl.
                                  nicht mehr aktuell                         */
Boolean UsesForwards;          /* Hinweisflag: benutzt Vorwaertsdefinitionen */
LongInt MomLocHandle;          /* Merker, den lokale Symbole erhalten        */
LongInt TmpSymCounter,         /* counters for local symbols                 */
        FwdSymCounter,
        BackSymCounter;
char TmpSymCounterVal[10];     /* representation as string                   */
TTmpSymLog TmpSymLog[LOCSYMSIGHT];
LongInt TmpSymLogDepth;

LongInt LocHandleCnt;          /* mom. verwendeter lokaler Handle            */

Boolean BalanceTree;           /* Symbolbaum ausbalancieren                  */

static char BaseIds[3] =
{
  '%', '@', '$'
};
static char BaseLetters[4] =
{
  'B', 'O', 'H', 'Q'
};
static Byte BaseVals[4] =
{
  2, 8, 16, 8
};

typedef struct sSymbolEntry
{
  TTree Tree;
  Byte SymType;
  ShortInt SymSize;
  Boolean Defined, Used, Changeable;
  SymbolVal SymWert;
  PCrossRef RefList;
  Byte FileNum;
  LongInt LineNum;
  tSymbolFlags Flags;
  TRelocEntry *Relocs;
} TSymbolEntry, *PSymbolEntry;

typedef struct sSymbolStackEntry
{
  struct sSymbolStackEntry *Next;
  SymbolVal Contents;
} TSymbolStackEntry, *PSymbolStackEntry;

typedef struct sSymbolStack
{
  struct sSymbolStack *Next;
  char *Name;
  PSymbolStackEntry Contents;
} TSymbolStack, *PSymbolStack;

typedef struct sDefSymbol
{
  struct sDefSymbol *Next;
  char *SymName;
  TempResult Wert;
} TDefSymbol, *PDefSymbol;

typedef struct sCToken
{
  struct sCToken *Next;
  char *Name;
  LongInt Parent;
  ChunkList Usage;
} TCToken, *PCToken;

typedef struct
{
  char *Id;
  int IdLen;
  Boolean Dyadic;
  Byte Priority;
  Boolean MayInt;
  Boolean MayFloat;
  Boolean MayString;
  Boolean Present;
  void (*pFunc)(TempResult *pErg, TempResult *pLVal, TempResult *pRVal);
} Operator;

typedef struct sLocHeap
{
  struct sLocHeap *Next;
  LongInt Cont;
} TLocHeap, *PLocHandle;

typedef struct sRegDefList
{
  struct sRegDefList *Next;
  LongInt Section;
  char *Value;
  Boolean Used;
} TRegDefList, *PRegDefList;

typedef struct sRegDef
{
  struct sRegDef *Left, *Right;
  char *Orig;
  PRegDefList Defs, DoneDefs;
} TRegDef, *PRegDef;

static PSymbolEntry FirstSymbol, FirstLocSymbol;
static PDefSymbol FirstDefSymbol;
/*static*/ PCToken FirstSection;
static PRegDef FirstRegDef;
static Boolean DoRefs;              /* Querverweise protokollieren */
static PLocHandle FirstLocHandle;
static PSymbolStack FirstStack;
static PCToken MomSection;
static char *LastGlobSymbol;

void AsmParsInit(void)
{
  FirstSymbol = NULL;

  FirstLocSymbol = NULL; MomLocHandle = -1; SetMomSection(-1);
  FirstSection = NULL;
  FirstLocHandle = NULL;
  FirstStack = NULL;
  FirstRegDef = NULL;
  DoRefs = True;
  RadixBase = 10;
  OutRadixBase = 16;
}


Boolean RangeCheck(LargeInt Wert, IntType Typ)
{
#ifndef HAS64
  if (((int)Typ) >= ((int)SInt32))
    return True;
#else
  if (((int)Typ) >= ((int)Int64))
    return True;
#endif
  else
    return ((Wert >= IntTypeDefs[(int)Typ].Min) && (Wert <= IntTypeDefs[(int)Typ].Max));
}

Boolean FloatRangeCheck(Double Wert, FloatType Typ)
{
  switch (Typ)
  {
    case Float32:
      return (fabs(Wert) <= 3.4e38);
    case Float64:
      return (fabs(Wert) <= 1.7e308);
/**     case FloatCo: return fabs(Wert) <= 9.22e18; */
    case Float80:
      return True;
    case FloatDec:
      return True;
    default:
      return False;
  }
/**   if (Typ == FloatDec) && (fabs(Wert) > 1e1000) WrError(40);**/
}

Boolean SingleBit(LargeInt Inp, LargeInt *Erg)
{
  *Erg = 0;
  do
  {
    if (!Odd(Inp))
      (*Erg)++;
    if (!Odd(Inp))
      Inp = Inp >> 1;
  }
  while ((*Erg != LARGEBITS) && (!Odd(Inp)));
  return (*Erg != LARGEBITS) && (Inp == 1);
}	
	

static Boolean ProcessBk(char **Start, char *Erg)
{
  LongInt System = 0, Acc = 0, Digit = 0;
  char ch;
  int cnt;
  Boolean Finish;

  switch (mytoupper(**Start))
  {
    case '\'': case '\\': case '"':
      *Erg = **Start;
      (*Start)++;
      return True;
    case 'H':
      *Erg = '\'';
      (*Start)++;
      return True;
    case 'I':
      *Erg = '"';
      (*Start)++;
    return True;
    case 'B':
      *Erg = Char_BS;
      (*Start)++;
      return True;
    case 'A':
      *Erg = Char_BEL;
      (*Start)++;
      return True;
    case 'E':
      *Erg = Char_ESC;
      (*Start)++;
       return True;
    case 'T':
      *Erg = Char_HT;
      (*Start)++;
       return True;
    case 'N':
      *Erg = Char_LF;
      (*Start)++;
      return True;
    case 'R':
      *Erg = Char_CR;
      (*Start)++;
      return True;
    case 'X':
      System = 16;
      (*Start)++;
      /* no break */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      if (System == 0)
        System = (**Start == '0') ? 8 : 10;
      cnt = (System == 16) ? 1 : ((System == 10) ? 0 : -1);
      do
      {
        ch = mytoupper(**Start);
        Finish = False;
        if ((ch >= '0') && (ch <= '9'))
          Digit = ch - '0';
        else if ((System == 16) && (ch >= 'A') && (ch <= 'F'))
          Digit = (ch - 'A') + 10;
        else
          Finish = True;
        if (!Finish)
        {
          (*Start)++;
          cnt++;
          if (Digit >= System)
          {
            WrError(1320);
            return False;
          }
          Acc = (Acc * System) + Digit;
        }
      }
      while ((!Finish) && (cnt < 3));
      if (!ChkRange(Acc, 0, 255))
        return False;
      *Erg = Acc;
      return True;
    default:
      WrError(2010);
      return False;
  }
}

static void ReplaceBkSlashes(char *s)
{
  char *pSrc, *pDest;

  pSrc = pDest = s;
  while (*pSrc)
  {
    if (*pSrc == '\\')
    {
      pSrc++;
      if (ProcessBk(&pSrc, pDest))
        pDest++;
    }
    else
      *pDest++ = *pSrc++;
  }
  *pDest = '\0';
}

Boolean ExpandSymbol(char *Name)
{
  char *p1, *p2;
  String h;
  Boolean OK;

  do
  {
    p1 = strchr(Name, '{');
    if (!p1)
      return True;
    strmaxcpy(h, p1 + 1, 255);
    p2 = QuotPos(h, '}');
    if (!p2)
    {
      WrXError(1020, Name);
      return False;
    }
    strcpy(p1, p2 + 1);
    *p2 = '\0';
    FirstPassUnknown = False;
    EvalStringExpression(h, &OK, h);
    if (FirstPassUnknown)
    {
      WrError(1820);
      return False;
    }
    if (!CaseSensitive)
      UpString(h);
    strmaxins(Name, h, p1 - Name, 255);
  }
  while (p1);
  return True;
}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* check whether this is a local symbol and expand local counter if yes.  They
   have to be handled in different places of the parser, therefore two separate
   functions */

void InitTmpSymbols(void)
{
  TmpSymCounter = FwdSymCounter = BackSymCounter = 0;
  *TmpSymCounterVal = '\0';
  TmpSymLogDepth = 0;
  *LastGlobSymbol = '\0';
}

static void AddTmpSymLog(Boolean Back, LongInt Counter)
{
  /* shift out oldest value */

  if (TmpSymLogDepth)
  {
    LongInt ShiftCnt = min(TmpSymLogDepth, LOCSYMSIGHT - 1);

    memmove(TmpSymLog + 1, TmpSymLog, sizeof(TTmpSymLog) * (ShiftCnt));
  }

  /* insert new one */

  TmpSymLog[0].Back = Back;
  TmpSymLog[0].Counter = Counter;
  if (TmpSymLogDepth < LOCSYMSIGHT)
    TmpSymLogDepth++;
}

static Boolean ChkTmp1(char *Name, Boolean Define)
{
  char *Src, *Dest;
  Boolean Result = FALSE;

  /* $$-Symbols: append current $$-counter */

  if (!strncmp(Name, "$$", 2))
  {
    /* manually copy since this will implicitly give us the point to append
       the number */

    for (Src = Name + 2, Dest = Name; *Src; *(Dest++) = *(Src++));

    /* append number. only generate the number once */

    if (*TmpSymCounterVal == '\0')
      sprintf(TmpSymCounterVal, "%d", TmpSymCounter);
    strcpy(Dest, TmpSymCounterVal);
    Result = TRUE;
  }

  /* no special local symbol: increment $$-counter */

  else if (Define)
  {
    TmpSymCounter++;
    *TmpSymCounterVal = '\0';
  }

  return Result;
}

static Boolean ChkTmp2(char *Name, Boolean Define)
{
  char *Src;
  int Cnt;
  Boolean Result = FALSE;

  /* Note: We have to deal with three symbol definitions:

      "-" for backward-only referencing
      "+" for forward-only referencing
      "/" for either way of referencing

      "/" and "+" are both expanded to forward symbol names, so the
      forward refencing to both types is unproblematic, however
      only "/" and "-" are stored in the backlog of the three
      most-recent symbols for backward referencing.
  */

  /* backward references ? */

  if (*Name == '-')
  {
    for (Src = Name; *Src; Src++)
      if (*Src != '-')
        break;
    Cnt = Src - Name;
    if (!*Src)
    {
      if ((Define) && (Cnt == 1))
      {
        sprintf(Name, "__back%d", BackSymCounter);
        AddTmpSymLog(TRUE, BackSymCounter);
        BackSymCounter++;
        Result = TRUE;
      }

      /* TmpSymLogDepth cannot become larger than LOCSYMSIGHT, so we only
         have to check against the log's actual depth. */

      else if (Cnt <= TmpSymLogDepth)
      {
        Cnt--;
        sprintf(Name, "__%s%d",
                TmpSymLog[Cnt].Back ? "back" : "forw",
                TmpSymLog[Cnt].Counter);
        Result = TRUE;
      }
    }
  }

  /* forward references ? */

  else if (*Name == '+')
  {
    for (Src = Name; *Src; Src++)
      if (*Src != '+')
        break;
    Cnt = Src - Name;
    if (!*Src)
    {
      if ((Define) && (Cnt == 1))
      {
        sprintf(Name, "__forw%d", FwdSymCounter++);
        Result = TRUE;
      }
      else if (Cnt <= LOCSYMSIGHT)
      {
        sprintf(Name, "__forw%d", FwdSymCounter + (Cnt - 1));
        Result = TRUE;
      }
    }
  }

  /* slash: only allowed for definition, but add to log for backward ref. */

  else if ((!strcmp(Name, "/")) && (Define))
  {
    AddTmpSymLog(FALSE, FwdSymCounter);
    sprintf(Name, "__forw%d", FwdSymCounter);
    FwdSymCounter++;
    Result = TRUE;
  }

  return Result;
}

static Boolean ChkTmp3(char *Name, Boolean Define)
{
  Boolean Result = FALSE;

  if ('.' == *Name)
  {
    String Tmp;

    strmaxcpy(Tmp, LastGlobSymbol, 255);
    strmaxcat(Tmp, Name, 255);
    strmaxcpy(Name, Tmp, 255);

    Result = TRUE;
  }
  else if (Define)
  {
    strmaxcpy(LastGlobSymbol, Name, 255);
  }

  return Result;
}

static Boolean ChkTmp(char *Name, Boolean Define)
{
  Boolean Result = FALSE;

  if (ChkTmp1(Name, Define))
    Result = TRUE;
  if (ChkTmp2(Name, Define))
    Result = TRUE;
  if (ChkTmp3(Name, Define))
    Result = TRUE;
  return Result;
}

Boolean IdentifySection(char *Name, LongInt *Erg)
{
  PSaveSection SLauf;
  sint Depth;

  if (!ExpandSymbol(Name))
    return False;
  if (!CaseSensitive)
    NLS_UpString(Name);

  if (*Name == '\0')
  {
    *Erg = -1;
    return True;
  }
  else if (((strlen(Name) == 6) || (strlen(Name) == 7))
       && (!strncasecmp(Name, "PARENT", 6))
       && ((strlen(Name) == 6) || ((Name[6] >= '0') && (Name[6] <= '9'))))
  {
    Depth = (strlen(Name) == 6) ? 1 : Name[6] - AscOfs;
    SLauf = SectionStack;
    *Erg = MomSectionHandle;
    while ((Depth > 0) && (*Erg != -2))
    {
      if (!SLauf) *Erg = -2;
      else
      {
        *Erg = SLauf->Handle;
        SLauf = SLauf->Next;
      }
      Depth--;
    }
    if (*Erg == -2)
    {
      WrError(1484);
      return False;
    }
    else
      return True;
  }
  else if (!strcmp(Name, GetSectionName(MomSectionHandle)))
  {
    *Erg = MomSectionHandle;
    return True;
  }
  else
  {
    SLauf = SectionStack;
    while ((SLauf) && (strcmp(GetSectionName(SLauf->Handle), Name)))
      SLauf = SLauf->Next;
    if (!SLauf)
    {
      WrError(1484);
      return False;
    }
    else
    {
      *Erg = SLauf->Handle;
      return True;
    }
  }
}

static Boolean GetSymSection(char *Name, LongInt *Erg)
{
  String Part;
  char *q;
  int l = strlen(Name);

  if (Name[l - 1] != ']')
  {
    *Erg = -2;
    return True;
  }

  Name[l - 1] = '\0';
  q = RQuotPos(Name, '[');
  Name[l - 1] = ']';
  if (Name + l - q <= 1)
  {
    WrXError(1020, Name);
    return False;
  }

  Name[l - 1] = '\0';
  strmaxcpy(Part, q + 1, 255);
  *q = '\0';

  return IdentifySection(Part, Erg);
}

/*****************************************************************************
 * Function:    DigitVal
 * Purpose:     return value of an integer digit
 * Result:      value or -1 for error
 *****************************************************************************/

static int DigitVal(char ch, int Base)
{
  int erg;

  if ((ch >= '0') && (ch <= '9'))
    erg = ch - '0';
  else if ((ch >= 'A') && (ch <= 'Z'))
    erg = ch - 'A' + 10;
  else
    return - 1;

  if (erg >= Base)
    return -1;

  return erg;
}

/*****************************************************************************
 * Function:    ConstIntVal
 * Purpose:     evaluate integer constant
 * Result:      integer value
 *****************************************************************************/

LargeInt ConstIntVal(const char *pExpr, IntType Typ, Boolean *pResult)
{
  Byte Digit;
  LargeInt Wert;
  int l;

  /* empty string is interpreted as 0 */

  if (!*pExpr)
  {
    *pResult = True;
    return 0;
  }

  *pResult = False;
  Wert = 0;

  /* ASCII herausfiltern */

  if (*pExpr == '\'')
  {
    const char *pRun;
    String Copy;

    /* consistency check: closing ' must be present precisely at end; skip escaped characters */

    for (pRun = pExpr + 1; (*pRun) && (*pRun != '\''); pRun++)
    {
      if (*pRun == '\\')
        pRun++;
    }
    if ((*pRun != '\'') || (pRun[1]))
      return -1;

    strmaxcpy(Copy, pExpr + 1, STRINGSIZE);
    l = strlen(Copy);
    Copy[l - 1] = '\0'; ReplaceBkSlashes(Copy);

    for (pRun = Copy; *pRun; pRun++)
    {
      Digit = (usint) *pRun;
      Wert = (Wert << 8) + CharTransTable[Digit & 0xff];
    }
  }

  /* Zahlenkonstante */

  else
  {
    Boolean NegFlag = False;
    TConstMode ActMode = ConstModeC;
    int Search;
    Byte Base;
    char ch;
    Boolean Found;

    /* sign: */

    switch (*pExpr)
    {
      case '-':
        NegFlag = True;
        /* explicitly no break */
      case '+':
        pExpr++;
        break;
    }
    l = strlen(pExpr);

    /* automatic syntax determination: */

    if (RelaxedMode)
    {
      Found = False;

      if ((l >= 2) && (*pExpr == '0') && (mytoupper(pExpr[1]) == 'X'))
      {
        ActMode = ConstModeC;
        Found = True;
      }

      if ((!Found) && (l >= 2))
      {
        for (Search = 0; Search < 3; Search++)
          if (*pExpr == BaseIds[Search])
          {
            ActMode = ConstModeMoto;
            Found = True;
            break;
          }
      }

      if ((!Found) && (l >= 2) && (*pExpr >= '0') && (*pExpr <= '9'))
      {
        ch = mytoupper(pExpr[l - 1]);
        if (DigitVal(ch, RadixBase) == -1)
        {
          for (Search = 0; Search < sizeof(BaseLetters) / sizeof(*BaseLetters); Search++)
            if (ch == BaseLetters[Search])
            {
              ActMode = ConstModeIntel;
              Found = True;
              break;
            }
        }
      }

      if ((!Found) && (l >= 3) && (pExpr[1] == '\'') && (pExpr[l - 1] == '\''))
      {
        switch (mytoupper(*pExpr))
        {
          case 'H':
          case 'X':
          case 'B':
          case 'O':
            ActMode = ConstModeWeird;
            Found = True;
            break;
        }
      }

      if (!Found)
        ActMode = ConstModeC;
    }
    else /* !RelaxedMode */
      ActMode = ConstMode;

    /* Zahlensystem ermitteln/pruefen */

    Base = RadixBase;
    switch (ActMode)
    {
      case ConstModeIntel:
        ch = mytoupper(pExpr[l - 1]);
        if (DigitVal(ch, RadixBase) == -1)
        {
          for (Search = 0; Search < sizeof(BaseLetters) / sizeof(*BaseLetters); Search++)
            if (ch == BaseLetters[Search])
            {
              Base = BaseVals[Search];
              l--;
              break;
            }
        }
        break;
      case ConstModeMoto:
        for (Search = 0; Search < 3; Search++)
          if (*pExpr == BaseIds[Search])
          {
            Base = BaseVals[Search];
            pExpr++; l--;
            break;
          }
        break;
      case ConstModeC:
        if (!strcmp(pExpr, "0"))
        {
          *pResult = True;
          return 0;
        }
        if (*pExpr != '0') Base = RadixBase;
        else if (l < 2) return -1;
        else
        {
          pExpr++; l--;
          ch = mytoupper(*pExpr);
          if ((RadixBase != 10) && (DigitVal(ch, RadixBase) != -1))
            Base = RadixBase;
          else
            switch (mytoupper(*pExpr))
            {
              case 'X':
                pExpr++;
                l--;
                Base = 16;
                break;
              case 'B':
                pExpr++;
                l--;
                Base = 2;
                break;
              default:
                Base = 8;
            }
        }
        break;
      case ConstModeWeird:
        if ((l < 3) || (pExpr[1] != '\'') || (pExpr[l - 1] != '\''))
          return -1;
        switch (mytoupper(*pExpr))
        {
          case 'X':
          case 'H':
            Base = 16;
            break;
          case 'B':
            Base = 2;
            break;
          case 'O':
            Base = 8;
            break;
          default:
            return -1;
        }
        pExpr += 2;
        l -= 3;
        break;
    }

    if (!*pExpr)
      return -1;

    if (ActMode == ConstModeIntel)
    {
      if ((*pExpr < '0') || (*pExpr > '9'))
        return -1;
    }

    /* we may have decremented l, so do not run until string end */

    while (l > 0)
    {
      Search = DigitVal(mytoupper(*pExpr), Base);
      if (Search == -1)
        return -1;
      Wert = Wert * Base + Search;
      pExpr++; l--;
    }

    if (NegFlag)
      Wert = -Wert;
  }

  /* post-processing, range check */

  *pResult = RangeCheck(Wert, Typ);
  if (*pResult)
    return Wert;
  else if (HardRanges)
  {
    WrError(1320);
    return -1;
  }
  else
  {
    *pResult = True;
    WrError(260);
    return Wert&IntTypeDefs[(int)Typ].Mask;
  }
}

/*****************************************************************************
 * Function:    ConstFloatVal
 * Purpose:     evaluate floating point constant
 * Result:      value
 *****************************************************************************/

Double ConstFloatVal(const char *pExpr, FloatType Typ, Boolean *pResult)
{
  Double Erg;
  char *pEnd;

  UNUSED(Typ);

  if (*pExpr)
  {
    /* Some strtod() implementations interpret hex constants starting with '0x'.  We
       don't want this here.  Either 0x for hex constants is allowed, then it should
       have been parsed before by ConstIntVal(), or not, then we don't want the constant
       be stored as float. */

    if ((strlen(pExpr) >= 2)
     && (pExpr[0] == '0')
     && (toupper(pExpr[1]) == 'X'))
    {
      Erg = 0;
      *pResult = False;
    }

    else
    {
      Erg = strtod(pExpr, &pEnd);
      *pResult = (*pEnd == '\0');
    }
  }
  else
  {
    Erg = 0.0;
    *pResult = True;
  }
  return Erg;
}

/*****************************************************************************
 * Function:    ConstStringVal
 * Purpose:     evaluate string constant
 * Result:      value
 *****************************************************************************/

void ConstStringVal(const char *pExpr, tDynString *pDest, Boolean *pResult)
{
  String Copy;
  char *pPos, *pCurr;
  int l, TLen;

  *pResult = False;

  l = strlen(pExpr);
  if ((l < 2) || (*pExpr != '"') || (pExpr[l - 1] != '"'))
    return;

  strmaxcpy(Copy, pExpr + 1, STRINGSIZE);
  Copy[strlen(Copy) - 1] = '\0';

  /* go through source */

  pCurr = Copy;
  pDest->Length = 0;
  while (*pCurr)
  {
    /* copy part up to next '\' verbatim: */

    pPos = strchr(pCurr, '\\');
    if (pPos)
      *pPos = '\0';
    if (strchr(pCurr, '"'))
      return;
    TLen = strlen(pCurr);
    DynStringAppend(pDest, pCurr, TLen);

    /* are we done? If not, advance pointer to behind '\' */

    if (!pPos)
      break;
    pCurr = pPos + 1;

    if (pPos)
    {
      /* stringification? */

      if (*pCurr == '{')
      {
        TempResult t;
        char *pStr;
        String Str;

        /* cut out part in {...} */

        pPos = QuotPos(++pCurr, '}');
        if (!pPos)
          return;
        *pPos = '\0';
        KillBlanks(pCurr);

        /* evaluate expression */

        FirstPassUnknown = False;
        EvalExpression(pCurr, &t);
        if (FirstPassUnknown)
        {
          WrXError(1820, pCurr);
          return;
        }
        if (t.Relocs)
        {
          WrError(1150);
          FreeRelocs(&t.Relocs);
          return;
        }

        /* append result */

        switch (t.Typ)
        {
          case TempInt:
            TLen = SysString(Str, sizeof(Str), t.Contents.Int, OutRadixBase, 0);
            pStr = Str;
            break;
          case TempFloat:
            pStr = FloatString(t.Contents.Float);
            TLen = strlen(pStr);
            break;
          case TempString:
            pStr = t.Contents.Ascii.Contents;
            TLen = t.Contents.Ascii.Length;
            break;
          default:
            return;
        }
        DynStringAppend(pDest, pStr, TLen);

        /* advance source pointer to behind '}' */

        pCurr = pPos + 1;
      }

      /* simple character escape: */

      else
      {
        char Res;

        if (!ProcessBk(&pCurr, &Res))
          return;
        DynStringAppend(pDest, &Res, 1);
      }
    }
  }

  *pResult = True;
}


static PSymbolEntry FindLocNode(
#ifdef __PROTOS__
const char *Name, TempType SearchType
#endif
);

static PSymbolEntry FindNode(
#ifdef __PROTOS__
const char *Name, TempType SearchType
#endif
);

/*****************************************************************************
 * Function:    EvalExpression
 * Purpose:     evaluate expression
 * Result:      implicitly in pErg
 *****************************************************************************/


static void EvalExpression_ChgFloat(TempResult *pTemp)
{
  if (pTemp->Typ != TempInt)
    return;
  pTemp->Typ = TempFloat;
  pTemp->Contents.Float = pTemp->Contents.Int;
}

static void EvalExpression_DummyOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  UNUSED(pLVal);
  UNUSED(pRVal);
  UNUSED(pErg);
}

static void EvalExpression_OneComplOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  UNUSED(pLVal);
  pErg->Typ = TempInt;
  pErg->Contents.Int = ~(pRVal->Contents.Int);
}

static void EvalExpression_ShLeftOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = pLVal->Contents.Int << pRVal->Contents.Int;
}

static void EvalExpression_ShRightOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = pLVal->Contents.Int >> pRVal->Contents.Int;
}

static void EvalExpression_BitMirrorOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  int z;

  if ((pRVal->Contents.Int < 1) || (pRVal->Contents.Int > 32)) WrError(1320);
  else
  {
    pErg->Typ = TempInt;
    pErg->Contents.Int = (pLVal->Contents.Int >> pRVal->Contents.Int) << pRVal->Contents.Int;
    pRVal->Contents.Int--;
    for (z = 0; z <= pRVal->Contents.Int; z++)
    {
      if ((pLVal->Contents.Int & (1 << (pRVal->Contents.Int - z))) != 0)
        pErg->Contents.Int |= (1 << z);
    }
  }
}

static void EvalExpression_BinAndOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = pLVal->Contents.Int & pRVal->Contents.Int;
}

static void EvalExpression_BinOrOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = pLVal->Contents.Int | pRVal->Contents.Int;
}

static void EvalExpression_BinXorOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = pLVal->Contents.Int ^ pRVal->Contents.Int;
}

static void EvalExpression_PotOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  LargeInt HVal;

  switch (pErg->Typ = pLVal->Typ)
  {
    case TempInt:
      if (pRVal->Contents.Int < 0) pErg->Contents.Int = 0;
      else
      {
        pErg->Contents.Int = 1;
        while (pRVal->Contents.Int > 0)
        {
          if (pRVal->Contents.Int & 1)
            pErg->Contents.Int *= pLVal->Contents.Int;
          pRVal->Contents.Int >>= 1;
          if (pRVal->Contents.Int != 0)
            pLVal->Contents.Int *= pLVal->Contents.Int;
        }
      }
      break;
    case TempFloat:
      if (pRVal->Contents.Float == 0.0)
        pErg->Contents.Float = 1.0;
      else if (pLVal->Contents.Float == 0.0)
        pErg->Contents.Float = 0.0;
      else if (pLVal->Contents.Float > 0)
        pErg->Contents.Float = pow(pLVal->Contents.Float, pRVal->Contents.Float);
      else if ((fabs(pRVal->Contents.Float) <= ((double)MaxLongInt)) && (floor(pRVal->Contents.Float) == pRVal->Contents.Float))
      {
        HVal = (LongInt) floor(pRVal->Contents.Float + 0.5);
        if (HVal < 0)
        {
          pLVal->Contents.Float = 1 / pLVal->Contents.Float;
          HVal = -HVal;
        }
        pErg->Contents.Float = 1.0;
        while (HVal > 0)
        {
          if ((HVal & 1) == 1)
            pErg->Contents.Float *= pLVal->Contents.Float;
          pLVal->Contents.Float *= pLVal->Contents.Float;
          HVal >>= 1;
        }
      }
      else
      {
        WrError(1890);
        pErg->Typ = TempNone;
      }
      break;
    default:
      break;
  }
}

static void EvalExpression_MultOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  switch (pErg->Typ = pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = pLVal->Contents.Int * pRVal->Contents.Int;
      break;
    case TempFloat:
      pErg->Contents.Float = pLVal->Contents.Float * pRVal->Contents.Float;
      break;
    default:
      break;
  }
}

static void EvalExpression_DivOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  switch (pLVal->Typ)
  {
    case TempInt:
      if (pRVal->Contents.Int == 0) WrError(1310);
      else
      {
        pErg->Typ = TempInt;
        pErg->Contents.Int = pLVal->Contents.Int / pRVal->Contents.Int;
      }
      break;
    case TempFloat:
      if (pRVal->Contents.Float == 0.0) WrError(1310);
      else
      {
        pErg->Typ = TempFloat;
        pErg->Contents.Float = pLVal->Contents.Float / pRVal->Contents.Float;
      }
    default:
      break;
  }
}

static void EvalExpression_ModOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  if (pRVal->Contents.Int == 0) WrError(1310);
  else
  {
    pErg->Typ = TempInt;
    pErg->Contents.Int = pLVal->Contents.Int % pRVal->Contents.Int;
  }
}

static void EvalExpression_AddOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  switch (pErg->Typ = pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = pLVal->Contents.Int + pRVal->Contents.Int;
      pErg->Relocs = MergeRelocs(&(pLVal->Relocs), &(pRVal->Relocs), TRUE);
      break;
    case TempFloat:
      pErg->Contents.Float = pLVal->Contents.Float + pRVal->Contents.Float;
      break;
    case TempString:
      DynString2DynString(&pErg->Contents.Ascii, &pLVal->Contents.Ascii);
      DynStringAppendDynString(&pErg->Contents.Ascii, &pRVal->Contents.Ascii);
      break;
    default:
      break;
  }
}

static void EvalExpression_SubOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  switch (pErg->Typ = pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = pLVal->Contents.Int - pRVal->Contents.Int;
      pErg->Relocs = MergeRelocs(&(pLVal->Relocs), &(pRVal->Relocs), FALSE);
      break;
    case TempFloat:
      pErg->Contents.Float = pLVal->Contents.Float - pRVal->Contents.Float;
      break;
    default:
      break;
  }
}

static void EvalExpression_LogNotOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  UNUSED(pLVal);
  pErg->Typ = TempInt;
  pErg->Contents.Int = (pRVal->Contents.Int == 0) ? 1 : 0;
}

static void EvalExpression_LogAndOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = ((pLVal->Contents.Int != 0) && (pRVal->Contents.Int != 0)) ? 1 : 0;
}

static void EvalExpression_LogOrOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = ((pLVal->Contents.Int != 0) || (pRVal->Contents.Int != 0)) ? 1 : 0;
}

static void EvalExpression_LogXorOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  pErg->Contents.Int = ((pLVal->Contents.Int != 0) != (pRVal->Contents.Int != 0)) ? 1 : 0;
}

static void EvalExpression_EqOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  switch (pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = (pLVal->Contents.Int == pRVal->Contents.Int) ? 1 : 0;
      break;
    case TempFloat:
      pErg->Contents.Int = (pLVal->Contents.Float == pRVal->Contents.Float) ? 1 : 0;
      break;
    case TempString:
      pErg->Contents.Int = (DynStringCmp(&pLVal->Contents.Ascii, &pRVal->Contents.Ascii) == 0) ? 1 : 0;
      break;
    default:
      break;
  }
}

static void EvalExpression_GtOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  switch (pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = (pLVal->Contents.Int > pRVal->Contents.Int) ? 1 : 0;
      break;
    case TempFloat:
      pErg->Contents.Int = (pLVal->Contents.Float > pRVal->Contents.Float) ? 1 : 0;
      break;
    case TempString:
      pErg->Contents.Int = (DynStringCmp(&pLVal->Contents.Ascii, &pRVal->Contents.Ascii) > 0) ? 1 : 0;
      break;
    default:
      break;
  }
}

static void EvalExpression_LtOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  switch (pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = (pLVal->Contents.Int < pRVal->Contents.Int) ? 1 : 0;
      break;
    case TempFloat:
      pErg->Contents.Int = (pLVal->Contents.Float < pRVal->Contents.Float) ? 1 : 0;
      break;
    case TempString:
      pErg->Contents.Int = (DynStringCmp(&pLVal->Contents.Ascii, &pRVal->Contents.Ascii) < 0) ? 1 : 0;
      break;
    default:
      break;
  }
}

static void EvalExpression_LeOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  switch (pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = (pLVal->Contents.Int <= pRVal->Contents.Int) ? 1 : 0;
      break;
    case TempFloat:
      pErg->Contents.Int = (pLVal->Contents.Float <= pRVal->Contents.Float) ? 1 : 0;
      break;
    case TempString:
      pErg->Contents.Int = (DynStringCmp(&pLVal->Contents.Ascii, &pRVal->Contents.Ascii) <= 0) ? 1 : 0;
      break;
    default:
      break;
  }
}

static void EvalExpression_GeOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  switch (pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = (pLVal->Contents.Int >= pRVal->Contents.Int) ? 1 : 0;
      break;
    case TempFloat:
      pErg->Contents.Int = (pLVal->Contents.Float >= pRVal->Contents.Float) ? 1 : 0;
      break;
    case TempString:
      pErg->Contents.Int = (DynStringCmp(&pLVal->Contents.Ascii, &pRVal->Contents.Ascii) >= 0) ? 1 : 0;
      break;
    default:
      break;
  }
}

static void EvalExpression_UneqOp(TempResult *pErg, TempResult *pLVal, TempResult *pRVal)
{
  pErg->Typ = TempInt;
  switch (pLVal->Typ)
  {
    case TempInt:
      pErg->Contents.Int = (pLVal->Contents.Int != pRVal->Contents.Int) ? 1 : 0;
      break;
    case TempFloat:
      pErg->Contents.Int = (pLVal->Contents.Float != pRVal->Contents.Float) ? 1 : 0;
      break;
    case TempString:
      pErg->Contents.Int = (DynStringCmp(&pLVal->Contents.Ascii, &pRVal->Contents.Ascii) != 0) ? 1 : 0;
      break;
    default:
      break;
  }
}

#define LEAVE goto func_exit

void EvalExpression(const char *pExpr, TempResult *pErg)
{
  static const Operator Operators[] =
  {
    {" " , 1 , False,  0, False, False, False, False, EvalExpression_DummyOp},
    {"~" , 1 , False,  1, True , False, False, False, EvalExpression_OneComplOp},
    {"<<", 2 , True ,  3, True , False, False, False, EvalExpression_ShLeftOp},
    {">>", 2 , True ,  3, True , False, False, False, EvalExpression_ShRightOp},
    {"><", 2 , True ,  4, True , False, False, False, EvalExpression_BitMirrorOp},
    {"&" , 1 , True ,  5, True , False, False, False, EvalExpression_BinAndOp},
    {"|" , 1 , True ,  6, True , False, False, False, EvalExpression_BinOrOp},
    {"!" , 1 , True ,  7, True , False, False, False, EvalExpression_BinXorOp},
    {"^" , 1 , True ,  8, True , True , False, False, EvalExpression_PotOp},
    {"*" , 1 , True , 11, True , True , False, False, EvalExpression_MultOp},
    {"/" , 1 , True , 11, True , True , False, False, EvalExpression_DivOp},
    {"#" , 1 , True , 11, True , False, False, False, EvalExpression_ModOp},
    {"+" , 1 , True , 13, True , True , True , False, EvalExpression_AddOp},
    {"-" , 1 , True , 13, True , True , False, False, EvalExpression_SubOp},
    {"~~", 2 , False,  2, True , False, False, False, EvalExpression_LogNotOp},
    {"&&", 2 , True , 15, True , False, False, False, EvalExpression_LogAndOp},
    {"||", 2 , True , 16, True , False, False, False, EvalExpression_LogOrOp},
    {"!!", 2 , True , 17, True , False, False, False, EvalExpression_LogXorOp},
    {"=" , 1 , True , 23, True , True , True , False, EvalExpression_EqOp},
    {"==", 2 , True , 23, True , True , True , False, EvalExpression_EqOp},
    {">" , 1 , True , 23, True , True , True , False, EvalExpression_GtOp},
    {"<" , 1 , True , 23, True , True , True , False, EvalExpression_LtOp},
    {"<=", 2 , True , 23, True , True , True , False, EvalExpression_LeOp},
    {">=", 2 , True , 23, True , True , True , False, EvalExpression_GeOp},
    {"<>", 2 , True , 23, True , True , True , False, EvalExpression_UneqOp},
    /* termination marker */
    {NULL, 0 , False,  0, False, False, False, False, NULL}
  },
  /* minus may have one or two operands */
  MinusMonadicOperator =
  {
    "-" ,1 , False, 13, True , True , False, False, EvalExpression_SubOp
  };
  const Operator *pOp;
  const Operator *FOps[sizeof(Operators) / sizeof(*Operators)];
  LongInt FOpCnt = 0;

  Boolean OK, FFound;
  TempResult LVal, RVal, MVal;
  int z1, cnt;
  char Save = '\0';
  sint LKlamm, RKlamm, WKlamm, zop;
  sint OpMax, OpPos = -1;
  Boolean InHyp, InQuot;
  Double  FVal;
  PSymbolEntry Ptr;
  PFunction ValFunc;
  String Copy, stemp, ftemp;
  char *KlPos, *zp, *DummyPtr, *pOpPos;
  PRelocEntry TReloc;

  ChkStack();

  if (MakeDebug)
    fprintf(Debug, "Parse '%s'\n", pExpr);

  memset(&LVal, 0, sizeof(LVal));
  memset(&RVal, 0, sizeof(RVal));

  /* Annahme Fehler */

  pErg->Typ = TempNone;
  pErg->Relocs = NULL;
  pErg->Flags = 0;

  (void)CopyNoBlanks(Copy, pExpr, STRINGSIZE);

  /* sort out local symbols like - and +++.  Do it now to get them out of the
     formula parser's way. */

  if (ChkTmp2(Copy, FALSE))
    strmaxcpy(stemp, Copy, STRINGSIZE);
  else
    strmaxcpy(stemp, pExpr, STRINGSIZE);

  /* Programmzaehler ? */

  if ((PCSymbol) && (!strcasecmp(Copy, PCSymbol)))
  {
    pErg->Typ = TempInt;
    pErg->Contents.Int = EProgCounter();
    pErg->Relocs = NULL;
    LEAVE;
  }

  /* Konstanten ? */

  pErg->Contents.Int = ConstIntVal(Copy, (IntType) (IntTypeCnt-1), &OK);
  if (OK)
  {
    pErg->Typ = TempInt;
    pErg->Relocs = NULL;
    LEAVE;
  }

  pErg->Contents.Float = ConstFloatVal(Copy, Float80, &OK);
  if (OK)
  {
    pErg->Typ = TempFloat;
    pErg->Relocs = NULL;
    LEAVE;
  }

  ConstStringVal(Copy, &pErg->Contents.Ascii, &OK);
  if (OK)
  {
    pErg->Typ = TempString;
    pErg->Relocs = NULL;
    LEAVE;
  }

  /* durch Codegenerator gegebene Konstanten ? */

  pErg->Relocs = NULL;
  InternSymbol(Copy, pErg);
  if (pErg->Typ != TempNone)
    LEAVE;

  /* find out which operators *might* occur in expression */

  OpMax = 0;
  LKlamm = 0;
  RKlamm = 0;
  WKlamm = 0;
  InHyp = False;
  InQuot = False;
  for (pOp = Operators + 1; pOp->Id; pOp++)
  {
    pOpPos = (pOp->IdLen == 1) ? (strchr(Copy, *pOp->Id)) : (strstr(Copy, pOp->Id));
    if (pOpPos)
      FOps[FOpCnt++] = pOp;
  }

  /* nach Operator hoechster Rangstufe ausserhalb Klammern suchen */

  for (zp = Copy; *zp; zp++)
  {
    switch (*zp)
    {
      case '(':
        if (!(InHyp || InQuot))
          LKlamm++;
        break;
      case ')':
        if (!(InHyp || InQuot))
          RKlamm++;
        break;
      case '{':
        if (!(InHyp || InQuot))
          WKlamm++;
        break;
      case '}':
        if (!(InHyp || InQuot))
          WKlamm--;
        break;
      case '"':
        if (!InHyp)
          InQuot = !InQuot;
        break;
      case '\'':
        if (!InQuot)
          InHyp = !InHyp;
        break;
      default:
        if ((LKlamm == RKlamm) && (WKlamm == 0) && (!InHyp) && (!InQuot))
        {
          Boolean OpFnd = False;
          sint OpLen = 0, LocOpMax = 0;

          for (zop = 0; zop < FOpCnt; zop++)
          {
            pOp = FOps[zop];
            if ((!strncmp(zp, pOp->Id, pOp->IdLen)) && (pOp->IdLen >= OpLen))
            {
              OpFnd = True;
              OpLen = pOp->IdLen;
              LocOpMax = pOp - Operators;
              if (Operators[LocOpMax].Priority >= Operators[OpMax].Priority)
              {
                OpMax = LocOpMax;
                OpPos = zp - Copy;
              }
            }
          }
          if (OpFnd)
            zp += Operators[LocOpMax].IdLen - 1;
        }
    }
  }

  /* Klammerfehler ? */

  if (LKlamm != RKlamm)
  {
    WrXError(1300, Copy);
    LEAVE;
  }

  /* Operator gefunden ? */

  if (OpMax)
  {
    pOp = Operators + OpMax;

    /* Minuszeichen sowohl mit einem als auch 2 Operanden */

    if (!strcmp(pOp->Id, "-"))
    {
      if (!OpPos)
        pOp = &MinusMonadicOperator;
    }

    /* Operandenzahl pruefen */

    if (((pOp->Dyadic) == (OpPos == 0))
     || (OpPos == (int)strlen(Copy)-1))
    {
      WrError(1110);
      LEAVE;
    }

    /* Teilausdruecke rekursiv auswerten */

    Save = Copy[OpPos];
    Copy[OpPos] = '\0';
    EvalExpression(Copy + OpPos + strlen(pOp->Id), &RVal);
    if (pOp->Dyadic)
      EvalExpression(Copy, &LVal);
    else if (RVal.Typ == TempFloat)
    {
      LVal.Typ = TempFloat;
      LVal.Contents.Float = 0.0;
    }
    else
    {
      LVal.Typ = TempInt;
      LVal.Contents.Int = 0;
      LVal.Relocs = NULL;
    }
    Copy[OpPos] = Save;

    /* Abbruch, falls dabei Fehler */

    if ((LVal.Typ == TempNone) || (RVal.Typ == TempNone))
      LEAVE;

    /* relokatible Symbole nur fuer + und - erlaubt */

    if ((OpMax != 12) && (OpMax != 13) && (LVal.Relocs || RVal.Relocs))
    {
      WrError(1150);
      LEAVE;
    }

    /* both operands for a dyadic operator must have same type */

    if ((pOp->Dyadic) && (LVal.Typ != RVal.Typ))
    {
      if ((LVal.Typ == TempString) || (RVal.Typ == TempString))
      {
        WrError(1135);
        LEAVE;
      }
      if (LVal.Typ == TempInt)
        EvalExpression_ChgFloat(&LVal);
      if (RVal.Typ == TempInt)
        EvalExpression_ChgFloat(&RVal);
    }

    /* optionally convert int to float if operator only supports float */

    switch (RVal.Typ)
    {
      case TempInt:
        if (!pOp->MayInt)
        {
          if (!pOp->MayFloat)
          {
            WrError(1135);
            LEAVE;
          }
          else
          {
            EvalExpression_ChgFloat(&RVal);
            if (pOp->Dyadic) EvalExpression_ChgFloat(&LVal);
          }
        }
        break;
      case TempFloat:
        if (!pOp->MayFloat)
        {
          WrError(1135);
          LEAVE;
        }
        break;
      case TempString:
        if (!pOp->MayString)
        {
          WrError(1135);
          LEAVE;
        }
        break;
      default:
        break;
    }

    /* Operanden abarbeiten */

    pOp->pFunc(pErg, &LVal, &RVal);
    LEAVE;
  } /* if (OpMax) */

  /* kein Operator gefunden: Klammerausdruck ? */

  if (LKlamm != 0)
  {
    /* erste Klammer suchen, Funktionsnamen abtrennen */

    KlPos = strchr(Copy, '(');

    /* Funktionsnamen abschneiden */

    *KlPos = '\0';
    strmaxcpy(ftemp, Copy, 255);
    strmov(Copy, KlPos + 1);
    Copy[strlen(Copy)-1] = '\0';

    /* Nullfunktion: nur Argument */

    if (ftemp[0] == '\0')
    {
      EvalExpression(Copy, &LVal);
      *pErg = LVal;
      LEAVE;
    }

    /* selbstdefinierte Funktion ? */

    ValFunc = FindFunction(ftemp);
    if (ValFunc)
    {
      char *pArg = Copy;

      strmaxcpy(ftemp, ValFunc->Definition, 255);
      for (z1 = 1; z1 <= ValFunc->ArguCnt; z1++)
      {
        if (!pArg)
        {
          WrError(1490);
          LEAVE;
        }

        KlPos = QuotPos(pArg, ',');
        if (KlPos)
          *KlPos = '\0';

        EvalExpression(pArg, &LVal);
        if (LVal.Relocs)
        {
          WrError(1150);
          FreeRelocs(&LVal.Relocs);
          return;
        }

        if (!KlPos)
          pArg = NULL;
        else
          pArg = KlPos + 1;

        strmaxcpy(stemp, "(", 255);
        switch (LVal.Typ)
        {
          case TempInt:
          {
            String s;

            LargeString(s, LVal.Contents.Int);
            sprintf(stemp + 1, "%s", s);
            break;
          }
          case TempFloat:
            sprintf(stemp + 1, "%0.16e", LVal.Contents.Float);
            KillBlanks(stemp);
            break;
          case TempString:
            stemp[1] = '"';
            snstrlenprint(stemp + 2, 252, LVal.Contents.Ascii.Contents, LVal.Contents.Ascii.Length);
            strmaxcat(stemp, "\"", 255);
            break;
          default:
            LEAVE;
        }
        strmaxcat(stemp,")", 255);
        ExpandLine(stemp, z1, ftemp);
      }
      if (pArg)
      {
        WrError(1490);
        LEAVE;
      }
      EvalExpression(ftemp, pErg);
      LEAVE;
    }

    /* hier einmal umwandeln ist effizienter */

    NLS_UpString(ftemp);

    /* symbolbezogene Funktionen */

    if (!strcmp(ftemp, "SYMTYPE"))
    {
      pErg->Typ = TempInt;
      pErg->Contents.Int = FindRegDef(Copy, &DummyPtr) ? 0x80 : GetSymbolType(Copy);
      LEAVE;
    }

    else if (!strcmp(ftemp, "ASSUMEDVAL"))
    {
      for (z1 = 0; z1 < ASSUMERecCnt; z1++)
        if (!strcasecmp(Copy, pASSUMERecs[z1].Name))
        {
          pErg->Typ = TempInt;
          pErg->Contents.Int = *(pASSUMERecs[z1].Dest);
          LEAVE;
        }
      WrXError(1010, Copy);
      LEAVE;
    }

    /* Unterausdruck auswerten (interne Funktionen maxmimal mit drei Argumenten) */

    z1 = 0; KlPos = Copy;
    do
    {
      zp = QuotPos(KlPos, ',');
      if (zp) *zp = '\0';
      switch (z1)
      {
        case 0:
          EvalExpression(KlPos, &LVal);
          if (LVal.Typ == TempNone)
            LEAVE;
          TReloc = LVal.Relocs;
          break;
        case 1:
          EvalExpression(KlPos, &MVal);
          if (MVal.Typ == TempNone)
            LEAVE;
          TReloc = MVal.Relocs;
          break;
        case 2:
          EvalExpression(KlPos, &RVal);
          if (RVal.Typ == TempNone)
            LEAVE;
          TReloc = RVal.Relocs;
          break;
        default:
          WrError(1490);
          LEAVE;
      }
      if (TReloc)
      {
        WrError(1150);
        FreeRelocs(&TReloc);
        LEAVE;
      }
      if (zp)
        KlPos = zp + 1;
      z1++;
    }
    while (zp);

    /* ein paar Funktionen mit zwei, drei Argumenten */

    if (z1 == 3)
    {
      if (!strcmp(ftemp, "SUBSTR"))
      {
        if ((LVal.Typ != TempString) || (MVal.Typ != TempInt) || (RVal.Typ != TempInt)) WrError(1135);
        else
        {
          cnt = LVal.Contents.Ascii.Length - MVal.Contents.Int;
          if ((RVal.Contents.Int != 0) && (RVal.Contents.Int < cnt))
            cnt = RVal.Contents.Int;
          if (cnt < 0)
            cnt = 0;
          pErg->Contents.Ascii.Length = 0;
          DynStringAppend(&pErg->Contents.Ascii, LVal.Contents.Ascii.Contents + MVal.Contents.Int, cnt);
          pErg->Typ = TempString;
        }
      }
      else
        WrXError(1860, ftemp);
      LEAVE;
    }
    else if (z1 == 2)
    {
      if (!strcmp(ftemp, "STRSTR"))
      {
        if ((LVal.Typ != TempString) || (MVal.Typ != TempString)) WrError(1135);
        else
        {
          pErg->Contents.Int = DynStringFind(&LVal.Contents.Ascii, &MVal.Contents.Ascii);
          pErg->Typ = TempInt;
        }
      }
      else if (!strcmp(ftemp, "CHARFROMSTR"))
      {
        if ((LVal.Typ != TempString) || (MVal.Typ != TempInt)) WrError(1135);
        else
        {
          pErg->Typ = TempInt;
          pErg->Contents.Int = ((MVal.Contents.Int >= 0) && (MVal.Contents.Int < LVal.Contents.Ascii.Length)) ? LVal.Contents.Ascii.Contents[MVal.Contents.Int] : -1;
        }
      }
      else
        WrXError(1860, ftemp);
      LEAVE;
    }

    /* expression type ? */

    if (!strcmp(ftemp, "EXPRTYPE"))
    {
      pErg->Typ = TempInt;
      switch (LVal.Typ)
      {
        case TempInt:
          pErg->Contents.Int = 0;
          break;
        case TempFloat:
          pErg->Contents.Int = 1;
          break;
        case TempString:
          pErg->Contents.Int = 2;
          break;
        default:
          pErg->Contents.Int = -1;
      }
      LEAVE;
    }

    /* Funktionen fuer Stringargumente */

    if (LVal.Typ == TempString)
    {
      /* in Grossbuchstaben wandeln ? */

      if (!strcmp(ftemp, "UPSTRING"))
      {
        pErg->Typ = TempString;
        DynString2DynString(&pErg->Contents.Ascii, &LVal.Contents.Ascii);
        for (KlPos = pErg->Contents.Ascii.Contents;
             KlPos < pErg->Contents.Ascii.Contents + pErg->Contents.Ascii.Length;
             KlPos++)
          *KlPos = mytoupper(*KlPos);
      }

      /* in Kleinbuchstaben wandeln ? */

      else if (!strcmp(ftemp, "LOWSTRING"))
      {
        pErg->Typ = TempString;
        DynString2DynString(&pErg->Contents.Ascii, &LVal.Contents.Ascii);
        for (KlPos = pErg->Contents.Ascii.Contents;
             KlPos < pErg->Contents.Ascii.Contents + pErg->Contents.Ascii.Length;
             KlPos++)
          *KlPos = mytolower(*KlPos);
      }

      /* Laenge ermitteln ? */

      else if (!strcmp(ftemp, "STRLEN"))
      {
        pErg->Typ = TempInt;
        pErg->Contents.Int = LVal.Contents.Ascii.Length;
      }

      /* Parser aufrufen ? */

      else if (!strcmp(ftemp, "VAL"))
      {
        String Tmp;

        DynString2CString(Tmp, &LVal.Contents.Ascii, sizeof(Tmp));
        EvalExpression(Tmp, pErg);
      }

      /* nix gefunden ? */

      else
      {
        WrXError(1860, ftemp);
        pErg->Typ = TempNone;
      }
    }

    /* Funktionen fuer Zahlenargumente */

    else
    {
      FFound = False;
      pErg->Typ = TempNone;

      /* reine Integerfunktionen */

      if (!strcmp(ftemp, "TOUPPER"))
      {
        if (LVal.Typ != TempInt) WrError(1135);
        else if ((LVal.Contents.Int < 0) || (LVal.Contents.Int > 255)) WrError(1320);
        else
        {
          pErg->Typ = TempInt;
          pErg->Contents.Int = toupper(LVal.Contents.Int);
        }
        FFound = True;
      }

      else if (!strcmp(ftemp, "TOLOWER"))
      {
        if (LVal.Typ != TempInt) WrError(1135);
        else if ((LVal.Contents.Int < 0) || (LVal.Contents.Int > 255)) WrError(1320);
        else
        {
          pErg->Typ = TempInt;
          pErg->Contents.Int = tolower(LVal.Contents.Int);
        }
        FFound = True;
      }

      else if (!strcmp(ftemp, "BITCNT"))
      {
        if (LVal.Typ != TempInt) WrError(1135);
        else
        {
          pErg->Typ = TempInt;
          pErg->Contents.Int = 0;
          for (z1 = 0; z1 < LARGEBITS; z1++)
          {
            pErg->Contents.Int += (LVal.Contents.Int & 1);
            LVal.Contents.Int = LVal.Contents.Int >> 1;
          }
        }
        FFound = True;
      }

      else if (!strcmp(ftemp, "FIRSTBIT"))
      {
        if (LVal.Typ != TempInt) WrError(1135);
        else
        {
          pErg->Typ = TempInt;
          pErg->Contents.Int = 0;
          do
          {
            if (!Odd(LVal.Contents.Int))
              pErg->Contents.Int++;
            LVal.Contents.Int = LVal.Contents.Int >> 1;
          }
          while ((pErg->Contents.Int < LARGEBITS) && (!Odd(LVal.Contents.Int)));
          if (pErg->Contents.Int >= LARGEBITS)
            pErg->Contents.Int = -1;
        }
        FFound = True;
      }

      else if (!strcmp(ftemp, "LASTBIT"))
      {
        if (LVal.Typ != TempInt) WrError(1135);
        else
        {
          pErg->Typ = TempInt;
          pErg->Contents.Int = -1;
          for (z1 = 0; z1 < LARGEBITS; z1++)
          {
            if (Odd(LVal.Contents.Int))
              pErg->Contents.Int = z1;
            LVal.Contents.Int = LVal.Contents.Int >> 1;
          }
        }
        FFound = True;
      }

      else if (!strcmp(ftemp, "BITPOS"))
      {
        if (LVal.Typ != TempInt) WrError(1135);
        else
        {
          pErg->Typ = TempInt;
          if (!SingleBit(LVal.Contents.Int, &pErg->Contents.Int))
          {
            pErg->Contents.Int = -1;
            WrError(1540);
          }
        }
        FFound = True;
      }

      /* variable Integer/Float-Funktionen */

      else if (!strcmp(ftemp, "ABS"))
      {
        switch (pErg->Typ = LVal.Typ)
        {
          case TempInt:
            pErg->Contents.Int = (LVal.Contents.Int < 0) ? -LVal.Contents.Int : LVal.Contents.Int;
            break;
          case TempFloat:
            pErg->Contents.Float = fabs(LVal.Contents.Float);
            break;
          default:
            break;
        }
        FFound = True;
      }

      else if (!strcmp(ftemp, "SGN"))
      {
        pErg->Typ = TempInt;
        switch (LVal.Typ)
        {
          case TempInt:
            if (LVal.Contents.Int < 0)
              pErg->Contents.Int = -1;
            else if (LVal.Contents.Int > 0)
              pErg->Contents.Int = 1;
            else
              pErg->Contents.Int = 0;
            break;
          case TempFloat:
            if (LVal.Contents.Float < 0)
              pErg->Contents.Int = -1;
            else if (LVal.Contents.Float > 0)
              pErg->Contents.Int = 1;
            else
              pErg->Contents.Int = 0;
            break;
          default:
            break;
        }
        FFound = True;
      }

      /* Funktionen Float und damit auch Int */

      if (!FFound)
      {
        /* Typkonvertierung */

        EvalExpression_ChgFloat(&LVal);
        pErg->Typ = TempFloat;

        /* Integerwandlung */

        if (!strcmp(ftemp, "INT"))
        {
          if (fabs(LVal.Contents.Float) > MaxLargeInt)
          {
            pErg->Typ = TempNone;
            WrError(1320);
          }
          else
          {
            pErg->Typ = TempInt;
            pErg->Contents.Int = (LargeInt) floor(LVal.Contents.Float);
          }
        }

        /* Quadratwurzel */

        else if (!strcmp(ftemp, "SQRT"))
        {
          if (LVal.Contents.Float < 0)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else pErg->Contents.Float = sqrt(LVal.Contents.Float);
        }

        /* trigonometrische Funktionen */

        else if (!strcmp(ftemp, "SIN"))
          pErg->Contents.Float = sin(LVal.Contents.Float);
        else if (!strcmp(ftemp, "COS"))
          pErg->Contents.Float = cos(LVal.Contents.Float);
        else if (!strcmp(ftemp, "TAN"))
        {
          if (cos(LVal.Contents.Float) == 0.0)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else
            pErg->Contents.Float = tan(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "COT"))
        {
          if ((FVal = sin(LVal.Contents.Float)) == 0.0)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else
            pErg->Contents.Float = cos(LVal.Contents.Float)/FVal;
        }

        /* inverse trigonometrische Funktionen */

        else if (!strcmp(ftemp, "ASIN"))
        {
          if (fabs(LVal.Contents.Float) > 1)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else
            pErg->Contents.Float = asin(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "ACOS"))
        {
          if (fabs(LVal.Contents.Float) > 1)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else
            pErg->Contents.Float = acos(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "ATAN"))
          pErg->Contents.Float = atan(LVal.Contents.Float);
        else if (!strcmp(ftemp, "ACOT"))
          pErg->Contents.Float = M_PI / 2 - atan(LVal.Contents.Float);

        /* exponentielle & hyperbolische Funktionen */

        else if (!strcmp(ftemp, "EXP"))
        {
          if (LVal.Contents.Float>709)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = exp(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "ALOG"))
        {
          if (LVal.Contents.Float>308)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = exp(LVal.Contents.Float * log(10.0));
        }
        else if (!strcmp(ftemp, "ALD"))
        {
          if (LVal.Contents.Float>1022)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = exp(LVal.Contents.Float * log(2.0));
        }
        else if (!strcmp(ftemp, "SINH"))
        {
          if (LVal.Contents.Float > 709)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = sinh(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "COSH"))
        {
          if (LVal.Contents.Float > 709)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = cosh(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "TANH"))
        {
          if (LVal.Contents.Float > 709)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = tanh(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "COTH"))
        {
          if (LVal.Contents.Float > 709)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else if ((FVal = tanh(LVal.Contents.Float)) == 0.0)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else 
            pErg->Contents.Float = 1.0 / FVal;
        }

        /* logarithmische & inverse hyperbolische Funktionen */

        else if (!strcmp(ftemp, "LN"))
        {
          if (LVal.Contents.Float <= 0)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else
            pErg->Contents.Float = log(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "LOG"))
        {
          if (LVal.Contents.Float <= 0)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else
            pErg->Contents.Float = log10(LVal.Contents.Float);
        }
        else if (!strcmp(ftemp, "LD"))
        {
          if (LVal.Contents.Float <= 0)
          {
            pErg->Typ = TempNone;
            WrError(1870);
          }
          else
            pErg->Contents.Float = log(LVal.Contents.Float) / log(2.0);
        }
        else if (!strcmp(ftemp, "ASINH"))
          pErg->Contents.Float = log(LVal.Contents.Float+sqrt(LVal.Contents.Float*LVal.Contents.Float + 1));
        else if (!strcmp(ftemp, "ACOSH"))
        {
          if (LVal.Contents.Float < 1)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else pErg->Contents.Float = log(LVal.Contents.Float+sqrt(LVal.Contents.Float*LVal.Contents.Float - 1));
        }
        else if (!strcmp(ftemp, "ATANH"))
        {
          if (fabs(LVal.Contents.Float) >= 1)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = 0.5 * log((1 + LVal.Contents.Float) / (1 - LVal.Contents.Float));
        }
        else if (!strcmp(ftemp, "ACOTH"))
        {
          if (fabs(LVal.Contents.Float) <= 1)
          {
            pErg->Typ = TempNone;
            WrError(1880);
          }
          else
            pErg->Contents.Float = 0.5 * log((LVal.Contents.Float + 1) / (LVal.Contents.Float - 1));
        }

        /* nix gefunden ? */

        else
        {
          WrXError(1860, ftemp);
          pErg->Typ = TempNone;
        }
      }
    }
    LEAVE;
  }

  /* nichts dergleichen, dann einfaches Symbol: urspruenglichen Wert wieder
     herstellen, dann Pruefung auf $$-tempraere Symbole */

  strmaxcpy(Copy, stemp, 255);
  KillPrefBlanks(Copy);
  KillPostBlanks(Copy);

  ChkTmp1(Copy, FALSE);

  /* interne Symbole ? */

  if (!strcasecmp(Copy, "MOMFILE"))
  {
    pErg->Typ = TempString;
    CString2DynString(&pErg->Contents.Ascii, CurrFileName);
    LEAVE;
  }

  if (!strcasecmp(Copy, "MOMLINE"))
  {
    pErg->Typ = TempInt;
    pErg->Contents.Int = CurrLine;
    LEAVE;
  }

  if (!strcasecmp(Copy, "MOMPASS"))
  {
    pErg->Typ = TempInt;
    pErg->Contents.Int = PassNo;
    LEAVE;
  }

  if (!strcasecmp(Copy, "MOMSECTION"))
  {
    pErg->Typ  =  TempString;
    CString2DynString(&pErg->Contents.Ascii, GetSectionName(MomSectionHandle));
    LEAVE;
  }

  if (!strcasecmp(Copy, "MOMSEGMENT"))
  {
    pErg->Typ = TempString;
    CString2DynString(&pErg->Contents.Ascii, SegNames[ActPC]);
    LEAVE;
  }

  if (!ExpandSymbol(Copy))
    LEAVE;

  KlPos = strchr(Copy, '[');
  if (KlPos)
  {
    Save = (*KlPos);
    *KlPos = '\0';
  }
  OK = ChkSymbName(Copy);
  if (KlPos)
    *KlPos = Save;
  if (!OK)
  {
    WrXError(1020, Copy);
    LEAVE;
  }

  Ptr = FindLocNode(Copy, TempAll);
  if (!Ptr)
    Ptr = FindNode(Copy, TempAll);
  if (Ptr)
  {
    switch (pErg->Typ = Ptr->SymWert.Typ)
    {
      case TempInt:
        pErg->Contents.Int = Ptr->SymWert.Contents.IWert;
        break;
      case TempFloat:
        pErg->Contents.Float = Ptr->SymWert.Contents.FWert;
        break;
      case TempString:
        pErg->Contents.Ascii.Length = 0;
        DynStringAppend(&pErg->Contents.Ascii, Ptr->SymWert.Contents.String.Contents, Ptr->SymWert.Contents.String.Length);
        break;
      default:
        break;
    }
    if (pErg->Typ != TempNone)
    {
      pErg->Relocs = DupRelocs(Ptr->Relocs);
      pErg->Flags = Ptr->Flags;
    }
    if (Ptr->SymType != 0)
      TypeFlag |= (1 << Ptr->SymType);
    if ((Ptr->SymSize != -1) && (SizeFlag == -1))
      SizeFlag = Ptr->SymSize;
    if (!Ptr->Defined)
    {
      if (Repass)
        SymbolQuestionable = True;
      UsesForwards = True;
    }
    Ptr->Used = True;
    LEAVE;
  }

  /* Symbol evtl. im ersten Pass unbekannt */

  if (PassNo <= MaxSymPass)
  {
    pErg->Typ = TempInt;
    pErg->Contents.Int = EProgCounter();
    Repass = True;
    if ((MsgIfRepass) && (PassNo >= PassNoForMessage))
      WrXError(170, Copy);
    FirstPassUnknown = True;
  }

  /* alles war nix, Fehler */

  else
    WrXError(1010, Copy);

func_exit:

  if (LVal.Relocs)
    FreeRelocs(&LVal.Relocs);
  if (RVal.Relocs)
    FreeRelocs(&RVal.Relocs);
}

static const int TypeNums[] =
{
  0, Num_OpTypeInt, Num_OpTypeFloat, 0, Num_OpTypeString, 0, 0, 0
};

LargeInt EvalIntExpressionWithFlags(const char *pExpr, IntType Type, Boolean *pResult, tSymbolFlags *pFlags)
{
  TempResult t;
  LargeInt Result;

  *pResult = False;
  TypeFlag = 0;
  SizeFlag = -1;
  UsesForwards = False;
  SymbolQuestionable = False;
  FirstPassUnknown = False;
  if (pFlags)
    *pFlags = 0;

  EvalExpression(pExpr, &t);
  SetRelocs(t.Relocs);
  switch (t.Typ)
  {
    case TempInt:
      Result = t.Contents.Int;
      if (pFlags)
        *pFlags = t.Flags;
      break;
    case TempString:
    {
      int l = t.Contents.Ascii.Length;

      if ((l > 0) && (l <= 4))
      {
        char *pRun;
        Byte Digit;

        Result = 0;
        for (pRun = t.Contents.Ascii.Contents;
             pRun < t.Contents.Ascii.Contents + l;
             pRun++)
        {
          Digit = (usint) *pRun;
          Result = (Result << 8) | CharTransTable[Digit & 0xff];
        }
        break;
      }
    }
    default:
      if (t.Typ != TempNone)
      {
        char Msg[50];

        sprintf(Msg, "%s %s %s %s",
                getmessage(Num_ErrMsgExpected), getmessage(Num_OpTypeInt),
                getmessage(Num_ErrMsgButGot), getmessage(TypeNums[t.Typ]));
        WrXError(1135, Msg);
      }
      FreeRelocs(&LastRelocs);
      return -1;
  }

  if (FirstPassUnknown)
    Result &= IntTypeDefs[(int)Type].Mask;

  if (!RangeCheck(Result, Type))
  {
    if (HardRanges)
    {
      FreeRelocs(&LastRelocs);
      WrError(1320);
      return -1;
    }
    else
    {
      WrError(260);
      *pResult = True;
      return Result & IntTypeDefs[(int)Type].Mask;
    }
  }
  else
  {
    *pResult = True;
    return Result;
  }
}

LargeInt EvalIntDisplacement(char *pExpr, IntType Type, Boolean *pResult)
{
  char SaveLeft;
  LargeInt Result;

  /* save the original character.  We assume there is space to the left! */

  pExpr--;
  SaveLeft = *pExpr;
  *pExpr = '0';

  /* evaluate */

  Result = EvalIntExpression(pExpr, Type, pResult);

  /* restore character */

  *pExpr = SaveLeft;

  return Result;
}

Double EvalFloatExpression(const char *pExpr, FloatType Type, Boolean *pResult)
{
  TempResult t;

  *pResult = False;
  TypeFlag = 0; SizeFlag = -1;
  UsesForwards = False;
  SymbolQuestionable = False;
  FirstPassUnknown = False;

  EvalExpression(pExpr, &t);
  switch (t.Typ)
  {
    case TempNone:
      return -1;
    case TempInt:
      t.Contents.Float = t.Contents.Int;
      break;
    case TempString:
    {
      char Msg[50];
 
      sprintf(Msg, "%s %s %s %s",
              getmessage(Num_ErrMsgExpected), getmessage(Num_OpTypeFloat),
              getmessage(Num_ErrMsgButGot), getmessage(Num_OpTypeString));
      WrXError(1135, Msg);
      return -1;
    }
    default:
      break;
  }

  if (!FloatRangeCheck(t.Contents.Float, Type))
  {
    WrError(1320);
    return -1;
  }

  *pResult = True;
  return t.Contents.Float;
}

void EvalStringExpression(const char *pExpr, Boolean *pResult, char *pEvalResult)
{
  TempResult t;

  *pResult = False;
  TypeFlag = 0;
  SizeFlag = -1;
  UsesForwards = False;
  SymbolQuestionable = False;
  FirstPassUnknown = False;

  EvalExpression(pExpr, &t);
  if (t.Typ != TempString)
  {
    *pEvalResult = '\0';
    if (t.Typ != TempNone)
    {
      char Msg[50];

      sprintf(Msg, "%s %s %s %s",
              getmessage(Num_ErrMsgExpected), getmessage(Num_OpTypeString),
              getmessage(Num_ErrMsgButGot), getmessage(TypeNums[t.Typ]));
      WrXError(1135, Msg);
    }
  }
  else
  {
    DynString2CString(pEvalResult, &t.Contents.Ascii, 255);
    *pResult = True;
  }
}


static void FreeSymbolEntry(PSymbolEntry *Node, Boolean Destroy)
{
  PCrossRef Lauf;

  if ((*Node)->Tree.Name)
  {
    free((*Node)->Tree.Name);
   (*Node)->Tree.Name = NULL;
  }

  if ((*Node)->SymWert.Typ == TempString)
    free((*Node)->SymWert.Contents.String.Contents);

  while ((*Node)->RefList)
  {
    Lauf = (*Node)->RefList->Next;
    free((*Node)->RefList);
    (*Node)->RefList = Lauf;
  }

  FreeRelocs(&((*Node)->Relocs));

  if (Destroy)
  {
    free(*Node);
    Node = NULL;
  }
}

static char *serr, *snum;
typedef struct
{
  Boolean MayChange, DoCross;
} TEnterStruct, *PEnterStruct;

static Boolean SymbolAdder(PTree *PDest, PTree Neu, void *pData)
{
  PSymbolEntry NewEntry = (PSymbolEntry)Neu, *Node;
  PEnterStruct EnterStruct = (PEnterStruct) pData;

  /* added to an empty leaf ? */

  if (!PDest)
  {
    NewEntry->Defined = True;
    NewEntry->Used = False;
    NewEntry->Changeable = EnterStruct->MayChange;
    NewEntry->RefList = NULL;
    if (EnterStruct->DoCross)
    {
      NewEntry->FileNum = GetFileNum(CurrFileName);
      NewEntry->LineNum = CurrLine;
    }
    return True;
  }

  /* replace en entry: check for validity */

  Node = (PSymbolEntry*)PDest;

  /* tried to redefine a symbol with EQU ? */

  if (((*Node)->Defined) && (!(*Node)->Changeable) && (!EnterStruct->MayChange))
  {
    strmaxcpy(serr, (*Node)->Tree.Name, 255);
    if (EnterStruct->DoCross)
    {
      sprintf(snum, ",%s %s:%ld", getmessage(Num_PrevDefMsg),
              GetFileName((*Node)->FileNum), (long)((*Node)->LineNum));
      strmaxcat(serr, snum, 255);
    }
    WrXError(1000, serr);
    FreeSymbolEntry(&NewEntry, TRUE);
    return False;
  }

  /* tried to reassign a constant (EQU) a value with SET and vice versa ? */

  else if ( ((*Node)->Defined) && (EnterStruct->MayChange != (*Node)->Changeable) )
  {
    strmaxcpy(serr, (*Node)->Tree.Name, 255);
    if (EnterStruct->DoCross)
    {
      sprintf(snum, ",%s %s:%ld", getmessage(Num_PrevDefMsg),
              GetFileName((*Node)->FileNum), (long)((*Node)->LineNum));
      strmaxcat(serr, snum, 255);
    }
    WrXError((*Node)->Changeable ? 2035 : 2030, serr);
    FreeSymbolEntry(&NewEntry, TRUE);
    return False;
  }

  else
  {
    if (!EnterStruct->MayChange)
    {
      if ((NewEntry->SymWert.Typ != (*Node)->SymWert.Typ)
       || ((NewEntry->SymWert.Typ == TempString) && (strlencmp(NewEntry->SymWert.Contents.String.Contents, NewEntry->SymWert.Contents.String.Length, (*Node)->SymWert.Contents.String.Contents, (*Node)->SymWert.Contents.String.Length)))
       || ((NewEntry->SymWert.Typ == TempFloat ) && (NewEntry->SymWert.Contents.FWert != (*Node)->SymWert.Contents.FWert))
       || ((NewEntry->SymWert.Typ == TempInt   ) && (NewEntry->SymWert.Contents.IWert != (*Node)->SymWert.Contents.IWert)))
       {
         if ((!Repass) && (JmpErrors>0))
         {
           if (ThrowErrors)
             ErrorCount -= JmpErrors;
           JmpErrors = 0;
         }
         Repass = True;
         if ((MsgIfRepass) && (PassNo >= PassNoForMessage))
         {
           strmaxcpy(serr, Neu->Name, 255);
           if (Neu->Attribute != -1)
           {
             strmaxcat(serr, "[", 255);
             strmaxcat(serr, GetSectionName(Neu->Attribute), 255);
             strmaxcat(serr, "]", 255);
           }
           WrXError(80, serr);
         }
       }
    }
    if (EnterStruct->DoCross)
    {
      NewEntry->LineNum = (*Node)->LineNum;
      NewEntry->FileNum = (*Node)->FileNum;
    }
    NewEntry->RefList = (*Node)->RefList;
    (*Node)->RefList = NULL;
    NewEntry->Defined = True;
    NewEntry->Used = (*Node)->Used;
    NewEntry->Changeable = EnterStruct->MayChange;
    FreeSymbolEntry(Node, False);
    return True;
  }
}

static void EnterLocSymbol(PSymbolEntry Neu)
{
  TEnterStruct EnterStruct;
  PTree TreeRoot;

  Neu->Tree.Attribute = MomLocHandle;
  if (!CaseSensitive)
    NLS_UpString(Neu->Tree.Name);
  EnterStruct.MayChange = EnterStruct.DoCross = FALSE;
  TreeRoot = &FirstLocSymbol->Tree;
  EnterTree(&TreeRoot, (&Neu->Tree), SymbolAdder, &EnterStruct);
  FirstLocSymbol = (PSymbolEntry)TreeRoot;
}

static void EnterSymbol_Search(PForwardSymbol *Lauf, PForwardSymbol *Prev,
                               PForwardSymbol **RRoot, PSymbolEntry Neu,
                               PForwardSymbol *Root, Byte ResCode, Byte *SearchErg)
{
  *Lauf = (*Root);
  *Prev = NULL;
  *RRoot = Root;
  while ((*Lauf) && (strcmp((*Lauf)->Name, Neu->Tree.Name)))
  {
    *Prev = (*Lauf);
    *Lauf = (*Lauf)->Next;
  }
  if (*Lauf)
    *SearchErg = ResCode;
}

static void EnterSymbol(PSymbolEntry Neu, Boolean MayChange, LongInt ResHandle)
{
  PForwardSymbol Lauf, Prev;
  PForwardSymbol *RRoot;
  Byte SearchErg;
  String CombName;
  PSaveSection RunSect;
  LongInt MSect;
  PSymbolEntry Copy;
  TEnterStruct EnterStruct;
  PTree TreeRoot = &(FirstSymbol->Tree);

  if (!CaseSensitive)
    NLS_UpString(Neu->Tree.Name);

  SearchErg = 0;
  EnterStruct.MayChange = MayChange;
  EnterStruct.DoCross = MakeCrossList;
  Neu->Tree.Attribute = (ResHandle == -2) ? MomSectionHandle : ResHandle;
  if ((SectionStack) && (Neu->Tree.Attribute == MomSectionHandle))
  {
    EnterSymbol_Search(&Lauf, &Prev, &RRoot, Neu, &(SectionStack->LocSyms),
                       1, &SearchErg);
    if (!Lauf)
      EnterSymbol_Search(&Lauf, &Prev, &RRoot, Neu,
                         &(SectionStack->GlobSyms), 2, &SearchErg);
    if (!Lauf)
      EnterSymbol_Search(&Lauf, &Prev, &RRoot, Neu,
                         &(SectionStack->ExportSyms), 3, &SearchErg);
    if (SearchErg == 2)
      Neu->Tree.Attribute = Lauf->DestSection;
    if (SearchErg == 3)
    {
      strmaxcpy(CombName, Neu->Tree.Name, 255);
      RunSect = SectionStack;
      MSect = MomSectionHandle;
      while ((MSect != Lauf->DestSection) && (RunSect))
      {
        strmaxprep(CombName, "_", 255);
        strmaxprep(CombName, GetSectionName(MSect), 255);
        MSect = RunSect->Handle;
        RunSect = RunSect->Next;
      }
      Copy = (PSymbolEntry) calloc(1, sizeof(TSymbolEntry));
      *Copy = (*Neu);
      Copy->Tree.Name = strdup(CombName);
      Copy->Tree.Attribute = Lauf->DestSection;
      Copy->Relocs = DupRelocs(Neu->Relocs);
      if (Copy->SymWert.Typ == TempString)
      {
        Copy->SymWert.Contents.String.Contents = (char*)malloc(Neu->SymWert.Contents.String.Length);
        memcpy(Copy->SymWert.Contents.String.Contents, Neu->SymWert.Contents.String.Contents,
               Copy->SymWert.Contents.String.Length = Neu->SymWert.Contents.String.Length);
      }
      EnterTree(&TreeRoot, &(Copy->Tree), SymbolAdder, &EnterStruct);
    }
    if (Lauf)
    {
      free(Lauf->Name);
      free(Lauf->pErrorPos);
      if (!Prev)
        *RRoot = Lauf->Next;
      else
        Prev->Next = Lauf->Next;
      free(Lauf);
    }
  }
  EnterTree(&TreeRoot, &(Neu->Tree), SymbolAdder, &EnterStruct);
  FirstSymbol = (PSymbolEntry)TreeRoot;
}

void PrintSymTree(char *Name)
{
  fprintf(Debug, "---------------------\n");
  fprintf(Debug, "Enter Symbol %s\n\n", Name);
  PrintSymbolTree();
  PrintSymbolDepth();
}

void EnterIntSymbolWithFlags(const char *Name_O, LargeInt Wert, Byte Typ, Boolean MayChange, tSymbolFlags Flags)
{
  PSymbolEntry Neu;
  LongInt DestHandle;
  String Name;

  strmaxcpy(Name, Name_O, 255);
  if (!ExpandSymbol(Name))
    return;
  if (!GetSymSection(Name, &DestHandle))
    return;
  (void)ChkTmp(Name, TRUE);
  if (!ChkSymbName(Name))
  {
    WrXError(1020, Name);
    return;
  }

  Neu = (PSymbolEntry) calloc(1, sizeof(TSymbolEntry));
  Neu->Tree.Name = strdup(Name);
  Neu->SymWert.Typ = TempInt;
  Neu->SymWert.Contents.IWert = Wert;
  Neu->SymType = Typ;
  Neu->Flags = Flags;
  Neu->SymSize = -1;
  Neu->RefList = NULL;
  Neu->Relocs = NULL;

  if ((MomLocHandle == -1) || (DestHandle != -2))
  {
    EnterSymbol(Neu, MayChange, DestHandle);
    if (MakeDebug)
      PrintSymTree(Name);
  }
  else
    EnterLocSymbol(Neu);
}

void EnterExtSymbol(const char *Name_O, LargeInt Wert, Byte Typ, Boolean MayChange)
{
  PSymbolEntry Neu;
  LongInt DestHandle;
  String Name;

  strmaxcpy(Name, Name_O, 255);
  if (!ExpandSymbol(Name))
    return;
  if (!GetSymSection(Name, &DestHandle))
    return;
  if (!ChkSymbName(Name))
  {
    WrXError(1020, Name);
    return;
  }

  Neu = (PSymbolEntry) calloc(1, sizeof(TSymbolEntry));
  Neu->Tree.Name = strdup(Name);
  Neu->SymWert.Typ = TempInt;
  Neu->SymWert.Contents.IWert = Wert;
  Neu->SymType = Typ;
  Neu->SymSize = -1;
  Neu->RefList = NULL;
  Neu->Relocs = (PRelocEntry) malloc(sizeof(TRelocEntry));
  Neu->Relocs->Next = NULL;
  Neu->Relocs->Ref = strdup(Name);
  Neu->Relocs->Add = True;

  if ((MomLocHandle == -1) || (DestHandle != -2))
  {
    EnterSymbol(Neu, MayChange, DestHandle);
    if (MakeDebug)
      PrintSymTree(Name);
  }
  else
    EnterLocSymbol(Neu);
}

void EnterRelSymbol(const char *Name_O, LargeInt Wert, Byte Typ, Boolean MayChange)
{
  PSymbolEntry Neu;
  LongInt DestHandle;
  String Name;

  strmaxcpy(Name, Name_O, 255);
  if (!ExpandSymbol(Name))
    return;
  if (!GetSymSection(Name, &DestHandle))
    return;
  if (!ChkSymbName(Name))
  {
    WrXError(1020, Name);
    return;
  }

  Neu = (PSymbolEntry) calloc(1, sizeof(TSymbolEntry));
  Neu->Tree.Name = strdup(Name);
  Neu->SymWert.Typ = TempInt;
  Neu->SymWert.Contents.IWert = Wert;
  Neu->SymType = Typ;
  Neu->SymSize = -1;
  Neu->RefList = NULL;
  Neu->Relocs = (PRelocEntry) malloc(sizeof(TRelocEntry));
  Neu->Relocs->Next = NULL;
  Neu->Relocs->Ref = strdup(RelName_SegStart);
  Neu->Relocs->Add = True;

  if ((MomLocHandle == -1) || (DestHandle != -2))
  {
    EnterSymbol(Neu, MayChange, DestHandle);
    if (MakeDebug)
      PrintSymTree(Name);
  }
  else
    EnterLocSymbol(Neu);
}

void EnterFloatSymbol(const char *Name_O, Double Wert, Boolean MayChange)
{
  PSymbolEntry Neu;
  LongInt DestHandle;
  String Name;

  strmaxcpy(Name, Name_O, 255);
  if (!ExpandSymbol(Name))
    return;
  if (!GetSymSection(Name, &DestHandle))
    return;
  (void)ChkTmp(Name, TRUE);
  if (!ChkSymbName(Name))
  {
    WrXError(1020, Name);
    return;
  }
  Neu = (PSymbolEntry) calloc(1, sizeof(TSymbolEntry));
  Neu->Tree.Name = strdup(Name);
  Neu->SymWert.Typ = TempFloat;
  Neu->SymWert.Contents.FWert = Wert;
  Neu->SymType = 0;
  Neu->SymSize = -1;
  Neu->RefList = NULL;
  Neu->Relocs = NULL;

  if ((MomLocHandle == -1) || (DestHandle != -2))
  {
    EnterSymbol(Neu, MayChange, DestHandle);
    if (MakeDebug)
      PrintSymTree(Name);
  }
  else
    EnterLocSymbol(Neu);
}

void EnterDynStringSymbol(const char *Name_O, const tDynString *pValue, Boolean MayChange)
{
  PSymbolEntry Neu;
  LongInt DestHandle;
  String Name;

  strmaxcpy(Name, Name_O, 255);
  if (!ExpandSymbol(Name))
    return;
  if (!GetSymSection(Name, &DestHandle))
    return;
  (void)ChkTmp(Name, TRUE);
  if (!ChkSymbName(Name))
  {
    WrXError(1020, Name);
    return;
  }
  Neu = (PSymbolEntry) calloc(1, sizeof(TSymbolEntry));
  Neu->Tree.Name = strdup(Name);
  Neu->SymWert.Contents.String.Contents = (char*)malloc(pValue->Length);
  memcpy(Neu->SymWert.Contents.String.Contents, pValue->Contents, pValue->Length);
  Neu->SymWert.Contents.String.Length = pValue->Length;
  Neu->SymWert.Typ = TempString;
  Neu->SymType = 0;
  Neu->SymSize = -1;
  Neu->RefList = NULL;
  Neu->Relocs = NULL;

  if ((MomLocHandle == -1) || (DestHandle != -2))
  {
    EnterSymbol(Neu, MayChange, DestHandle);
    if (MakeDebug)
      PrintSymTree(Name);
  }
  else
    EnterLocSymbol(Neu);
}

void EnterStringSymbol(const char *Name_O, const char *pValue, Boolean MayChange)
{
  tDynString DynString;

  DynString.Length = 0;
  DynStringAppend(&DynString, pValue, -1);
  EnterDynStringSymbol(Name_O, &DynString, MayChange);
}

static void AddReference(PSymbolEntry Node)
{
  PCrossRef Lauf, Neu;

  /* Speicher belegen */

  Neu = (PCrossRef) malloc(sizeof(TCrossRef));
  Neu->LineNum = CurrLine;
  Neu->OccNum = 1;
  Neu->Next = NULL;

  /* passende Datei heraussuchen */

  Neu->FileNum = GetFileNum(CurrFileName);

  /* suchen, ob Eintrag schon existiert */

  Lauf = Node->RefList;
  while ((Lauf)
     && ((Lauf->FileNum != Neu->FileNum) || (Lauf->LineNum != Neu->LineNum)))
   Lauf = Lauf->Next;

  /* schon einmal in dieser Datei in dieser Zeile aufgetaucht: nur Zaehler
    rauf: */

  if (Lauf)
  {
    Lauf->OccNum++;
   free(Neu);
  }

  /* ansonsten an Kettenende anhaengen */

  else if (!Node->RefList) Node->RefList = Neu;

  else
  {
    Lauf = Node->RefList;
    while (Lauf->Next)
      Lauf = Lauf->Next;
    Lauf->Next = Neu;
  }
}

static PSymbolEntry FindNode_FNode(char *Name, TempType SearchType, LongInt Handle)
{
  PSymbolEntry Lauf;

  Lauf = (PSymbolEntry) SearchTree((PTree)FirstSymbol, Name, Handle);

  if (Lauf)
  {
    if (Lauf->SymWert.Typ & SearchType)
    {
      if (MakeCrossList && DoRefs)
        AddReference(Lauf);
    }
    else
      Lauf = NULL;
  }

  return Lauf;
}

static Boolean FindNode_FSpec(char *Name, PForwardSymbol Root)
{
  while ((Root) && (strcmp(Root->Name, Name)))
    Root = Root->Next;
  return (Root != NULL);
}

static PSymbolEntry FindNode(const char *Name_O, TempType SearchType)
{
  PSaveSection Lauf;
  LongInt DestSection;
  PSymbolEntry Result = NULL;
  String Name;

  strmaxcpy(Name, Name_O, 255);
  ChkTmp3(Name, FALSE);

  if (!GetSymSection(Name, &DestSection))
    return NULL;

  if (!CaseSensitive)
    NLS_UpString(Name);

  if (SectionStack)
    if (PassNo <= MaxSymPass)
      if (FindNode_FSpec(Name, SectionStack->LocSyms)) DestSection = MomSectionHandle;

  if (DestSection == -2)
  {
    Result = FindNode_FNode(Name, SearchType, MomSectionHandle);
    if (Result)
      return Result;
    Lauf = SectionStack;
    while (Lauf)
    {
      Result = FindNode_FNode(Name, SearchType, Lauf->Handle);
      if (Result)
        break;
      Lauf = Lauf->Next;
    }
  }
  else
    Result = FindNode_FNode(Name, SearchType, DestSection);

  return Result;
}

static PSymbolEntry FindLocNode_FNode(char *Name, TempType SearchType, LongInt Handle)
{
  PSymbolEntry Lauf;

  Lauf = (PSymbolEntry) SearchTree((PTree)FirstLocSymbol, Name, Handle);

  if (Lauf)
  {
    if (!(Lauf->SymWert.Typ & SearchType))
      Lauf = NULL;
  }

  return Lauf;
}

static PSymbolEntry FindLocNode(const char *Name_O, TempType SearchType)
{
  PLocHandle RunLocHandle;
  PSymbolEntry Result = NULL;
  String Name;

  strmaxcpy(Name, Name_O, 255);
  ChkTmp3(Name, FALSE);
  if (!CaseSensitive)
    NLS_UpString(Name);

  if (MomLocHandle == -1)
    return NULL;

  Result = FindLocNode_FNode(Name, SearchType, MomLocHandle);
  if (Result)
    return Result;

  RunLocHandle = FirstLocHandle;
  while ((RunLocHandle) && (RunLocHandle->Cont != -1))
  {
    Result = FindLocNode_FNode(Name, SearchType, RunLocHandle->Cont);
    if (Result)
      break;
    RunLocHandle = RunLocHandle->Next;
  }

  return Result;
}
/**
void SetSymbolType(char *Name, Byte NTyp)
{
  PSymbolEntry Lauf;
  Boolean HRef;

  if (!ExpandSymbol(Name))
    return;
  HRef = DoRefs;
  DoRefs =F alse;
  Lauf = FindLocNode(Name, TempInt);
  if (!Lauf)
    Lauf = FindNode(Name, TempInt);
  if (Lauf)
    Lauf->SymType = NTyp;
  DoRefs = HRef;
}
**/

Boolean GetIntSymbol(const char *Name, LargeInt *Wert, PRelocEntry *Relocs)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;
  Lauf = FindLocNode(NName, TempInt);
  if (!Lauf)
    Lauf = FindNode(NName, TempInt);
  if (Lauf)
  {
    *Wert = Lauf->SymWert.Contents.IWert;
    if (Relocs)
      *Relocs = Lauf->Relocs;
    if (Lauf->SymType != 0)
      TypeFlag |= (1 << Lauf->SymType);
    if ((Lauf->SymSize != -1) && (SizeFlag != -1))
      SizeFlag = Lauf->SymSize;
    Lauf->Used = True;
  }
  else
  {
    if (PassNo > MaxSymPass)
      WrXError(1010, Name);
    else
      FirstPassUnknown = True;
    *Wert = EProgCounter();
  }
  return (Lauf != NULL);
}

Boolean GetFloatSymbol(const char *Name, Double *Wert)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;
  Lauf = FindLocNode(Name, TempFloat);
  if (!Lauf)
    Lauf = FindNode(NName, TempFloat);
  if (Lauf)
  {
    *Wert = Lauf->SymWert.Contents.FWert;
    Lauf->Used = True;
  }
  else
  {
    if (PassNo>MaxSymPass)
      WrXError(1010, Name);
    *Wert = 0;
  }
  return (Lauf != NULL);
}

Boolean GetStringSymbol(const char *Name, char *Wert)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;
  Lauf = FindLocNode(NName, TempString);
  if (!Lauf)
    Lauf = FindNode(NName, TempString);
  if (Lauf)
  {
    memcpy(Wert, Lauf->SymWert.Contents.String.Contents, Lauf->SymWert.Contents.String.Length);
    Wert[Lauf->SymWert.Contents.String.Length] = '\0';
    Lauf->Used = True;
  }
  else
  {
    if (PassNo > MaxSymPass)
      WrXError(1010, Name);
    *Wert = '\0';
  }
  return (Lauf != NULL);
}

void SetSymbolSize(const char *Name, ShortInt Size)
{
  PSymbolEntry Lauf;
  Boolean HRef;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return;
  HRef = DoRefs;
  DoRefs = False;
  Lauf = FindLocNode(NName, TempInt);
  if (!Lauf)
    Lauf = FindNode(Name, TempInt);
  if (Lauf)
    Lauf->SymSize = Size;
  DoRefs = HRef;
}

ShortInt GetSymbolSize(const char *Name)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return -1;
  Lauf = FindLocNode(NName, TempInt);
  if (!Lauf)
    Lauf = FindNode(NName, TempInt);
  return ((Lauf) ? Lauf->SymSize : -1);
}

Boolean IsSymbolFloat(const char *Name)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;

  Lauf = FindLocNode(NName, TempFloat);
  if (!Lauf)
    Lauf = FindNode(NName, TempFloat);
  return ((Lauf) && (Lauf->SymWert.Typ == TempFloat));
}

Boolean IsSymbolString(const char *Name)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;

  Lauf = FindLocNode(NName, TempString);
  if (!Lauf)
    Lauf = FindNode(NName, TempString);
  return ((Lauf) && (Lauf->SymWert.Typ == TempString));
}

Boolean IsSymbolDefined(const char *Name)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;

  Lauf = FindLocNode(NName, TempAll);
  if (!Lauf)
    Lauf = FindNode(NName, TempAll);
  return ((Lauf) && (Lauf->Defined));
}

Boolean IsSymbolUsed(const char *Name)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;

  Lauf = FindLocNode(NName, TempAll);
  if (!Lauf)
    Lauf = FindNode(NName, TempAll);
  return ((Lauf) && (Lauf->Used));
}

Boolean IsSymbolChangeable(const char *Name)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return False;

  Lauf = FindLocNode(NName, TempAll);
  if (!Lauf)
    Lauf = FindNode(NName, TempAll);
  return ((Lauf) && (Lauf->Changeable));
}

Integer GetSymbolType(const char *Name)
{
  PSymbolEntry Lauf;
  String NName;

  strmaxcpy(NName, Name, 255);
  if (!ExpandSymbol(NName))
    return -1;

  Lauf = FindLocNode(Name, TempAll);
  if (!Lauf)
    Lauf = FindNode(Name, TempAll);
  return Lauf ? Lauf->SymType : -1;
}

static void ConvertSymbolVal(SymbolVal *Inp, TempResult *Outp)
{
  switch (Outp->Typ = Inp->Typ)
  {
    case TempInt:
      Outp->Contents.Int = Inp->Contents.IWert;
      break;
    case TempFloat:
      Outp->Contents.Float = Inp->Contents.FWert;
      break;
    case TempString:
      Outp->Contents.Ascii.Length = 0;
      DynStringAppend(&Outp->Contents.Ascii, Inp->Contents.String.Contents, Inp->Contents.String.Length);
      break;
    default:
      break;
  }
}

typedef struct
{
  int Width, cwidth;
  LongInt Sum, USum;
  String Zeilenrest;
} TListContext;

static void PrintSymbolList_AddOut(char *s, char *Zeilenrest, int Width)
{
  if ((int)(strlen(s) + strlen(Zeilenrest)) > Width)
  {
    Zeilenrest[strlen(Zeilenrest) - 1] = '\0';
    WrLstLine(Zeilenrest);
    strmaxcpy(Zeilenrest, s, 255);
  }
  else
    strmaxcat(Zeilenrest, s, 255);
}

static void PrintSymbolList_PNode(PTree Tree, void *pData)
{
  PSymbolEntry Node = (PSymbolEntry) Tree;
  TListContext *pContext = (TListContext*) pData;
  String s1, sh;
  int l1, nBlanks;
  TempResult t;

  ConvertSymbolVal(&(Node->SymWert), &t);
  StrSym(&t, False, s1, sizeof(s1));

  strmaxcpy(sh, Tree->Name, 255);
  if (Tree->Attribute != -1)
  {
    strmaxcat(sh, " [", 255);
    strmaxcat(sh, GetSectionName(Tree->Attribute), 255);
    strmaxcat(sh, "]", 255);
  }
  strmaxprep(sh, (Node->Used) ? " " : "*", 255);
  l1 = (strlen(s1) + strlen(sh) + 7);
  for (nBlanks = pContext->cwidth - 1 - l1; nBlanks < 0; nBlanks += pContext->cwidth);
  strmaxprep(s1, Blanks(nBlanks), 255);
  strmaxprep(s1, " : ", 255);
  strmaxprep(s1, sh, 255);
  strmaxcat(s1, " ", 255);
  s1[l1 = strlen(s1)] = SegShorts[Node->SymType]; s1[l1 + 1] = '\0';
  strmaxcat(s1, " | ", 255);
  PrintSymbolList_AddOut(s1, pContext->Zeilenrest, pContext->Width);
  pContext->Sum++;
  if (!Node->Used)
    pContext->USum++;
}

void PrintSymbolList(void)
{
  int ActPageWidth;
  TListContext Context;

  Context.Width = (PageWidth == 0) ? 80 : PageWidth;
  NewPage(ChapDepth, True);
  WrLstLine(getmessage(Num_ListSymListHead1));
  WrLstLine(getmessage(Num_ListSymListHead2));
  WrLstLine("");

  Context.Zeilenrest[0] = '\0';
  Context.Sum = Context.USum = 0;
  ActPageWidth = (PageWidth == 0) ? 80 : PageWidth;
  Context.cwidth = ActPageWidth >> 1;
  IterTree((PTree)FirstSymbol, PrintSymbolList_PNode, &Context);
  if (Context.Zeilenrest[0] != '\0')
  {
    Context.Zeilenrest[strlen(Context.Zeilenrest) - 1] = '\0';
    WrLstLine(Context.Zeilenrest);
  }
  WrLstLine("");
  sprintf(Context.Zeilenrest, "%7lu%s",
          (unsigned long)Context.Sum,
          getmessage((Context.Sum == 1) ? Num_ListSymSumMsg : Num_ListSymSumsMsg));
  WrLstLine(Context.Zeilenrest);
  sprintf(Context.Zeilenrest, "%7lu%s",
          (unsigned long)Context.USum,
          getmessage((Context.USum == 1) ? Num_ListUSymSumMsg : Num_ListUSymSumsMsg));
  WrLstLine(Context.Zeilenrest);
  WrLstLine("");
}

typedef struct
{
  FILE *f;
  Boolean HWritten;
  int Space;
} TDebContext;

static void PrintDebSymbols_PNode(PTree Tree, void *pData)
{
  PSymbolEntry Node = (PSymbolEntry) Tree;
  TDebContext *DebContext = (TDebContext*) pData;
  int l1;
  TempResult t;
  String s;

  if (Node->SymType != DebContext->Space)
    return;

  if (!DebContext->HWritten)
  {
    fprintf(DebContext->f, "\n"); ChkIO(10004);
    fprintf(DebContext->f, "Symbols in Segment %s\n", SegNames[DebContext->Space]); ChkIO(10004);
    DebContext->HWritten = True;
  }

  fprintf(DebContext->f, "%s", Node->Tree.Name); ChkIO(10004);
  l1 = strlen(Node->Tree.Name);
  if (Node->Tree.Attribute != -1)
  {
    sprintf(s, "[%d]", (int)Node->Tree.Attribute);
    fprintf(DebContext->f, "%s", s); ChkIO(10004);
    l1 += strlen(s);
  }
  fprintf(DebContext->f, "%s ", Blanks(37 - l1)); ChkIO(10004);
  switch (Node->SymWert.Typ)
  {
    case TempInt:
      fprintf(DebContext->f, "Int    ");
      break;
    case TempFloat:
      fprintf(DebContext->f, "Float  ");
      break;
    case TempString:
      fprintf(DebContext->f, "String ");
      break;
    default:
      break;
  }
  ChkIO(10004);
  if (Node->SymWert.Typ == TempString)
  {
    errno = 0;
    l1 = fstrlenprint(DebContext->f, Node->SymWert.Contents.String.Contents, Node->SymWert.Contents.String.Length);
    ChkIO(10004);
  }
  else
  {
    ConvertSymbolVal(&(Node->SymWert), &t);
    StrSym(&t, False, s, sizeof(s));
    l1 = strlen(s);
    fprintf(DebContext->f, "%s", s); ChkIO(10004);
  }
  fprintf(DebContext->f, "%s %-3d %d\n", Blanks(25-l1), Node->SymSize, (int)Node->Used);
  ChkIO(10004);
}

void PrintDebSymbols(FILE *f)
{
  TDebContext DebContext;

  DebContext.f = f;
  for (DebContext.Space = 0; DebContext.Space < PCMax; DebContext.Space++)
  {
    DebContext.HWritten = False;
    IterTree((PTree)FirstSymbol, PrintDebSymbols_PNode, &DebContext);
  }
}

typedef struct
{
  FILE *f;
  LongInt Handle;
} TNoISymContext;

static void PrNoISection(PTree Tree, void *pData)
{
  PSymbolEntry Node = (PSymbolEntry)Tree;
  TNoISymContext *pContext = (TNoISymContext*) pData;

  if (((1 << Node->SymType) & NoICEMask) && (Node->Tree.Attribute == pContext->Handle) && (Node->SymWert.Typ == TempInt))
  {
    errno = 0; fprintf(pContext->f, "DEFINE %s 0x", Node->Tree.Name); ChkIO(10004);
    errno = 0; fprintf(pContext->f, LargeHIntFormat, Node->SymWert.Contents.IWert); ChkIO(10004);
    errno = 0; fprintf(pContext->f, "\n"); ChkIO(10004);
  }
}

void PrintNoISymbols(FILE *f)
{
  PCToken CurrSection;
  TNoISymContext Context;

  Context.f = f;
  Context.Handle = -1;
  IterTree((PTree)FirstSymbol, PrNoISection, &Context);
  Context.Handle++;
  for (CurrSection = FirstSection; CurrSection; CurrSection = CurrSection->Next)
   if (ChunkSum(&CurrSection->Usage)>0)
   {
     fprintf(f, "FUNCTION %s ", CurrSection->Name); ChkIO(10004);
     fprintf(f, LargeIntFormat, ChunkMin(&CurrSection->Usage)); ChkIO(10004);
     fprintf(f, "\n"); ChkIO(10004);
     IterTree((PTree)FirstSymbol, PrNoISection, &Context);
     Context.Handle++;
     fprintf(f, "}FUNC "); ChkIO(10004);
     fprintf(f, LargeIntFormat, ChunkMax(&CurrSection->Usage)); ChkIO(10004);
     fprintf(f, "\n"); ChkIO(10004);
   }
}

void PrintSymbolTree(void)
{
  DumpTree((PTree)FirstSymbol);
}

static void ClearSymbolList_ClearNode(PTree Node, void *pData)
{
  PSymbolEntry SymbolEntry = (PSymbolEntry) Node;
  UNUSED(pData);

  FreeSymbolEntry(&SymbolEntry, FALSE);
}

void ClearSymbolList(void)
{
  PTree TreeRoot;

  TreeRoot = &(FirstSymbol->Tree);
  FirstSymbol = NULL;
  DestroyTree(&TreeRoot, ClearSymbolList_ClearNode, NULL);
  TreeRoot = &(FirstLocSymbol->Tree);
  FirstLocSymbol = NULL;
  DestroyTree(&TreeRoot, ClearSymbolList_ClearNode, NULL);
}

/*-------------------------------------------------------------------------*/
/* Stack-Verwaltung */

Boolean PushSymbol(char *SymName_O, char *StackName_O)
{
  PSymbolEntry Src;
  PSymbolStack LStack, NStack, PStack;
  PSymbolStackEntry Elem;
  String SymName, StackName;

  strmaxcpy(SymName, SymName_O, 255);
  if (!ExpandSymbol(SymName))
    return False;

  Src = FindNode(SymName, TempAll);
  if (!Src)
  {
    WrXError(1010, SymName);
    return False;
  }

  strmaxcpy(StackName, (*StackName_O == '\0') ? DefStackName : StackName_O, 255);
  if (!ExpandSymbol(StackName))
    return False;
  if (!ChkSymbName(StackName))
  {
    WrXError(1020, StackName);
    return False;
  }

  LStack = FirstStack;
  PStack = NULL;
  while ((LStack) && (strcmp(LStack->Name, StackName) < 0))
  {
    PStack = LStack;
    LStack = LStack->Next;
  }

  if ((!LStack) || (strcmp(LStack->Name, StackName)>0))
  {
    NStack = (PSymbolStack) malloc(sizeof(TSymbolStack));
    NStack->Name = strdup(StackName);
    NStack->Contents = NULL;
    NStack->Next = LStack;
    if (!PStack)
      FirstStack = NStack;
    else
      PStack->Next = NStack;
    LStack = NStack;
  }

  Elem = (PSymbolStackEntry) malloc(sizeof(TSymbolStackEntry));
  Elem->Next = LStack->Contents;
  Elem->Contents = Src->SymWert;
  LStack->Contents = Elem;

  return True;
}

Boolean PopSymbol(char *SymName_O, char *StackName_O)
{
  PSymbolEntry Dest;
  PSymbolStack LStack, PStack;
  PSymbolStackEntry Elem;
  String SymName, StackName;

  strmaxcpy(SymName, SymName_O, 255);
  if (!ExpandSymbol(SymName))
    return False;

  Dest = FindNode(SymName, TempAll);
  if (!Dest)
  {
    WrXError(1010, SymName);
    return False;
  }

  strmaxcpy(StackName, (*StackName_O == '\0') ? DefStackName : StackName_O, 255);
  if (!ExpandSymbol(StackName))
    return False;
  if (!ChkSymbName(StackName))
  {
    WrXError(1020, StackName);
    return False;
  }

  LStack = FirstStack;
  PStack = NULL;
  while ((LStack) && (strcmp(LStack->Name, StackName) < 0))
  {
    PStack = LStack;
    LStack = LStack->Next;
  }

  if ((!LStack) || (strcmp(LStack->Name, StackName)>0))
  {
    WrXError(1530, StackName);
    return False;
  }

  Elem = LStack->Contents;
  Dest->SymWert = Elem->Contents;
  LStack->Contents = Elem->Next;
  if (!LStack->Contents)
  {
    if (!PStack)
      FirstStack = LStack->Next;
    else
      PStack->Next = LStack->Next;
    free(LStack->Name);
    free(LStack);
  }
  free(Elem);

  return True;
}

void ClearStacks(void)
{
  PSymbolStack Act;
  PSymbolStackEntry Elem;
  int z;
  String s;

  while (FirstStack)
  {
    z = 0;
    Act = FirstStack;
    while (Act->Contents)
    {
      Elem = Act->Contents;
      Act->Contents = Elem->Next;
      free(Elem);
      z++;
    }
    sprintf(s, "%s(%d)",  Act->Name,  z);
    WrXError(230, s);
    free(Act->Name);
    FirstStack = Act->Next;
    free(Act);
  }
}

/*-------------------------------------------------------------------------*/
/* Funktionsverwaltung */

void EnterFunction(char *FName, char *FDefinition, Byte NewCnt)
{
  PFunction Neu;
  String FName_N;

  if (!CaseSensitive)
  {
    strmaxcpy(FName_N, FName, 255);
    NLS_UpString(FName_N);
    FName = FName_N;
  }

  if (!ChkSymbName(FName))
  {
    WrXError(1020, FName);
    return;
  }

  if (FindFunction(FName))
  {
    if (PassNo == 1)
      WrXError(1000, FName);
    return;
  }

  Neu = (PFunction) malloc(sizeof(TFunction));
  Neu->Next = FirstFunction;
  Neu->ArguCnt = NewCnt;
  Neu->Name = strdup(FName);
  Neu->Definition = strdup(FDefinition);
  FirstFunction = Neu;
}

PFunction FindFunction(char *Name)
{
  PFunction Lauf = FirstFunction;
  String Name_N;

  if (!CaseSensitive)
  {
    strmaxcpy(Name_N, Name, 255);
    NLS_UpString(Name_N);
    Name = Name_N;
  }

  while ((Lauf) && (strcmp(Lauf->Name, Name)))
    Lauf = Lauf->Next;
  return Lauf;
}

void PrintFunctionList(void)
{
  PFunction Lauf;
  String OneS;
  Boolean cnt;

  if (!FirstFunction)
    return;

  NewPage(ChapDepth, True);
  WrLstLine(getmessage(Num_ListFuncListHead1));
  WrLstLine(getmessage(Num_ListFuncListHead2));
  WrLstLine("");

  OneS[0] = '\0';
  Lauf = FirstFunction;
  cnt = False;
  while (Lauf)
  {
    strmaxcat(OneS, Lauf->Name, 255);
    if (strlen(Lauf->Name) < 37)
      strmaxcat(OneS, Blanks(37-strlen(Lauf->Name)), 255);
    if (!cnt) strmaxcat(OneS, " | ", 255);
    else
    {
      WrLstLine(OneS);
      OneS[0] = '\0';
    }
    cnt = !cnt;
    Lauf = Lauf->Next;
  }
  if (cnt)
  {
    OneS[strlen(OneS)-1] = '\0';
    WrLstLine(OneS);
  }
  WrLstLine("");
}

void ClearFunctionList(void)
{
  PFunction Lauf;

  while (FirstFunction)
  {
    Lauf = FirstFunction->Next;
    free(FirstFunction->Name);
    free(FirstFunction->Definition);
    free(FirstFunction);
    FirstFunction = Lauf;
  }
}

/*-------------------------------------------------------------------------*/

static void ResetSymbolDefines_ResetNode(PTree Node, void *pData)
{
  PSymbolEntry SymbolEntry = (PSymbolEntry) Node;
  UNUSED(pData);

  SymbolEntry->Defined = False;
  SymbolEntry->Used = False;
}

void ResetSymbolDefines(void)
{
  IterTree(&(FirstSymbol->Tree), ResetSymbolDefines_ResetNode, NULL);
  IterTree(&(FirstLocSymbol->Tree), ResetSymbolDefines_ResetNode, NULL);
}

void SetFlag(Boolean *Flag, const char *Name, Boolean Wert)
{
  *Flag = Wert;
  EnterIntSymbol(Name, *Flag ? 1 : 0, 0, True);
}

void AddDefSymbol(char *Name, TempResult *Value)
{
  PDefSymbol Neu;

  Neu = FirstDefSymbol;
  while (Neu)
  {
    if (!strcmp(Neu->SymName, Name))
      return;
    Neu = Neu->Next;
  }

  Neu = (PDefSymbol) malloc(sizeof(TDefSymbol));
  Neu->Next = FirstDefSymbol;
  Neu->SymName = strdup(Name);
  Neu->Wert = (*Value);
  FirstDefSymbol = Neu;
}

void RemoveDefSymbol(char *Name)
{
  PDefSymbol Save, Lauf;

  if (!FirstDefSymbol)
    return;

  if (!strcmp(FirstDefSymbol->SymName, Name))
  {
    Save = FirstDefSymbol;
    FirstDefSymbol = FirstDefSymbol->Next;
  }
  else
  {
    Lauf = FirstDefSymbol;
    while ((Lauf->Next) && (strcmp(Lauf->Next->SymName, Name)))
      Lauf = Lauf->Next;
    if (!Lauf->Next)
      return;
    Save = Lauf->Next;
    Lauf->Next = Lauf->Next->Next;
  }
  free(Save->SymName);
  free(Save);
}

void CopyDefSymbols(void)
{
  PDefSymbol Lauf;

  Lauf = FirstDefSymbol;
  while (Lauf)
  {
    switch (Lauf->Wert.Typ)
    {
      case TempInt:
        EnterIntSymbol(Lauf->SymName, Lauf->Wert.Contents.Int, 0, True);
        break;
      case TempFloat:
        EnterFloatSymbol(Lauf->SymName, Lauf->Wert.Contents.Float, True);
        break;
      case TempString:
        EnterDynStringSymbol(Lauf->SymName, &Lauf->Wert.Contents.Ascii, True);
        break;
      default:
        break;
    }
    Lauf = Lauf->Next;
  }
}

void PrintSymbolDepth(void)
{
  LongInt TreeMin, TreeMax;

  GetTreeDepth(&(FirstSymbol->Tree), &TreeMin, &TreeMax);
  fprintf(Debug, " MinTree %ld\n", (long)TreeMin);
  fprintf(Debug, " MaxTree %ld\n", (long)TreeMax);
}

LongInt GetSectionHandle(char *SName_O, Boolean AddEmpt, LongInt Parent)
{
  PCToken Lauf, Prev;
  LongInt z;
  String SName;

  strmaxcpy(SName, SName_O, 255);
  if (!CaseSensitive)
    NLS_UpString(SName);

  Lauf = FirstSection;
  Prev = NULL;
  z = 0;
  while ((Lauf) && ((strcmp(Lauf->Name, SName)) || (Lauf->Parent != Parent)))
  {
    z++;
    Prev = Lauf;
    Lauf = Lauf->Next;
  }

  if (!Lauf)
  {
    if (AddEmpt)
    {
      Lauf = (PCToken) malloc(sizeof(TCToken));
      Lauf->Parent = MomSectionHandle;
      Lauf->Name = strdup(SName);
      Lauf->Next = NULL;
      InitChunk(&(Lauf->Usage));
      if (!Prev)
        FirstSection = Lauf;
      else
        Prev->Next = Lauf;
    }
    else
      z = -2;
  }
  return z;
}

char *GetSectionName(LongInt Handle)
{
  PCToken Lauf = FirstSection;
  static char *Dummy = "";

  if (Handle == -1)
    return Dummy;
  while ((Handle > 0) && (Lauf))
  {
    Lauf = Lauf->Next;
    Handle--;
  }
  return Lauf ? Lauf->Name : Dummy;
}

void SetMomSection(LongInt Handle)
{
  LongInt z;

  MomSectionHandle = Handle;
  if (Handle < 0)
    MomSection = NULL;
  else
  {
    MomSection = FirstSection;
    for (z = 1; z <= Handle; z++)
      if (MomSection)
        MomSection = MomSection->Next;
  }
}

void AddSectionUsage(LongInt Start, LongInt Length)
{
  if ((ActPC != SegCode) || (!MomSection))
    return;
  AddChunk(&(MomSection->Usage), Start, Length, False);
}

void ClearSectionUsage(void)
{
  PCToken Tmp;

  for (Tmp = FirstSection; Tmp; Tmp = Tmp->Next)
    ClearChunk(&(Tmp->Usage));
}

static void PrintSectionList_PSection(LongInt Handle, int Indent)
{
  PCToken Lauf;
  LongInt Cnt;
  String h;

  ChkStack();
  if (Handle != -1)
  {
    strmaxcpy(h, Blanks(Indent << 1), 255);
    strmaxcat(h, GetSectionName(Handle), 255);
    WrLstLine(h);
  }
  Lauf = FirstSection;
  Cnt = 0;
  while (Lauf)
  {
    if (Lauf->Parent == Handle)
      PrintSectionList_PSection(Cnt, Indent + 1);
    Lauf = Lauf->Next;
    Cnt++;
  }
}

void PrintSectionList(void)
{
  if (!FirstSection)
    return;

  NewPage(ChapDepth, True);
  WrLstLine(getmessage(Num_ListSectionListHead1));
  WrLstLine(getmessage(Num_ListSectionListHead2));
  WrLstLine("");
  PrintSectionList_PSection(-1, 0);
}

void PrintDebSections(FILE *f)
{
  PCToken Lauf;
  LongInt Cnt, z, l, s;
  char Str[30];

  Lauf = FirstSection; Cnt = 0;
  while (Lauf)
  {
    fputs("\nInfo for Section ", f); ChkIO(10004);
    fprintf(f, LongIntFormat, Cnt); ChkIO(10004);
    fputc(' ', f); ChkIO(10004);
    fputs(GetSectionName(Cnt), f); ChkIO(10004);
    fputc(' ', f); ChkIO(10004);
    fprintf(f, LongIntFormat, Lauf->Parent); ChkIO(10004);
    fputc('\n', f); ChkIO(10004);
    for (z = 0; z < Lauf->Usage.RealLen; z++)
    {
      l = Lauf->Usage.Chunks[z].Length;
      s = Lauf->Usage.Chunks[z].Start;
      HexString(Str, sizeof(Str), s, 0);
      fputs(Str, f);
      ChkIO(10004);
      if (l == 1)
        fprintf(f, "\n");
      else
      {
        HexString(Str, sizeof(Str), s + l - 1, 0);
        fprintf(f, "-%s\n", Str);
      }
      ChkIO(10004);
    }
    Lauf = Lauf->Next;
    Cnt++;
  }
}

void ClearSectionList(void)
{
  PCToken Tmp;

  while (FirstSection)
  {
    Tmp = FirstSection;
    free(Tmp->Name);
    ClearChunk(&(Tmp->Usage));
    FirstSection = Tmp->Next; free(Tmp);
  }
}

/*---------------------------------------------------------------------------------*/

static void PrintCrossList_PNode(PTree Node, void *pData)
{
  int FileZ;
  PCrossRef Lauf;
  String LinePart, LineAcc;
  String h, h2;
  TempResult t;
  PSymbolEntry SymbolEntry = (PSymbolEntry) Node;
  UNUSED(pData);

  if (!SymbolEntry->RefList)
    return;

  ConvertSymbolVal(&(SymbolEntry->SymWert), &t);
  strcpy(h, " (=");
  StrSym(&t, False, h2, sizeof(h2));
  strmaxcat(h, h2, 255);
  strmaxcat(h, ", ", 255);
  strmaxcat(h, GetFileName(SymbolEntry->FileNum), 255);
  strmaxcat(h, ":", 255);
  sprintf(h2, LongIntFormat, SymbolEntry->LineNum); strmaxcat(h, h2, 255);
  strmaxcat(h, "):", 255);
  if (Node->Attribute != -1)
  {
    strmaxprep(h, "] ", 255);
    strmaxprep(h, GetSectionName(Node->Attribute), 255);
    strmaxprep(h, " [", 255);
  }

  strmaxprep(h, Node->Name, 255);
  strmaxprep(h, getmessage(Num_ListCrossSymName), 255);
  WrLstLine(h);

  for (FileZ = 0; FileZ < GetFileCount(); FileZ++)
  {
    Lauf = SymbolEntry->RefList;

    while ((Lauf) && (Lauf->FileNum != FileZ))
      Lauf = Lauf->Next;

    if (Lauf)
    {
      strcpy(h, " ");
      strmaxcat(h, getmessage(Num_ListCrossFileName), 255);
      strmaxcat(h, GetFileName(FileZ), 255);
      strmaxcat(h, " :", 255);
      WrLstLine(h);
      strcpy(LineAcc, "   ");
      while (Lauf)
      {
        sprintf(LinePart, "%5ld", (long)Lauf->LineNum);
        strmaxcat(LineAcc, LinePart, 255);
        if (Lauf->OccNum != 1)
        {
          sprintf(LinePart, "(%2ld)", (long)Lauf->OccNum);
          strmaxcat(LineAcc, LinePart, 255);
        }
        else strmaxcat(LineAcc, "    ", 255);
        if (strlen(LineAcc) >= 72)
        {
          WrLstLine(LineAcc);
          strcpy(LineAcc, "  ");
        }
        Lauf = Lauf->Next;
      }
      if (strcmp(LineAcc, "  "))
        WrLstLine(LineAcc);
    }
  }
  WrLstLine("");
}

void PrintCrossList(void)
{
  WrLstLine("");
  WrLstLine(getmessage(Num_ListCrossListHead1));
  WrLstLine(getmessage(Num_ListCrossListHead2));
  WrLstLine("");
  IterTree(&(FirstSymbol->Tree), PrintCrossList_PNode, NULL);
  WrLstLine("");
}

static void ClearCrossList_CNode(PTree Tree, void *pData)
{
  PCrossRef Lauf;
  PSymbolEntry SymbolEntry = (PSymbolEntry) Tree;
  UNUSED(pData);

  while (SymbolEntry->RefList)
  {
    Lauf = SymbolEntry->RefList->Next;
    free(SymbolEntry->RefList);
    SymbolEntry->RefList = Lauf;
  }
}

void ClearCrossList(void)
{
  IterTree(&(FirstSymbol->Tree), ClearCrossList_CNode, NULL);
}

/*--------------------------------------------------------------------------*/

LongInt GetLocHandle(void)
{
  return LocHandleCnt++;
}

void PushLocHandle(LongInt NewLoc)
{
  PLocHandle NewLocHandle;

  NewLocHandle = (PLocHandle) malloc(sizeof(TLocHeap));
  NewLocHandle->Cont = MomLocHandle;
  NewLocHandle->Next = FirstLocHandle;
  FirstLocHandle = NewLocHandle; MomLocHandle = NewLoc;
}

void PopLocHandle(void)
{
  PLocHandle OldLocHandle;

  OldLocHandle = FirstLocHandle;
  if (!OldLocHandle) return;
  MomLocHandle = OldLocHandle->Cont;
  FirstLocHandle = OldLocHandle->Next;
  free(OldLocHandle);
}

void ClearLocStack()
{
  while (MomLocHandle != -1)
    PopLocHandle();
}

/*--------------------------------------------------------------------------*/

static PRegDef LookupReg(char *Name, Boolean CreateNew)
{
  PRegDef Run, Neu, Prev;
  int cmperg = 0;

  Prev = NULL;
  Run = FirstRegDef;
  while (Run)
  {
    cmperg = strcmp(Run->Orig, Name);
    if (!cmperg)
      break;
    Prev = Run;
    Run = (cmperg < 0) ? Run->Left : Run->Right;
  }
  if ((!Run) && (CreateNew))
  {
    Neu = (PRegDef) malloc(sizeof(TRegDef));
    Neu->Orig = strdup(Name);
    Neu->Left = Neu->Right = NULL;
    Neu->Defs = NULL;
    Neu->DoneDefs = NULL;
    if (!Prev)
      FirstRegDef = Neu;
    else if (cmperg < 0)
      Prev->Left = Neu;
    else
      Prev->Right = Neu;
    return Neu;
  }
  else
    return Run;
}

void AddRegDef(char *Orig_N, char *Repl_N)
{
  PRegDef Node;
  PRegDefList Neu;
  String Orig, Repl;

  strmaxcpy(Orig, Orig_N, 255);
  strmaxcpy(Repl, Repl_N, 255);
  if (!CaseSensitive)
  {
    NLS_UpString(Orig);
    NLS_UpString(Repl);
  }
  if (!ChkSymbName(Orig))
  {
    WrXError(1020, Orig);
    return;
  }
  if (!ChkSymbName(Repl))
  {
    WrXError(1020, Repl);
    return;
  }
  Node = LookupReg(Orig, True);
  if ((Node->Defs) && (Node->Defs->Section == MomSectionHandle))
    WrXError(1000, Orig);
  else
  {
    Neu = (PRegDefList) malloc(sizeof(TRegDefList));
    Neu->Next = Node->Defs;
    Neu->Section = MomSectionHandle;
    Neu->Value = strdup(Repl);
    Neu->Used = False;
    Node->Defs = Neu;
  }
}

Boolean FindRegDef(const char *Name_N, char **Erg)
{
  LongInt Sect;
  PRegDef Node;
  PRegDefList Def;
  String Name;

  if (*Name_N == '[')
    return FALSE;

  strmaxcpy(Name, Name_N, 255);

  if (!GetSymSection(Name, &Sect))
    return False;
  if (!CaseSensitive)
    NLS_UpString(Name);
  Node = LookupReg(Name, False);
  if (!Node)
    return False;
  Def = Node->Defs;
  if (Sect != -2)
    while ((Def) && (Def->Section != Sect))
      Def = Def->Next;
  if (!Def)
    return False;
  else
  {
    *Erg = Def->Value;
    Def->Used = True;
    return True;
  }
}

static void TossRegDefs_TossSingle(PRegDef Node, LongInt Sect)
{
  PRegDefList Tmp;

  if (!Node)
    return;
  ChkStack();

  if ((Node->Defs) && (Node->Defs->Section == Sect))
  {
    Tmp = Node->Defs;
    Node->Defs = Node->Defs->Next;
    Tmp->Next = Node->DoneDefs;
    Node->DoneDefs = Tmp;
  }

  TossRegDefs_TossSingle(Node->Left, Sect);
  TossRegDefs_TossSingle(Node->Right, Sect);
}

void TossRegDefs(LongInt Sect)
{
  TossRegDefs_TossSingle(FirstRegDef, Sect);
}

static void ClearRegDefList(PRegDefList Start)
{
  PRegDefList Tmp;

  while (Start)
  {
    Tmp = Start;
    Start = Start->Next;
    free(Tmp->Value);
    free(Tmp);
  }
}

static void CleanupRegDefs_CleanupNode(PRegDef Node)
{
  if (!Node)
    return;
  ChkStack();
  ClearRegDefList(Node->DoneDefs);
  Node->DoneDefs = NULL;
  CleanupRegDefs_CleanupNode(Node->Left);
  CleanupRegDefs_CleanupNode(Node->Right);
}

void CleanupRegDefs(void)
{
  CleanupRegDefs_CleanupNode(FirstRegDef);
}

static void ClearRegDefs_ClearNode(PRegDef Node)
{
  if (!Node)
    return;
  ChkStack();
  ClearRegDefList(Node->Defs);
  Node->Defs = NULL;
  ClearRegDefList(Node->DoneDefs);
  Node->DoneDefs = NULL;
  ClearRegDefs_ClearNode(Node->Left);
  ClearRegDefs_ClearNode(Node->Right);
  free(Node->Orig);
  free(Node);
}

void ClearRegDefs(void)
{
  ClearRegDefs_ClearNode(FirstRegDef);
}

static int cwidth;

static void PrintRegDefs_PNode(PRegDef Node, char *buf, LongInt *Sum, LongInt *USum)
{
  PRegDefList Lauf;
  String tmp, tmp2;

  for (Lauf = Node->DoneDefs; Lauf; Lauf = Lauf->Next)
  {
    if (Lauf->Section != -1)
      sprintf(tmp2, "[%s]", GetSectionName(Lauf->Section));
    else
      *tmp2 = '\0';
    sprintf(tmp, "%c%s%s --> %s", (Lauf->Used) ? ' ' : '*', Node->Orig, tmp2, Lauf->Value);
    if ((int)strlen(tmp) > cwidth - 3)
    {
      if (*buf != '\0')
        WrLstLine(buf);
      *buf = '\0';
      WrLstLine(tmp);
    }
    else
    {
      strmaxcat(tmp, Blanks(cwidth - 3 - strlen(tmp)), 255);
      if (*buf == '\0')
        strcpy(buf, tmp);
      else
      {
        strcat(buf, " | ");
        strcat(buf, tmp);
        WrLstLine(buf);
        *buf = '\0';
      }
    }
    (*Sum)++;
    if (!Lauf->Used)
      (*USum)++;
  }
}

static void PrintRegDefs_PrintSingle(PRegDef Node, char *buf, LongInt *Sum, LongInt *USum)
{
  if (!Node)
    return;
  ChkStack();

  PrintRegDefs_PrintSingle(Node->Left, buf, Sum, USum);
  PrintRegDefs_PNode(Node, buf, Sum, USum);
  PrintRegDefs_PrintSingle(Node->Right, buf, Sum, USum);
}

void PrintRegDefs(void)
{
  String buf;
  LongInt Sum, USum;
  LongInt ActPageWidth;

  if (!FirstRegDef)
    return;

  NewPage(ChapDepth, True);
  WrLstLine(getmessage(Num_ListRegDefListHead1));
  WrLstLine(getmessage(Num_ListRegDefListHead2));
  WrLstLine("");

  *buf = '\0';
  Sum = 0;
  USum = 0;
  ActPageWidth = (PageWidth == 0) ? 80 : PageWidth;
  cwidth = ActPageWidth >> 1;
  PrintRegDefs_PrintSingle(FirstRegDef, buf, &Sum, &USum);

  if (*buf != '\0')
    WrLstLine(buf);
  WrLstLine("");
  sprintf(buf, "%7ld%s",
          (long) Sum,
          getmessage((Sum == 1) ? Num_ListRegDefSumMsg : Num_ListRegDefSumsMsg));
  WrLstLine(buf);
  sprintf(buf, "%7ld%s",
          (long)USum,
          getmessage((USum == 1) ? Num_ListRegDefUSumMsg : Num_ListRegDefUSumsMsg));
  WrLstLine("");
}

/*--------------------------------------------------------------------------*/

void ClearCodepages(void)
{
  PTransTable Old;

  while (TransTables)
  {
    Old = TransTables;
    TransTables = Old->Next;
    free(Old->Name);
    free(Old->Table);
    free(Old);
  }
}

void PrintCodepages(void)
{
  char buf[500];
  PTransTable Table;
  int z, cnt, cnt2;

  NewPage(ChapDepth, True);
  WrLstLine(getmessage(Num_ListCodepageListHead1));
  WrLstLine(getmessage(Num_ListCodepageListHead2));
  WrLstLine("");

  cnt2 = 0;
  for (Table = TransTables; Table; Table = Table->Next)
  {
    for (z = cnt = 0; z < 256; z++)
      if (Table->Table[z] != z)
        cnt++;
    sprintf(buf, "%s (%d%s)", Table->Name, cnt,
            getmessage((cnt == 1) ? Num_ListCodepageChange : Num_ListCodepagePChange));
    WrLstLine(buf);
    cnt2++;
  }
  WrLstLine("");
  sprintf(buf, "%d%s", cnt2,
          getmessage((cnt2 == 1) ? Num_ListCodepageSumMsg : Num_ListCodepageSumsMsg));
}

/*--------------------------------------------------------------------------*/

void asmpars_init(void)
{
  serr = (char*)malloc(sizeof(char) * STRINGSIZE);
  snum = (char*)malloc(sizeof(char) * STRINGSIZE);
  FirstDefSymbol = NULL;
  FirstFunction = NULL;
  BalanceTree = False;
  IntTypeDefs[(int)Int32].Min--;
  IntTypeDefs[(int)SInt32].Min--;
#ifdef HAS64
  IntTypeDefs[(int)Int64].Min--;
#endif
  LastGlobSymbol = (char*)malloc(sizeof(char) * 256);
}
