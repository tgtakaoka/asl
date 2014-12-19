/* code870c.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator TLCS-870                                                    */
/*                                                                           */
/*****************************************************************************/
/* $Id: code870c.c,v 1.7 2014/11/23 17:53:46 alfred Exp $                  */
/*****************************************************************************
 * $Log: code870c.c,v $
 * Revision 1.7  2014/11/23 17:53:46  alfred
 * - add J pseudo instruction
 * - correct P and M conditions
 *
 * Revision 1.6  2014/11/23 17:05:54  alfred
 * *** empty log message ***
 *
 * Revision 1.5  2014/11/22 11:47:36  alfred
 * - first complete TLCS-870/C target
 *
 * Revision 1.4  2014/11/21 22:11:37  alfred
 * - up to LD CF
 *
 * Revision 1.3  2014/11/19 22:55:01  alfred
 * - CMP works, possibly other ALU ops
 *
 * Revision 1.2  2014/11/18 18:33:28  alfred
 * - got up to before XCH
 *
 * Revision 1.1  2014/11/17 23:51:32  alfred
 * - begun with TLCS-870/C
 *
 *****************************************************************************/

#include "stdinc.h" 

#include <ctype.h>
#include <string.h>

#include "nls.h"
#include "bpemu.h"
#include "strutil.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmitree.h"
#include "codepseudo.h"
#include "intpseudo.h"
#include "codevars.h"
#include "headids.h"

typedef struct
{
  char *Name;
  Word Code;
} CondRec;

#define ConditionCnt 20

enum
{
  ModNone = -1,
  ModReg8 = 0,
  ModReg16 = 1,
  ModImm = 2,
  ModMem = 3,
};

#define MModReg8 (1 << ModReg8)
#define MModReg16 (1 << ModReg16)
#define MModImm (1 << ModImm)
#define MModAbs (1 << ModAbs)
#define MModMem (1 << ModMem)

#define AccReg 0
#define WAReg 0

#define Reg8Cnt 8
static char *Reg8Names = "AWCBEDLH";

static CPUVar CPU870C;
static ShortInt OpSize;
static Byte AdrVals[4];
static ShortInt AdrType;
static Byte AdrMode;

static CondRec *Conditions;

/*--------------------------------------------------------------------------*/

static Boolean DecodeRegDisp(char *Asc, Byte *pRegFlag, LongInt *pDispAcc, Boolean *pFirstFlag)
{
  static char *AdrRegs[] = 
  {
    "DE", "HL", "IX", "IY", "SP", "C"
  };
  static const int AdrRegCnt = sizeof(AdrRegs) / sizeof(*AdrRegs);
  Boolean OK, NegFlag, NNegFlag;
  char *PPos, *NPos, *EPos;
  char *AdrPart;
  LongInt DispPart;
  int z;

  *pRegFlag = 0;
  *pDispAcc = 0;
  NegFlag = False;
  OK = True;
  *pFirstFlag = False;
  AdrPart = Asc;
  while ((OK) && (AdrPart))
  {
    PPos = QuotPos(AdrPart, '+');
    NPos = QuotPos(AdrPart, '-');
    if (!PPos)
      EPos = NPos;
    else if (!NPos)
      EPos = PPos;
    else
      EPos = min(NPos, PPos);
    NNegFlag = ((EPos) && (*EPos == '-'));
    if (EPos)
      *EPos = '\0';

    for (z = 0; z < AdrRegCnt; z++)
      if (!strcasecmp(AdrPart, AdrRegs[z]))
        break;
    if (z >= AdrRegCnt)
    {
      FirstPassUnknown = False;
      DispPart = EvalIntExpression(AdrPart, Int32, &OK);
      *pFirstFlag = *pFirstFlag || FirstPassUnknown;
      *pDispAcc = NegFlag ? *pDispAcc - DispPart :  *pDispAcc + DispPart;
    }
    else if ((NegFlag) || (*pRegFlag & (1 << z)))
    {
      WrError(1350);
      OK = False;
    }
    else
      *pRegFlag |= 1 << z;

    NegFlag = NNegFlag;
    AdrPart = EPos ? EPos + 1 : NULL;
  }
  if (*pDispAcc != 0)
    *pRegFlag |= 1 << AdrRegCnt;

  return OK;
}

static void DecodeAdr(char *Asc, Byte Erl, Boolean IsDest)
{
  static char *Reg16Names[] = 
  {
    "WA", "BC", "DE", "HL", "IX", "IY", "SP"
  };
  static const int Reg16Cnt = sizeof(Reg16Names) / sizeof(*Reg16Names);

  int z;
  Byte RegFlag;
  LongInt DispAcc;
  Boolean OK, FirstFlag;

  AdrType = ModNone;
  AdrCnt = 0;

  if (strlen(Asc) == 1)
  {
    for (z = 0; z < Reg8Cnt; z++)
      if (mytoupper(*Asc) == Reg8Names[z])
      {
        AdrType = ModReg8;
        OpSize = 0;
        AdrMode = z;
        goto chk;
      }
  }

  for (z = 0; z < Reg16Cnt; z++)
    if (!strcasecmp(Asc, Reg16Names[z]))
    {
      AdrType = ModReg16;
      OpSize = 1;
      AdrMode = z;
      goto chk;
    }

  if (IsIndirect(Asc))
  {
    Asc++;
    Asc[strlen(Asc) - 1] = '\0';

    if ((!strcasecmp(Asc, "+SP")) && (!IsDest))
    {
      AdrType = ModMem;
      AdrMode = 0xe6;
      goto chk;
    }
    if ((!strcasecmp(Asc, "SP-")) && (IsDest))
    {
      AdrType = ModMem;
      AdrMode = 0xe6;
      goto chk;
    }
    if ((!strcasecmp(Asc, "PC+A")) && (!IsDest))
    {
      AdrType = ModMem;
      AdrMode = 0x4f;
      goto chk;
    }

    if (DecodeRegDisp(Asc, &RegFlag, &DispAcc, &FirstFlag))
      switch (RegFlag)
      {
        case 0x40: /* (nnnn) (nn) */
          AdrType = ModMem;
          AdrVals[0] = DispAcc & 0xff;
          if (DispAcc > 0xff)
          {
            AdrMode = 0xe1;
            AdrCnt = 2;
            AdrVals[1] = (DispAcc >> 8) & 0xff;
          }
          else
          {
            AdrMode = 0xe0;
            AdrCnt = 1;
          }
          break;
        case 0x08: /* (IY) */
          AdrType = ModMem;
          AdrMode = 0xe5;
          break;
        case 0x04: /* (IX) */
          AdrType = ModMem;
          AdrMode = 0xe4;
          break;
        case 0x02: /* (HL) */
          AdrType = ModMem;
          AdrMode = 0xe3;
          break;
        case 0x01: /* (DE) */
          AdrType = ModMem;
          AdrMode = 0xe2;
          break;
        case 0x50: /* (SP+dd) */
          if (FirstFlag)
            DispAcc &= 0x7f;
          if (ChkRange(DispAcc, -128, 127))
          {
            AdrType = ModMem;
            AdrMode = 0xd6;
            AdrCnt = 1;
            AdrVals[0] = DispAcc & 0xff;
          }
          break;
        case 0x48: /* (IY+dd) */
          if (FirstFlag)
            DispAcc &= 0x7f;
          if (ChkRange(DispAcc, -128, 127))
          {
            AdrType = ModMem;
            AdrMode = 0xd5;
            AdrCnt = 1;
            AdrVals[0] = DispAcc & 0xff;
          }
          break;
        case 0x44: /* (IX+dd) */
          if (FirstFlag)
            DispAcc &= 0x7f;
          if (ChkRange(DispAcc, -128, 127))
          {
            AdrType = ModMem;
            AdrMode = 0xd4;
            AdrCnt = 1;
            AdrVals[0] = DispAcc & 0xff;
          }
          break;
        case 0x42: /* (HL+dd) */
          if (FirstFlag)
            DispAcc &= 0x7f;
          if (ChkRange(DispAcc, -128, 127))
          {
            AdrType = ModMem;
            AdrMode = 0xd7;
            AdrCnt = 1;
            AdrVals[0] = DispAcc & 0xff;
          }
          break;
        case 0x22:  /* (HL+c) */
          AdrType = ModMem;
          AdrMode = 0xe7;
          break;
        default:
          WrError(1350);
      }
    goto chk;
  }
  else
  {
    switch (OpSize)
    {
      case -1:
        WrError(1132);
        break;
      case 0:
        AdrVals[0] = EvalIntExpression(Asc, Int8, &OK);
        if (OK)
        {
          AdrType = ModImm;
          AdrCnt = 1;
        }
        break;
      case 1:
        DispAcc = EvalIntExpression(Asc, Int16, &OK);
        if (OK)
        {
          AdrType = ModImm;
          AdrCnt = 2;
          AdrVals[0] = DispAcc & 0xff;
          AdrVals[1] = (DispAcc >> 8) & 0xff;
        }
        break;
    }
  }

chk:
  if ((AdrType != ModNone) && (!((1<<AdrType) & Erl)))
  {
    AdrType = ModNone;
    AdrCnt = 0;
    WrError(1350);
  }
}

static Byte MakeDestMode(Byte AdrMode)
{
  if ((AdrMode & 0xf0) == 0xe0)
    return AdrMode + 0x10;
  else
    return AdrMode - 0x80;
}

static Boolean DecodeSPDisp(char *Asc, Byte *pDisp, Boolean *pDispNeg)
{
  Boolean OK;
  LongInt DispAcc;

  *pDisp = 0;
  *pDispNeg = False;

  /* avoid ambiguities - LD SP,SP should be coded as LD rr,rr */

  if (IsIndirect(Asc))
    return False;
  if (strncasecmp(Asc, "SP", 2))
    return False;
  if (strlen(Asc) < 3)
    return False;

  DispAcc = EvalIntExpression(Asc + 2, Int16, &OK);
  if (!OK)
    return False;

  if (FirstPassUnknown)
    DispAcc &= 0xff;
  if (ChkRange(DispAcc, -255, 255))
  {
    *pDispNeg = DispAcc < 0;
    *pDisp = *pDispNeg ? -DispAcc : DispAcc;
  }
  return True; /* return True even if disp is out of range, addressing mode was properly detected */
}

static Boolean SplitBit(char *Asc, Byte *Erg)
{
  char *p;

  p = RQuotPos(Asc, '.');
  if (!p)
    return False;
  *p++ = '\0';

  if (strlen(p) != 1) return False;
  else if ((*p >= '0') && (*p <= '7'))
  {
    *Erg = *p - '0';
    return True;
  }
  else if (toupper(*p) == 'A')
  {
    *Erg = 8;
    return True;
  }
  else
    return False;
}

static void CodeMem(Byte Entry, Byte Opcode)
{
  BAsmCode[0] = Entry + AdrMode;
  memcpy(BAsmCode + 1, AdrVals, AdrCnt);
  BAsmCode[1 + AdrCnt] = Opcode;
}

static int DecodeCondition(char *pCondStr, int Start)
{
  int Condition;

  NLS_UpString(pCondStr);
  for (Condition = Start; Condition < ConditionCnt; Condition++)
    if (!strcmp(ArgStr[1], Conditions[Condition].Name))
      break;

  return Condition;
}

/*--------------------------------------------------------------------------*/

static void DecodeFixed(Word Code)
{
  if (ArgCnt != 0) WrError(1110);
  else
  {
    CodeLen = 0;
    if (Hi(Code) != 0)
      BAsmCode[CodeLen++] = Hi(Code);
    BAsmCode[CodeLen++] = Lo(Code);
  }
}

static void DecodeLD(Word Code)
{
  Byte HReg, Bit, HCnt, HMode, HVals[2];
  Boolean OK, NegFlag;

  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else if (!strcasecmp(ArgStr[1], "PSW"))
  {
    BAsmCode[2] = EvalIntExpression(ArgStr[2], Int8, &OK);
    if (OK)
    {
      BAsmCode[0] = 0xe8;
      BAsmCode[1] = 0xde;
      CodeLen = 3;
    }
  }
  else if (!strcasecmp(ArgStr[1], "RBS"))
  {
    BAsmCode[1] = EvalIntExpression(ArgStr[2], UInt1, &OK) << 1;
    if (OK)
    {
      BAsmCode[0] = 0xf9;
      CodeLen = 2;
    }
  }
  else if ((!strcasecmp(ArgStr[1], "SP")) && (DecodeSPDisp(ArgStr[2], BAsmCode + 1, &NegFlag)))
  {
    BAsmCode[0] = NegFlag ? 0x3f : 0x37;
    CodeLen = 2;
  }
  else if (!strcasecmp(ArgStr[1], "CF"))
  {
    if (!SplitBit(ArgStr[2], &Bit)) WrError(1510);
    else
    {
      DecodeAdr(ArgStr[2], (Bit < 8 ? MModReg8 : 0) | MModMem, False);
      switch (AdrType)
      {
        case ModReg8:
          CodeLen = 2;
          BAsmCode[0] = 0xe8 | AdrMode;
          BAsmCode[1] = 0x58 | Bit;
          break;
        case ModMem:
          if ((Bit < 8) && (AdrMode == 0xe0))
          {
            CodeLen = 2;
            BAsmCode[0] = 0x58 | Bit;
            BAsmCode[1] = AdrVals[0];
          }
          else if (Bit < 8)
          {
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, 0x58 | Bit);
          }
          else
          {
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, 0xfc);
          }
          break;
      }
    }
  }
  else if (!strcasecmp(ArgStr[2], "CF"))
  {
    if (!SplitBit(ArgStr[1], &Bit)) WrError(1510);
    else
    {
      DecodeAdr(ArgStr[1], (Bit < 8 ? MModReg8 : 0) | MModMem, False);
      switch (AdrType)
      {
        case ModReg8:
          CodeLen = 2;
          BAsmCode[0] = 0xe8 | AdrMode;
          BAsmCode[1] = 0xe8 | Bit;
          break;
        case ModMem:
          if (Bit < 8)
          {
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, 0xe8 | Bit);
          }
          else
          {
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, 0xf3);
          }
          break;
      }
    }
  }
  else
  {
    DecodeAdr(ArgStr[1], MModReg8 | MModReg16 | MModMem, TRUE);
    switch (AdrType)
    {
      case ModReg8:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg8 | MModMem | MModImm, FALSE);
        switch (AdrType)
        {
          case ModReg8:
            if (HReg == AccReg)
            {
              CodeLen = 1; /* OK */
              BAsmCode[0] = 0x10 | AdrMode;
            }
            else if (AdrMode == AccReg)
            {
              CodeLen = 1; /* OK */
              BAsmCode[0] = 0x40 | HReg;
            }
            else
            {
              CodeLen = 2; /* OK */
              BAsmCode[0] = 0xe8 | AdrMode;
              BAsmCode[1] = 0x40 | HReg;
            }
            break;
          case ModMem:
            if ((HReg == AccReg) && (AdrMode == 0xe3))   /* A,(HL) */
            {
              CodeLen = 1; /* OK */
              BAsmCode[0] = 0x0d;
            }
            else if ((HReg == AccReg) && (AdrMode == 0xe0)) /* A,(nn) */
            {
              CodeLen = 2; /* OK */
              BAsmCode[0] = 0x0c;
              BAsmCode[1] = AdrVals[0];
            }
            else
            {
              CodeLen = 2 + AdrCnt; /* OK */
              CodeMem(0x00, 0x40 | HReg);
            }
            break;
          case ModImm:
            CodeLen = 2; /* OK */
            BAsmCode[0] = 0x18 | HReg;
            BAsmCode[1] = AdrVals[0];
            break;
        }
        break;
      case ModReg16:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg16 | MModMem | MModImm, FALSE);
        switch (AdrType)
        {
          case ModReg16:
            CodeLen = 2; /* OK */
            BAsmCode[0] = 0xe8 | AdrMode;
            BAsmCode[1] = 0x18 | HReg;
            break;
          case ModMem:
            CodeLen = 2 + AdrCnt; /* OK */
            BAsmCode[0] = AdrMode;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            BAsmCode[1 + AdrCnt] = 0x48 + HReg;
            break;
          case ModImm:
            CodeLen = 3; /* OK */
            BAsmCode[0] = 0x48 | HReg;
            memcpy(BAsmCode + 1, AdrVals, 2);
            break;
        }
        break;
      case ModMem:
        memcpy(HVals, AdrVals, AdrCnt);
        HCnt = AdrCnt;
        HMode = AdrMode;
        OpSize = 0;
        DecodeAdr(ArgStr[2], MModReg8 | MModReg16 | MModImm, FALSE);
        switch (AdrType)
        {
          case ModReg8:
            if ((HMode == 0xe3) && (AdrMode == AccReg))   /* (HL),A */
            {
              CodeLen = 1; /* OK */
              BAsmCode[0] = 0x0f;
            }
            else if ((HMode == 0xe0) && (AdrMode == AccReg))
            {
              CodeLen = 2; /* OK */
              BAsmCode[0] = 0x0e;
              BAsmCode[1] = AdrVals[0];
            }
            else
            {
              CodeLen = 2 + HCnt; /* OK */
              BAsmCode[0] = MakeDestMode(HMode);
              memcpy(BAsmCode + 1, HVals, HCnt);
              BAsmCode[1 + HCnt] = 0x78 | AdrMode;
            }
            break;
          case ModReg16:
            CodeLen = 2 + HCnt; /* OK */
            BAsmCode[0] = MakeDestMode(HMode);
            memcpy(BAsmCode + 1, HVals, HCnt);
            BAsmCode[1 + HCnt] = 0x68 | AdrMode;
            break;
          case ModImm:
            if (HMode == 0xe0) /* (nn),nn */
            {
              CodeLen = 3;
              BAsmCode[0] = 0x0a;
              BAsmCode[1] = HVals[0];
              BAsmCode[2] = AdrVals[0];
            }
            else
            {
              CodeLen = 1 + HCnt + 1 + AdrCnt;
              BAsmCode[0] = MakeDestMode(HMode);
              memcpy(BAsmCode + 1, HVals, HCnt);
              BAsmCode[1 + HCnt] = 0xf9;
              BAsmCode[2 + HCnt] = AdrVals[0];
            }
            break;
        }
        break;
    }
  }
}

static void DecodeLDW(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    Boolean OK;
    Integer AdrInt = EvalIntExpression(ArgStr[2], Int16, &OK);
    if (OK)
    {
      DecodeAdr(ArgStr[1], MModReg16 | MModMem, TRUE);
      switch (AdrType)
      {
        case ModReg16:
          CodeLen = 3;
          BAsmCode[0] = 0x48 | AdrMode;
          BAsmCode[1] = AdrInt & 0xff;
          BAsmCode[2] = AdrInt >> 8;
          break;
        case ModMem:
          if (AdrMode == 0xe3) /* HL */
          {
            CodeLen = 3;
            BAsmCode[0] = 0x09;
            BAsmCode[1] = AdrInt & 0xff;
            BAsmCode[2] = AdrInt >> 8;
          }
          else if (AdrMode != 0xe0) WrError(1350);  /* (nn) */
          else
          {
            CodeLen = 3;
            BAsmCode[0] = 0x08;
            BAsmCode[1] = AdrInt & 0xff;
            BAsmCode[2] = AdrInt >> 8;
          }
          break;
      }
    }
  }
}

static void DecodePUSH_POP(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else if (!strcasecmp(ArgStr[1], "PSW"))
  {
    CodeLen = 2;
    BAsmCode[0] = 0xe8;
    BAsmCode[1] = 0xdc | Code;
  }
  else
  {
    DecodeAdr(ArgStr[1], MModReg16, False);
    if (AdrType != ModNone)
    {
      if (AdrMode < 4)
      {
        CodeLen = 1;
        BAsmCode[0] = (Code << 7) | 0x50 | AdrMode;
      }
      else
      {
        CodeLen = 2;
        BAsmCode[0] = 0xe8 | AdrMode;
        BAsmCode[1] = 0xd8 | Code;
      }
    }
  }
}

static void DecodeXCH(Word Code)
{
  Byte HReg, HCnt;

  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg8 | MModReg16 | MModMem, FALSE); /* set IsDest FALSE for mirrored MemOp */
    switch (AdrType)
    {
      case ModReg8:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg8 | MModMem, FALSE);
        switch (AdrType)
        {
          case ModReg8:
            CodeLen = 2;
            BAsmCode[0] = 0xe8 | AdrMode;
            BAsmCode[1] = 0x70 | HReg;
            break;
          case ModMem:
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, 0x70 | HReg);
            break;
        }
        break;
      case ModReg16:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg16 | MModMem, FALSE);
        switch (AdrType)
        {
          case ModReg16:
            CodeLen = 2;
            BAsmCode[0] = 0xe8 | AdrMode;
            BAsmCode[1] = 0x78 | HReg;
            break;
          case ModMem:
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, 0xd8 | HReg);
            break;
        }
        break;
      case ModMem:
        BAsmCode[0] = AdrMode;
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        HCnt = AdrCnt;
        DecodeAdr(ArgStr[2], MModReg8 | MModReg16, FALSE);
        switch (AdrType)
        {
          case ModReg8:
            CodeLen = 2 + HCnt;
            BAsmCode[1 + HCnt] = 0x70 | AdrMode;
            break;
          case ModReg16:
            CodeLen = 2 + HCnt;
            BAsmCode[1 + HCnt] = 0xd8 | AdrMode;
            break;
        }
        break;
    }
  }
}

static void DecodeALU(Word Code)
{
  Byte HReg, HLen;

  if (ArgCnt != 2) WrError(1110);
  else if (!strcasecmp(ArgStr[1], "CF"))
  {
    Byte Bit;

    if (Code != 5) WrError(1350); /* XOR only */
    else if (!SplitBit(ArgStr[2], &Bit)) WrError(1510);
    else if (Bit >= 8) WrError(1350); /* only fixed bit # */
    else
    {
      DecodeAdr(ArgStr[2], MModReg8 | MModMem, False);
      switch (AdrType)
      {
        case ModReg8:
          CodeLen = 2;
          BAsmCode[0] = 0xe8 | AdrMode;
          BAsmCode[1] = 0x50 | Bit;
          break;
        case ModMem:
          CodeLen = 2 + AdrCnt;
          CodeMem(0x00, 0x50 | Bit);
          break;
      }
    }
  }
  else
  {
    DecodeAdr(ArgStr[1], MModReg8 | MModReg16 | MModMem, FALSE); /* (+SP) allowed as dest mem op instead of (SP-) */
    switch (AdrType)
    {
      case ModReg8:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg8 | MModMem | MModImm, FALSE);
        switch (AdrType)
        {
          case ModReg8:
            CodeLen = 2;
            BAsmCode[0] = 0xe8 | AdrMode;
            BAsmCode[1] = (HReg << 3) | Code;
            break;
          case ModMem:
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, (HReg << 3) | Code);
            break;
          case ModImm:
            if (HReg == AccReg)
            {
              CodeLen = 2;
              BAsmCode[0] = 0x60 | Code;
              BAsmCode[1] = AdrVals[0];
            }
            else
            {
              CodeLen = 3;
              BAsmCode[0] = 0xe8 | HReg;
              BAsmCode[1] = 0x60 | Code;
              BAsmCode[2] = AdrVals[0];
            }
            break;
        }
        break;
      case ModReg16:
        HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModImm | MModMem | MModReg16, FALSE);
        switch (AdrType)
        {
          case ModImm:
            CodeLen = 4;
            BAsmCode[0] = 0xe8 | HReg;
            BAsmCode[1] = 0x68 | Code;
            memcpy(BAsmCode + 2, AdrVals, AdrCnt);
            break;
          case ModMem:
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, 0x80 | (HReg << 3) | Code);
            break;
          case ModReg16:
            CodeLen = 2;
            BAsmCode[0] = 0xe8 | AdrMode;
            BAsmCode[1] = 0x80 | (HReg << 3) | Code;
            break;
        }
        break;
      case ModMem:
        if ((0xe0 == AdrMode) && (Code == 7))
        {
          BAsmCode[0] = Code;
          BAsmCode[1] = AdrVals[0];
          HLen = 2;
        }
        else
        {
          CodeMem(0x00, 0x60 | Code);
          HLen = 2 + AdrCnt;
        }
        OpSize = 0;
        DecodeAdr(ArgStr[2], MModImm, FALSE);
        if (AdrType == ModImm)
        {
          BAsmCode[HLen] = AdrVals[0];
          CodeLen = HLen + 1;
        }
        break;
    }
  }
}

static void DecodeINC_DEC(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg8 | MModReg16 | MModMem, False);
    switch (AdrType)
    {
      case ModReg8:
        CodeLen = 1;
        BAsmCode[0] = 0x20 | Code | AdrMode;
        break;
      case ModReg16:
        CodeLen = 1;
        BAsmCode[0] = 0x30 | Code | AdrMode;
        break;
      case ModMem:
        CodeLen = 2 + AdrCnt;
        CodeMem(0x00, 0xf0 | Code);
        break;
    }
  }
}

static void DecodeReg(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg8, False);
    if (AdrType != ModNone)
    {
      CodeLen = 1;
      BAsmCode[0] = Lo(Code) | AdrMode;
      if (Hi(Code))
        BAsmCode[CodeLen++] = Hi(Code);
    }
  }
}

static void DecodeReg16(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg16, False);
    if (AdrType != ModNone)
    {
      CodeLen = 1;
      BAsmCode[0] = Lo(Code) | AdrMode;
      if (Hi(Code))
        BAsmCode[CodeLen++] = Hi(Code);
    }
  }
}

static void DecodeMUL(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg8, False);
    if (AdrType == ModReg8)
    {
      Byte HReg = AdrMode;
      DecodeAdr(ArgStr[2], MModReg8, False);
      if (AdrType == ModReg8)
      {
        if ((HReg ^ AdrMode) != 1) WrError(1760);
        else
        {
          CodeLen = 2;
          BAsmCode[0] = 0xe8 | (HReg >> 1);
          BAsmCode[1] = 0xf2;
        }
      }
    }
  }
}

static void DecodeDIV(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg16, False);
    if (AdrType == ModReg16)
    {
      if ((AdrMode == 1) || (AdrMode > 3)) WrError(1350); /* WA DE HL */
      else
      {
        Byte HReg = AdrMode;
        DecodeAdr(ArgStr[2], MModReg8, False);
        if (AdrType == ModReg8)
        {
          if (AdrMode != 2) WrError(1350);  /* C */
          else
          {
            CodeLen = 2;
            BAsmCode[0] = 0xe8 | HReg;
            BAsmCode[1] = 0xf3;
          }
        }
      }
    }
  }
}

static void DecodeNEG(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else if (strcasecmp(ArgStr[1], "CS")) WrError(1350);
  else
  {
    DecodeAdr(ArgStr[2], MModReg16, False);
    if (AdrType == ModReg16)
    {
      CodeLen = 2;
      BAsmCode[0] = 0xe8 | AdrMode;
      BAsmCode[1] = 0xfa;
    }
  }
}

static void DecodeROLD_RORD(Word Code)
{
  if (ArgCnt != 2) WrError(1110);
  else if (strcasecmp(ArgStr[1], "A")) WrError(1350);
  else
  {
    DecodeAdr(ArgStr[2], MModMem, False);
    if (AdrType != ModNone)
    {
      CodeLen = 2 + AdrCnt;
      CodeMem(0x00, Code);
    }
  }
}

static void DecodeTEST_CPL_SET_CLR(Word Code)
{
  Byte Bit;

  if (ArgCnt != 1) WrError(1110);
  else if (!strcasecmp(ArgStr[1], "CF"))
  {
    switch (Lo(Code))
    {
      case 0xc0:
        BAsmCode[0] = 0xc5;
        CodeLen = 1;
        break;
      case 0xc8:
        BAsmCode[0] = 0xc4;
        CodeLen = 1;
        break;
      case 0xe0:
        BAsmCode[0] = 0xc6;
        CodeLen = 1;
        break;
      default:
        WrError(1350);
    }
  }
  else if (!SplitBit(ArgStr[1], &Bit)) WrError(1510);
  else
  {
    DecodeAdr(ArgStr[1], (Bit < 8 ? MModReg8 : 0) | MModMem, False);
    switch (AdrType)
    {
      case ModReg8:
        CodeLen = 2;
        BAsmCode[0] = 0xe8 | AdrMode;
        BAsmCode[1] = Lo(Code) | Bit;
        break;
      case ModMem:
        if ((Bit < 8) && (AdrMode == 0xe0) && (Lo(Code) != 0xe0)) /* no short addr. for CPL */
        {
          CodeLen = 2;
          BAsmCode[0] = Lo(Code) | Bit;
          BAsmCode[1] = AdrVals[0];
        }
        else if (Bit < 8)
        {
          CodeLen = 2 + AdrCnt;
          CodeMem(0x00, Lo(Code) | Bit);
        }
        else
        {
          CodeLen = 2 + AdrCnt;
          CodeMem(0x00, Hi(Code));
        }
        break;
    }
  }
}

static void DecodeJRS(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    Integer AdrInt, Condition;
    Boolean OK;

    Condition = DecodeCondition(ArgStr[1], ConditionCnt - 2);
    if ((Condition >= ConditionCnt) || (Condition < ConditionCnt - 2)) WrXError(1360, ArgStr[1]); /* only T/F allowed */
    else
    {
      AdrInt = EvalIntExpression(ArgStr[2], Int16, &OK) - (EProgCounter() + 2);
      if (OK)
      {
        if (((AdrInt < -16) || (AdrInt > 15)) && (!SymbolQuestionable)) WrError(1370);
        else
        {
          CodeLen = 1;
          BAsmCode[0] = 0x80 | ((Conditions[Condition].Code - 0xde) << 5) | (AdrInt & 0x1f);
        }
      }
    }
  }
}

static void DecodeJR(Word Code)
{
  UNUSED(Code);

  if ((ArgCnt != 2) && (ArgCnt != 1)) WrError(1110);
  else
  {
    Integer Condition, AdrInt;
    Boolean OK;

    Condition = (ArgCnt == 1) ? -1 : DecodeCondition(ArgStr[1], 0);
    if (Condition >= ConditionCnt) WrXError(1360, ArgStr[1]);
    else
    {
      int Delta = ((Condition < 0) || (!Hi(Conditions[Condition].Code))) ? 2 : 3;

      AdrInt = EvalIntExpression(ArgStr[ArgCnt], Int16, &OK) - (EProgCounter() + Delta);
      if (OK)
      {
        if (((AdrInt < -128) || (AdrInt > 127)) && (!SymbolQuestionable)) WrError(1370);
        else
        {
          CodeLen = Delta;
          if (3 == Delta)
            BAsmCode[0] = Hi(Conditions[Condition].Code);
          BAsmCode[CodeLen - 2] = (Condition == -1) ?  0xfc : Lo(Conditions[Condition].Code);
          BAsmCode[CodeLen - 1] = AdrInt & 0xff;
        }
      }
    }
  }
}

static void DecodeJ(Word Code)
{
  UNUSED(Code);

  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    int CondIndex = (ArgCnt == 1) ? -1 : DecodeCondition(ArgStr[1], 0);

    if (CondIndex >= ConditionCnt) WrXError(1360, ArgStr[1]);
    else
    {
      Word CondCode = (CondIndex >= 0) ? Conditions[CondIndex].Code : 0;

      OpSize = 1;
      DecodeAdr(ArgStr[ArgCnt], MModReg16 | MModMem | MModImm, False);
      switch (AdrType)
      {
        case ModReg16: /* -> JP */
          if (CondIndex >= 0) WrXError(1360, ArgStr[1]);
          else
          {
            CodeLen = 2;
            BAsmCode[0] = 0xe8 | AdrMode;
            BAsmCode[1] = Code;
          }
          break;
        case ModMem: /* -> JP */
          if (CondIndex >= 0) WrXError(1360, ArgStr[1]);
          else
          {
            CodeLen = 2 + AdrCnt;
            CodeMem(0x00, Code);
          }
          break;
        case ModImm:
        {
          Word Adr = ((Word)(AdrVals[1] << 8)) | AdrVals[0];
          Integer Dist = Adr - (EProgCounter() + 2);
          int Delta = ((CondIndex < 0) || (!Hi(CondCode))) ? 2 : 3;

          if ((Dist >= -16) && (Dist < 15) && (CondIndex >= ConditionCnt - 2)) /* JRS T/F */
          {
            CodeLen = 1;
            BAsmCode[0] = 0x80 | ((CondCode - 0xde) << 5) | (Dist & 0x1f);
          }
          else if ((Dist >= -128) && (Dist < 127))
          {
            if (CondIndex < 0) /* JR dist */
            {
              CodeLen = 2;
              BAsmCode[0] = 0xfc;
              BAsmCode[1] = Dist & 0xff;
            }
            else /* JR cc, dist */
            {
              CodeLen = Delta;
              if (3 == Delta)
                BAsmCode[0] = Hi(CondCode);
              BAsmCode[CodeLen - 2] = Lo(CondCode);
              BAsmCode[CodeLen - 1] = Dist & 0xff;
            }
          }
          else
          {
            if (CondIndex < 0) /* JP dest */
            {
              CodeLen = 3;
              BAsmCode[CodeLen - 3] = 0xfe;   
              BAsmCode[CodeLen - 2] = Lo(Adr);
              BAsmCode[CodeLen - 1] = Hi(Adr);
            }
            else /* JR !cc, JP dest */
            {
              CondCode ^= 1;
              CodeLen = Delta + 3;
              if (3 == Delta)
                BAsmCode[0] = Hi(CondCode);
              BAsmCode[CodeLen - 5] = Lo(CondCode);
              BAsmCode[CodeLen - 4] = 3;
              BAsmCode[CodeLen - 3] = 0xfe;
              BAsmCode[CodeLen - 2] = Lo(Adr);
              BAsmCode[CodeLen - 1] = Hi(Adr);
            }
          }
          break;
        }
      }
    }
  }
}

static void DecodeJP_CALL(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    OpSize = 1;
    DecodeAdr(ArgStr[1], MModReg16 | MModMem | MModImm, False);
    switch (AdrType)
    {
      case ModReg16:
        CodeLen = 2;
        BAsmCode[0] = 0xe8 | AdrMode;
        BAsmCode[1] = Code;
        break;
      case ModMem:
        CodeLen = 2 + AdrCnt;
        CodeMem(0x00, Code);
        break;
      case ModImm:
        CodeLen = 3;
        BAsmCode[0] = Code;
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        break;
    }
  }
}

static void DecodeCALLV(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;
    Byte HVal = EvalIntExpression(ArgStr[1], Int4, &OK);
    if (OK)
    {
      CodeLen = 1;
      BAsmCode[0] = 0x70 | (HVal & 15);
    }
  }
}

static void DecodeUnimplemented(Word Code)
{
  UNUSED(Code);

  WrError(2060);
}

/*--------------------------------------------------------------------------*/

static void AddFixed(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeFixed);
}

static void AddCond(char *NName, Word NCode)
{
  if (InstrZ >= ConditionCnt) exit(255);
  Conditions[InstrZ].Name = NName;
  Conditions[InstrZ++].Code = NCode;
}

static void AddReg(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeReg);
}

static void AddReg16(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeReg16);
}

static void InitFields(void)
{
  InstTable = CreateInstTable(203);
  AddInstTable(InstTable, "LD", 0, DecodeLD);
  AddInstTable(InstTable, "LDW", 0, DecodeLDW);
  AddInstTable(InstTable, "PUSH", 0, DecodePUSH_POP);
  AddInstTable(InstTable, "POP", 1, DecodePUSH_POP);
  AddInstTable(InstTable, "XCH", 0, DecodeXCH);
  AddInstTable(InstTable, "INC", 0, DecodeINC_DEC);
  AddInstTable(InstTable, "DEC", 8, DecodeINC_DEC);
  AddInstTable(InstTable, "MUL", 0, DecodeMUL);
  AddInstTable(InstTable, "DIV", 0, DecodeDIV);
  AddInstTable(InstTable, "NEG", 0, DecodeNEG);
  AddInstTable(InstTable, "ROLD", 0xf6, DecodeROLD_RORD);
  AddInstTable(InstTable, "RORD", 0xf7, DecodeROLD_RORD);
  AddInstTable(InstTable, "CLR", 0xfac8, DecodeTEST_CPL_SET_CLR);
  AddInstTable(InstTable, "TEST", 0xfc58, DecodeTEST_CPL_SET_CLR);
  AddInstTable(InstTable, "CPL", 0xfbe0, DecodeTEST_CPL_SET_CLR);
  AddInstTable(InstTable, "SET", 0xf2c0, DecodeTEST_CPL_SET_CLR);
  AddInstTable(InstTable, "JR", 0, DecodeJR);
  AddInstTable(InstTable, "JRS", 0, DecodeJRS);
  AddInstTable(InstTable, "JP", 0xfe, DecodeJP_CALL);
  AddInstTable(InstTable, "J", 0, DecodeJ);
  AddInstTable(InstTable, "CALL", 0, DecodeUnimplemented); /* would be on TLCS-870, but is JR here */
  AddInstTable(InstTable, "CALLV", 0, DecodeCALLV);

  AddFixed("DI"  , 0xc83a);
  AddFixed("EI"  , 0xc03a);
  AddInstTable(InstTable, "RET" , 0x0005, DecodeUnimplemented);
  AddInstTable(InstTable, "RETI", 0x0004, DecodeUnimplemented);
  AddInstTable(InstTable, "RETN", 0xe804, DecodeUnimplemented);
  AddInstTable(InstTable, "SWI" , 0x00ff, DecodeUnimplemented);
  AddInstTable(InstTable, "NOP" , 0x0000, DecodeUnimplemented);

  Conditions = (CondRec *) malloc(sizeof(CondRec)*ConditionCnt); InstrZ = 0;
  AddCond("EQ" , 0x00d8); AddCond("Z"  , 0x00d8);
  AddCond("NE" , 0x00d9); AddCond("NZ" , 0x00d9);
  AddCond("CS" , 0x00da); AddCond("LT" , 0x00da);
  AddCond("CC" , 0x00db); AddCond("GE" , 0x00db);
  AddCond("LE" , 0x00dc); AddCond("GT" , 0x00dd);
  AddCond("M"  , 0xe8d0); AddCond("P"  , 0xe8d1);
  AddCond("SLT", 0xe8d2); AddCond("SGE", 0xe8d3);
  AddCond("SLE", 0xe8d4); AddCond("SGT", 0xe8d5);
  AddCond("VS" , 0xe8d6); AddCond("VC" , 0xe8d7);
  AddCond("T"  , 0x00de); AddCond("F"  , 0x00df);

  AddReg("DAA" , 0xdae8);  AddReg("DAS" , 0xdbe8);
  AddReg("SHLC", 0xf4e8);  AddReg("SHRC", 0xf5e8);
  AddReg("ROLC", 0xf6e8);  AddReg("RORC", 0xf7e8);
  AddReg("SWAP", 0xffe8);

  AddReg16("SHLCA", 0xf0e8); AddReg16("SHRCA", 0xf1e8);

  InstrZ = 0;
  AddInstTable(InstTable, "ADDC", InstrZ++, DecodeALU);
  AddInstTable(InstTable, "ADD" , InstrZ++, DecodeALU);
  AddInstTable(InstTable, "SUBB", InstrZ++, DecodeALU);
  AddInstTable(InstTable, "SUB" , InstrZ++, DecodeALU);
  AddInstTable(InstTable, "AND" , InstrZ++, DecodeALU);
  AddInstTable(InstTable, "XOR" , InstrZ++, DecodeALU);
  AddInstTable(InstTable, "OR"  , InstrZ++, DecodeALU);
  AddInstTable(InstTable, "CMP" , InstrZ++, DecodeALU);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);

  free(Conditions);
}

/*--------------------------------------------------------------------------*/

static void MakeCode_870C(void)
{
  CodeLen = 0;
  DontPrint = False;
  OpSize = -1;

  /* zu ignorierendes */

  if (Memo(""))
    return;

  /* Pseudoanweisungen */

  if (DecodeIntelPseudo(False))
    return;

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200, OpPart);
}

static Boolean IsDef_870C(void)
{
  return False;
}

static void SwitchFrom_870C(void)
{
  DeinitFields();
}

static void SwitchTo_870C(void)
{
  PFamilyDescr FoundDescr;

  FoundDescr = FindFamilyByName("TLCS-870/C");

  TurnWords = False;
  ConstMode = ConstModeIntel;
  SetIsOccupied = True;

  PCSymbol = "$";
  HeaderID = FoundDescr->Id;
  NOPCode = 0x00;
  DivideChars = ",";
  HasAttrs = False;

  ValidSegs = 1 << SegCode;
  Grans[SegCode] = 1;
  ListGrans[SegCode] = 1;
  SegInits[SegCode] = 0;
  SegLimits[SegCode] = 0xffff;

  MakeCode = MakeCode_870C;
  IsDef = IsDef_870C;
  SwitchFrom = SwitchFrom_870C;
  InitFields();
}

void code870c_init(void)
{
  CPU870C = AddCPU("TLCS-870/C", SwitchTo_870C);
}