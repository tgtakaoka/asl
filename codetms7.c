/* codetms7.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator TMS7000-Familie                                             */
/*                                                                           */
/* Historie: 26. 2.1997 Grundsteinlegung                                     */
/*            9. 3.2000 'ambiguous else'-Warnungen beseitigt                 */
/*                                                                           */
/*****************************************************************************/
/* $Id: codetms7.c,v 1.6 2009/02/08 12:49:20 alfred Exp $                    */
/*****************************************************************************
 * $Log: codetms7.c,v $
 * Revision 1.6  2009/02/08 12:49:20  alfred
 * - correct DINT coding, rework to new style & instruction hash table
 *
 * Revision 1.5  2007/11/24 22:48:07  alfred
 * - some NetBSD changes
 *
 * Revision 1.4  2005/10/02 10:00:46  alfred
 * - ConstLongInt gets default base, correct length check on KCPSM3 registers
 *
 * Revision 1.3  2005/09/08 17:31:05  alfred
 * - add missing include
 *
 * Revision 1.2  2004/05/29 11:33:04  alfred
 * - relocated DecodeIntelPseudo() into own module
 *
 *****************************************************************************/

#include "stdinc.h"
#include <ctype.h>
#include <string.h>

#include "strutil.h"
#include "bpemu.h"
#include "nls.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmitree.h"  
#include "intpseudo.h"
#include "codevars.h"


#define ModNone (-1)
#define ModAccA 0
#define MModAccA (1 << ModAccA)       /* A */
#define ModAccB 1
#define MModAccB (1 << ModAccB)       /* B */
#define ModReg 2
#define MModReg (1 << ModReg)         /* Rn */
#define ModPort 3
#define MModPort (1 << ModPort)       /* Pn */
#define ModAbs 4
#define MModAbs (1 << ModAbs)         /* nnnn */
#define ModBRel 5
#define MModBRel (1 << ModBRel)       /* nnnn(B) */
#define ModIReg 6
#define MModIReg (1 << ModIReg)       /* *Rn */
#define ModImm 7
#define MModImm (1 << ModImm)         /* #nn */
#define ModImmBRel 8
#define MModImmBRel (1 << ModImmBRel) /* #nnnn(b) */


static CPUVar CPU70C40,CPU70C20,CPU70C00,
              CPU70CT40,CPU70CT20,
              CPU70C82,CPU70C42,CPU70C02,
              CPU70C48,CPU70C08;

static Byte OpSize;
static ShortInt AdrType;
static Byte AdrVals[2];

/*---------------------------------------------------------------------------*/
/* helpers */

static void ChkAdr(Word Mask)
{
  if ((AdrType != -1) && ((Mask & (1L << AdrType)) == 0))
  {
    WrError(1350); AdrType = ModNone; AdrCnt = 0;
  }
}

static void DecodeAdr(char *Asc, Word Mask)
{
  Integer HVal;
  int Lev, l;
  char *p;
  Boolean OK;

  AdrType = ModNone; AdrCnt = 0;

  if (!strcasecmp(Asc, "A"))
  {
    if (Mask & MModAccA) AdrType = ModAccA;
    else if (Mask & MModReg)
    {
      AdrCnt = 1; AdrVals[0] = 0; AdrType = ModReg;
    }
    else
    {
      AdrCnt = 2; AdrVals[0] = 0; AdrVals[1] = 0; AdrType = ModAbs;
    }
    ChkAdr(Mask); return;
  }

  if (!strcasecmp(Asc, "B"))
  {
    if (Mask & MModAccB) AdrType = ModAccB;
    else if (Mask & MModReg)
    {
      AdrCnt = 1; AdrVals[0] = 1; AdrType = ModReg;
    }
    else
    {
      AdrCnt = 2; AdrVals[0] = 0; AdrVals[1] = 1; AdrType = ModAbs;
    }
    ChkAdr(Mask); return;
  }

  if ((*Asc == '#') || (*Asc == '%'))
  {
    Asc++; l = strlen(Asc);
    if ((l >= 3) & (!strcasecmp(Asc + l - 3,"(B)")))
    {
      Asc[l - 3] = '\0';
      HVal = EvalIntExpression(Asc, Int16, &OK);
      if (OK)
      {
        AdrVals[0] = Hi(HVal); AdrVals[1] = Lo(HVal);
        AdrType = ModImmBRel; AdrCnt = 2;
      }
    }
    else
    {
      switch (OpSize)
      {
        case 0:
          AdrVals[0] = EvalIntExpression(Asc, Int8, &OK);
          break;
        case 1:
          HVal = EvalIntExpression(Asc, Int16, &OK);
          AdrVals[0] = Hi(HVal);
          AdrVals[1] = Lo(HVal);
          break;
      }
      if (OK)
      {
        AdrCnt = 1 + OpSize;
        AdrType = ModImm;
      }
    }
    ChkAdr(Mask); return;
  }

  if (*Asc == '*')
  {
    AdrVals[0] = EvalIntExpression(Asc + 1, Int8, &OK);
    if (OK)
    {
      AdrCnt = 1;
      AdrType = ModIReg;
    }
    ChkAdr(Mask); return;
  }

  if (*Asc == '@')
    Asc++;

  if ((*Asc) && (Asc[strlen(Asc) - 1]==')'))
  {
    p = Asc + strlen(Asc) - 2; Lev = 0;
    while ((p >= Asc) && (Lev != -1))
    {
      switch (*p)
      {
        case '(': Lev--; break;
        case ')': Lev++; break;
      }
      if (Lev != -1) p--;
    }
    if (p < Asc)
    {
      WrError(1300); p=Nil;
    }
  }
  else
    p = NULL;

  if (p == Nil)
  {
    HVal = EvalIntExpression(Asc, Int16, &OK);
    if (OK)
    {
      if ((Mask & MModReg) && (!Hi(HVal)))
      {
        AdrVals[0] = Lo(HVal); AdrCnt = 1; AdrType = ModReg;
      }
      else if ((Mask & MModPort) && (Hi(HVal) == 0x01))
      {
        AdrVals[0] = Lo(HVal); AdrCnt = 1; AdrType = ModPort;
      }
      else
      {
        AdrVals[0] = Hi(HVal); AdrVals[1] = Lo(HVal); AdrCnt = 2;
        AdrType = ModAbs;
      }
    }
    ChkAdr(Mask); return;
  }
  else
  {
    FirstPassUnknown = False; *p = '\0';
    HVal = EvalIntExpression(Asc, Int16, &OK);
    if (OK)
    {
      p++; p[strlen(p) - 1] = '\0';
      if (strcasecmp(p, "B") == 0)
      {
        AdrVals[0] = Hi(HVal); AdrVals[1] = Lo(HVal); AdrCnt = 2;
        AdrType = ModBRel;
      }
      else WrXError(1445,p);
    }
    ChkAdr(Mask); return;
  }
}

static void PutCode(Word Code)
{
  if (Hi(Code))
    BAsmCode[CodeLen++] = Hi(Code);
  BAsmCode[CodeLen++] = Lo(Code);
}

/*---------------------------------------------------------------------------*/
/* decoders */

static void DecodeFixed(Word Index)
{
  if (ArgCnt!=0) WrError(1110);
  else
    PutCode(Index);
}

static void DecodeRel8(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;
    Integer AdrInt = EvalIntExpression(ArgStr[1], UInt16, &OK) - (EProgCounter() + 2);

    if (OK)
    {
      if ((AdrInt > 127) || (AdrInt<-128)) WrError(1370);
      else
      {
        CodeLen = 2;
        BAsmCode[0] = Index;
        BAsmCode[1] = AdrInt & 0xff;
      }
    }
  }
}

static void DecodeALU1(Word Index)
{
  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[2], MModAccA | MModAccB | MModReg);
    switch (AdrType)
    {
      case ModAccA:
        DecodeAdr(ArgStr[1], MModAccB | MModReg | MModImm);
        switch (AdrType)
        {
          case ModAccB:
            CodeLen = 1;
            BAsmCode[0] = 0x60 | Index;
            break;
          case ModReg:
            CodeLen = 2;
            BAsmCode[0] = 0x10 | Index;
            BAsmCode[1] = AdrVals[0];
            break;
          case ModImm:
            CodeLen = 2;
            BAsmCode[0] = 0x20 | Index;
            BAsmCode[1] = AdrVals[0];
            break;
        }
        break;
      case ModAccB:
        DecodeAdr(ArgStr[1], MModReg | MModImm);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 2;
            BAsmCode[0] = 0x30 | Index;
            BAsmCode[1] = AdrVals[0];
            break;
          case ModImm:
            CodeLen=2;
            BAsmCode[0] = 0x50 | Index;
            BAsmCode[1] = AdrVals[0];
            break;
        }
        break;
      case ModReg:
        BAsmCode[2] = AdrVals[0];
        DecodeAdr(ArgStr[1], MModReg | MModImm);
        switch (AdrType)
        {
          case ModReg:
            CodeLen = 3;
            BAsmCode[0] = 0x40 | Index;
            BAsmCode[1] = AdrVals[0];
            break;
          case ModImm:
            CodeLen = 3;
            BAsmCode[0] = 0x70 | Index;
            BAsmCode[1] = AdrVals[0];
            break;
        }
        break;
    }
  }
}

static void DecodeALU2(Word Index)
{
  Boolean IsP = ((Index & 0x8000) != 0),
          IsRela = ((Index & 0x4000) != 0);

  Index &= ~0xc000;

  if (((IsRela) && (ArgCnt != 3)) OR ((!IsRela) && (ArgCnt != 2))) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[2], MModPort | (IsP ? 0 : MModAccA | MModAccB | MModReg));
    switch (AdrType)
    {
      case ModAccA:
        DecodeAdr(ArgStr[1], MModAccB | MModReg | MModImm);
        switch (AdrType)
        {
          case ModAccB:
            BAsmCode[0] = 0x60 | Index;
            CodeLen = 1;
            break;
          case ModReg:
            BAsmCode[0] = 0x10 | Index;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0x20 | Index;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
        break;
      case ModAccB:
        DecodeAdr(ArgStr[1], MModReg | MModImm);
        switch (AdrType)
        {
          case ModReg:
            BAsmCode[0] = 0x30 | Index;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0x50 | Index;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
        break;
      case ModReg:
        BAsmCode[2] = AdrVals[0];
        DecodeAdr(ArgStr[1], MModReg | MModImm);
        switch (AdrType)
        {
          case ModReg:
            BAsmCode[0] = 0x40 | Index;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
            break;
          case ModImm:
            BAsmCode[0] = 0x70 | Index;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
        }
        break;
      case ModPort:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[1], MModAccA | MModAccB | MModImm);
        switch (AdrType)
        {
          case ModAccA:
            BAsmCode[0] = 0x80 | Index;
            CodeLen = 2;
            break;
          case ModAccB:
            BAsmCode[0] = 0x90 | Index;
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0xa0 | Index;
            BAsmCode[2] = BAsmCode[1];
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
        }
        break;
    }
    if ((CodeLen != 0) && (IsRela))
    {
      Boolean OK;
      Integer AdrInt = EvalIntExpression(ArgStr[3], Int16, &OK) - (EProgCounter() + CodeLen + 1);

      if (!OK) CodeLen = 0;
      else if ((!SymbolQuestionable) && ((AdrInt > 127) OR (AdrInt < -128)))
      {
        WrError(1370); CodeLen = 0;
      }
      else
        BAsmCode[CodeLen++] = AdrInt & 0xff;
    }
  }
}

static void DecodeJmp(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAbs | MModIReg | MModBRel);
    switch (AdrType)
    {
      case ModAbs:
        CodeLen = 3;
        BAsmCode[0] = 0x80 | Index;
        memcpy(BAsmCode + 1, AdrVals, 2);
        break;
      case ModIReg:
        CodeLen = 2;
        BAsmCode[0] = 0x90 | Index;
        BAsmCode[1] = AdrVals[0];
        break;
      case ModBRel:
        CodeLen = 3;
        BAsmCode[0] = 0xa0 | Index;
        memcpy(BAsmCode + 1, AdrVals, 2);
        break;
    }
  }
}

static void DecodeABReg(Word Index)
{
  if (((!Memo("DJNZ")) && (ArgCnt != 1))
   || (( Memo("DJNZ")) && (ArgCnt != 2))) WrError(1110);
  else if (!strcasecmp(ArgStr[1], "ST"))
  {
    if ((Memo("PUSH")) || (Memo("POP")))
    {
      BAsmCode[0] = 8 | (Ord(Memo("PUSH")) * 6);
      CodeLen = 1;
    }
    else WrError(1350);
  }
  else
  {
    DecodeAdr(ArgStr[1], MModAccA | MModAccB | MModReg);
    switch (AdrType)
    {
      case ModAccA:
        BAsmCode[0] = 0xb0 | Index;
        CodeLen = 1;
        break;
      case ModAccB:
        BAsmCode[0] = 0xc0 | Index;
        CodeLen = 1;
        break;
      case ModReg:
        BAsmCode[0] = 0xd0 | Index;
        BAsmCode[1] = AdrVals[0];
        CodeLen = 2;
        break;
    }
    if ((Memo("DJNZ")) && (CodeLen != 0))
    {
      Boolean OK;
      Integer AdrInt = EvalIntExpression(ArgStr[2], UInt16, &OK) - (EProgCounter() + CodeLen + 1);

      if (!OK)
        CodeLen = 0;
      else if ((!SymbolQuestionable) & ((AdrInt > 127) || (AdrInt < -128)))
      {
        WrError(1370); CodeLen = 0;
      }
      else
        BAsmCode[CodeLen++]=AdrInt & 0xff;
    }
  }
}

static void DecodeMOV(Word IsMOVP)
{
  if (ArgCnt!=2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[2], MModPort| MModAccA| MModAccB |
                        (IsMOVP ? 0: MModReg| MModAbs | MModIReg| MModBRel));
    switch (AdrType)
    {
      case ModAccA:
        DecodeAdr(ArgStr[1],MModPort+
                            (IsMOVP ? 0 : MModReg | MModAbs | MModIReg | MModBRel | MModAccB | MModImm));
        switch (AdrType)
        {
          case ModReg:
            BAsmCode[0] = 0x12;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModAbs:
            BAsmCode[0] = 0x8a;
            memcpy(BAsmCode + 1, AdrVals, 2);
            CodeLen = 3;
            break;
          case ModIReg:
            BAsmCode[0] = 0x9a;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModBRel:
            BAsmCode[0] = 0xaa;
            memcpy(BAsmCode + 1, AdrVals, 2);
            CodeLen = 3;
            break;
          case ModAccB:
            BAsmCode[0] = 0x62;
            CodeLen = 1;
            break;
          case ModPort:
            BAsmCode[0] = 0x80;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0x22;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
        break;
      case ModAccB:
        DecodeAdr(ArgStr[1], MModPort | (IsMOVP ? 0 : MModAccA | MModReg | MModImm));
        switch (AdrType)
        {
          case ModAccA:
            BAsmCode[0] = 0xc0;
            CodeLen = 1;
            break;
          case ModReg:
            BAsmCode[0] = 0x32;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModPort:
            BAsmCode[0] = 0x91;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0x52;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
        break;
      case ModReg:
        BAsmCode[1] = BAsmCode[2] = AdrVals[0];
        DecodeAdr(ArgStr[1], MModAccA | MModAccB | MModReg | MModPort | MModImm);
        switch (AdrType) 
        {
          case ModAccA:
            BAsmCode[0] = 0xd0;
            CodeLen = 2;
            break;
          case ModAccB:
            BAsmCode[0] = 0xd1;
            CodeLen = 2;
            break;
          case ModReg:
            BAsmCode[0] = 0x42;
            BAsmCode[1] = AdrVals[0];
            CodeLen=3;
            break;
          case ModPort:
            BAsmCode[0] = 0xa2;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
            break;
          case ModImm:
            BAsmCode[0] = 0x72;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
            break;
        }
        break;
      case ModPort:
        BAsmCode[1] = BAsmCode[2] = AdrVals[0];
        DecodeAdr(ArgStr[1], MModAccA | MModAccB | MModImm);
        switch (AdrType)
        {
          case ModAccA:
            BAsmCode[0] = 0x82;
            CodeLen = 2;
            break;
          case ModAccB:
            BAsmCode[0] = 0x92;
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0xa2;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
            break;
        }
        break;
      case ModAbs:
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        DecodeAdr(ArgStr[1], MModAccA);
        if (AdrType != ModNone)
        {
          BAsmCode[0] = 0x8b;
          CodeLen = 3;
        }
        break;
      case ModIReg:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[1], MModAccA);
        if (AdrType != ModNone)
        {
          BAsmCode[0] = 0x9b;
          CodeLen = 2;
        }
        break;
      case ModBRel:
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        DecodeAdr(ArgStr[1], MModAccA);
        if (AdrType != ModNone)
        {
          BAsmCode[0] = 0xab;
          CodeLen = 3;
        }
        break;
    }
  }
}

static void DecodeLDA(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAbs | MModBRel | MModIReg);
    switch (AdrType)
    {
      case ModAbs:
        BAsmCode[0] = 0x8a;
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        CodeLen = 1 + AdrCnt;
        break;
      case ModBRel:
        BAsmCode[0] = 0xaa;
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        CodeLen = 1 + AdrCnt;
        break;
      case ModIReg:
        BAsmCode[0] = 0x9a;
        BAsmCode[1] = AdrVals[0];
        CodeLen = 2;
        break;
    }
  }
}

static void DecodeSTA(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAbs | MModBRel | MModIReg);
    switch (AdrType)
    {
      case ModAbs:
        BAsmCode[0] = 0x8b;
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        CodeLen = 1 + AdrCnt;
        break;
      case ModBRel:
        BAsmCode[0] = 0xab;
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        CodeLen = 1 + AdrCnt;
        break;
      case ModIReg:
        BAsmCode[0] = 0x9b;
        BAsmCode[1] = AdrVals[0];
        CodeLen = 2;
        break;
    }
  }
}

static void DecodeMOVWD(Word Index)
{
  UNUSED(Index);

  OpSize = 1;
  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[2], MModReg);
    if (AdrType != ModNone)
    {
      Byte z = AdrVals[0];

      DecodeAdr(ArgStr[1], MModReg | MModImm | MModImmBRel);
      switch (AdrType)
      {
        case ModReg:
          BAsmCode[0] = 0x98;
          BAsmCode[1] = AdrVals[0];
          BAsmCode[2] = z;
          CodeLen = 3;
          break;
        case ModImm:
          BAsmCode[0] = 0x88;
          memcpy(BAsmCode + 1, AdrVals, 2);
          BAsmCode[3] = z;
          CodeLen = 4;
          break;
        case ModImmBRel:
          BAsmCode[0] = 0xa8;
          memcpy(BAsmCode + 1, AdrVals, 2);
          BAsmCode[3] = z;
          CodeLen = 4;
          break;
      }
    }
  }
}

static void DecodeCMP(Word IsCMPA)
{
  if (((!IsCMPA) && (ArgCnt != 2)) || ((IsCMPA) && (ArgCnt != 1))) WrError(1110);
  else
  {
    if (IsCMPA)
      AdrType = ModAccA;
    else
      DecodeAdr(ArgStr[2], MModAccA | MModAccB | MModReg);
    switch (AdrType)
    {
      case ModAccA:
        DecodeAdr(ArgStr[1], MModAbs | MModIReg | MModBRel | MModAccB | MModReg | MModImm);
        switch (AdrType)
        {
          case ModAbs:
            BAsmCode[0] = 0x8d;
            memcpy(BAsmCode + 1,AdrVals, 2);
            CodeLen = 3;
            break;
          case ModIReg:
            BAsmCode[0] = 0x9d;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModBRel:
            BAsmCode[0] = 0xad;
            memcpy(BAsmCode + 1, AdrVals, 2);
            CodeLen = 3;
            break;
          case ModAccB:
            BAsmCode[0] = 0x6d;
            CodeLen = 1;
            break;
          case ModReg:
            BAsmCode[0] = 0x1d;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0x2d;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
        break;
      case ModAccB:
        DecodeAdr(ArgStr[1], MModReg | MModImm);
        switch (AdrType)
        {
          case ModReg:
            BAsmCode[0] = 0x3d;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0x5d;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
        break;
      case ModReg:
        BAsmCode[2] = AdrVals[0];
        DecodeAdr(ArgStr[1], MModReg | MModImm);
        switch (AdrType)
        {
          case ModReg:
            BAsmCode[0] = 0x4d;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
            break;
          case ModImm:
            BAsmCode[0] = 0x7d;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 3;
            break;
        }
        break;
    }
  }
}

static void DecodeTRAP(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;

    FirstPassUnknown = False;
    BAsmCode[0] = EvalIntExpression(ArgStr[1], UInt5, &OK);
    if (FirstPassUnknown)
      BAsmCode[0] &= 15;
    if (OK)
    {
      if (BAsmCode[0] > 23) WrError(1320);
      else
      {
        BAsmCode[0] = 0xff - BAsmCode[0];
        CodeLen = 1;
      }
    }
  }
}

static void DecodeTST(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  {
    DecodeAdr(ArgStr[1], MModAccA | MModAccB);
    switch (AdrType)
    {
      case ModAccA:
        BAsmCode[0] = 0xb0;
        CodeLen = 1;
        break;
      case ModAccB:
        BAsmCode[0] = 0xc1;
        CodeLen = 1;
        break;
    }
  }
}

/*---------------------------------------------------------------------------*/
/* dynamic instruction table handling */

static void InitFixed(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeFixed);
}

static void InitRel8(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeRel8);
}

static void InitALU1(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeALU1);
}

static void InitALU2(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeALU2);
}

static void InitJmp(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeJmp);
}

static void InitABReg(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeABReg);
}

static void InitFields(void)
{
  InstTable = CreateInstTable(107);

  AddInstTable(InstTable, "MOV" , 0, DecodeMOV);
  AddInstTable(InstTable, "MOVP", 1, DecodeMOV);
  AddInstTable(InstTable, "LDA" , 0, DecodeLDA);
  AddInstTable(InstTable, "STA" , 0, DecodeSTA);
  AddInstTable(InstTable, "MOVW", 1, DecodeMOVWD);
  AddInstTable(InstTable, "MOVD", 2, DecodeMOVWD);
  AddInstTable(InstTable, "CMP" , 0, DecodeCMP);
  AddInstTable(InstTable, "CMPA", 1, DecodeCMP);
  AddInstTable(InstTable, "TRAP", 0, DecodeTRAP);
  AddInstTable(InstTable, "TST" , 0, DecodeTST);

  InitFixed("CLRC" , 0x00b0); InitFixed("DINT" , 0x0006);
  InitFixed("EINT" , 0x0005); InitFixed("IDLE" , 0x0001);
  InitFixed("LDSP" , 0x000d); InitFixed("NOP"  , 0x0000);
  InitFixed("RETI" , 0x000b); InitFixed("RTI"  , 0x000b);
  InitFixed("RETS" , 0x000a); InitFixed("RTS"  , 0x000a);
  InitFixed("SETC" , 0x0007); InitFixed("STSP" , 0x0009);
  InitFixed("TSTA" , 0x00b0); InitFixed("TSTB" , 0x00c1);

  InitRel8("JMP", 0xe0); InitRel8("JC" , 0xe3); InitRel8("JEQ", 0xe2);
  InitRel8("JHS", 0xe3); InitRel8("JL" , 0xe7); InitRel8("JN" , 0xe1);
  InitRel8("JNC", 0xe7); InitRel8("JNE", 0xe6); InitRel8("JNZ", 0xe6);
  InitRel8("JP" , 0xe4); InitRel8("JPZ", 0xe5); InitRel8("JZ" , 0xe2);

  InitALU1("ADC",  9); InitALU1("ADD",  8);
  InitALU1("DAC", 14); InitALU1("DSB", 15);
  InitALU1("SBB", 11); InitALU1("SUB", 10);
  InitALU1("MPY", 12);

  InitALU2("AND"  , 0x0003); InitALU2("BTJO" , 0x4006);
  InitALU2("BTJZ" , 0x4007); InitALU2("OR"   , 0x0004); InitALU2("XOR" , 0x0005);
  InitALU2("ANDP" , 0x8003); InitALU2("BTJOP", 0xc006);
  InitALU2("BTJZP", 0xc007); InitALU2("ORP"  , 0x8004); InitALU2("XORP", 0x8005);

  InitJmp("BR"  ,12); InitJmp("CALL" ,14);

  InitABReg("CLR"  , 5); InitABReg("DEC"  , 2); InitABReg("DECD" ,11);
  InitABReg("INC"  , 3); InitABReg("INV"  , 4); InitABReg("POP"  , 9);
  InitABReg("PUSH" , 8); InitABReg("RL"   ,14); InitABReg("RLC"  ,15);
  InitABReg("RR"   ,12); InitABReg("RRC"  ,13); InitABReg("SWAP" , 7);
  InitABReg("XCHB" , 6); InitABReg("DJNZ" ,10);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
}

static void MakeCode_TMS7(void)
{
  CodeLen = 0; DontPrint = False; OpSize = 0;

  /* zu ignorierendes */

  if (Memo("")) return;

  /* Pseudoanweisungen */

  if (DecodeIntelPseudo(True)) return;

  /* remainder */

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200,OpPart);
}

static Boolean IsDef_TMS7(void)
{
  return False;
}

static void InternSymbol_TMS7(char *pAsc, TempResult *pErg)
{
  String h;
  Boolean Err;

  pErg->Typ = TempNone;
  if ((strlen(pAsc) < 2) || ((mytoupper(*pAsc) != 'R') && (mytoupper(*pAsc) != 'P')))
    return;

  strmaxcpy(h, pAsc + 1, 255);
  if ((*h == '0') && (strlen(h) > 1))
    *h = '$';
  pErg->Contents.Int = ConstLongInt(h, &Err, 10);
  if ((!Err) || (pErg->Contents.Int < 0) || (pErg->Contents.Int > 255))
    return;

  pErg->Typ = TempInt;
  if (mytoupper(*pAsc) == 'P')
    pErg->Contents.Int += 0x100;
}

static void SwitchFrom_TMS7(void)
{
  DeinitFields();
}

static void SwitchTo_TMS7(void)
{
  TurnWords = False; ConstMode = ConstModeIntel; SetIsOccupied = False;

  PCSymbol = "$"; HeaderID = 0x73; NOPCode = 0x00;
  DivideChars = ","; HasAttrs = False;

  ValidSegs=1 << SegCode;
  Grans[SegCode] = 1; ListGrans[SegCode] = 1; SegInits[SegCode] = 0;
  SegLimits[SegCode] = 0xffff;

  MakeCode = MakeCode_TMS7; IsDef = IsDef_TMS7;
  SwitchFrom = SwitchFrom_TMS7; InternSymbol = InternSymbol_TMS7;

  InitFields();
}

void codetms7_init(void)
{
  CPU70C00  = AddCPU("TMS70C00", SwitchTo_TMS7);
  CPU70C20  = AddCPU("TMS70C20", SwitchTo_TMS7);
  CPU70C40  = AddCPU("TMS70C40", SwitchTo_TMS7);
  CPU70CT20 = AddCPU("TMS70CT20",SwitchTo_TMS7);
  CPU70CT40 = AddCPU("TMS70CT40",SwitchTo_TMS7);
  CPU70C02  = AddCPU("TMS70C02", SwitchTo_TMS7);
  CPU70C42  = AddCPU("TMS70C42", SwitchTo_TMS7);
  CPU70C82  = AddCPU("TMS70C82", SwitchTo_TMS7);
  CPU70C08  = AddCPU("TMS70C08", SwitchTo_TMS7);
  CPU70C48  = AddCPU("TMS70C48", SwitchTo_TMS7);
}
