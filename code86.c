/* code86.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator 8086/V-Serie                                                */
/*                                                                           */
/* Historie:                                                                 */
/*            2. 1.1999 ChkPC-Anpassung                                      */
/*            9. 3.2000 'ambiguous else'-Warnungen beseitigt                 */
/*           14. 1.2001 silenced warnings about unused parameters            */
/*                                                                           */
/*****************************************************************************/
/* $Id: code86.c,v 1.15 2017/06/07 19:29:05 alfred Exp $                      */
/*****************************************************************************
 * $Log: code86.c,v $
 * Revision 1.15  2017/06/07 19:29:05  alfred
 * - correct registration of ROR4
 *
 * Revision 1.14  2014/12/07 19:14:00  alfred
 * - silence a couple of Borland C related warnings and errors
 *
 * Revision 1.13  2014/12/02 13:33:19  alfred
 * - do not use strncpy()
 *
 * Revision 1.12  2014/11/16 13:15:07  alfred
 * - remove some superfluous semicolons
 *
 * Revision 1.11  2014/11/05 15:47:15  alfred
 * - replace InitPass callchain with registry
 *
 * Revision 1.10  2014/08/10 13:12:33  alfred
 * - rework to current style
 *
 * Revision 1.9  2014/06/09 12:46:02  alfred
 * - add missing parentheses
 *
 * Revision 1.8  2014/03/08 21:06:36  alfred
 * - rework ASSUME framework
 *
 * Revision 1.7  2010/08/27 14:52:42  alfred
 * - some more overlapping strcpy() cleanups
 *
 * Revision 1.6  2010/04/17 13:14:22  alfred
 * - address overlapping strcpy()
 *
 * Revision 1.5  2008/04/06 09:37:52  alfred
 * - catch immediate addressing on invalid operand size, some reformatting
 *
 * Revision 1.4  2008/03/30 21:28:00  alfred
 * - correct intersegment CALL/JMP
 *
 * Revision 1.3  2005/09/08 16:53:42  alfred
 * - use common PInstTable
 *
 * Revision 1.2  2004/05/29 11:33:01  alfred
 * - relocated DecodeIntelPseudo() into own module
 *
 *****************************************************************************/

#include "stdinc.h"
#include <string.h>

#include "bpemu.h"
#include "strutil.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmallg.h"
#include "codepseudo.h"
#include "intpseudo.h" 
#include "asmitree.h"
#include "codevars.h"

/*---------------------------------------------------------------------------*/

typedef struct
{
  char *Name;
  CPUVar MinCPU;
  Word Code;
} FixedOrder;

typedef struct
{
  CPUVar MinCPU;
  Word Code;
  Byte Add;
} AddOrder;

#define NO_FWAIT_FLAG 0x2000

#define Reg16OrderCnt 3
#define StringOrderCnt 14

#define SegRegCnt 3
static char *SegRegNames[SegRegCnt + 1] =
{
  "ES", "CS", "SS", "DS"
};
static Byte SegRegPrefixes[SegRegCnt + 1] =
{
  0x26, 0x2e, 0x36, 0x3e
};

#define TypeNone (-1)
#define TypeReg8 0
#define TypeReg16 1
#define TypeRegSeg 2
#define TypeMem 3
#define TypeImm 4
#define TypeFReg 5

static ShortInt AdrType;
static Byte AdrMode;
static Byte AdrVals[6];
static ShortInt OpSize;
static Boolean UnknownFlag;

static Boolean NoSegCheck;

static Byte Prefixes[6];
static Byte PrefixLen;

static Byte SegAssumes[SegRegCnt + 1];

static CPUVar CPU8086, CPU80186, CPUV30, CPUV35;

static FixedOrder *StringOrders;
static AddOrder *Reg16Orders;

/*------------------------------------------------------------------------------------*/

static void PutCode(Word Code)
{
  if (Hi(Code) != 0)
    BAsmCode[CodeLen++] = Hi(Code);
  BAsmCode[CodeLen++] = Lo(Code);
}

static void MoveAdr(int Dest)
{
  memcpy(BAsmCode + CodeLen + Dest, AdrVals, AdrCnt);
}

static Byte Sgn(Byte inp)
{
  return (inp > 127) ? 0xff : 0;
}

static void AddPrefix(Byte Prefix)
{
  Prefixes[PrefixLen++] = Prefix;
}

static void AddPrefixes(void)
{
  if ((CodeLen != 0) && (PrefixLen != 0))
  {
    memmove(BAsmCode + PrefixLen, BAsmCode, CodeLen);
    memcpy(BAsmCode, Prefixes, PrefixLen);
    CodeLen += PrefixLen;
  }
}

static Boolean AbleToSign(Word Arg)
{
  return ((Arg <= 0x7f) || (Arg >= 0xff80));
}

static Boolean MinOneIs0(void)
{
  if ((UnknownFlag) && (OpSize == -1))
  {
    OpSize = 0;
    return True;
  }
  else
    return False;
}

static void ChkOpSize(ShortInt NewSize)
{
  if (OpSize == -1)
    OpSize = NewSize;
  else if (OpSize != NewSize)
  {
    AdrType = TypeNone;
    WrError(1131);
  }
}

static void ChkSingleSpace(Byte Seg, Byte EffSeg, Byte MomSegment)
{
  Byte z;

  /* liegt Operand im zu pruefenden Segment? nein-->vergessen */

  if (!(MomSegment & (1 << Seg)))
    return;

  /* zeigt bish. benutztes Segmentregister auf dieses Segment? ja-->ok */

  if (EffSeg == Seg)
    return;

  /* falls schon ein Override gesetzt wurde, nur warnen */

  if (PrefixLen > 0)
    WrError(70);

  /* ansonsten ein passendes Segment suchen und warnen, falls keines da */

  else
  {
    z = 0;
    while ((z <= SegRegCnt) && (SegAssumes[z] != Seg))
      z++;
    if (z > SegRegCnt)
      WrXError(75, SegNames[Seg]);
    else
      AddPrefix(SegRegPrefixes[z]);
  }
}

static void ChkSpaces(ShortInt SegBuffer, Byte MomSegment)
{
  Byte EffSeg;

  if (NoSegCheck)
    return;

  /* in welches Segment geht das benutzte Segmentregister ? */

  EffSeg = SegAssumes[SegBuffer];

  /* Zieloperand in Code-/Datensegment ? */

  ChkSingleSpace(SegCode, EffSeg, MomSegment);
  ChkSingleSpace(SegXData, EffSeg, MomSegment);
  ChkSingleSpace(SegData, EffSeg, MomSegment);
}

static void DecodeAdr(char *Asc)
{
  static const int RegCnt = 8;
  static const char *Reg16Names[8] =
  {
    "AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"
  };
  static const char *Reg8Names[8] =
  {
    "AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"
  };
  static const Byte RMCodes[8] =
  {
    11, 12, 21, 22, 1, 2 , 20, 10
  };

  int RegZ, z;
  Boolean IsImm;
  ShortInt IndexBuf, BaseBuf;
  Byte SumBuf;
  LongInt DispAcc, DispSum;
  char *p, *p1, *p2;
  Boolean HasAdr;
  Boolean OK,OldNegFlag, NegFlag;
  String AdrPart, AddPart;
  ShortInt SegBuffer;
  Byte MomSegment;
  ShortInt FoundSize;

  AdrType = TypeNone; AdrCnt = 0;
  SegBuffer = -1; MomSegment = 0;

  for (RegZ = 0; RegZ < RegCnt; RegZ++)
  {
    if (!strcasecmp(Asc, Reg16Names[RegZ]))
    {
      AdrType = TypeReg16; AdrMode = RegZ;
      ChkOpSize(1);
      return;
    }
    if (!strcasecmp(Asc, Reg8Names[RegZ]))
    {
      AdrType = TypeReg8; AdrMode = RegZ;
      ChkOpSize(0);
      return;
    }
  }

  for (RegZ = 0; RegZ <= SegRegCnt; RegZ++)
    if (!strcasecmp(Asc,SegRegNames[RegZ]))
    {
      AdrType = TypeRegSeg; AdrMode = RegZ;
      ChkOpSize(1);
      return;
    }

  if (FPUAvail)
  {
    if (!strcasecmp(Asc,"ST"))
    {
      AdrType = TypeFReg; AdrMode = 0;
      ChkOpSize(4);
      return;
    }

    if ((strlen(Asc) > 4) && (!strncasecmp(Asc, "ST(", 3)) && (Asc[strlen(Asc) - 1] == ')'))
    {
      Asc[strlen(Asc) - 1] = '\0';
      AdrMode = EvalIntExpression(Asc + 3, UInt3, &OK);
      if (OK)
      {
        AdrType = TypeFReg;
        ChkOpSize(4);
      }
      return;
    }
  }

  IsImm = True;
  IndexBuf = 0; BaseBuf = 0;
  DispAcc = 0; FoundSize = -1;

  if (!strncasecmp(Asc, "WORD PTR", 8))
  {
    Asc += 8;
    FoundSize = 1;
    IsImm = False;
    KillBlanks(Asc);
  }
  else if (!strncasecmp(Asc, "BYTE PTR", 8))
  {
    Asc += 8;
    FoundSize = 0;
    IsImm = False;
    KillBlanks(Asc);
  }
  else if (!strncasecmp(Asc, "DWORD PTR", 9))
  {
    Asc += 9;
    FoundSize = 2;
    IsImm = False;
    KillBlanks(Asc);
  }
  else if (!strncasecmp(Asc, "QWORD PTR", 9))
  {
    Asc += 9;
    FoundSize = 3;
    IsImm = False;
    KillBlanks(Asc);
  }
  else if (!strncasecmp(Asc, "TBYTE PTR", 9))
  {
    Asc += 9;
    FoundSize = 4;
    IsImm = False;
    KillBlanks(Asc);
  }

  if ((strlen(Asc) > 2) && (Asc[2] == ':'))
  {
    memcpy(AddPart, Asc, 2);
    AddPart[2] = '\0';
    for (z = 0; z <= SegRegCnt; z++)
      if (!strcasecmp(AddPart, SegRegNames[z]))
      {
        Asc += 3;
        SegBuffer = z;
        AddPrefix(SegRegPrefixes[SegBuffer]);
      }
  }

  do
  {
    p = QuotPos(Asc, '[');
    HasAdr = (p != NULL);

    if (p != Asc)
    {
      FirstPassUnknown = False;
      if (p)
        *p = '\0';
      DispAcc += EvalIntExpression(Asc, Int16, &OK);
      if (!OK)
         return;
      UnknownFlag = UnknownFlag || FirstPassUnknown;
      MomSegment |= TypeFlag;
      if (FoundSize == -1)
        FoundSize = SizeFlag;
      if (!p)
        *Asc = '\0';
      else
      {
        *p = '['; strmov(Asc, p);
      }
    }

    if (HasAdr)
    {
      IsImm = False;

      p = RQuotPos(Asc, ']');
      if (!p)
      {
        WrError(1300);
        return;
      }

      *p = '\0';
      strmaxcpy(AdrPart, Asc + 1, 255);
      strmov(Asc, p + 1);
      OldNegFlag = False;

      do
      {
        NegFlag = False;
        p1 = QuotPos(AdrPart, '+');
        p2 = QuotPos(AdrPart, '-');
        if (((p1 > p2) || (!p1)) && (p2))
        {
          p = p2;
          NegFlag = True;
        }
        else
          p = p1;

        if (!p)
        {
          strmov(AddPart, AdrPart);
          *AdrPart = '\0';
        }
        else
        {
          *p = '\0';
          strcpy(AddPart, AdrPart);
          strmov(AdrPart, p + 1);
        }

        if (!strcasecmp(AddPart, "BX"))
        {
          if ((OldNegFlag) || (BaseBuf != 0))
            return;
          else
            BaseBuf = 1;
        }
        else if (!strcasecmp(AddPart, "BP"))
        {
          if ((OldNegFlag) || (BaseBuf != 0))
            return;
          else
            BaseBuf = 2;
        }
        else if (!strcasecmp(AddPart, "SI"))
        {
          if ((OldNegFlag) || (IndexBuf != 0))
            return;
          else
            IndexBuf = 1;
        }
        else if (!strcasecmp(AddPart, "DI"))
        {
          if ((OldNegFlag) || (IndexBuf !=0 ))
            return;
          else
            IndexBuf = 2;
        }
        else
        {
          FirstPassUnknown = False;
          DispSum = EvalIntExpression(AddPart, Int16, &OK);
          if (!OK)
            return;
          UnknownFlag = UnknownFlag || FirstPassUnknown;
          DispAcc = OldNegFlag ? DispAcc - DispSum : DispAcc + DispSum;
          MomSegment |= TypeFlag;
          if (FoundSize == -1)
            FoundSize = SizeFlag;
        }
        OldNegFlag = NegFlag;
      }
      while (*AdrPart != '\0');
    }
  }
  while (*Asc);

  SumBuf = BaseBuf * 10 + IndexBuf;

  /* welches Segment effektiv benutzt ? */

  if (SegBuffer == -1)
    SegBuffer = (BaseBuf == 2) ? 2 : 3;

  /* nur Displacement */

  if (0 == SumBuf)
  {
    /* immediate */

    if (IsImm)
    {
      if (((UnknownFlag) && (OpSize == 0)) || (MinOneIs0()))
        DispAcc &= 0xff;
      switch (OpSize)
      {
        case -1:
          WrError(1132);
          break;
        case 0:
          if ((DispAcc <- 128) || (DispAcc > 255)) WrError(1320);
          else
          {
            AdrType = TypeImm;
            AdrVals[0] = DispAcc & 0xff;
            AdrCnt = 1;
          }
          break;
        case 1:
          AdrType = TypeImm;
          AdrVals[0] = Lo(DispAcc);
          AdrVals[1] = Hi(DispAcc);
          AdrCnt = 2;
          break;
        default:
          WrError(1130);
          break;
      }
    }

    /* absolut */

    else
    {
      AdrType = TypeMem;
      AdrMode = 0x06;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc);
      AdrCnt = 2;
      if (FoundSize != -1)
        ChkOpSize(FoundSize);
      ChkSpaces(SegBuffer, MomSegment);
    }
  }

  /* kombiniert */

  else
  {
    AdrType = TypeMem;
    for (z = 0; z < 8; z++)
      if (SumBuf == RMCodes[z])
        AdrMode = z;
    if (DispAcc == 0)
    {
      if (SumBuf == 20)
      {
        AdrMode += 0x40;
        AdrVals[0] = 0;
        AdrCnt = 1;
      }
    }
    else if (AbleToSign(DispAcc))
    {
      AdrMode += 0x40;
      AdrVals[0] = DispAcc & 0xff;
      AdrCnt = 1;
    }
    else
    {
      AdrMode += 0x80;
      AdrVals[0] = Lo(DispAcc);
      AdrVals[1] = Hi(DispAcc);
      AdrCnt = 2;
    }
    ChkSpaces(SegBuffer, MomSegment);
    if (FoundSize != -1)
      ChkOpSize(FoundSize);
  }
}

static void AddFWait(Word *pCode)
{
  if (*pCode & NO_FWAIT_FLAG)
    *pCode &= ~NO_FWAIT_FLAG;
  else
    AddPrefix(0x9b);
}

static Boolean FPUEntry(Word *pCode)
{
  if (!FPUAvail)
  {
    WrXError(1200, OpPart);
    return FALSE;
  }

  AddFWait(pCode);
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static void DecodeMOV(Word Index)
{
  Byte AdrByte;
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg8:
      case TypeReg16:
        AdrByte = AdrMode;
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
          case TypeReg16:
            BAsmCode[CodeLen++] = 0x8a | OpSize;
            BAsmCode[CodeLen++] = 0xc0 | (AdrByte << 3) | AdrMode;
            break;
          case TypeMem:
            if ((AdrByte == 0) && (AdrMode == 6))
            {
              BAsmCode[CodeLen] = 0xa0 | OpSize;
              MoveAdr(1);
              CodeLen += 1 + AdrCnt;
            }
            else
            {
              BAsmCode[CodeLen++] = 0x8a | OpSize;
              BAsmCode[CodeLen++] = AdrMode | (AdrByte << 3);
              MoveAdr(0);
              CodeLen += AdrCnt;
            }
            break;
          case TypeRegSeg:
            if (OpSize == 0) WrError(1131);
            else
            {
              BAsmCode[CodeLen++] = 0x8c;
              BAsmCode[CodeLen++] = 0xc0 | (AdrMode << 3) | AdrByte;
            }
            break;
          case TypeImm:
            BAsmCode[CodeLen++] = 0xb0 | (OpSize << 3) | AdrByte;
            MoveAdr(0);
            CodeLen += AdrCnt;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      case TypeMem:
         BAsmCode[CodeLen + 1] = AdrMode;
         MoveAdr(2);
         AdrByte = AdrCnt;
         DecodeAdr(ArgStr[2]);
         switch (AdrType)
         {
           case TypeReg8:
           case TypeReg16:
            if ((AdrMode == 0) && (BAsmCode[CodeLen + 1] == 6))
            {
              BAsmCode[CodeLen] = 0xa2 | OpSize;
              memmove(BAsmCode + CodeLen + 1, BAsmCode + CodeLen + 2, AdrByte);
              CodeLen += 1 + AdrByte;
            }
            else
            {
              BAsmCode[CodeLen] = 0x88 | OpSize;
              BAsmCode[CodeLen + 1] |= AdrMode << 3;
              CodeLen += 2 + AdrByte;
            }
            break;
          case TypeRegSeg:
            BAsmCode[CodeLen] = 0x8c;
            BAsmCode[CodeLen + 1] |= AdrMode << 3;
            CodeLen += 2 + AdrByte;
            break;
          case TypeImm:
            BAsmCode[CodeLen] = 0xc6 | OpSize;
            MoveAdr(2 + AdrByte);
            CodeLen += 2 + AdrByte + AdrCnt;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      case TypeRegSeg:
        BAsmCode[CodeLen + 1] = AdrMode << 3;
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg16:
            BAsmCode[CodeLen++] = 0x8e;
            BAsmCode[CodeLen++] |= 0xc0 + AdrMode;
            break;
          case TypeMem:
            BAsmCode[CodeLen] = 0x8e;
            BAsmCode[CodeLen + 1] |= AdrMode;
            MoveAdr(2);
            CodeLen += 2 + AdrCnt;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeINCDEC(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg16:
        BAsmCode[CodeLen] = 0x40 | AdrMode | Index;
        CodeLen++;
        break;
      case TypeReg8:
        BAsmCode[CodeLen] = 0xfe;
        BAsmCode[CodeLen + 1] = 0xc0 | AdrMode | Index;
        CodeLen += 2;
        break;
      case TypeMem:
        MinOneIs0();
        if (OpSize == -1) WrError(1132);
        else
        {
          BAsmCode[CodeLen] = 0xfe | OpSize; /* ANSI :-0 */
          BAsmCode[CodeLen + 1] = AdrMode | Index;
          MoveAdr(2);
          CodeLen += 2 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeINT(Word Index)
{
  Boolean OK;
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    BAsmCode[CodeLen + 1] = EvalIntExpression(ArgStr[1], Int8, &OK);
    if (OK)
    {
      if (BAsmCode[1] == 3)
        BAsmCode[CodeLen++] = 0xcc;
      else
      {
        BAsmCode[CodeLen] = 0xcd;
        CodeLen += 2;
      }
    }
  }
  AddPrefixes();
}

static void DecodeINOUT(Word Index)
{
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else
  {
    char *pPortArg = Index ? ArgStr[1] : ArgStr[2],
         *pRegArg = Index ? ArgStr[2] : ArgStr[1];

    DecodeAdr(pRegArg);
    switch (AdrType)
    {
      case TypeReg8:
      case TypeReg16:
        if (AdrMode != 0) WrError(1350);
        else if (!strcasecmp(pPortArg, "DX"))
          BAsmCode[CodeLen++] = 0xec | OpSize | Index;
        else
        {
          BAsmCode[CodeLen + 1] = EvalIntExpression(ArgStr[2], UInt8, &OK);
          if (OK)
          {
            ChkSpace(SegIO);
            BAsmCode[CodeLen] = 0xe4 | OpSize | Index;
            CodeLen += 2;
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeCALLJMP(Word Index)
{
  Byte AdrByte;
  Word AdrWord;
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    char *pAdr = ArgStr[1];

    if (!strncmp(pAdr, "SHORT ", 6))
    {
      AdrByte = 2;
      pAdr += 6;
      KillPrefBlanks(pAdr);
    }
    else if ((!strncmp(pAdr, "LONG ", 5)) || (!strncmp(pAdr, "NEAR ", 5)))
    {
      AdrByte = 1;
      pAdr +=  5;
      KillPrefBlanks(pAdr);
    }
    else
      AdrByte = 0;
    OK = True;
    if (Index == 0)
    {
      if (AdrByte == 2)
      {
        WrError(1350);
        OK = False;
      }
      else
        AdrByte = 1;
    }

    if (OK)
    {
      OpSize = 1;
      DecodeAdr(ArgStr[1]);
      switch (AdrType)
      {
        case TypeReg16:
          BAsmCode[0] = 0xff;
          BAsmCode[1] = AdrMode | (0xd0 + (Index << 4));
          CodeLen = 2;
          break;
        case TypeMem:
          BAsmCode[0] = 0xff;
          BAsmCode[1] = AdrMode | (0x10 + (Index << 4));
          MoveAdr(2);
          CodeLen = 2 + AdrCnt;
          break;
        case TypeImm:
          ChkSpace(SegCode);
          AdrWord = (((Word) AdrVals[1]) << 8) | AdrVals[0];
          if ((AdrByte == 2) || ((AdrByte == 0) && (AbleToSign(AdrWord - EProgCounter() - 2))))
          {
            AdrWord -= EProgCounter() + 2;
            if (!AbleToSign(AdrWord)) WrError(1330);
            else
            {
              BAsmCode[0] = 0xeb;
              BAsmCode[1] = Lo(AdrWord);
              CodeLen = 2;
            }
          }
          else
          {
            AdrWord -= EProgCounter() + 3;
            ChkSpace(SegCode);
            BAsmCode[0] = 0xe8 | Index;
            BAsmCode[1] = Lo(AdrWord);
            BAsmCode[2] = Hi(AdrWord);
            CodeLen = 3;
            AdrWord++;
          }
          break;
        default:
         if (AdrType != TypeNone)
           WrError(1350);
      }
    }
  }
  AddPrefixes();
}

static void DecodePUSHPOP(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    OpSize = 1; DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg16:
        BAsmCode[CodeLen] = 0x50 |  AdrMode | (Index << 3);
        CodeLen++;
        break;
      case TypeRegSeg:
        BAsmCode[CodeLen] = 0x06 | (AdrMode << 3) | Index;
        CodeLen++;
        break;
      case TypeMem:
        BAsmCode[CodeLen] = 0x8f;
        BAsmCode[CodeLen + 1] = AdrMode;
        if (Index == 0)
        {
          BAsmCode[CodeLen] += 0x70;
          BAsmCode[CodeLen + 1] += 0x30;
        }
        MoveAdr(2);
        CodeLen += 2 + AdrCnt;
        break;
      case TypeImm:
        if (MomCPU < CPU80186) WrError(1500);
        else if (Index == 1) WrError(1350);
        else
        {
          BAsmCode[CodeLen] = 0x68;
          BAsmCode[CodeLen + 1] = AdrVals[0];
          if (Sgn(AdrVals[0]) == AdrVals[1])
          {
            BAsmCode[CodeLen] += 2;
            CodeLen += 2;
          }
          else
          {
            BAsmCode[CodeLen + 2] = AdrVals[1];
            CodeLen += 3;
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeNOTNEG(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    MinOneIs0();
    BAsmCode[CodeLen] = 0xf6 | OpSize;
    BAsmCode[CodeLen + 1] = 0x10 | Index;
    switch (AdrType)
    {
      case TypeReg8:
      case TypeReg16:
        BAsmCode[CodeLen + 1] |= 0xc0 | AdrMode;
        CodeLen += 2;
        break;
      case TypeMem:
        if (OpSize == -1) WrError(1132);
        else
        {
          BAsmCode[CodeLen + 1] |= AdrMode;
          MoveAdr(2);
          CodeLen += 2 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeRET(Word Index)
{
  Word AdrWord;
  Boolean OK;

  if (ArgCnt > 1) WrError(1110);
  else if (ArgCnt == 0)
   BAsmCode[CodeLen++] = 0xc3 | Index;
  else
  {
    AdrWord = EvalIntExpression(ArgStr[1], Int16, &OK);
    if (OK)
    {
      BAsmCode[CodeLen++] = 0xc2 | Index;
      BAsmCode[CodeLen++] = Lo(AdrWord);
      BAsmCode[CodeLen++] = Hi(AdrWord);
    }
  }
}

static void DecodeTEST(Word Index)
{
  Byte AdrByte;
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg8:
      case TypeReg16:
        BAsmCode[CodeLen + 1] = (AdrMode << 3);
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
          case TypeReg16:
            BAsmCode[CodeLen + 1] += 0xc0 | AdrMode;
            BAsmCode[CodeLen] = 0x84 | OpSize;
            CodeLen += 2;
            break;
          case TypeMem:
            BAsmCode[CodeLen + 1] |= AdrMode;
            BAsmCode[CodeLen] = 0x84 | OpSize;
            MoveAdr(2);
            CodeLen += 2 + AdrCnt;
            break;
          case TypeImm:
            if (((BAsmCode[CodeLen+1] >> 3) & 7) == 0)
            {
              BAsmCode[CodeLen] = 0xa8 | OpSize;
              MoveAdr(1);
              CodeLen += 1 + AdrCnt;
            }
            else
            {
              BAsmCode[CodeLen] = OpSize | 0xf6;
              BAsmCode[CodeLen + 1] = (BAsmCode[CodeLen + 1] >> 3) | 0xc0;
              MoveAdr(2);
              CodeLen += 2 + AdrCnt;
            }
            break;
          default:
            if (AdrType != TypeNone) WrError(1350);
        }
        break;
      case TypeMem:
        BAsmCode[CodeLen + 1] = AdrMode;
        AdrByte = AdrCnt;
        MoveAdr(2);
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
          case TypeReg16:
            BAsmCode[CodeLen] = 0x84 | OpSize;
            BAsmCode[CodeLen + 1] += (AdrMode << 3);
            CodeLen += 2 + AdrByte;
            break;
          case TypeImm:
            BAsmCode[CodeLen] = OpSize | 0xf6;
            MoveAdr(2 + AdrByte);
            CodeLen += 2 + AdrCnt + AdrByte;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeXCHG(Word Index)
{
  Byte AdrByte;
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg8:
      case TypeReg16:
        AdrByte = AdrMode;
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
          case TypeReg16:
            if ((OpSize == 1) && ((AdrMode == 0) || (AdrByte == 0)))
            {
              BAsmCode[CodeLen] = 0x90 | AdrMode | AdrByte;
              CodeLen++;
            }
            else
            {
              BAsmCode[CodeLen] = 0x86 | OpSize;
              BAsmCode[CodeLen+1] = AdrMode | 0xc0 | (AdrByte << 3);
              CodeLen += 2;
            }
            break;
          case TypeMem:
            BAsmCode[CodeLen] = 0x86 | OpSize;
            BAsmCode[CodeLen+1] = AdrMode | (AdrByte << 3);
            MoveAdr(2);
            CodeLen += AdrCnt + 2;
            break; 
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      case TypeMem:
        BAsmCode[CodeLen + 1] = AdrMode;
        MoveAdr(2);
        AdrByte = AdrCnt;
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
          case TypeReg16:
            BAsmCode[CodeLen] = 0x86 | OpSize;
            BAsmCode[CodeLen+1] |= (AdrMode << 3);
            CodeLen += AdrByte + 2;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeCALLJMPF(Word Index)
{
  char *p;
  Word AdrWord;
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    p = QuotPos(ArgStr[1], ':');
    if (!p)
    {
      DecodeAdr(ArgStr[1]);
      switch (AdrType)
      {
        case TypeMem:
          BAsmCode[CodeLen] = 0xff;
          BAsmCode[CodeLen + 1] = AdrMode | Hi(Index);
          MoveAdr(2);
          CodeLen += 2 + AdrCnt;
          break;
        default:
          if (AdrType != TypeNone)
            WrError(1350);
      }
    }
    else
    {
      *p = '\0';
      AdrWord = EvalIntExpression(ArgStr[1], UInt16, &OK);
      if (OK)
      {
        BAsmCode[CodeLen + 3] = Lo(AdrWord);
        BAsmCode[CodeLen + 4] = Hi(AdrWord);
        AdrWord = EvalIntExpression(p + 1, UInt16, &OK);
        if (OK)
        {
          BAsmCode[CodeLen + 1] = Lo(AdrWord);
          BAsmCode[CodeLen + 2] = Hi(AdrWord);
          BAsmCode[CodeLen] = Lo(Index);
          CodeLen += 5;
        }
      }
    }
  }
  AddPrefixes();
}

static void DecodeENTER(Word Index)
{
  Word AdrWord;
  Boolean OK;
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else if (MomCPU < CPU80186) WrError(1500);
  else
  {
    AdrWord = EvalIntExpression(ArgStr[1], Int16, &OK);
    if (OK)
    {
      BAsmCode[CodeLen+1] = Lo(AdrWord);
      BAsmCode[CodeLen+2] = Hi(AdrWord);
      BAsmCode[CodeLen+3] = EvalIntExpression(ArgStr[2], Int8, &OK);
      if (OK)
      {
        BAsmCode[CodeLen] = 0xc8;
        CodeLen += 4;
      }
    }
  }
  AddPrefixes();
}

static void DecodeFixed(Word Code)
{
  if (ArgCnt != 0) WrError(1110);
  else
    PutCode(Code);
  AddPrefixes();
}

static void DecodeALU2(Word Index)
{
  Byte AdrByte;

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg8:
      case TypeReg16:
        BAsmCode[CodeLen + 1] = AdrMode << 3;
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
          case TypeReg16:
            BAsmCode[CodeLen + 1] |= 0xc0 | AdrMode;
            BAsmCode[CodeLen] = (Index << 3) | 2 | OpSize;
            CodeLen += 2;
            break;
          case TypeMem:
            BAsmCode[CodeLen + 1] |= AdrMode;
            BAsmCode[CodeLen] = (Index << 3) | 2 | OpSize;
            MoveAdr(2);
            CodeLen += 2 + AdrCnt;
            break;
          case TypeImm:
            if (((BAsmCode[CodeLen+1] >> 3) & 7) == 0)
            {
              BAsmCode[CodeLen] = (Index << 3) | 4 | OpSize;
              MoveAdr(1);
              CodeLen += 1 + AdrCnt;
            }
            else
            {
              BAsmCode[CodeLen] = OpSize | 0x80;
              if ((OpSize == 1) && (Sgn(AdrVals[0]) == AdrVals[1]))
              {
                AdrCnt = 1;
                BAsmCode[CodeLen] |= 2;
              }
              BAsmCode[CodeLen + 1] = (BAsmCode[CodeLen + 1] >> 3) + 0xc0 + (Index << 3);
              MoveAdr(2);
              CodeLen += 2 + AdrCnt;
            }
            break;
          default:
            if (AdrType != TypeNone) WrError(1350);
        }
        break;
      case TypeMem:
        BAsmCode[CodeLen + 1] = AdrMode;
        AdrByte = AdrCnt;
        MoveAdr(2);
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
          case TypeReg16:
            BAsmCode[CodeLen] = (Index << 3) | OpSize;
            BAsmCode[CodeLen + 1] |= (AdrMode << 3);
            CodeLen += 2 + AdrByte;
            break;
          case TypeImm:
            BAsmCode[CodeLen] = OpSize | 0x80;
            if ((OpSize == 1) && (Sgn(AdrVals[0]) == AdrVals[1]))
            {
              AdrCnt = 1;
              BAsmCode[CodeLen] += 2;
            }
            BAsmCode[CodeLen + 1] += (Index << 3);
            MoveAdr(2 + AdrByte);
            CodeLen += 2 + AdrCnt + AdrByte;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      default: 
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeRel(Word Code)
{
  Word AdrWord;
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    AdrWord = EvalIntExpression(ArgStr[1], Int16, &OK);
    if (OK)
    {
      ChkSpace(SegCode);
      AdrWord -= EProgCounter() + 2;
      if (Hi(Code) != 0)
        AdrWord--;
      if ((AdrWord >= 0x80) && (AdrWord < 0xff80) && (!SymbolQuestionable)) WrError(1370);
      else
      {
        PutCode(Code);
        BAsmCode[CodeLen++] = Lo(AdrWord);
      }
    }
  }
}

static void DecodeASSUME(void)
{
  Boolean OK;
  int z, z2, z3;
  char *p;
  String SegPart, ValPart;

  if (ArgCnt == 0) WrError(1110);
  else
  {
    z = 1 ; OK = True;
    while ((z <= ArgCnt) && (OK))
    {
      OK = False; p = QuotPos(ArgStr[z], ':');
      if (p)
      {
        *p = '\0';
        strmaxcpy(SegPart, ArgStr[z], 255);
        strmaxcpy(ValPart, p + 1, 255);
      }
      else
      {
        strmaxcpy(SegPart, ArgStr[z], 255);
        *ValPart = '\0';
      }
      z2 = 0;
      while ((z2 <= SegRegCnt) && (strcasecmp(SegPart, SegRegNames[z2])))
        z2++;
      if (z2 > SegRegCnt) WrXError(1962, SegPart);
      else
      {
        z3 = 0;
        while ((z3 <= PCMax) && (strcasecmp(ValPart, SegNames[z3])))
          z3++;
        if (z3 > PCMax) WrXError(1961, ValPart);
        else if ((z3 != SegCode) && (z3 != SegData) && (z3 != SegXData) && (z3 != SegNone)) WrError(1960);
        else
        {
          SegAssumes[z2] = z3;
          OK = True;
        }
      }
      z++;
    }
  }
}

static void DecodePORT(Word Code)
{
  UNUSED(Code);

  CodeEquate(SegIO, 0, 0xffff);
}

static void DecodeFPUFixed(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 0) WrError(1110);
  else
  {
    PutCode(Code);
    AddPrefixes();
  }
}

static void DecodeFPUSt(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    if (AdrType == TypeFReg)
    {
      PutCode(Code);
      BAsmCode[CodeLen-1] |= AdrMode;
      AddPrefixes();
    }
    else if (AdrType != TypeNone)
      WrError(1350);
  }
}

static void DecodeFLD(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeFReg:
        BAsmCode[CodeLen] = 0xd9;
        BAsmCode[CodeLen + 1] = 0xc0 | AdrMode;
        CodeLen += 2;
        break;
      case TypeMem:
        if ((OpSize == -1) && (UnknownFlag))
          OpSize = 2;
        if (OpSize == -1) WrError(1132);
        else if (OpSize < 2) WrError(1130);
        else
        {
          MoveAdr(2);
          BAsmCode[CodeLen + 1] = AdrMode;
          switch (OpSize)
          {
            case 2:
              BAsmCode[CodeLen] = 0xd9;
              break;
            case 3:
              BAsmCode[CodeLen] = 0xdd;
              break;
            case 4:
              BAsmCode[CodeLen] = 0xdb;
              BAsmCode[CodeLen + 1] += 0x28;
              break;
          }
          CodeLen += 2+AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone) WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFILD(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeMem:
        if ((OpSize  == -1) && (UnknownFlag)) OpSize = 1;
        if (OpSize == -1) WrError(1132);
        else if ((OpSize < 1) || (OpSize > 3)) WrError(1130);
        else
        {
          MoveAdr(2);
          BAsmCode[CodeLen + 1] = AdrMode;
          switch (OpSize)
          {
            case 1:
              BAsmCode[CodeLen] = 0xdf;
              break;
            case 2:
              BAsmCode[CodeLen] = 0xdb;
              break;
            case 3:
              BAsmCode[CodeLen] = 0xdf;
              BAsmCode[CodeLen + 1] |= 0x28;
              break;
          }
          CodeLen += 2 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone) WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFBLD(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeMem:
        if ((OpSize == -1) && (UnknownFlag))
          OpSize = 4;
        if (OpSize == -1) WrError(1132);
        else if (OpSize != 4) WrError(1130);
        else
        {
          BAsmCode[CodeLen] = 0xdf;
          MoveAdr(2);
          BAsmCode[CodeLen+1] = AdrMode+0x20;
          CodeLen += 2+AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone) WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFST_FSTP(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeFReg:
        BAsmCode[CodeLen] = 0xdd;
        BAsmCode[CodeLen + 1] = Code | AdrMode;
        CodeLen += 2;
        break;
      case TypeMem:
        if ((OpSize == -1) && (UnknownFlag))
          OpSize = 2;
        if (OpSize == -1) WrError(1132);
        else if ((OpSize < 2) || ((OpSize == 4) && (Code == 0xd0))) WrError(1130);
        else
        {
          MoveAdr(2);
          BAsmCode[CodeLen + 1] = AdrMode | 0x10 | (Code & 8);
          switch (OpSize)
          {
            case 2:
              BAsmCode[CodeLen] = 0xd9;
              break;
            case 3:
              BAsmCode[CodeLen] = 0xdd;
            case 4:
              BAsmCode[CodeLen] = 0xdb;
              BAsmCode[CodeLen + 1] |= 0x20;
              break;
          }
          CodeLen += 2 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone) WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFIST_FISTP(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeMem:
        if ((OpSize == -1) && (UnknownFlag)) OpSize = 1;
        if (OpSize == -1) WrError(1132);
        else if ((OpSize < 1) || (OpSize == 4) || ((OpSize == 3) && (Code == 0x10))) WrError(1130);
        else
        {
          MoveAdr(2);
          BAsmCode[CodeLen + 1] = AdrMode | Code;
          switch (OpSize)
          {
            case 1:
              BAsmCode[CodeLen] = 0xdf;
              break;
            case 2:
              BAsmCode[CodeLen] = 0xdb;
              break;
            case 3:
              BAsmCode[CodeLen] = 0xdf;
              BAsmCode[CodeLen + 1] = 0x20;
              break;
          }
          CodeLen += 2 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone) WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFBSTP(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeMem:
        if ((OpSize == -1) && (UnknownFlag))
          OpSize = 1;
        if (OpSize == -1) WrError(1132);
        else if (OpSize != 4) WrError(1130);
        else
        {
          BAsmCode[CodeLen] = 0xdf;
          BAsmCode[CodeLen + 1] = AdrMode | 0x30;
          MoveAdr(2);
          CodeLen += 2 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFCOM_FCOMP(Word Code)
{
  if (!FPUEntry(&Code))
    return;     

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeFReg:
        BAsmCode[CodeLen] = 0xd8;
        BAsmCode[CodeLen+1] = Code | AdrMode;
        CodeLen += 2;
        break;
      case TypeMem:
        if ((OpSize == -1) && (UnknownFlag))
          OpSize = 1;
        if (OpSize == -1) WrError(1132);
        else if ((OpSize != 2) && (OpSize != 3)) WrError(1130);
        else
        {
          BAsmCode[CodeLen] = (OpSize == 2) ? 0xd8 : 0xdc;
          BAsmCode[CodeLen + 1] = AdrMode | 0x10 | (Code & 8);
          MoveAdr(2);
          CodeLen += 2 + AdrCnt;
        }
        break;
      default: 
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFICOM_FICOMP(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeMem:
        if ((OpSize == -1) && (UnknownFlag))
          OpSize = 1;
        if (OpSize == -1) WrError(1132);
        else if ((OpSize != 1) && (OpSize != 2)) WrError(1130);
        else
        {
          BAsmCode[CodeLen] = (OpSize == 1) ? 0xde  : 0xda;
          BAsmCode[CodeLen+1] = AdrMode | Code;
          MoveAdr(2);
          CodeLen += 2 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone) WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFADD_FMUL(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt == 0)
  {
    BAsmCode[CodeLen] = 0xde;
    BAsmCode[CodeLen + 1] = 0xc1 + Code;
    CodeLen += 2;
  }
  else if (ArgCnt > 2) WrError(1110);
  else 
  {
    char *pArg1 = ArgStr[1],
         *pArg2 = ArgStr[2],
         ArgST[10];

    if (ArgCnt == 1)
    {
      pArg2 = ArgStr[1];
      pArg1 = ArgST;
      strcpy(ArgST, "ST");
    }

    DecodeAdr(pArg1);
    OpSize = -1;
    switch (AdrType)
    {
      case TypeFReg:
        if (AdrMode != 0)   /* ST(i) ist Ziel */
        {
          BAsmCode[CodeLen + 1] = AdrMode;
          DecodeAdr(pArg2);
          if ((AdrType != TypeFReg) || (AdrMode != 0)) WrError(1350);
          else
          {
            BAsmCode[CodeLen] = 0xdc;
            BAsmCode[CodeLen + 1] += 0xc0 + Code;
            CodeLen += 2;
          }
        }
        else                      /* ST ist Ziel */
        {
          DecodeAdr(pArg2);
          switch (AdrType)
          {
            case TypeFReg:
              BAsmCode[CodeLen] = 0xd8;
              BAsmCode[CodeLen + 1] = 0xc0 + AdrMode + Code;
              CodeLen += 2;
              break;
            case TypeMem:
              if ((OpSize == -1) && (UnknownFlag)) OpSize = 2;
              if (OpSize == -1) WrError(1132);
              else if ((OpSize != 2) && (OpSize != 3)) WrError(1130);
              else
              {
                BAsmCode[CodeLen] = (OpSize == 2) ? 0xd8 : 0xdc;
                BAsmCode[CodeLen + 1] = AdrMode + Code;
                MoveAdr(2);
                CodeLen += AdrCnt + 2;
              }
              break;
            default:
              if (AdrType != TypeNone)
                WrError(1350);
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFIADD_FIMUL(Word Code)
{
  char *pArg1 = ArgStr[1],
       *pArg2 = ArgStr[2],
       ArgST[10];

  if (!FPUEntry(&Code))
    return;

  if (ArgCnt == 1)
  {
    pArg2 = ArgStr[1];
    pArg1 = ArgST;
    strcpy(ArgST, "ST");
  }
  else if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(pArg1);
    switch (AdrType)
    {
      case TypeFReg:
        if (AdrMode != 0) WrError(1350);
        else
        {
          OpSize = -1;
          DecodeAdr(pArg2);
          if ((AdrType != TypeMem) && (AdrType != TypeNone)) WrError(1350);
          else if (AdrType != TypeNone)
          {
            if ((OpSize == -1) && (UnknownFlag)) OpSize = 1;
            if (OpSize == -1) WrError(1132);
            else if ((OpSize != 1) && (OpSize != 2)) WrError(1130);
            else
            {
              BAsmCode[CodeLen] = (OpSize == 1) ? 0xde : 0xda;
              BAsmCode[CodeLen+1] = AdrMode + Code;
              MoveAdr(2);
              CodeLen += 2 + AdrCnt;
            }
          }
        }
        break;
      default: 
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFADDP_FMULP(Word Code)
{
  if (!FPUEntry(&Code))
    return;
   
  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[2]);
    switch (AdrType)
    {
      case TypeFReg:
        if (AdrMode != 0) WrError(1350);
        else
        {
          DecodeAdr(ArgStr[1]);
          if ((AdrType != TypeFReg) && (AdrType != TypeNone)) WrError(1350);
          else if (AdrType != TypeNone)
          {
            BAsmCode[CodeLen] = 0xde;
            BAsmCode[CodeLen + 1] = 0xc0 + AdrMode + Code;
            CodeLen += 2;
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFSUB_FSUBR_FDIV_FDIVR(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if (ArgCnt == 0)
  {
    BAsmCode[CodeLen] = 0xde;
    BAsmCode[CodeLen + 1] = 0xe1 + (Code ^ 8);
    CodeLen += 2;
  }
  else if (ArgCnt > 2) WrError(1110);
  else
  {
    char *pArg1 = ArgStr[1],
         *pArg2 = ArgStr[2],
         ArgST[10];

    if (ArgCnt == 1)
    {
      strcpy(ArgST, "ST");
      pArg1 = ArgST;
      pArg2 = ArgStr[1];
    }

    DecodeAdr(pArg1);
    OpSize = -1;
    switch (AdrType)
    {
      case TypeFReg:
        if (AdrMode != 0)   /* ST(i) ist Ziel */
        {
          BAsmCode[CodeLen + 1] = AdrMode;
          DecodeAdr(pArg2);
          switch (AdrType)
          {
            case TypeFReg:
              if (AdrMode != 0) WrError(1350);
              else
              {
                BAsmCode[CodeLen] = 0xdc;
                BAsmCode[CodeLen + 1] += 0xe0 + (Code ^ 8);
                CodeLen += 2;
              }
              break;
            default:
              if (AdrType != TypeNone)
                WrError(1350);
          }
        }
        else               /* ST ist Ziel */
        {
          DecodeAdr(pArg2);
          switch (AdrType)
          {
            case TypeFReg:
              BAsmCode[CodeLen] = 0xd8;
              BAsmCode[CodeLen + 1] = 0xe0 + AdrMode + Code;
              CodeLen += 2;
              break;
            case TypeMem:
              if ((OpSize == -1) && (UnknownFlag))
                OpSize = 2;
              if (OpSize == -1) WrError(1132);
              else if ((OpSize != 2) && (OpSize != 3)) WrError(1130);
              else
              {
                BAsmCode[CodeLen] = (OpSize == 2) ? 0xd8 : 0xdc;
                BAsmCode[CodeLen + 1] = AdrMode + 0x20 + Code;
                MoveAdr(2);
                CodeLen += AdrCnt + 2;
              }
              break;
            default:
              if (AdrType != TypeNone)
                WrError(1350);
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFISUB_FISUBR_FIDIV_FIDIVR(Word Code)
{
  if (!FPUEntry(&Code))
    return;

  if ((ArgCnt < 1) || (ArgCnt > 2)) WrError(1110);
  else
  {
    char *pArg1 = ArgStr[1],
         *pArg2 = ArgStr[2],
         ArgST[10];

    if (ArgCnt == 1)
    {
      strcpy(ArgST, "ST");
      pArg1 = ArgST;
      pArg2 = ArgStr[1];
    }

    DecodeAdr(pArg1);
    switch (AdrType)
    {
      case TypeFReg:
        if (AdrMode != 0) WrError(1350);
        else
        {
          OpSize = -1;
          DecodeAdr(pArg2);
          switch (AdrType)
          {
            case TypeMem:
              if ((OpSize == -1) && (UnknownFlag))
                OpSize = 1;
              if (OpSize == -1) WrError(1132);
              else if ((OpSize != 1) && (OpSize != 2)) WrError(1130);
              else
              {
                BAsmCode[CodeLen] = (OpSize == 1) ? 0xde : 0xda;
                BAsmCode[CodeLen + 1] = AdrMode + 0x20 + Code;
                MoveAdr(2);
                CodeLen += 2 + AdrCnt;
              }
              break;
            default:
              if (AdrType != TypeNone)
                WrError(1350);
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFSUBP_FSUBRP_FDIVP_FDIVRP(Word Code)
{
  if (!FPUEntry(&Code))
    return;
 
  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[2]);
    switch (AdrType)
    {
      case TypeFReg:
        if (AdrMode != 0) WrError(1350);
        else
        {
          DecodeAdr(ArgStr[1]);
          switch (AdrType)
          {
            case TypeFReg:
              BAsmCode[CodeLen] = 0xde;
              BAsmCode[CodeLen+1] = 0xe0 + AdrMode + (Code ^ 8);
              CodeLen += 2;
              break;
            default:
              if (AdrType != TypeNone)
                WrError(1350);
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFPU16(Word Code)
{
  if (!FPUEntry(&Code))
    return;
 
  if (ArgCnt != 1) WrError(1110);
  else
  {
    OpSize = 1;
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeMem:
        PutCode(Code);
        BAsmCode[CodeLen - 1] += AdrMode;
        MoveAdr(0);
        CodeLen += AdrCnt;
        break;
      default:
        if (AdrType != TypeNone) WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeFSAVE_FRSTOR(Word Code)
{
  if (!FPUEntry(&Code))
    return;
 
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeMem:
        BAsmCode[CodeLen] = 0xdd;
        BAsmCode[CodeLen + 1] = AdrMode + Code;
        MoveAdr(2);
        CodeLen += 2 + AdrCnt;
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeRept(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    int z2;

    for (z2 = 0; z2 < StringOrderCnt; z2++)
      if (!strcasecmp(StringOrders[z2].Name,ArgStr[1]))
        break;
    if (z2 >= StringOrderCnt) WrError(1985);
    else if (MomCPU < StringOrders[z2].MinCPU) WrError(1500);
    else
    {
      PutCode(Code);
      PutCode(StringOrders[z2].Code);
    }
  }
  AddPrefixes();
}

static void DecodeMul(Word Index)
{
  Boolean OK;
  Word AdrWord;

  switch (ArgCnt)
  {
    case 1:
      DecodeAdr(ArgStr[1]);
      switch (AdrType)
      {
        case TypeReg8:
        case TypeReg16:
          BAsmCode[CodeLen] = 0xf6 + OpSize;
          BAsmCode[CodeLen + 1] = 0xe0 + (Index << 3) + AdrMode;
          CodeLen += 2;
          break;
        case TypeMem:
          MinOneIs0();
          if (OpSize == -1) WrError(1132);
          else
          {
            BAsmCode[CodeLen] = 0xf6 + OpSize;
            BAsmCode[CodeLen+1] = 0x20 + (Index << 3) + AdrMode;
            MoveAdr(2);
            CodeLen += 2 + AdrCnt;
          }
          break;
        default:
          if (AdrType != TypeNone) WrError(1350);
      }
      break;
    case 2:
    case 3:
      if (MomCPU < CPU80186) WrError(1500);
      else if (1 != Index) WrError(1110); /* IMUL only */
      else
      {
        char *pArg1 = ArgStr[1],
             *pArg2 = (ArgCnt == 2) ? ArgStr[1] : ArgStr[2],
             *pArg3 = (ArgCnt == 2) ? ArgStr[2] : ArgStr[3];

        BAsmCode[CodeLen] = 0x69;
        DecodeAdr(pArg1);
        switch (AdrType)
        {
          case TypeReg16:
            BAsmCode[CodeLen + 1] = (AdrMode << 3);
            DecodeAdr(pArg2);
            if (AdrType == TypeReg16)
            {
              AdrType = TypeMem;
              AdrMode += 0xc0;
            }
            switch (AdrType)
            {
              case TypeMem:
                BAsmCode[CodeLen + 1] += AdrMode;
                MoveAdr(2);
                AdrWord = EvalIntExpression(pArg3, Int16, &OK);
                if (OK)
                {
                  BAsmCode[CodeLen + 2 + AdrCnt] = Lo(AdrWord);
                  BAsmCode[CodeLen + 3 + AdrCnt] = Hi(AdrWord);
                  CodeLen += 2 + AdrCnt + 2;
                  if ((AdrWord >= 0xff80) || (AdrWord < 0x80))
                  {
                    CodeLen--;
                    BAsmCode[CodeLen-AdrCnt - 2 - 1] += 2;
                  }
                }
                break;
              default:
                if (AdrType != TypeNone)
                  WrError(1350);
            }
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
      }
      break;
    default:
      WrError(1110);
  }
  AddPrefixes();
}

static void DecodeModReg(Word Code)
{
  Byte AdrByte;

  NoSegCheck = Hi(Code) != 0;
  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg16:
        OpSize = Hi(Code) ? -1 : 2;
        AdrByte = (AdrMode << 3);
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeMem:
            PutCode(Lo(Code));
            BAsmCode[CodeLen] = AdrByte + AdrMode;
            MoveAdr(1);
            CodeLen += 1 + AdrCnt;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeShift(Word Code)
{
  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    MinOneIs0();
    if (OpSize == -1) WrError(1132);
    else switch (AdrType)
    {
      case TypeReg8:
      case TypeReg16:
      case TypeMem:
        BAsmCode[CodeLen] = OpSize;
        BAsmCode[CodeLen+1] = AdrMode + (Code << 3);
        if (AdrType != TypeMem)
          BAsmCode[CodeLen+1] += 0xc0;
        MoveAdr(2);
        if (!strcasecmp(ArgStr[2], "CL"))
        {
          BAsmCode[CodeLen] += 0xd2;
          CodeLen += 2 + AdrCnt;
        }
        else
        {
          Boolean OK;

          BAsmCode[CodeLen + 2 + AdrCnt] = EvalIntExpression(ArgStr[2], Int8, &OK);
          if (OK)
          {
            if (BAsmCode[CodeLen + 2 + AdrCnt] == 1)
            {
              BAsmCode[CodeLen] += 0xd0;
              CodeLen += 2 + AdrCnt;
            }
            else if (MomCPU < CPU80186) WrError(1500);
            else
            {
              BAsmCode[CodeLen] += 0xc0;
              CodeLen += 3 + AdrCnt;
            }
          }
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeROL4_ROR4(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else if (MomCPU < CPUV30) WrError(1500);
  else
  {
    DecodeAdr(ArgStr[1]);
    BAsmCode[CodeLen    ] = 0x0f;
    BAsmCode[CodeLen + 1] = Code;
    switch (AdrType)
    {
      case TypeReg8:
        BAsmCode[CodeLen+2] = 0xc0 + AdrMode;
        CodeLen += 3;
        break;
      case TypeMem:
        BAsmCode[CodeLen+2] = AdrMode;
        MoveAdr(3);
        CodeLen += 3 + AdrCnt;
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeBit1(Word Index)
{
  if (ArgCnt != 2) WrError(1110);
  else if (MomCPU < CPUV30) WrError(1500);
  else
  {
    DecodeAdr(ArgStr[1]);
    if ((AdrType == TypeReg8) || (AdrType == TypeReg16))
    {
      AdrMode += 0xc0;
      AdrType = TypeMem;
    }
    MinOneIs0();
    if (OpSize == -1) WrError(1132);
    else switch (AdrType)
    {
      case TypeMem:
        BAsmCode[CodeLen    ] = 0x0f;
        BAsmCode[CodeLen + 1] = 0x10 + (Index << 1) + OpSize;
        BAsmCode[CodeLen + 2] = AdrMode;
        MoveAdr(3);
        if (!strcasecmp(ArgStr[2], "CL"))
          CodeLen += 3 + AdrCnt;
        else
        {
          Boolean OK;

          BAsmCode[CodeLen + 1] += 8;
          BAsmCode[CodeLen + 3 + AdrCnt] = EvalIntExpression(ArgStr[2], Int4, &OK);
          if (OK)
            CodeLen += 4 + AdrCnt;
        }
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeINS_EXT(Word Code)
{
  if (ArgCnt != 2) WrError(1110);
  else if (MomCPU < CPUV30) WrError(1500);
  else
  {
    DecodeAdr(ArgStr[1]);
    if (AdrType != TypeNone)
    {
      if (AdrType != TypeReg8) WrError(1350);
      else
      {
        BAsmCode[CodeLen    ] = 0x0f;
        BAsmCode[CodeLen + 1] = Code;
        BAsmCode[CodeLen + 2] = 0xc0 + AdrMode;
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
            BAsmCode[CodeLen + 2] += (AdrMode << 3);
            CodeLen += 3;
            break;
          case TypeImm:
            if (AdrVals[0] > 15) WrError(1320);
            else
            {
              BAsmCode[CodeLen + 1] += 8;
              BAsmCode[CodeLen + 3] = AdrVals[1];
              CodeLen += 4;
            }
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
      }
    }
  }
  AddPrefixes();
}

static void DecodeFPO2(Word Code)
{
  UNUSED(Code);

  if ((ArgCnt == 0) || (ArgCnt > 2)) WrError(1110);
  else if (MomCPU < CPUV30) WrError(1500);
  else
  {
    Byte AdrByte;
    Boolean OK;

    AdrByte = EvalIntExpression(ArgStr[1], Int4, &OK);
    if (OK)
    {
      BAsmCode[CodeLen    ] = 0x66 + (AdrByte >> 3);
      BAsmCode[CodeLen + 1] = (AdrByte & 7) << 3;
      if (ArgCnt == 1)
      {
        BAsmCode[CodeLen + 1] += 0xc0;
        CodeLen += 2;
      }
      else
      {
        DecodeAdr(ArgStr[2]);
        switch (AdrType)
        {
          case TypeReg8:
            BAsmCode[CodeLen + 1] += 0xc0 + AdrMode;
            CodeLen += 2;
            break;
          case TypeMem:
            BAsmCode[CodeLen + 1] += AdrMode;
            MoveAdr(2);
            CodeLen += 2 + AdrCnt;
            break;
          default:
            if (AdrType != TypeNone)
              WrError(1350);
        }
      }
    }
  }
  AddPrefixes();
}

static void DecodeBTCLR(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 3) WrError(1110);
  else if (MomCPU < CPUV35) WrError(1500);
  else
  {
    Boolean OK;

    BAsmCode[CodeLen  ] = 0x0f;
    BAsmCode[CodeLen + 1] = 0x9c;
    BAsmCode[CodeLen + 2] = EvalIntExpression(ArgStr[1], Int8, &OK);
    if (OK)
    {
      BAsmCode[CodeLen + 3] = EvalIntExpression(ArgStr[2],UInt3,&OK);
      if (OK)
      {
        Word AdrWord;

        AdrWord = EvalIntExpression(ArgStr[3], Int16, &OK) - (EProgCounter() + 5);
        if (OK)
        {
          if ((!SymbolQuestionable) && ((AdrWord > 0x7f) && (AdrWord < 0xff80))) WrError(1330);
          else
          {
            BAsmCode[CodeLen + 4] = Lo(AdrWord);
            CodeLen += 5;
          }
        }
      }
    }
  }
  AddPrefixes();
}

static void DecodeReg16(Word Index)
{
  const AddOrder *pOrder = Reg16Orders + Index;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1]);
    switch (AdrType)
    {
      case TypeReg16:
        PutCode(pOrder->Code);
        BAsmCode[CodeLen++] = pOrder->Add + AdrMode;
        break;
      default:
        if (AdrType != TypeNone)
          WrError(1350);
    }
  }
  AddPrefixes();
}

static void DecodeString(Word Index)
{
  const FixedOrder *pOrder = StringOrders + Index;

  if (ArgCnt != 0) WrError(1110);
  else if (MomCPU < pOrder->MinCPU) WrError(1500);
  else
    PutCode(pOrder->Code);
  AddPrefixes();
}

/*---------------------------------------------------------------------------*/

static void AddFPU(char *NName, CPUVar NMin, Word NCode, InstProc NProc)
{
  if (MomCPU >= NMin)
  {
    char Instr[30];

    AddInstTable(InstTable, NName, NCode, NProc);
    sprintf(Instr, "%cN%s", *NName, NName + 1);
    AddInstTable(InstTable, Instr, NCode | NO_FWAIT_FLAG, NProc);
  }
}

static void AddFixed(char *NName, CPUVar NMin, Word NCode)
{
  if (MomCPU >= NMin)
    AddInstTable(InstTable, NName, NCode, DecodeFixed);
}

static void AddFPUFixed(char *NName, CPUVar NMin, Word NCode)
{
  AddFPU(NName, NMin, NCode, DecodeFPUFixed);
}

static void AddFPUSt(char *NName, CPUVar NMin, Word NCode)
{
  AddFPU(NName, NMin, NCode, DecodeFPUSt);
}

static void AddFPU16(char *NName, CPUVar NMin, Word NCode)
{
  AddFPU(NName, NMin, NCode, DecodeFPU16);
}

static void AddString(char *NName, CPUVar NMin, Word NCode)
{
  if (InstrZ >= StringOrderCnt) exit(255); 
  StringOrders[InstrZ].Name = NName;
  StringOrders[InstrZ].MinCPU = NMin;
  StringOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ++, DecodeString);
}

static void AddRept(char *NName, CPUVar NMin, Word NCode)
{
  if (MomCPU >= NMin)
    AddInstTable(InstTable, NName, NCode, DecodeRept);
}

static void AddRel(char *NName, CPUVar NMin, Word NCode)
{
  if (MomCPU >= NMin)
    AddInstTable(InstTable, NName, NCode, DecodeRel);
}

static void AddModReg(char *NName, CPUVar NMin, Word NCode)
{
  if (MomCPU >= NMin)
    AddInstTable(InstTable, NName, NCode, DecodeModReg);
}

static void AddShift(char *NName, CPUVar NMin, Word NCode)
{
  if (MomCPU >= NMin)
    AddInstTable(InstTable, NName, NCode, DecodeShift);
}

static void AddReg16(char *NName, CPUVar NMin, Word NCode, Byte NAdd)
{
  if (InstrZ >= Reg16OrderCnt) exit(255);
  Reg16Orders[InstrZ].MinCPU = NMin;
  Reg16Orders[InstrZ].Code = NCode;
  Reg16Orders[InstrZ].Add = NAdd;
  AddInstTable(InstTable, NName, InstrZ++, DecodeReg16);
}

static void InitFields(void)
{
  InstTable = CreateInstTable(403);
  SetDynamicInstTable(InstTable);

  AddInstTable(InstTable, "MOV"  , 0, DecodeMOV);
  AddInstTable(InstTable, "INC"  , 0, DecodeINCDEC);
  AddInstTable(InstTable, "DEC"  , 8, DecodeINCDEC);
  AddInstTable(InstTable, "INT"  , 0, DecodeINT);
  AddInstTable(InstTable, "IN"   , 0, DecodeINOUT);
  AddInstTable(InstTable, "OUT"  , 2, DecodeINOUT);
  AddInstTable(InstTable, "CALL" , 0, DecodeCALLJMP);
  AddInstTable(InstTable, "JMP"  , 1, DecodeCALLJMP);
  AddInstTable(InstTable, "PUSH" , 0, DecodePUSHPOP);
  AddInstTable(InstTable, "POP"  , 1, DecodePUSHPOP);
  AddInstTable(InstTable, "NOT"  , 0, DecodeNOTNEG);
  AddInstTable(InstTable, "NEG"  , 8, DecodeNOTNEG);
  AddInstTable(InstTable, "RET"  , 0, DecodeRET);
  AddInstTable(InstTable, "RETF" , 8, DecodeRET);
  AddInstTable(InstTable, "TEST" , 0, DecodeTEST);
  AddInstTable(InstTable, "XCHG" , 0, DecodeXCHG);
  AddInstTable(InstTable, "CALLF", 0x189a, DecodeCALLJMPF);
  AddInstTable(InstTable, "JMPF" , 0x28ea, DecodeCALLJMPF);
  AddInstTable(InstTable, "ENTER", 0, DecodeENTER);
  AddInstTable(InstTable, "PORT", 0, DecodePORT);
  AddInstTable(InstTable, "ROL4", 0x28, DecodeROL4_ROR4);
  AddInstTable(InstTable, "ROR4", 0x2a, DecodeROL4_ROR4);
  AddInstTable(InstTable, "INS", 0x31, DecodeINS_EXT);
  AddInstTable(InstTable, "EXT", 0x33, DecodeINS_EXT);
  AddInstTable(InstTable, "FPO2", 0, DecodeFPO2);
  AddInstTable(InstTable, "BTCLR", 0, DecodeBTCLR);
  AddFPU("FLD", CPU8086, 0, DecodeFLD);
  AddFPU("FILD", CPU8086, 0, DecodeFILD);
  AddFPU("FBLD", CPU8086, 0, DecodeFBLD);
  AddFPU("FST", CPU8086, 0xd0, DecodeFST_FSTP);
  AddFPU("FSTP", CPU8086, 0xd8, DecodeFST_FSTP);
  AddFPU("FIST", CPU8086, 0x10, DecodeFIST_FISTP);
  AddFPU("FISTP", CPU8086, 0x18, DecodeFIST_FISTP);
  AddFPU("FBSTP", CPU8086, 0, DecodeFBSTP);
  AddFPU("FCOM", CPU8086, 0xd0, DecodeFCOM_FCOMP);
  AddFPU("FCOMP", CPU8086, 0xd8, DecodeFCOM_FCOMP);
  AddFPU("FICOM", CPU8086, 0x10, DecodeFICOM_FICOMP);
  AddFPU("FICOMP", CPU8086, 0x18, DecodeFICOM_FICOMP);
  AddFPU("FADD", CPU8086, 0, DecodeFADD_FMUL);
  AddFPU("FMUL", CPU8086, 8, DecodeFADD_FMUL);
  AddFPU("FIADD", CPU8086, 0, DecodeFIADD_FIMUL);
  AddFPU("FIMUL", CPU8086, 8, DecodeFIADD_FIMUL);
  AddFPU("FADDP", CPU8086, 0, DecodeFADDP_FMULP);
  AddFPU("FMULP", CPU8086, 8, DecodeFADDP_FMULP);
  AddFPU("FDIV" , CPU8086, 16, DecodeFSUB_FSUBR_FDIV_FDIVR);
  AddFPU("FDIVR", CPU8086, 24, DecodeFSUB_FSUBR_FDIV_FDIVR);
  AddFPU("FSUB" , CPU8086,  0, DecodeFSUB_FSUBR_FDIV_FDIVR);
  AddFPU("FSUBR", CPU8086,  8, DecodeFSUB_FSUBR_FDIV_FDIVR);
  AddFPU("FIDIV" , CPU8086, 16, DecodeFISUB_FISUBR_FIDIV_FIDIVR);
  AddFPU("FIDIVR", CPU8086, 24, DecodeFISUB_FISUBR_FIDIV_FIDIVR);
  AddFPU("FISUB" , CPU8086,  0, DecodeFISUB_FISUBR_FIDIV_FIDIVR);
  AddFPU("FISUBR", CPU8086,  8, DecodeFISUB_FISUBR_FIDIV_FIDIVR);
  AddFPU("FDIVP" , CPU8086, 16, DecodeFSUBP_FSUBRP_FDIVP_FDIVRP);
  AddFPU("FDIVRP", CPU8086, 24, DecodeFSUBP_FSUBRP_FDIVP_FDIVRP);
  AddFPU("FSUBP" , CPU8086,  0, DecodeFSUBP_FSUBRP_FDIVP_FDIVRP);
  AddFPU("FSUBRP", CPU8086,  8, DecodeFSUBP_FSUBRP_FDIVP_FDIVRP);
  AddFPU("FSAVE" , CPU8086, 0x30, DecodeFSAVE_FRSTOR);
  AddFPU("FRSTOR" , CPU8086, 0x20, DecodeFSAVE_FRSTOR);

  AddFixed("AAA",   CPU8086,  0x0037);  AddFixed("AAS",   CPU8086,  0x003f);
  AddFixed("AAM",   CPU8086,  0xd40a);  AddFixed("AAD",   CPU8086,  0xd50a);
  AddFixed("CBW",   CPU8086,  0x0098);  AddFixed("CLC",   CPU8086,  0x00f8);
  AddFixed("CLD",   CPU8086,  0x00fc);  AddFixed("CLI",   CPU8086,  0x00fa);
  AddFixed("CMC",   CPU8086,  0x00f5);  AddFixed("CWD",   CPU8086,  0x0099);
  AddFixed("DAA",   CPU8086,  0x0027);  AddFixed("DAS",   CPU8086,  0x002f);
  AddFixed("HLT",   CPU8086,  0x00f4);  AddFixed("INTO",  CPU8086,  0x00ce);
  AddFixed("IRET",  CPU8086,  0x00cf);  AddFixed("LAHF",  CPU8086,  0x009f);
  AddFixed("LOCK",  CPU8086,  0x00f0);  AddFixed("NOP",   CPU8086,  0x0090);
  AddFixed("POPF",  CPU8086,  0x009d);  AddFixed("PUSHF", CPU8086,  0x009c);
  AddFixed("SAHF",  CPU8086,  0x009e);  AddFixed("STC",   CPU8086,  0x00f9);
  AddFixed("STD",   CPU8086,  0x00fd);  AddFixed("STI",   CPU8086,  0x00fb);
  AddFixed("WAIT",  CPU8086,  0x009b);  AddFixed("XLAT",  CPU8086,  0x00d7);
  AddFixed("LEAVE", CPU80186, 0x00c9);  AddFixed("PUSHA", CPU80186, 0x0060);
  AddFixed("POPA",  CPU80186, 0x0061);  AddFixed("ADD4S", CPUV30,   0x0f20);
  AddFixed("SUB4S", CPUV30,   0x0f22);  AddFixed("CMP4S", CPUV30,   0x0f26);
  AddFixed("STOP",  CPUV35,   0x0f9e);  AddFixed("RETRBI",CPUV35,   0x0f91);
  AddFixed("FINT",  CPUV35,   0x0f92);  AddFixed("MOVSPA",CPUV35,   0x0f25);
  AddFixed("SEGES", CPU8086,  0x0026);  AddFixed("SEGCS", CPU8086,  0x002e);
  AddFixed("SEGSS", CPU8086,  0x0036);  AddFixed("SEGDS", CPU8086,  0x003e);
  AddFixed("FWAIT", CPU8086,  0x009b);  

  AddFPUFixed("FCOMPP", CPU8086, 0xded9); AddFPUFixed("FTST",   CPU8086, 0xd9e4);
  AddFPUFixed("FXAM",   CPU8086, 0xd9e5); AddFPUFixed("FLDZ",   CPU8086, 0xd9ee);
  AddFPUFixed("FLD1",   CPU8086, 0xd9e8); AddFPUFixed("FLDPI",  CPU8086, 0xd9eb);
  AddFPUFixed("FLDL2T", CPU8086, 0xd9e9); AddFPUFixed("FLDL2E", CPU8086, 0xd9ea);
  AddFPUFixed("FLDLG2", CPU8086, 0xd9ec); AddFPUFixed("FLDLN2", CPU8086, 0xd9ed);
  AddFPUFixed("FSQRT",  CPU8086, 0xd9fa); AddFPUFixed("FSCALE", CPU8086, 0xd9fd);
  AddFPUFixed("FPREM",  CPU8086, 0xd9f8); AddFPUFixed("FRNDINT",CPU8086, 0xd9fc);
  AddFPUFixed("FXTRACT",CPU8086, 0xd9f4); AddFPUFixed("FABS",   CPU8086, 0xd9e1);
  AddFPUFixed("FCHS",   CPU8086, 0xd9e0); AddFPUFixed("FPTAN",  CPU8086, 0xd9f2);
  AddFPUFixed("FPATAN", CPU8086, 0xd9f3); AddFPUFixed("F2XM1",  CPU8086, 0xd9f0);
  AddFPUFixed("FYL2X",  CPU8086, 0xd9f1); AddFPUFixed("FYL2XP1",CPU8086, 0xd9f9);
  AddFPUFixed("FINIT",  CPU8086, 0xdbe3); AddFPUFixed("FENI",   CPU8086, 0xdbe0);
  AddFPUFixed("FDISI",  CPU8086, 0xdbe1); AddFPUFixed("FCLEX",  CPU8086, 0xdbe2);
  AddFPUFixed("FINCSTP",CPU8086, 0xd9f7); AddFPUFixed("FDECSTP",CPU8086, 0xd9f6);
  AddFPUFixed("FNOP",   CPU8086, 0xd9d0);

  AddFPUSt("FXCH",  CPU8086, 0xd9c8);
  AddFPUSt("FFREE", CPU8086, 0xddc0);

  AddFPU16("FLDCW",  CPU8086, 0xd928);
  AddFPU16("FSTCW",  CPU8086, 0xd938);
  AddFPU16("FSTSW",  CPU8086, 0xdd38);
  AddFPU16("FSTENV", CPU8086, 0xd930);
  AddFPU16("FLDENV", CPU8086, 0xd920);

  StringOrders = (FixedOrder *) malloc(sizeof(FixedOrder) * StringOrderCnt); InstrZ = 0;
  AddString("CMPSB", CPU8086,  0x00a6);
  AddString("CMPSW", CPU8086,  0x00a7);
  AddString("LODSB", CPU8086,  0x00ac);
  AddString("LODSW", CPU8086,  0x00ad);
  AddString("MOVSB", CPU8086,  0x00a4);
  AddString("MOVSW", CPU8086,  0x00a5);
  AddString("SCASB", CPU8086,  0x00ae);
  AddString("SCASW", CPU8086,  0x00af);
  AddString("STOSB", CPU8086,  0x00aa);
  AddString("STOSW", CPU8086,  0x00ab);
  AddString("INSB",  CPU80186, 0x006c);
  AddString("INSW",  CPU80186, 0x006d);
  AddString("OUTSB", CPU80186, 0x006e);
  AddString("OUTSW", CPU80186, 0x006f);

  AddRept("REP",   CPU8086,  0x00f3);
  AddRept("REPE",  CPU8086,  0x00f3);
  AddRept("REPZ",  CPU8086,  0x00f3);
  AddRept("REPNE", CPU8086,  0x00f2);
  AddRept("REPNZ", CPU8086,  0x00f2);
  AddRept("REPC",  CPUV30,   0x0065);
  AddRept("REPNC", CPUV30,   0x0064);

  AddRel("JA",    CPU8086, 0x0077); AddRel("JNBE",  CPU8086, 0x0077);
  AddRel("JAE",   CPU8086, 0x0073); AddRel("JNB",   CPU8086, 0x0073);
  AddRel("JB",    CPU8086, 0x0072); AddRel("JNAE",  CPU8086, 0x0072);
  AddRel("JBE",   CPU8086, 0x0076); AddRel("JNA",   CPU8086, 0x0076);
  AddRel("JC",    CPU8086, 0x0072); AddRel("JCXZ",  CPU8086, 0x00e3);
  AddRel("JE",    CPU8086, 0x0074); AddRel("JZ",    CPU8086, 0x0074);
  AddRel("JG",    CPU8086, 0x007f); AddRel("JNLE",  CPU8086, 0x007f);
  AddRel("JGE",   CPU8086, 0x007d); AddRel("JNL",   CPU8086, 0x007d);
  AddRel("JL",    CPU8086, 0x007c); AddRel("JNGE",  CPU8086, 0x007c);
  AddRel("JLE",   CPU8086, 0x007e); AddRel("JNG",   CPU8086, 0x007e);
  AddRel("JNC",   CPU8086, 0x0073); AddRel("JNE",   CPU8086, 0x0075);
  AddRel("JNZ",   CPU8086, 0x0075); AddRel("JNO",   CPU8086, 0x0071);
  AddRel("JNS",   CPU8086, 0x0079); AddRel("JNP",   CPU8086, 0x007b);
  AddRel("JPO",   CPU8086, 0x007b); AddRel("JO",    CPU8086, 0x0070);
  AddRel("JP",    CPU8086, 0x007a); AddRel("JPE",   CPU8086, 0x007a);
  AddRel("JS",    CPU8086, 0x0078); AddRel("LOOP",  CPU8086, 0x00e2);
  AddRel("LOOPE", CPU8086, 0x00e1); AddRel("LOOPZ", CPU8086, 0x00e1);
  AddRel("LOOPNE",CPU8086, 0x00e0); AddRel("LOOPNZ",CPU8086, 0x00e0);

  AddModReg("LDS",   CPU8086,  0x00c5);
  AddModReg("LEA",   CPU8086,  0x018d);
  AddModReg("LES",   CPU8086,  0x00c4);
  AddModReg("BOUND", CPU80186, 0x0062);

  AddShift("SHL",   CPU8086, 4); AddShift("SAL",   CPU8086, 4);
  AddShift("SHR",   CPU8086, 5); AddShift("SAR",   CPU8086, 7);
  AddShift("ROL",   CPU8086, 0); AddShift("ROR",   CPU8086, 1);
  AddShift("RCL",   CPU8086, 2); AddShift("RCR",   CPU8086, 3);

  Reg16Orders = (AddOrder *) malloc(sizeof(AddOrder) * Reg16OrderCnt); InstrZ = 0;
  AddReg16("BRKCS" , CPUV35, 0x0f2d, 0xc0);
  AddReg16("TSKSW" , CPUV35, 0x0f94, 0xf8);
  AddReg16("MOVSPB", CPUV35, 0x0f95, 0xf8);

  InstrZ = 0;
  AddInstTable(InstTable, "ADD", InstrZ++, DecodeALU2);
  AddInstTable(InstTable, "OR" , InstrZ++, DecodeALU2);
  AddInstTable(InstTable, "ADC", InstrZ++, DecodeALU2);
  AddInstTable(InstTable, "SBB", InstrZ++, DecodeALU2);
  AddInstTable(InstTable, "AND", InstrZ++, DecodeALU2);
  AddInstTable(InstTable, "SUB", InstrZ++, DecodeALU2);
  AddInstTable(InstTable, "XOR", InstrZ++, DecodeALU2);
  AddInstTable(InstTable, "CMP", InstrZ++, DecodeALU2);

  InstrZ = 0;
  AddInstTable(InstTable, "MUL" , InstrZ++, DecodeMul);
  AddInstTable(InstTable, "IMUL", InstrZ++, DecodeMul);
  AddInstTable(InstTable, "DIV" , InstrZ++, DecodeMul);
  AddInstTable(InstTable, "IDIV", InstrZ++, DecodeMul);

  InstrZ = 0;
  AddInstTable(InstTable, "TEST1", InstrZ++, DecodeBit1);
  AddInstTable(InstTable, "CLR1" , InstrZ++, DecodeBit1);
  AddInstTable(InstTable, "SET1" , InstrZ++, DecodeBit1);
  AddInstTable(InstTable, "NOT1" , InstrZ++, DecodeBit1);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
  free(StringOrders);
  free(Reg16Orders);
}

static void MakeCode_86(void)
{
  CodeLen = 0;
  DontPrint = False;
  OpSize = -1;
  PrefixLen = 0;
  NoSegCheck = False;
  UnknownFlag = False;

  /* zu ignorierendes */

  if (Memo(""))
    return;

  /* Pseudoanweisungen */

  if (DecodeIntelPseudo(False))
    return;

  /* vermischtes */

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200, OpPart);
}

static void InitCode_86(void)
{
  SegAssumes[0] = SegNone; /* ASSUME ES:NOTHING */
  SegAssumes[1] = SegCode; /* ASSUME CS:CODE */
  SegAssumes[2] = SegNone; /* ASSUME SS:NOTHING */
  SegAssumes[3] = SegData; /* ASSUME DS:DATA */
}

static Boolean IsDef_86(void)
{
  return (Memo("PORT"));
}

static void SwitchFrom_86(void)
{
  DeinitFields();
  ClearONOFF();
}

static void SwitchTo_86(void)
{
  TurnWords = False; ConstMode = ConstModeIntel; SetIsOccupied = False;

  PCSymbol = "$"; HeaderID = 0x42; NOPCode = 0x90;
  DivideChars = ","; HasAttrs = False;

  ValidSegs = (1 << SegCode) | (1 << SegData) | (1 << SegXData) | (1 << SegIO);
  Grans[SegCode ] = 1; ListGrans[SegCode ] = 1; SegInits[SegCode ] = 0;
  SegLimits[SegCode ] = 0xffff;
  Grans[SegData ] = 1; ListGrans[SegData ] = 1; SegInits[SegData ] = 0;
  SegLimits[SegData ] = 0xffff;
  Grans[SegXData] = 1; ListGrans[SegXData] = 1; SegInits[SegXData] = 0;
  SegLimits[SegXData] = 0xffff;
  Grans[SegIO   ] = 1; ListGrans[SegIO   ] = 1; SegInits[SegIO   ] = 0;
  SegLimits[SegIO   ] = 0xffff;

  pASSUMEOverride = DecodeASSUME;

  MakeCode = MakeCode_86; IsDef = IsDef_86;
  SwitchFrom = SwitchFrom_86; InitFields();
  AddONOFF("FPU",&FPUAvail,FPUAvailName,False);
}

void code86_init(void)
{
  CPU8086  = AddCPU("8086" ,SwitchTo_86);
  CPU80186 = AddCPU("80186",SwitchTo_86);
  CPUV30   = AddCPU("V30"  ,SwitchTo_86);
  CPUV35   = AddCPU("V35"  ,SwitchTo_86);

  AddInitPassProc(InitCode_86);
}
