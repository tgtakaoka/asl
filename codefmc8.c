/* codefmc8.c */ 
/****************************************************************************/
/* AS, C-Version                                                            */
/*                                                                          */
/* Codegenerator fuer Fujitsu-F2MC8L-Prozessoren                            */
/*                                                                          */
/* Historie:   4. 7.1999 Grundsteinlegung                                   */
/*            29. 7.1999 doppelte Variable entfernt                         */
/*             9. 3.2000 'ambiguous else'-Warnungen beseitigt               */
/*           14. 1.2001 silenced warnings about unused parameters           */
/*                                                                          */
/****************************************************************************/
/* $Id: codefmc8.c,v 1.5 2014/06/19 10:07:04 alfred Exp $                   */
/****************************************************************************
 * $Log: codefmc8.c,v $
 * Revision 1.5  2014/06/19 10:07:04  alfred
 * - rework to current style
 *
 * Revision 1.4  2007/11/24 22:48:06  alfred
 * - some NetBSD changes
 *
 * Revision 1.3  2005/09/08 16:53:43  alfred
 * - use common PInstTable
 *
 * Revision 1.2  2004/05/29 11:33:03  alfred
 * - relocated DecodeIntelPseudo() into own module
 *
 ****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "bpemu.h"
#include "strutil.h"
#include "asmdef.h"
#include "asmpars.h"
#include "asmsub.h"
#include "intpseudo.h"
#include "asmitree.h"
#include "codevars.h"
#include "headids.h"

/*--------------------------------------------------------------------------*/
/* Definitionen */

#define ModNone (-1)
enum
{
  ModAcc,
  ModDir,
  ModExt,
  ModIIX,
  ModIEP,
  ModIA,
  ModReg,
  ModReg16,
  ModT,
  ModPC,
  ModPS,
  ModImm
};
#define MModAcc   (1 << ModAcc)
#define MModDir   (1 << ModDir)
#define MModExt   (1 << ModExt)
#define MModIIX   (1 << ModIIX)
#define MModIEP   (1 << ModIEP)
#define MModIA    (1 << ModIA)
#define MModReg   (1 << ModReg)
#define MModReg16 (1 << ModReg16)
#define MModT     (1 << ModT)
#define MModPC    (1 << ModPC)
#define MModPS    (1 << ModPS)
#define MModImm   (1 << ModImm)

static CPUVar CPU89190;

static int AdrMode;
static Byte AdrPart, AdrVals[2];
static int OpSize;

/*--------------------------------------------------------------------------*/
/* Adressdekoder */

static void DecodeAdr(char *Asc, unsigned Mask)
{
  Boolean OK;
  Word Address;

  AdrMode = ModNone;
  AdrCnt = 0;

  /* Register ? */

  if (!strcasecmp(Asc, "A"))
   AdrMode = ModAcc;

  else if (!strcasecmp(Asc, "SP"))
  {
    AdrMode = ModReg16; 
    AdrPart = 1;
  }

  else if (!strcasecmp(Asc, "IX"))
  {
    AdrMode = ModReg16;
    AdrPart = 2;
  }

  else if (!strcasecmp(Asc, "EP"))
  {
    AdrMode = ModReg16;
    AdrPart = 3;
  }

  else if (!strcasecmp(Asc, "T"))
    AdrMode = ModT;

  else if (!strcasecmp(Asc, "PC"))
    AdrMode = ModPC;

  else if (!strcasecmp(Asc, "PS"))
    AdrMode = ModPS;

  else if ((strlen(Asc) == 2) && (mytoupper(*Asc) == 'R') && (Asc[1]>= '0') && (Asc[1] <= '7'))
  {
    AdrMode = ModReg;
    AdrPart = Asc[1] - '0' + 8;
  }

  /* immediate ? */

  else if (*Asc == '#')
  {
    if (OpSize)
    {
      Address = EvalIntExpression(Asc + 1, Int16, &OK);
      if (OK)
      {
        /***Problem: Byte order? */
        AdrVals[0] = Lo(Address);
        AdrVals[1] = Hi(Address);
        AdrMode = ModImm;
        AdrCnt = 2;
        AdrPart = 4;
      }
    }
    else
    {
      AdrVals[0] = EvalIntExpression(Asc + 1, Int8, &OK);
      if (OK)
      {
        AdrMode = ModImm;
        AdrCnt = 1;
        AdrPart = 4;
      }
    }
  }

  /* indirekt ? */

  else if (!strcasecmp(Asc, "@EP"))
  {
    AdrMode = ModIEP;
    AdrPart = 7;
  }

  else if (!strcasecmp(Asc, "@A"))
  {
    AdrMode = ModIA;
    AdrPart = 7;
  }

  else if (!strncasecmp(Asc, "@IX", 3))
  {
    /***Problem: Offset signed oder unsigned? */
    AdrVals[0] = EvalIntExpression(Asc + 3, SInt8, &OK);
    if (OK)
    {
      AdrMode = ModIIX;
      AdrCnt = 1;
      AdrPart = 6;
    }
  }

  /* direkt ? */

  else
  {
    Address = EvalIntExpression(Asc, (Mask & MModExt) ? UInt16 : UInt8, &OK);
    if (OK)
    {
      if ((Mask & MModDir) && (Hi(Address) == 0))
      {
        AdrVals[0] = Lo(Address);
        AdrMode = ModDir;
        AdrCnt = 1;
        AdrPart = 5;
      }
      else
      {
        AdrMode = ModExt;
        AdrCnt = 2;
        /***Problem: Byte order? */
        AdrVals[0] = Lo(Address);
        AdrVals[1] = Hi(Address);
      }
    }
  }

  /* erlaubt ? */

  if ((AdrMode != ModNone) && ((Mask & (1 << AdrMode)) == 0))
  {
    WrError(1350);
    AdrMode = ModNone;
    AdrCnt = 0;
  }
}

static Boolean DecodeBitAdr(char *Asc, Byte *Adr, Byte *Bit)
{
  char *sep;
  Boolean OK;

  sep = strchr(Asc, ':');
  if (!sep)
    return FALSE;
  *sep = '\0';

  *Adr = EvalIntExpression(Asc, UInt8, &OK);
  if (!OK) return FALSE;

  *Bit = EvalIntExpression(sep + 1, UInt3, &OK);
  if (!OK) return FALSE;

  return TRUE;
}

/*--------------------------------------------------------------------------*/
/* ind. Decoder */

static void DecodeFixed(Word Code)
{
  if (ArgCnt != 0) WrError(1110);
  else
  {
    BAsmCode[0] = Code;
    CodeLen = 1;
  }
}

static void DecodeAcc(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAcc);
    if (AdrMode != ModNone)
    {
      BAsmCode[0] = Code;
      CodeLen = 1;
    }
  }
}

static void DecodeALU(Word Code)
{
  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAcc);
    if (AdrMode != ModNone)
    {
      if (ArgCnt == 1)
      {
        BAsmCode[0] = Code + 2;
        CodeLen = 1;
      }
      else
      {
        OpSize = 0;
        DecodeAdr(ArgStr[2], MModDir | MModIIX | MModIEP | MModReg | MModImm);
        if (AdrMode != ModNone)
        {
          BAsmCode[0] = Code + AdrPart;
          memcpy(BAsmCode + 1, AdrVals, AdrCnt);
          CodeLen = 1 + AdrCnt;
        }
      }
    }
  }
}

static void DecodeRel(Word Code)
{
  Integer Adr;
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    Adr = EvalIntExpression(ArgStr[1], UInt16, &OK) - (EProgCounter() + 2);
    if (OK)
    {
      if (((Adr < -128) || (Adr > 127)) && (!SymbolQuestionable)) WrError(1370);
      else
      {
        BAsmCode[0] = Code; 
        BAsmCode[1] = Adr & 0xff;
        CodeLen = 2;
      }
    }
  }
}

static void DecodeMOV(Word Index)
{
  Byte HReg;
  UNUSED(Index);
  
  if (ArgCnt != 2) WrError(1110);
  else
  {
    OpSize = 0;
    DecodeAdr(ArgStr[1], MModAcc | MModDir | MModIIX | MModIEP | MModExt | MModReg | MModIA);
    switch (AdrMode)
    {
      case ModAcc:
        DecodeAdr(ArgStr[2], MModDir | MModIIX | MModIEP | MModIA | MModReg | MModImm| MModExt);
        switch (AdrMode)
        {
          case ModDir:
          case ModIIX:
          case ModIEP:
          case ModReg:
          case ModImm:
            BAsmCode[0] = 0x00 + AdrPart;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            CodeLen = 1 + AdrCnt;
            break;
          case ModExt:
            BAsmCode[0] = 0x60;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            CodeLen = 1 + AdrCnt;
            break;
          case ModIA:
            BAsmCode[0] = 0x92;
            CodeLen = 1 + AdrCnt;
            break;
        }
        break;

      case ModDir:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAcc | MModImm);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0x45;
            CodeLen = 2;
            break;
          case ModImm:
            BAsmCode[0] = 0x85;       /***turn Byte 1+2 for F2MC ? */
            BAsmCode[2] = AdrVals[0];
           CodeLen = 3;
           break;
       }
       break;

      case ModIIX:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAcc | MModImm);
        switch (AdrMode)
        {
         case ModAcc:
           BAsmCode[0] = 0x46;
           CodeLen = 2;
           break;
         case ModImm:
           BAsmCode[0] = 0x86;       /***turn Byte 1+2 for F2MC ? */
           BAsmCode[2] = AdrVals[0];
           CodeLen = 3;
           break;
        }
        break;

      case ModIEP:
        DecodeAdr(ArgStr[2], MModAcc | MModImm);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0x47;
            CodeLen = 1;
            break;
          case ModImm:
            BAsmCode[0] = 0x87;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
       break;

      case ModExt:
        BAsmCode[1] = AdrVals[0];
        BAsmCode[2] = AdrVals[1];
        DecodeAdr(ArgStr[2], MModAcc);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0x61;
            CodeLen = 3;
            break;
        }
        break;

      case ModReg:
        HReg = AdrPart;
        DecodeAdr(ArgStr[2], MModAcc | MModImm);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0x40 + HReg;
            CodeLen = 1;
            break;
          case ModImm:
            BAsmCode[0] = 0x80 + HReg;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
            break;
        }
        break;

      case ModIA:
        DecodeAdr(ArgStr[2], MModT);
        switch (AdrMode)
        {
         case ModT:
           BAsmCode[0] = 0x82;
           CodeLen = 1;
           break;
        }
        break;
    }
  }
}

static void DecodeMOVW(Word Index)
{
  Byte HReg;
  UNUSED(Index);

  OpSize = 1;
  if (ArgCnt != 2) WrError(1110);
  else 
  {
    DecodeAdr(ArgStr[1], MModAcc | MModDir | MModIIX | MModExt | MModIEP | MModReg16 | MModIA | MModPS);
    switch(AdrMode)
    {
      case ModAcc:
        DecodeAdr(ArgStr[2], MModImm | MModDir | MModIEP | MModIIX | MModExt | MModIA | MModReg16 | MModPS | MModPC);
        switch (AdrMode)
        {
          case ModImm:
            BAsmCode[0] = 0xe4;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            CodeLen = 1 + AdrCnt;
            break;
          case ModExt:
            BAsmCode[0] = 0xc4;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            CodeLen = 1 + AdrCnt;
            break;
          case ModDir:
          case ModIEP:
          case ModIIX:
            BAsmCode[0] = 0xc0 | AdrPart;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            CodeLen = 1 + AdrCnt;
            break;
          case ModIA:
            BAsmCode[0] = 0x93;
            CodeLen = 1;
            break;
          case ModReg16:
            BAsmCode[0] = 0xf0 + AdrPart;
            CodeLen = 1;
            break;
          case ModPS:
            BAsmCode[0] = 0x70;
            CodeLen = 1;
            break;
          case ModPC:
            BAsmCode[0] = 0xf0;
            CodeLen = 1;
            break;
        }
        break;
       
      case ModDir:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAcc);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0xd5;
            CodeLen = 2;
            break;
        }
        break;

      case ModIIX:
        BAsmCode[1] = AdrVals[0];
        DecodeAdr(ArgStr[2], MModAcc);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0xd6;
            CodeLen = 2;
            break;
        }
        break;

      case ModExt:
        BAsmCode[1] = AdrVals[0];
        BAsmCode[2] = AdrVals[1]; 
        DecodeAdr(ArgStr[2], MModAcc);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0xd4;
            CodeLen = 3;
            break;
        }
        break;

      case ModIEP:
        DecodeAdr(ArgStr[2], MModAcc);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0xd7;
            CodeLen = 1;
            break;
        }
        break;

      case ModReg16:
        HReg = AdrPart;
        DecodeAdr(ArgStr[2], MModAcc | MModImm);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0xe0 + HReg;
            CodeLen = 1;
            break;
          case ModImm:
            BAsmCode[0] = 0xe4 + HReg;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            CodeLen = 1 + AdrCnt;
            break; 
        }
        break;

      case ModIA:
        DecodeAdr(ArgStr[2], MModT);
        switch (AdrMode)
        {
          case ModT:
            BAsmCode[0] = 0x83;
            CodeLen = 1;
            break;
        }
        break;

      case ModPS:
        DecodeAdr(ArgStr[2], MModAcc);
        switch (AdrMode)
        {
          case ModAcc:
            BAsmCode[0] = 0x71;
            CodeLen = 1;
            break;
        }
        break;
    }
  }
}

static void DecodeBit(Word Index)
{
  Byte Adr, Bit;

  if (ArgCnt != 1) WrError(1110);
  else if (!DecodeBitAdr(ArgStr[1], &Adr, &Bit)) WrError(1350);
  else
  {
    BAsmCode[0] = 0xa0 + (Index << 3) + Bit;
    BAsmCode[1] = Adr;
    CodeLen = 2;
  }
}

static void DecodeXCH(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAcc | MModT);
    switch (AdrMode)
    {
      case ModAcc:
       DecodeAdr(ArgStr[2], MModT);
       if (AdrMode != ModNone)
       {
         BAsmCode[0] = 0x42;
         CodeLen = 1;
       }
       break;
      case ModT:
       DecodeAdr(ArgStr[2], MModAcc);
       if (AdrMode != ModNone)
       {
         BAsmCode[0] = 0x42;
         CodeLen = 1;
       }
       break;
    }
  }
}

static void DecodeXCHW(Word Index)
{
  Byte HReg;
  UNUSED(Index);

  if (ArgCnt != 2) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAcc | MModT | MModReg16 | MModPC);
    switch (AdrMode)
    {
      case ModAcc:
        DecodeAdr(ArgStr[2], MModT | MModReg16 | MModPC);
        switch (AdrMode)
        {
          case ModT:
            BAsmCode[0] = 0x43;
            CodeLen = 1;
            break;
          case ModReg16:
            BAsmCode[0] = 0xf4 + AdrPart;
            CodeLen = 1;
            break;
          case ModPC:
            BAsmCode[0] = 0xf4;
            CodeLen = 1;
            break;
        }
        break;
      case ModT:
        DecodeAdr(ArgStr[2], MModAcc);
        if (AdrMode != ModNone)
        {
          BAsmCode[0] = 0x43;
          CodeLen = 1;
        }
        break;
      case ModReg16:
        HReg = AdrPart;
        DecodeAdr(ArgStr[2], MModAcc);
        if (AdrMode != ModNone)
        {
          BAsmCode[0] = 0xf4 | HReg;
          CodeLen = 1;
        }
        break;
      case ModPC:
        DecodeAdr(ArgStr[2], MModAcc);
        if (AdrMode != ModNone)
        {
          BAsmCode[0] = 0xf4;
          CodeLen = 1;
        }
    }
  }
}

static void DecodeINCDEC(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModReg);
    if (AdrMode != ModNone)
    {
      BAsmCode[0] = 0xc0 + (Index << 4) + AdrPart;
      CodeLen = 1;
    }
  }
}

static void DecodeINCDECW(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAcc | MModReg16);
    switch (AdrMode)
    {
      case ModAcc:
        BAsmCode[0] = 0xc0 + (Index << 4);
        CodeLen = 1;
        break;
      case ModReg16:
        BAsmCode[0] = 0xc0 + (Index << 4) + AdrPart;
        CodeLen = 1;
        break;
    }
  }
}

static void DecodeCMP(Word Index)
{
  Byte HReg;
  UNUSED(Index);

  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModAcc | MModDir | MModIIX | MModIEP | MModReg);
    switch (AdrMode)
    {
      case ModAcc:
        if (ArgCnt == 1)
        {
          BAsmCode[0] = 0x12;
          CodeLen = 1;
        }
        else
        {
          DecodeAdr(ArgStr[2], MModDir | MModIEP | MModIIX | MModReg | MModImm);
          if (AdrMode != ModNone)
          {
            BAsmCode[0] = 0x10 + AdrPart;
            memcpy(BAsmCode + 1, AdrVals, AdrCnt);
            CodeLen = 1 + AdrCnt;
          }
        }
        break;
      case ModDir:
        if (ArgCnt != 2) WrError(1110);
        else
        {
          BAsmCode[1] = AdrVals[0];
          DecodeAdr(ArgStr[2], MModImm);
          if (AdrMode != ModNone)
          {
            BAsmCode[0] = 0x95;
            BAsmCode[2] = AdrVals[0]; /* reverse for F2MC8 */
            CodeLen = 3;
          }
        }
        break;
      case ModIIX:
        if (ArgCnt != 2) WrError(1110);
        else
        {
          BAsmCode[1] = AdrVals[0];
          DecodeAdr(ArgStr[2], MModImm);
          if (AdrMode != ModNone)
          {
            BAsmCode[0] = 0x96;
            BAsmCode[2] = AdrVals[0]; /* reverse for F2MC8 */
            CodeLen = 3;
          }
        }
        break;
      case ModIEP:
        if (ArgCnt != 2) WrError(1110);
        else
        {
          DecodeAdr(ArgStr[2], MModImm);
          if (AdrMode != ModNone)
          {
            BAsmCode[0] = 0x97;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
          }
        }
        break;
      case ModReg:
        if (ArgCnt != 2) WrError(1110);
        else
        {
          HReg = AdrPart;
          DecodeAdr(ArgStr[2], MModImm);
          if (AdrMode != ModNone)
          {
            BAsmCode[0] = 0x90 + HReg;
            BAsmCode[1] = AdrVals[0];
            CodeLen = 2;
          }
        }
        break;
    }
  }
}

static void DecodeBitBr(Word Index)
{
  Byte Bit, BAdr;
  Boolean OK;
  Integer Adr;

  if (ArgCnt != 2) WrError(1110);
  else if (!DecodeBitAdr(ArgStr[1], &BAdr, &Bit)) WrError(1350);
  else
  {
    Adr = EvalIntExpression(ArgStr[2], UInt16, &OK) - (EProgCounter() + 3);
    if (OK)
    {
      if (((Adr < -128) || (Adr > 127)) && (!SymbolQuestionable)) WrError(1370);
      else
      {
        BAsmCode[0] = 0xb0 + (Index << 3) + Bit;
        BAsmCode[1] = BAdr; /* reverse for F2MC8? */
        BAsmCode[2] = Adr & 0xff;
        CodeLen = 3;
      }
    }
  }
}

static void DecodeJmp(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    DecodeAdr(ArgStr[1], MModExt | ((Index) ? 0 : MModIA));
    switch (AdrMode)
    {
      case ModIA:
        BAsmCode[0] = 0xe0;
        CodeLen = 1;
        break;
      case ModExt:
        BAsmCode[0] = 0x21 | (Index << 4);
        memcpy(BAsmCode + 1, AdrVals, AdrCnt);
        CodeLen = 1 + AdrCnt;
        break;
    }
  }
}

static void DecodeCALLV(Word Index)
{
  Boolean OK;
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else if (*ArgStr[1] != '#') WrError(1120);
  else
  {
    BAsmCode[0] = 0xb8 + EvalIntExpression(ArgStr[1] + 1, UInt3, &OK);
    if (OK)
      CodeLen = 1;
  }
}

static void DecodeStack(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else  
  {
    DecodeAdr(ArgStr[1], MModAcc | MModReg16);
    switch (AdrMode)
    {
      case ModAcc:
        BAsmCode[0] = 0x40 + (Index << 4);
        CodeLen = 1;
        break;
      case ModReg16:
        if (AdrPart != 2) WrXError(1445, ArgStr[1]);
        else
        {
          BAsmCode[0] = 0x41 + (Index << 4);
          CodeLen = 1;
        }
        break;
    }
  }
}

/*--------------------------------------------------------------------------*/
/* Codetabellen */

static void AddFixed(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeFixed);
}

static void AddALU(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeALU);
}

static void AddAcc(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeAcc);
}

static void AddRel(char *NName, Byte NCode)
{  
  AddInstTable(InstTable, NName, NCode, DecodeRel);
}

static void InitFields(void)
{
  InstTable = CreateInstTable(201);

  AddFixed("SWAP"  , 0x10); AddFixed("DAA"   , 0x84);
  AddFixed("DAS"   , 0x94); AddFixed("RET"   , 0x20);
  AddFixed("RETI"  , 0x30); AddFixed("NOP"   , 0x00);
  AddFixed("CLRC"  , 0x81); AddFixed("SETC"  , 0x91);
  AddFixed("CLRI"  , 0x80); AddFixed("SETI"  , 0x90);

  AddALU("ADDC"  , 0x20); AddALU("SUBC"  , 0x30);
  AddALU("XOR"   , 0x50); AddALU("AND"   , 0x60);
  AddALU("OR"    , 0x70);

  AddAcc("ADDCW" , 0x23); AddAcc("SUBCW" , 0x33);
  AddAcc("MULU"  , 0x01); AddAcc("DIVU"  , 0x11);
  AddAcc("ANDW"  , 0x63); AddAcc("ORW"   , 0x73);
  AddAcc("XORW"  , 0x53); AddAcc("CMPW"  , 0x13);
  AddAcc("RORC"  , 0x03); AddAcc("ROLC"  , 0x02);

  AddRel("BEQ", 0xfd); AddRel("BZ" , 0xfd);
  AddRel("BNZ", 0xfc); AddRel("BNE", 0xfc);
  AddRel("BC" , 0xf9); AddRel("BLO", 0xf9);
  AddRel("BNC", 0xf8); AddRel("BHS", 0xf8);
  AddRel("BN" , 0xfb); AddRel("BP" , 0xfa);
  AddRel("BLT", 0xff); AddRel("BGE", 0xfe);

  AddInstTable(InstTable, "MOV"  , 0, DecodeMOV);
  AddInstTable(InstTable, "MOVW" , 0, DecodeMOVW);
  AddInstTable(InstTable, "XCH"  , 0, DecodeXCH);
  AddInstTable(InstTable, "XCHW" , 0, DecodeXCHW);
  AddInstTable(InstTable, "SETB" , 1, DecodeBit);
  AddInstTable(InstTable, "CLRB" , 0, DecodeBit);
  AddInstTable(InstTable, "INC"  , 0, DecodeINCDEC);
  AddInstTable(InstTable, "DEC"  , 1, DecodeINCDEC);
  AddInstTable(InstTable, "INCW" , 0, DecodeINCDECW);
  AddInstTable(InstTable, "DECW" , 1, DecodeINCDECW);
  AddInstTable(InstTable, "CMP"  , 0, DecodeCMP);
  AddInstTable(InstTable, "BBC"  , 0, DecodeBitBr);
  AddInstTable(InstTable, "BBS"  , 1, DecodeBitBr);
  AddInstTable(InstTable, "JMP"  , 0, DecodeJmp);
  AddInstTable(InstTable, "CALL" , 1, DecodeJmp);
  AddInstTable(InstTable, "CALLV", 0, DecodeCALLV);
  AddInstTable(InstTable, "PUSHW", 0, DecodeStack);
  AddInstTable(InstTable, "POPW" , 1, DecodeStack);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
}

/*--------------------------------------------------------------------------*/
/* Interface zu AS */

static void MakeCode_F2MC8(void)
{
  /* Leeranweisung ignorieren */

  if (Memo("")) return;

  /* Pseudoanweisungen */

  if (DecodeIntelPseudo(False)) return;

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200, OpPart);
}

static Boolean IsDef_F2MC8(void)
{
  return FALSE;
}

static void SwitchFrom_F2MC8(void)
{
  DeinitFields();
}

static void SwitchTo_F2MC8(void)
{
  PFamilyDescr FoundDescr;

  FoundDescr = FindFamilyByName("F2MC8");

  TurnWords = False;
  ConstMode = ConstModeIntel;
  SetIsOccupied = False;

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

  MakeCode = MakeCode_F2MC8;
  IsDef = IsDef_F2MC8;
  SwitchFrom = SwitchFrom_F2MC8;
  InitFields();
}

/*--------------------------------------------------------------------------*/
/* Initialisierung */

void codef2mc8_init(void)
{
  CPU89190 = AddCPU("MB89190", SwitchTo_F2MC8);
}
