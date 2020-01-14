/* code166.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* AS-Codegenerator Siemens 80C16x                                           */
/*                                                                           */
/* Historie: 11.11.1996 (alaaf) Grundsteinlegung                             */
/*            9. 5.1998 Registersymbole                                      */
/*            3. 1.1999 ChkPC angepasst                                      */
/*            9. 3.2000 'ambiguous else'-Warnungen beseitigt                 */
/*                                                                           */
/*****************************************************************************/
/* $Id: code166.c,v 1.12 2014/12/07 19:13:59 alfred Exp $                     */
/*****************************************************************************
 * $Log: code166.c,v $
 * Revision 1.12  2014/12/07 19:13:59  alfred
 * - silence a couple of Borland C related warnings and errors
 *
 * Revision 1.11  2014/11/05 15:47:13  alfred
 * - replace InitPass callchain with registry
 *
 * Revision 1.10  2014/09/19 21:20:26  alfred
 * - rework to current style
 *
 * Revision 1.9  2014/03/08 21:06:35  alfred
 * - rework ASSUME framework
 *
 * Revision 1.8  2010/12/05 23:17:59  alfred
 * - use machine-dependent SFR start when transforming SFR addresses back to absolute
 *
 * Revision 1.7  2010/08/27 14:52:41  alfred
 * - some more overlapping strcpy() cleanups
 *
 * Revision 1.6  2010/04/17 13:14:19  alfred
 * - address overlapping strcpy()
 *
 * Revision 1.5  2007/11/24 22:48:03  alfred
 * - some NetBSD changes
 *
 * Revision 1.4  2005/10/02 10:00:44  alfred
 * - ConstLongInt gets default base, correct length check on KCPSM3 registers
 *
 * Revision 1.3  2005/09/08 17:31:02  alfred
 * - add missing include
 *
 * Revision 1.2  2004/05/29 11:33:00  alfred
 * - relocated DecodeIntelPseudo() into own module
 *
 *****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "nls.h"
#include "strutil.h"
#include "bpemu.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmitree.h"
#include "codepseudo.h"
#include "intpseudo.h" 
#include "codevars.h"

typedef struct
{
  CPUVar MinCPU;
  Word Code1,Code2;
} BaseOrder;

typedef struct
{
  char *Name;
  Byte Code;
} Condition;


#define FixedOrderCount 10
#define ConditionCount 20

#define DPPCount 4
static char *RegNames[6] = { "DPP0", "DPP1", "DPP2", "DPP3", "CP", "SP" };

static CPUVar CPU80C166, CPU80C167, CPU80C167CS;

static BaseOrder *FixedOrders;
static Condition *Conditions;
static int TrueCond;

static LongInt DPPAssumes[DPPCount];
static IntType MemInt, MemInt2;
static Byte OpSize;

static Boolean DPPChanged[DPPCount], N_DPPChanged[DPPCount];
static Boolean SPChanged, CPChanged, N_SPChanged, N_CPChanged;

static ShortInt ExtCounter;
static enum
{
  MemModeStd,       /* normal */
  MemModeNoCheck,   /* EXTS Rn */
  MemModeZeroPage,  /* EXTP Rn */
  MemModeFixedBank, /* EXTS nn */
  MemModeFixedPage  /* EXTP nn */
} MemMode;
static Word MemPage;
static Boolean ExtSFRs;

#define ASSUME166Count 4
static ASSUMERec ASSUME166s[ASSUME166Count] =
{
  { "DPP0", DPPAssumes + 0, 0, 15, -1 },
  { "DPP1", DPPAssumes + 1, 0, 15, -1 },
  { "DPP2", DPPAssumes + 2, 0, 15, -1 },
  { "DPP3", DPPAssumes + 3, 0, 15, -1 }
};

/*-------------------------------------------------------------------------*/

enum
{
  ModNone = -1,
  ModReg = 0,
  ModImm = 1,
  ModIReg = 2,
  ModPreDec = 3,
  ModPostInc = 4,
  ModIndex = 5,
  ModAbs = 6,
  ModMReg = 7,
  ModLAbs = 8,
};

#define MModReg (1 << ModReg)
#define MModImm (1 << ModImm)
#define MModIReg (1 << ModIReg)
#define MModPreDec (1 << ModPreDec)
#define MModPostInc (1 << ModPostInc)
#define MModIndex (1 << ModIndex)
#define MModAbs (1 << ModAbs)
#define MModMReg (1 << ModMReg)
#define MModLAbs (1 << ModLAbs)

static Byte AdrMode;
static Byte AdrVals[2];
static ShortInt AdrType;

static Boolean IsReg(const char *Asc, Byte *Erg, Boolean WordWise)
{
  Boolean err;
  char *s;

  if (FindRegDef(Asc, &s))
    Asc = s;

  if ((strlen(Asc) < 2) || (mytoupper(*Asc) != 'R'))
    return False;
  else if ((strlen(Asc) > 2) && (mytoupper(Asc[1]) == 'L') && (!WordWise))
  {
    *Erg = ConstLongInt(Asc + 2, &err, 10);
    *Erg <<= 1;
    return ((err) && (*Erg <= 15));
  }
  else if ((strlen(Asc) > 2) && (mytoupper(Asc[1]) == 'H') && (!WordWise))
  {
    *Erg = ConstLongInt(Asc + 2, &err, 10); *Erg <<= 1;
    (*Erg)++;
    return ((err) && (*Erg <= 15));
  }
  else
  {
    *Erg = ConstLongInt(Asc + 1, &err, 10);
    return ((err) && (*Erg <= 15));
  }
}

static Boolean IsRegM1(char *Asc, Byte *Erg, Boolean WordWise)
{
  char tmp;
  int l;
  Boolean b;

  if (*Asc != '\0')
  {
    tmp = Asc[l = (strlen(Asc) - 1)];
    Asc[l] = '\0';
    b = IsReg(Asc, Erg, WordWise);
    Asc[l] = tmp;
    return b;
  }
  else
    return False;
}

static LongInt SFRStart(void)
{
  return (ExtSFRs) ? 0xf000 : 0xfe00;
}

static LongInt SFREnd(void)
{
  return (ExtSFRs) ? 0xf1de : 0xffde;
}

static Boolean CalcPage(LongInt *Adr, Boolean DoAnyway)
{
  int z;
  Word Bank;

  switch (MemMode)
  {
    case MemModeStd:
      z = 0;
      while ((z <= 3) && (((*Adr) >> 14) != DPPAssumes[z]))
        z++;
      if (z > 3)
      {
        WrError(110);
        (*Adr) &= 0xffff;
        return DoAnyway;
      }
      else
      {
        *Adr = ((*Adr) & 0x3fff) + (z << 14);
        if (DPPChanged[z])
          WrXError(200, RegNames[z]);
        return True;
      }
    case MemModeZeroPage:
      (*Adr) &= 0x3fff;
      return True;
    case MemModeFixedPage:
      Bank = (*Adr) >> 14;
      (*Adr) &= 0x3fff;
      if (Bank != MemPage)
      {
        WrError(110);
        return (DoAnyway);
      }
      else
        return True;
    case MemModeNoCheck:
      (*Adr) &= 0xffff;
      return True;
    case MemModeFixedBank:
      Bank = (*Adr) >> 16; (*Adr) &= 0xffff;
      if (Bank != MemPage)
      {
        WrError(110);
        return (DoAnyway);
      }
      else
        return True;
    default:
      return False;
  }
}

static void DecideAbsolute(Boolean InCode, LongInt DispAcc, Word Mask, Boolean Dest)
{
#define DPPAdr 0xfe00
#define SPAdr 0xfe12
#define CPAdr 0xfe10

  int z;

  if (InCode)
  {
    if (((EProgCounter() >> 16) == (DispAcc >> 16)) && (Mask & MModAbs))
    {
      AdrType = ModAbs;
      AdrCnt = 2;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc);
    }
    else
    {
      AdrType = ModLAbs;
      AdrCnt = 2;
      AdrMode = DispAcc >> 16;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc);
    }
  }
  else if (((Mask & MModMReg) != 0) && (DispAcc >= SFRStart()) && (DispAcc <= SFREnd()) && (!(DispAcc & 1)))
  {
    AdrType = ModMReg;
    AdrCnt = 1;
    AdrVals[0] = (DispAcc - SFRStart()) >> 1;
  }
  else switch (MemMode)
  {
    case MemModeStd:
      z = 0;
      while ((z <= 3) && ((DispAcc >> 14) != DPPAssumes[z]))
        z++;
      if (z > 3)
      {
        WrError(110);
        z = (DispAcc >> 14) & 3;
      }
      AdrType = ModAbs;
      AdrCnt = 2;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = (Hi(DispAcc) & 0x3f) + (z << 6);
      if (DPPChanged[z])
        WrXError(200, RegNames[z]);
      break;
    case MemModeZeroPage:
      AdrType = ModAbs;
      AdrCnt = 2;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc) & 0x3f;
      break;
    case MemModeFixedPage:
      if ((DispAcc >> 14) != MemPage)
        WrError(110);
      AdrType = ModAbs;
      AdrCnt = 2;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc) & 0x3f;
      break;
    case MemModeNoCheck:
      AdrType = ModAbs;
      AdrCnt = 2;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc);
      break;
    case MemModeFixedBank:
      if ((DispAcc >> 16) != MemPage)
        WrError(110);
      AdrType = ModAbs;
      AdrCnt = 2;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc);
      break;
  }

  if ((AdrType != ModNone) && (Dest))
  {
    switch ((Word)DispAcc)
    {
      case SPAdr    : N_SPChanged = True; break;
      case CPAdr    : N_CPChanged = True; break;
      case DPPAdr   :
      case DPPAdr + 1 : N_DPPChanged[0] = True; break;
      case DPPAdr + 2 :
      case DPPAdr + 3 : N_DPPChanged[1] = True; break;
      case DPPAdr + 4 :
      case DPPAdr + 5 : N_DPPChanged[2] = True; break;
      case DPPAdr + 6 :
      case DPPAdr + 7 : N_DPPChanged[3] = True; break;
    }
  }
}

static void DecodeAdr(char *Asc, Word Mask, Boolean InCode, Boolean Dest)
{
  LongInt HDisp, DispAcc;
  char *PPos, *MPos;
  String Part;
  Boolean OK, NegFlag, NNegFlag;
  Byte HReg;

  AdrType = ModNone; AdrCnt = 0;

  /* immediate ? */

  if (*Asc == '#')
  {
    switch (OpSize)
    {
      case 0:
        AdrVals[0] = EvalIntExpression(Asc + 1, Int8, &OK);
        AdrVals[1] = 0;
        break;
      case 1:
        HDisp = EvalIntExpression(Asc + 1, Int16, &OK);
        AdrVals[0] = Lo(HDisp);
        AdrVals[1] = Hi(HDisp);
        break;
    }
    if (OK)
    {
      AdrType = ModImm;
      AdrCnt = OpSize + 1;
    }
  }

  /* Register ? */

  else if (IsReg(Asc, &AdrMode, OpSize == 1))
  {
    if ((Mask & MModReg) != 0)
      AdrType = ModReg;
    else
    {
      AdrType = ModMReg;
      AdrVals[0] = 0xf0 + AdrMode;
      AdrCnt = 1;
    }
    if (CPChanged)
      WrXError(200, RegNames[4]);
  }

  /* indirekt ? */

  else if ((*Asc == '[') && (Asc[strlen(Asc) - 1] == ']'))
  {
    Asc++;
    Asc[strlen(Asc) - 1] = '\0';

    /* Predekrement ? */

    if ((strlen(Asc) > 2) && (*Asc == '-') && (IsReg(Asc + 1, &AdrMode, True)))
      AdrType = ModPreDec;

    /* Postinkrement ? */

    else if ((strlen(Asc) > 2) && (Asc[strlen(Asc) - 1] == '+') && (IsRegM1(Asc, &AdrMode, True)))
      AdrType = ModPostInc;

    /* indiziert ? */

    else
    {
      NegFlag = False;
      DispAcc = 0;
      AdrMode = 0xff;
      while (*Asc != '\0')
      {
        MPos = QuotPos(Asc, '-');
        PPos = QuotPos(Asc, '+');
        if (((MPos < PPos) || (!PPos)) && (MPos))
        {
          PPos = MPos;
          NNegFlag = True;
        }
        else
          NNegFlag = False;
        if (!PPos)
        {
          strmaxcpy(Part, Asc, 255);
          *Asc = '\0';
        }
        else
        {
          *PPos = '\0';
          strmaxcpy(Part, Asc, 255);
          strmov(Asc, PPos + 1);
        }
        if (IsReg(Part, &HReg, True))
        {
          if ((NegFlag) || (AdrMode != 0xff))
            WrError(1350);
          else
            AdrMode = HReg;
        }
        else
        {
          char *pPart = Part;

          if (*pPart == '#')
            pPart++;
          HDisp = EvalIntExpression(pPart, Int32, &OK);
          if (OK)
            DispAcc = NegFlag ? DispAcc - HDisp : DispAcc + HDisp;
        }
        NegFlag = NNegFlag;
      }
      if (AdrMode == 0xff)
        DecideAbsolute(InCode, DispAcc, Mask, Dest);
      else if (DispAcc == 0)
        AdrType = ModIReg;
      else if (DispAcc > 0xffff)
        WrError(1320);
      else if (DispAcc < -0x8000l)
        WrError(1315);
      else
      {
        AdrVals[0] = Lo(DispAcc);
        AdrVals[1] = Hi(DispAcc);
        AdrType = ModIndex;
        AdrCnt = 2;
      }
    }
  }
  else
  {
    DispAcc = EvalIntExpression(Asc, MemInt, &OK);
    if (OK)
      DecideAbsolute(InCode, DispAcc, Mask, Dest);
  }

  if ((AdrType != ModNone) && (!((1 << AdrType) & Mask)))
  {
    WrError(1350);
    AdrType = ModNone;
    AdrCnt = 0;
  }
}

static int DecodeCondition(char *Name)
{
  int z;

  NLS_UpString(Name);
  for (z = 0; z < ConditionCount; z++)
    if (!strcmp(Conditions[z].Name, Name))
      break;
  return z;
}

static Boolean DecodeBitAddr(char *Asc, Word *Adr, Byte *Bit, Boolean MayBeOut)
{
  char *p;
  Word LAdr;
  Byte Reg;
  Boolean OK;

  p = QuotPos(Asc, '.');
  if (!p)
  {
    LAdr = EvalIntExpression(Asc, UInt16, &OK) & 0x1fff;
    if (OK)
    {
      if ((!MayBeOut) && ((LAdr >> 12) != Ord(ExtSFRs)))
      {
        WrError(1335);
        return False;
      }
      *Adr = LAdr >> 4;
      *Bit = LAdr & 15;
      if (!MayBeOut)
        *Adr = Lo(*Adr);
      return True;
    }
    else return False;
  }
  else if (p == Asc)
  {
    WrError(1350);
    return False;
  }
  else
  {
    *p = '\0';
    if (IsReg(Asc, &Reg, True))
      *Adr = 0xf0 + Reg;
    else
    {
      FirstPassUnknown = False;
      LAdr = EvalIntExpression(Asc, UInt16, &OK);
      if (!OK)
        return False;
      if (FirstPassUnknown)
        LAdr = 0xfd00;
      if (LAdr & 1)
      {
        WrError(1325);
        return False;
      }
      if ((LAdr >= 0xfd00) && (LAdr <= 0xfdfe))
        *Adr = (LAdr - 0xfd00)/2;
      else if ((LAdr >= 0xff00) && (LAdr <= 0xffde))
      {
        if ((ExtSFRs) && (!MayBeOut))
        {
          WrError(1335);
          return False;
        }
        *Adr = 0x80 + ((LAdr - 0xff00) / 2);
      }
      else if ((LAdr >= 0xf100) && (LAdr <= 0xf1de))
      {
        if ((!ExtSFRs) && (!MayBeOut))
        {
          WrError(1335);
          return False;
        }
        *Adr = 0x80 + ((LAdr - 0xf100) / 2);
        if (MayBeOut)
          (*Adr) += 0x100;
      }
      else
      {
        WrError(1320);
        return False;
      }
    }

    *Bit = EvalIntExpression(p + 1, UInt4, &OK);
    return OK;
  }
}

static Word WordVal(void)
{
  return AdrVals[0] + (((Word)AdrVals[1]) << 8);
}

static Boolean DecodePref(char *Asc, Byte *Erg)
{
  Boolean OK;

  if (*Asc != '#')
  {
    WrError(1350);
    return False;
  }
  strmov(Asc, Asc + 1);
  FirstPassUnknown = False;
  *Erg = EvalIntExpression(Asc, UInt3, &OK);
  if (FirstPassUnknown)
    *Erg = 1;
  if (!OK)
    return False;
  if (*Erg < 1)
    WrError(1315);
  else if (*Erg > 4)
    WrError(1320);
  else
  {
    (*Erg)--;
    return True;
  }
  return False;
}

/*-------------------------------------------------------------------------*/

static Boolean DecodePseudo(void)
{
  Word Adr;
  Byte Bit;

  if (Memo("BIT"))
  {
    if (ArgCnt != 1) WrError(1110);
    else if (DecodeBitAddr(ArgStr[1], &Adr, &Bit, True))
    {
      PushLocHandle(-1);
      EnterIntSymbol(LabPart, (Adr << 4) + Bit, SegNone, False);
      PopLocHandle();
      sprintf(ListLine, "=%02xH.%1x", Adr, Bit);
    }
    return True;
  }

  if (Memo("REG"))
  {
    if (ArgCnt != 1) WrError(1110);
    else
      AddRegDef(LabPart, ArgStr[1]);
    return True;
  }

  return False;
}

/*-------------------------------------------------------------------------*/

static void DecodeFixed(Word Index)
{
  const BaseOrder *pOrder = FixedOrders + Index;

  if (ArgCnt != 0) WrError(1110);
  else
  {
    CodeLen = 2;
    BAsmCode[0] = Lo(pOrder->Code1);
    BAsmCode[1] = Hi(pOrder->Code1);
    if (pOrder->Code2 != 0)
    {
      CodeLen = 4;
      BAsmCode[2] = Lo(pOrder->Code2);
      BAsmCode[3] = Hi(pOrder->Code2);
      if ((!strncmp(OpPart, "RET", 3)) && (SPChanged))
        WrXError(200, RegNames[5]);
    }
  }
}

static void DecodeMOV(Word Code)
{
  Byte HReg;
  LongInt AdrLong;

  OpSize = Hi(Code);
  Code = 1 - OpSize;

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg | MModMReg | MModIReg | MModPreDec | MModPostInc | MModIndex | MModAbs, False, True);
    switch (AdrType)
    {
      case ModReg:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg | MModImm | MModIReg | MModPostInc | MModIndex | MModAbs, False, False);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 2;
             BAsmCode[0] = 0xf0 + Code;
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
          case ModImm:
            if (WordVal() <= 15)
            {
              CodeLen = 2;
              BAsmCode[0] = 0xe0 + Code;
              BAsmCode[1] = (WordVal() << 4) + HReg;
            }
            else
            {
              CodeLen = 4;
              BAsmCode[0] = 0xe6 + Code;
              BAsmCode[1] = HReg + 0xf0;
              memcpy(BAsmCode + 2, AdrVals, 2);
            }
            break;
          case ModIReg:
            CodeLen = 2;
            BAsmCode[0] = 0xa8 + Code;
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
          case ModPostInc:
            CodeLen = 2;
            BAsmCode[0] = 0x98 + Code;
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
          case ModIndex:
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xd4 + (Code << 5);
            BAsmCode[1] = (HReg << 4) + AdrMode;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
          case ModAbs:
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xf2 + Code;
            BAsmCode[1] = 0xf0 + HReg;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
        }
        break;
      case ModMReg:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModImm | MModMReg | ((DPPAssumes[3] == 3) ? MModIReg : 0) | MModAbs, False, False);
        switch (AdrType)
        {
          case ModImm:
            CodeLen = 4;
            BAsmCode[0] = 0xe6 + Code;
            memcpy(BAsmCode + 2, AdrVals, 2);
            break;
          case ModMReg: /* BAsmCode[1] sicher absolut darstellbar, da Rn vorher */
                        /* abgefangen wird! */
            BAsmCode[0] = 0xf6 + Code;
            AdrLong = SFRStart() + (((Word)BAsmCode[1]) << 1);
            CalcPage(&AdrLong, True);
            BAsmCode[2] = Lo(AdrLong);
            BAsmCode[3] = Hi(AdrLong);
            BAsmCode[1] = AdrVals[0];
            CodeLen = 4;
            break;
          case ModIReg:
            CodeLen = 4; BAsmCode[0] = 0x94 + (Code << 5);
            BAsmCode[2] = BAsmCode[1] << 1;
            BAsmCode[3] = 0xfe + (BAsmCode[1] >> 7); /* ANSI :-0 */
            BAsmCode[1] = AdrMode;
            break;
          case ModAbs:
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xf2 + Code;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
        }
        break;
      case ModIReg:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg | MModIReg | MModPostInc | MModAbs, False, False);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 2;
            BAsmCode[0] = 0xb8 + Code;
            BAsmCode[1] = HReg + (AdrMode << 4);
            break;
          case ModIReg:
            CodeLen = 2;
           BAsmCode[0] = 0xc8 + Code;
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
          case ModPostInc:
            CodeLen = 2;
            BAsmCode[0] = 0xe8 + Code;
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
          case ModAbs:
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0x84 + (Code << 5);
            BAsmCode[1] = HReg; memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
        }
        break;
      case ModPreDec:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg, False, False);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 2;
            BAsmCode[0] = 0x88 + Code;
            BAsmCode[1] = HReg + (AdrMode << 4);
            break;
        }
        break;
      case ModPostInc:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModIReg, False, False);
        switch (AdrType)
        {
          case ModIReg:
            CodeLen = 2;
            BAsmCode[0] = 0xd8 + Code;
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
        }
        break;
      case ModIndex:
        BAsmCode[1] = AdrMode;
        memcpy(BAsmCode + 2, AdrVals, AdrCnt);
        DecodeAdr(ArgStr[2], MModReg, False, False);
        switch (AdrType)
        {
          case ModReg:
            BAsmCode[0] = 0xc4 + (Code << 5);
            CodeLen = 4;
            BAsmCode[1] += AdrMode << 4;
            break;
        }
        break;
      case ModAbs:
        memcpy(BAsmCode + 2, AdrVals, AdrCnt);
        DecodeAdr(ArgStr[2], MModIReg | MModMReg, False, False);
        switch (AdrType)
        {
          case ModIReg:
            CodeLen = 4;
            BAsmCode[0] = 0x94 + (Code << 5);
            BAsmCode[1] = AdrMode;
            break;
          case ModMReg:
            CodeLen = 4;
            BAsmCode[0] = 0xf6 + Code;
            BAsmCode[1] = AdrVals[0];
            break;
        }
        break;
    }
  }
}

static void DecodeMOVBS_MOVBZ(Word Code)
{
  Byte HReg;
  LongInt AdrLong;

  if (ArgCnt != 2) WrError(1110);
  else
  {
    OpSize = 1;
    DecodeAdr(ArgStr[1], MModReg | MModMReg | MModAbs, False, True);
    OpSize = 0;
    switch (AdrType)
    {
      case ModReg:
        HReg = AdrMode; DecodeAdr(ArgStr[2], MModReg | MModAbs, False, False);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 2;
            BAsmCode[0] = 0xc0 + Code;
            BAsmCode[1] = HReg + (AdrMode << 4);
            break;
          case ModAbs:
            CodeLen = 4;
            BAsmCode[0] = 0xc2 + Code;
            BAsmCode[1] = 0xf0 + HReg;
            memcpy(BAsmCode + 2, AdrVals, 2); 
            break;
        }
        break;
      case ModMReg:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAbs | MModMReg, False, False);
        switch (AdrType)
        {
          case ModMReg: /* BAsmCode[1] sicher absolut darstellbar, da Rn vorher */
                        /* abgefangen wird! */
            BAsmCode[0] = 0xc5 + Code;
            AdrLong = SFRStart() + (((Word)BAsmCode[1]) << 1);
            CalcPage(&AdrLong, True);
            BAsmCode[2] = Lo(AdrLong);
            BAsmCode[3] = Hi(AdrLong);
            BAsmCode[1] = AdrVals[0];
            CodeLen = 4;
            break;
          case ModAbs:
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xc2 + Code;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
        }
        break;
      case ModAbs:
        memcpy(BAsmCode + 2, AdrVals, AdrCnt);
        DecodeAdr(ArgStr[2], MModMReg, False, False);
        switch (AdrType)
        {
          case ModMReg:
            CodeLen = 4;
            BAsmCode[0] = 0xc5 + Code;
            BAsmCode[1] = AdrVals[0];
            break;
        }
        break;
    }
  }
}

static void DecodePUSH_POP(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModMReg, False, Code & 0x10);
    switch (AdrType)
    {
      case ModMReg:
        CodeLen = 2;
        BAsmCode[0] = Code;
        BAsmCode[1] = AdrVals[0];
        if (SPChanged) WrXError(200, RegNames[5]);
        break;
    }
  }
}

static void DecodeSCXT(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModMReg, False, True);
    switch (AdrType)
    {
      case ModMReg:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAbs | MModImm, False, False);
        if (AdrType != ModNone)
        {
          CodeLen = 4; BAsmCode[0] = 0xc6 + (Ord(AdrType == ModAbs) << 4);
          memcpy(BAsmCode + 2, AdrVals, 2);
        }
        break;
    }
  }
}

static void DecodeALU2(Word Code)
{
  Byte HReg;
  LongInt AdrLong;

  OpSize = Hi(Code);
  Code = (1 - OpSize) + (Lo(Code)  << 4);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg | MModMReg | MModAbs, False, True);
    switch (AdrType)
    {
      case ModReg:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg | MModIReg | MModPostInc | MModAbs | MModImm, False, False);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 2;
            BAsmCode[0] = Code;
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
          case ModIReg:
            if (AdrMode > 3) WrError(1350);
            else
            {
              CodeLen = 2;
              BAsmCode[0] = 0x08 + Code;
              BAsmCode[1] = (HReg << 4) + 8 + AdrMode;
            }
            break;
          case ModPostInc:
            if (AdrMode > 3) WrError(1350);
            else
            {
              CodeLen = 2;
              BAsmCode[0] = 0x08 + Code;
              BAsmCode[1] = (HReg << 4) + 12 + AdrMode;
            }
            break;
          case ModAbs:
            CodeLen = 4;
            BAsmCode[0] = 0x02 + Code;
            BAsmCode[1] = 0xf0 + HReg;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
          case ModImm:
            if (WordVal() <= 7)
            {
              CodeLen = 2;
              BAsmCode[0] = 0x08 + Code;
              BAsmCode[1] = (HReg << 4) + AdrVals[0];
            }
            else
            {
              CodeLen = 4;
              BAsmCode[0] = 0x06 + Code;
              BAsmCode[1] = 0xf0 + HReg;
              memcpy(BAsmCode + 2, AdrVals, 2);
            }
            break;
        }
        break;
      case ModMReg:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAbs | MModMReg | MModImm, False, False);
        switch (AdrType)
        {
          case ModAbs:
            CodeLen = 4;
            BAsmCode[0] = 0x02 + Code;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
          case ModMReg: /* BAsmCode[1] sicher absolut darstellbar, da Rn vorher */
                        /* abgefangen wird! */
            BAsmCode[0] = 0x04 + Code;
            AdrLong = SFRStart() + (((Word)BAsmCode[1]) << 1);
            CalcPage(&AdrLong, True);
            BAsmCode[2] = Lo(AdrLong);
            BAsmCode[3] = Hi(AdrLong);
            BAsmCode[1] = AdrVals[0];
            CodeLen = 4;
            break;
          case ModImm:
            CodeLen = 4;
            BAsmCode[0] = 0x06 + Code;
            memcpy(BAsmCode + 2, AdrVals, 2);
            break;
        }
        break;
      case ModAbs:
        memcpy(BAsmCode + 2, AdrVals, AdrCnt);
        DecodeAdr(ArgStr[2], MModMReg, False, False);
        switch (AdrType)
        {
          case ModMReg:
            CodeLen = 4;
            BAsmCode[0] = 0x04 + Code;
            BAsmCode[1] = AdrVals[0];
            break;
        }
        break;
    }
  }
}

static void DecodeCPL_NEG(Word Code)
{
  OpSize = Hi(Code);

  Code = Lo(Code) + ((1 - OpSize) << 5);
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg, False, True);
    if (AdrType == ModReg)
    {
      CodeLen = 2;
      BAsmCode[0] = Code;
      BAsmCode[1] = AdrMode << 4;
    }
  }
}

static void DecodeDiv(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg, False, False);
    if (AdrType == ModReg)
    {
      CodeLen = 2;
      BAsmCode[0] = 0x4b + (Code << 4);
      BAsmCode[1] = AdrMode * 0x11;
    }
  }
}

static void DecodeLoop(Word Code)
{
  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg, False, True);
    if (AdrType == ModReg)
    {
      BAsmCode[1] = AdrMode;
      DecodeAdr(ArgStr[2], MModAbs | MModImm, False, False);
      switch (AdrType)
      {
        case ModAbs:
          CodeLen = 4;
          BAsmCode[0] = Code + 2;
          BAsmCode[1] += 0xf0;
          memcpy(BAsmCode + 2, AdrVals, 2);
          break;
        case ModImm:
          if (WordVal() < 16)
          {
            CodeLen = 2;
            BAsmCode[0] = Code;
            BAsmCode[1] += (WordVal() << 4);
          }
          else
          {
            CodeLen = 4;
            BAsmCode[0] = Code + 6;
            BAsmCode[1] += 0xf0;
            memcpy(BAsmCode + 2, AdrVals, 2);
          }
          break;
      }
    }
  }
}

static void DecodeMul(Word Code)
{
  Byte HReg;

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg, False, False);
    switch (AdrType)
    {
      case ModReg:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg, False, False);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 2;
            BAsmCode[0] = 0x0b + (Code << 4);
            BAsmCode[1] = (HReg << 4) + AdrMode;
            break;
        }
        break;
    }
  }
}

static void DecodeShift(Word Code)
{
  Byte HReg;

  if (ArgCnt != 2) WrError(1110);
  else
  {
    OpSize = 1;
    DecodeAdr(ArgStr[1], MModReg, False, True);
    switch (AdrType)
    {
      case ModReg:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg | MModImm, False, True);
        switch (AdrType)
        {
          case ModReg:
            BAsmCode[0] = Code;
            BAsmCode[1] = AdrMode + (HReg << 4);
            CodeLen = 2;
            break;
          case ModImm:
            if (WordVal() > 15) WrError(1320);
            else
            {
              BAsmCode[0] = Code + 0x10;
              BAsmCode[1] = (WordVal() << 4) + HReg;
              CodeLen = 2;
            }
            break;
       }
       break;
    }
  }
}

static void DecodeBit2(Word Code)
{
  Byte BOfs1, BOfs2;
  Word BAdr1, BAdr2;

  if (ArgCnt != 2) WrError(1110);
  else
   if (DecodeBitAddr(ArgStr[1], &BAdr1, &BOfs1, False))
   if (DecodeBitAddr(ArgStr[2], &BAdr2, &BOfs2, False))
   {
     CodeLen = 4;
     BAsmCode[0] = Code;
     BAsmCode[1] = BAdr2;
     BAsmCode[2] = BAdr1;
     BAsmCode[3] = (BOfs2 << 4) + BOfs1;
   }
}

static void DecodeBCLR_BSET(Word Code)
{
  Byte BOfs;
  Word BAdr;

  if (ArgCnt != 1) WrError(1110);
  else if (DecodeBitAddr(ArgStr[1], &BAdr, &BOfs, False))
  {
    CodeLen = 2;
    BAsmCode[0] = (BOfs << 4) + Code;
    BAsmCode[1] = BAdr;
  }
}

static void DecodeBFLDH_BFLDL(Word Code)
{
  Byte BOfs;
  Word BAdr;

  if (ArgCnt != 3) WrError(1110);
  else 
  {
    strmaxcat(ArgStr[1], ".0", 255);
    if (DecodeBitAddr(ArgStr[1], &BAdr, &BOfs, False))
    {
      OpSize = 0;
      BAsmCode[1] = BAdr;
      DecodeAdr(ArgStr[2], MModImm, False, False);
      if (AdrType == ModImm)
      {
        BAsmCode[2] = AdrVals[0];
        DecodeAdr(ArgStr[3], MModImm, False, False);
        if (AdrType == ModImm)
        {
          BAsmCode[3] = AdrVals[0];
          CodeLen = 4;
          BAsmCode[0] = Code;
          if (Code & 0x10)
          {
            BAdr = BAsmCode[2];
            BAsmCode[2] = BAsmCode[3];
            BAsmCode[3] = BAdr;
          }
        }
      }
    }
  }
}

static void DecodeJMP(Word Code)
{
  UNUSED(Code);

  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    int Cond = (ArgCnt == 1) ? TrueCond : DecodeCondition(ArgStr[1]);
    if (Cond >= ConditionCount) WrXError(1360, ArgStr[1]);
    else
    {
      DecodeAdr(ArgStr[ArgCnt], MModAbs | MModLAbs | MModIReg, True, False);
      switch (AdrType)
      {
        case ModLAbs:
          if (Cond != TrueCond) WrXError(1360, ArgStr[1]);
          else
          {
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xfa;
            BAsmCode[1] = AdrMode;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
          }
          break;       
        case ModAbs:
        {
          LongInt AdrLong = WordVal() - (EProgCounter() + 2);
          if ((AdrLong <= 254) && (AdrLong >= -256) && ((AdrLong & 1) == 0))
          {
            CodeLen = 2;
            BAsmCode[0] = 0x0d + (Conditions[Cond].Code << 4);
            BAsmCode[1] = (AdrLong / 2) & 0xff;
          }
          else
          {
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xea;
            BAsmCode[1] = Conditions[Cond].Code << 4;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
          }
          break;
        }
        case ModIReg:
          CodeLen = 2; BAsmCode[0] = 0x9c;
          BAsmCode[1] = (Conditions[Cond].Code << 4) + AdrMode;
          break;
      }
    }
  }
}

static void DecodeCALL(Word Code)
{
  UNUSED(Code);

  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    int Cond = (ArgCnt == 1) ? TrueCond : DecodeCondition(ArgStr[1]);
    if (Cond >= ConditionCount) WrXError(1360, ArgStr[1]);
    else
    {
      DecodeAdr(ArgStr[ArgCnt], MModAbs | MModLAbs | MModIReg, True, False);
      switch (AdrType)
      {
        case ModLAbs:
          if (Cond != TrueCond) WrXError(1360, ArgStr[1]);
          else
          {
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xda;
            BAsmCode[1] = AdrMode;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
          }
          break;
        case ModAbs:
        {
          LongInt AdrLong = WordVal() - (EProgCounter() + 2);
          if ((AdrLong <= 254) && (AdrLong >= -256) && ((AdrLong&1) == 0) && (Cond == TrueCond))
          {
            CodeLen = 2;
            BAsmCode[0] = 0xbb;
            BAsmCode[1] = (AdrLong / 2) & 0xff;
          }
          else
          {
            CodeLen = 2 + AdrCnt;
            BAsmCode[0] = 0xca;
            BAsmCode[1] = 0x00 + (Conditions[Cond].Code << 4);
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
          }
          break;
        }
        case ModIReg:
          CodeLen = 2;
          BAsmCode[0] = 0xab;
          BAsmCode[1] = (Conditions[Cond].Code << 4) + AdrMode;
          break;
      }
    }
  }
}

static void DecodeJMPR(Word Code)
{
  UNUSED(Code);

  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    int Cond = (ArgCnt == 1) ? TrueCond : DecodeCondition(ArgStr[1]);
    if (Cond >= ConditionCount) WrXError(1360, ArgStr[1]);
    else
    {
      Boolean OK;
      LongInt AdrLong = EvalIntExpression(ArgStr[ArgCnt], MemInt, &OK) - (EProgCounter() + 2);
      if (OK)
      {
        if (AdrLong & 1) WrError(1375);
        else if ((!SymbolQuestionable) && ((AdrLong > 254) || (AdrLong < -256))) WrError(1370);
        else
        {
          CodeLen = 2;
          BAsmCode[0] = 0x0d + (Conditions[Cond].Code << 4);
          BAsmCode[1] = (AdrLong / 2) & 0xff;
        }
      }
    }
  }
}

static void DecodeCALLR(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;
    LongInt AdrLong = EvalIntExpression(ArgStr[ArgCnt], MemInt, &OK) - (EProgCounter() + 2);
    if (OK)
    {
      if (AdrLong & 1) WrError(1375);
      else if ((!SymbolQuestionable) && ((AdrLong > 254) || (AdrLong < -256))) WrError(1370);
      else
      {
        CodeLen = 2;
        BAsmCode[0] = 0xbb;
        BAsmCode[1] = (AdrLong / 2) & 0xff;
      }
    }
  }
}

static void DecodeJMPA_CALLA(Word Code)
{
  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    int Cond = (ArgCnt == 1) ? TrueCond : DecodeCondition(ArgStr[1]);
    if (Cond >= ConditionCount) WrXError(1360, ArgStr[1]);
    else
    {
      Boolean OK;
      LongInt AdrLong = EvalIntExpression(ArgStr[ArgCnt], MemInt, &OK);
      if (OK)
      {
        if ((AdrLong >> 16) != (EProgCounter() >> 16)) WrError(1910);
        else
        {
          CodeLen = 4;
          BAsmCode[0] = Code;
          BAsmCode[1] = 0x00 + (Conditions[Cond].Code << 4);
          BAsmCode[2] = Lo(AdrLong);
          BAsmCode[3] = Hi(AdrLong);
        }
      }
    }
  }
}

static void DecodeJMPS_CALLS(Word Code)
{
  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    Boolean OK;
    Word AdrWord;
    Byte AdrBank;
    LongInt AdrLong;

    if (ArgCnt == 1)
    {
      AdrLong = EvalIntExpression(ArgStr[1], MemInt, &OK);
      AdrWord = AdrLong & 0xffff;
      AdrBank = AdrLong >> 16;
    }
    else
    {
      AdrWord = EvalIntExpression(ArgStr[2], UInt16, &OK);
      AdrBank = OK ? EvalIntExpression(ArgStr[1], MemInt2, &OK) : 0;
    }
    if (OK)
    {
      CodeLen = 4;
      BAsmCode[0] = Code;
      BAsmCode[1] = AdrBank;
      BAsmCode[2] = Lo(AdrWord);
      BAsmCode[3] = Hi(AdrWord);
    }
  }
}

static void DecodeJMPI_CALLI(Word Code)
{
  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    int Cond = (ArgCnt == 1) ? TrueCond : DecodeCondition(ArgStr[1]);
    if (Cond >= ConditionCount) WrXError(1360, ArgStr[1]);
    else
    {
      DecodeAdr(ArgStr[ArgCnt], MModIReg, True, False);
      switch (AdrType)
      {
        case ModIReg:
          CodeLen = 2;
          BAsmCode[0] = Code;
          BAsmCode[1] = AdrMode + (Conditions[Cond].Code << 4);
          break;
      }
    }
  }
}

static void DecodeBJmp(Word Code)
{
  Byte BOfs;
  Word BAdr;

  if (ArgCnt != 2) WrError(1110);
  else if (DecodeBitAddr(ArgStr[1], &BAdr, &BOfs, False))
  {
    Boolean OK;
    LongInt AdrLong = EvalIntExpression(ArgStr[2], MemInt, &OK) - (EProgCounter() + 4);
    if (OK)
    {
      if ((AdrLong&1) == 1) WrError(1375);
      else if ((!SymbolQuestionable) && ((AdrLong < -256) || (AdrLong > 254))) WrError(1370);
      else
      {
        CodeLen = 4; BAsmCode[0] = 0x8a + (Code << 4);
        BAsmCode[1] = BAdr;
        BAsmCode[2] = (AdrLong / 2) & 0xff;
        BAsmCode[3] = BOfs << 4;
      }
    }
  }
}

static void DecodePCALL(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModMReg, False, False);
    switch (AdrType)
    {
      case ModMReg:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAbs, True, False);
        switch (AdrType)
        {
          case ModAbs:
            CodeLen = 4;
            BAsmCode[0] = 0xe2;
            memcpy(BAsmCode + 2, AdrVals, 2);
            break;
        }
        break;
    }
  }
}

static void DecodeRETP(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModMReg, False, False);
    switch (AdrType)
    {
      case ModMReg:
        BAsmCode[1] = AdrVals[0];
        BAsmCode[0] = 0xeb;
        CodeLen = 2;
        if (SPChanged)
          WrXError(200, RegNames[5]);
        break;
    }
  }
}

static void DecodeTRAP(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else if (*ArgStr[1] != '#') WrError(1350);
  else
  {
    Boolean OK;

    BAsmCode[1] = EvalIntExpression(ArgStr[1] + 1, UInt7, &OK) << 1;
    if (OK)
    {
      BAsmCode[0] = 0x9b;
      CodeLen = 2;
    }
  }
}

static void DecodeATOMIC(Word Code)
{
  Byte HReg;

  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else if (MomCPU < CPU80C167) WrError(1500);
  else if (DecodePref(ArgStr[1], &HReg))
  {
    CodeLen = 2;
    BAsmCode[0] = 0xd1;
    BAsmCode[1] = HReg << 4;
  }
}

static void DecodeEXTR(Word Code)
{
  Byte HReg;

  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else if (MomCPU < CPU80C167) WrError(1500);
  else if (DecodePref(ArgStr[1], &HReg))
  {
    CodeLen = 2;
    BAsmCode[0] = 0xd1;
    BAsmCode[1] = 0x80 + (HReg << 4);
    ExtCounter = HReg + 1;
    ExtSFRs = True;
  }
}

static void DecodeEXTP_EXTPR(Word Code)
{
  Byte HReg;

  if (ArgCnt != 2) WrError(1110);
  else if (MomCPU < CPU80C167) WrError(1500);
  else if (DecodePref(ArgStr[2], &HReg))
  {
    DecodeAdr(ArgStr[1], MModReg | MModImm, False, False);
    switch (AdrType)
    {
      case ModReg:
        CodeLen = 2;
        BAsmCode[0] = 0xdc;
        BAsmCode[1] = Code + 0x40 + (HReg << 4) + AdrMode;
        ExtCounter = HReg + 1;
        MemMode = MemModeZeroPage;
        break;
      case ModImm:
        CodeLen = 4;
        BAsmCode[0] = 0xd7;
        BAsmCode[1] = Code + 0x40 + (HReg << 4);
        BAsmCode[2] = WordVal() & 0xff;
        BAsmCode[3] = (WordVal() >> 8) & 3;
        ExtCounter = HReg + 1;
        MemMode = MemModeFixedPage;
        MemPage = WordVal() & 0x3ff;
        break;
    }
  }
}

static void DecodeEXTS_EXTSR(Word Code)
{
  Byte HReg;

  OpSize = 0;
  if (ArgCnt != 2) WrError(1110);
  else if (MomCPU < CPU80C167) WrError(1500);
  else if (DecodePref(ArgStr[2], &HReg))
  {
    DecodeAdr(ArgStr[1], MModReg | MModImm, False, False);
    switch (AdrType)
    {
      case ModReg:
        CodeLen = 2;
        BAsmCode[0] = 0xdc;
        BAsmCode[1] = Code + 0x00 + (HReg << 4) + AdrMode;
        ExtCounter = HReg + 1;
        MemMode = MemModeNoCheck;
        break;
      case ModImm:
        CodeLen = 4;
        BAsmCode[0] = 0xd7;
        BAsmCode[1] = Code + 0x00 + (HReg << 4);
        BAsmCode[2] = AdrVals[0];
        BAsmCode[3] = 0;
        ExtCounter = HReg + 1;
        MemMode = MemModeFixedBank;
        MemPage = AdrVals[0];
        break;
    }
  }
}

/*-------------------------------------------------------------------------*/

static void AddBInstTable(char *NName, Word NCode, InstProc Proc)
{
  char BName[30];

  AddInstTable(InstTable, NName, NCode | 0x0100, Proc);
  sprintf(BName, "%sB", NName);
  AddInstTable(InstTable, BName, NCode, Proc);
}

static void AddFixed(char *NName, CPUVar NMin, Word NCode1, Word NCode2)
{
  if (InstrZ >= FixedOrderCount) exit(255);
  FixedOrders[InstrZ].MinCPU = NMin;
  FixedOrders[InstrZ].Code1 = NCode1;
  FixedOrders[InstrZ].Code2 = NCode2;
  AddInstTable(InstTable, NName, InstrZ++, DecodeFixed);
}

static void AddShift(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeShift);
}

static void AddBit2(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeBit2);
}

static void AddLoop(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeLoop);
}

static void AddCondition(char *NName, Byte NCode)
{
  if (InstrZ >= ConditionCount) exit(255);
  Conditions[InstrZ].Name = NName;
  Conditions[InstrZ++].Code = NCode;
}

static void InitFields(void)
{
  InstTable = CreateInstTable(201);
  SetDynamicInstTable(InstTable);
  AddBInstTable("MOV", 0, DecodeMOV);
  AddInstTable(InstTable, "MOVBS", 0x10, DecodeMOVBS_MOVBZ);
  AddInstTable(InstTable, "MOVBZ", 0x00, DecodeMOVBS_MOVBZ);
  AddInstTable(InstTable, "PUSH", 0xec, DecodePUSH_POP);
  AddInstTable(InstTable, "POP", 0xfc, DecodePUSH_POP);
  AddInstTable(InstTable, "SCXT", 0, DecodeSCXT);
  AddBInstTable("CPL", 0x91, DecodeCPL_NEG);
  AddBInstTable("NEG", 0x81, DecodeCPL_NEG);
  AddInstTable(InstTable, "BCLR", 0x0e, DecodeBCLR_BSET);
  AddInstTable(InstTable, "BSET", 0x0f, DecodeBCLR_BSET);
  AddInstTable(InstTable, "BFLDL", 0x0a, DecodeBFLDH_BFLDL);
  AddInstTable(InstTable, "BFLDH", 0x1a, DecodeBFLDH_BFLDL);
  AddInstTable(InstTable, "JMP", 0, DecodeJMP);
  AddInstTable(InstTable, "CALL", 0, DecodeCALL);
  AddInstTable(InstTable, "JMPR", 0, DecodeJMPR);
  AddInstTable(InstTable, "CALLR", 0, DecodeCALLR);
  AddInstTable(InstTable, "JMPA", 0xea, DecodeJMPA_CALLA);
  AddInstTable(InstTable, "CALLA", 0xca, DecodeJMPA_CALLA);
  AddInstTable(InstTable, "JMPS", 0xfa, DecodeJMPS_CALLS);
  AddInstTable(InstTable, "CALLS", 0xda, DecodeJMPS_CALLS);
  AddInstTable(InstTable, "JMPI", 0x9c, DecodeJMPI_CALLI);
  AddInstTable(InstTable, "CALLI", 0xab, DecodeJMPI_CALLI);
  AddInstTable(InstTable, "PCALL", 0, DecodePCALL);
  AddInstTable(InstTable, "RETP", 0, DecodeRETP);
  AddInstTable(InstTable, "TRAP", 0, DecodeTRAP);
  AddInstTable(InstTable, "ATOMIC", 0, DecodeATOMIC);
  AddInstTable(InstTable, "EXTR", 0, DecodeEXTR);
  AddInstTable(InstTable, "EXTP", 0x00, DecodeEXTP_EXTPR);
  AddInstTable(InstTable, "EXTPR", 0x80, DecodeEXTP_EXTPR);
  AddInstTable(InstTable, "EXTS", 0x00, DecodeEXTS_EXTSR);
  AddInstTable(InstTable, "EXTSR", 0x80, DecodeEXTS_EXTSR);

  FixedOrders = (BaseOrder *) malloc(FixedOrderCount * sizeof(BaseOrder)); InstrZ = 0;
  AddFixed("DISWDT", CPU80C166, 0x5aa5, 0xa5a5);
  AddFixed("EINIT" , CPU80C166, 0x4ab5, 0xb5b5);
  AddFixed("IDLE"  , CPU80C166, 0x7887, 0x8787);
  AddFixed("NOP"   , CPU80C166, 0x00cc, 0x0000);
  AddFixed("PWRDN" , CPU80C166, 0x6897, 0x9797);
  AddFixed("RET"   , CPU80C166, 0x00cb, 0x0000);
  AddFixed("RETI"  , CPU80C166, 0x88fb, 0x0000);
  AddFixed("RETS"  , CPU80C166, 0x00db, 0x0000);
  AddFixed("SRST"  , CPU80C166, 0x48b7, 0xb7b7);
  AddFixed("SRVWDT", CPU80C166, 0x58a7, 0xa7a7);

  Conditions = (Condition *) malloc(sizeof(Condition) * ConditionCount); InstrZ = 0;
  TrueCond = InstrZ; AddCondition("UC" , 0x0); AddCondition("Z"  , 0x2);
  AddCondition("NZ" , 0x3); AddCondition("V"  , 0x4);
  AddCondition("NV" , 0x5); AddCondition("N"  , 0x6);
  AddCondition("NN" , 0x7); AddCondition("C"  , 0x8);
  AddCondition("NC" , 0x9); AddCondition("EQ" , 0x2);
  AddCondition("NE" , 0x3); AddCondition("ULT", 0x8);
  AddCondition("ULE", 0xf); AddCondition("UGE", 0x9);
  AddCondition("UGT", 0xe); AddCondition("SLT", 0xc);
  AddCondition("SLE", 0xb); AddCondition("SGE", 0xd);
  AddCondition("SGT", 0xa); AddCondition("NET", 0x1);

  InstrZ = 0;
  AddBInstTable("ADD" , InstrZ++, DecodeALU2);
  AddBInstTable("ADDC", InstrZ++, DecodeALU2);
  AddBInstTable("SUB" , InstrZ++, DecodeALU2);
  AddBInstTable("SUBC", InstrZ++, DecodeALU2);
  AddBInstTable("CMP" , InstrZ++, DecodeALU2);
  AddBInstTable("XOR" , InstrZ++, DecodeALU2);
  AddBInstTable("AND" , InstrZ++, DecodeALU2);
  AddBInstTable("OR"  , InstrZ++, DecodeALU2);

  AddShift("ASHR", 0xac); AddShift("ROL" , 0x0c);
  AddShift("ROR" , 0x2c); AddShift("SHL" , 0x4c);
  AddShift("SHR" , 0x6c);

  AddBit2("BAND", 0x6a); AddBit2("BCMP" , 0x2a);
  AddBit2("BMOV", 0x4a); AddBit2("BMOVN", 0x3a);
  AddBit2("BOR" , 0x5a); AddBit2("BXOR" , 0x7a);

  AddLoop("CMPD1", 0xa0); AddLoop("CMPD2", 0xb0);
  AddLoop("CMPI1", 0x80); AddLoop("CMPI2", 0x90);

  InstrZ = 0;
  AddInstTable(InstTable, "DIV"  , InstrZ++, DecodeDiv);
  AddInstTable(InstTable, "DIVU" , InstrZ++, DecodeDiv);
  AddInstTable(InstTable, "DIVL" , InstrZ++, DecodeDiv);
  AddInstTable(InstTable, "DIVLU", InstrZ++, DecodeDiv);

  InstrZ = 0;
  AddInstTable(InstTable, "JB"   , InstrZ++, DecodeBJmp);
  AddInstTable(InstTable, "JNB"  , InstrZ++, DecodeBJmp);
  AddInstTable(InstTable, "JBC"  , InstrZ++, DecodeBJmp);
  AddInstTable(InstTable, "JNBS" , InstrZ++, DecodeBJmp);

  InstrZ = 0;
  AddInstTable(InstTable, "MUL"  , InstrZ++, DecodeMul);
  AddInstTable(InstTable, "MULU" , InstrZ++, DecodeMul);
  AddInstTable(InstTable, "PRIOR", InstrZ++, DecodeMul);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
  free(FixedOrders);
  free(Conditions);
}

static void MakeCode_166(void)
{
  int z;

  CodeLen = 0;
  DontPrint = False;
  OpSize = 1;

  /* zu ignorierendes */

  if (Memo(""))
    return;

  /* Pseudoanweisungen */

  if (DecodePseudo())
    return;

  if (DecodeIntelPseudo(False))
    return;

  /* Pipeline-Flags weiterschalten */

  SPChanged = N_SPChanged; N_SPChanged = False;
  CPChanged = N_CPChanged; N_CPChanged = False;
  for (z = 0; z < DPPCount; z++)
  {
    DPPChanged[z] = N_DPPChanged[z];
    N_DPPChanged[z] = False;
  }

  /* Praefixe herunterzaehlen */

  if (ExtCounter >= 0)
   if (--ExtCounter < 0)
   {
     MemMode = MemModeStd;
     ExtSFRs = False;
   }

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200, OpPart);
}

static void InitCode_166(void)
{
  int z;

  for (z = 0; z < DPPCount; z++)
  {
    DPPAssumes[z] = z;
    N_DPPChanged[z] = False;
  }
  N_CPChanged = False;
  N_SPChanged = False;

  MemMode = MemModeStd;
  ExtSFRs = False;
  ExtCounter = (-1);
}

static Boolean IsDef_166(void)
{
  return (Memo("BIT")) || (Memo("REG"));
}

static void SwitchFrom_166(void)
{
  DeinitFields();
}

static void SwitchTo_166(void)
{
  Byte z;

  TurnWords = False;
  ConstMode = ConstModeIntel;
  SetIsOccupied = False;
  OpSize = 1;

  PCSymbol = "$";
  HeaderID = 0x4c;
  NOPCode = 0xcc00;
  DivideChars = ",";
  HasAttrs = False;

  ValidSegs = (1 << SegCode);
  Grans[SegCode] = 1; ListGrans[SegCode] = 1; SegInits[SegCode] = 0;

  MakeCode = MakeCode_166;
  IsDef = IsDef_166;
  SwitchFrom = SwitchFrom_166;

  if (MomCPU == CPU80C166)
  {
    MemInt = UInt18;
    MemInt2 = UInt2;
    ASSUME166s[0].Max = 15;
    SegLimits[SegCode] = 0x3ffffl;
  }
  else
  {
    MemInt = UInt24;
    MemInt2 = UInt8;
    ASSUME166s[0].Max = 1023;
    SegLimits[SegCode] = 0xffffffl;
  }
  for (z = 1; z < 4; z++)
    ASSUME166s[z].Max = ASSUME166s[0].Max;

  pASSUMERecs = ASSUME166s;
  ASSUMERecCnt = ASSUME166Count;

  InitFields();
}

void code166_init(void)
{
  CPU80C166 = AddCPU("80C166", SwitchTo_166);
  CPU80C167 = AddCPU("80C167", SwitchTo_166);
  CPU80C167CS = AddCPU("80C167CS", SwitchTo_166);

  AddInitPassProc(InitCode_166);
}
