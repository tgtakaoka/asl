/* code17c4x.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator PIC17C4x                                                    */
/*                                                                           */
/* Historie: 21.8.1996 Grundsteinlegung                                      */
/*            7. 7.1998 Fix Zugriffe auf CharTransTable wg. signed chars     */
/*           18. 8.1998 Bookkeeping-Aufruf in RES                            */
/*            3. 1.1999 ChkPC-Anpassung                                      */
/*            9. 3.2000 'ambiguous else'-Warnungen beseitigt                 */
/*                                                                           */
/*****************************************************************************/
/* $Id: code17c4x.c,v 1.6 2016/09/29 16:43:36 alfred Exp $                          */
/*****************************************************************************
 * $Log: code17c4x.c,v $
 * Revision 1.6  2016/09/29 16:43:36  alfred
 * - introduce common DecodeDATA/DecodeRES functions
 *
 * Revision 1.5  2014/10/03 13:18:15  alfred
 * - rework to current style
 *
 * Revision 1.4  2013/12/21 19:46:51  alfred
 * - dynamically resize code buffer
 *
 * Revision 1.3  2008/11/23 10:39:16  alfred
 * - allow strings with NUL characters
 *
 * Revision 1.2  2005/09/08 17:31:03  alfred
 * - add missing include
 *
 * Revision 1.1  2003/11/06 02:49:19  alfred
 * - recreated
 *
 * Revision 1.3  2003/05/02 21:23:09  alfred
 * - strlen() updates
 *
 * Revision 1.2  2002/08/14 18:43:48  alfred
 * - warn null allocation, remove some warnings
 *
 *****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "bpemu.h"
#include "strutil.h"
#include "chunks.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmitree.h"
#include "asmpars.h"
#include "codepseudo.h"
#include "fourpseudo.h"
#include "codevars.h"

/*---------------------------------------------------------------------------*/

static CPUVar CPU17C42;

/*---------------------------------------------------------------------------*/

static void DecodeFixed(Word Code)
{
  if (ArgCnt != 0) WrError(1110);
  else
  {
    CodeLen = 1;
    WAsmCode[0] = Code;
  }
}

static void DecodeLitt(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;
    Word AdrWord = EvalIntExpression(ArgStr[1], Int8, &OK);
    if (OK)
    {
      WAsmCode[0] = Code + (AdrWord & 0xff);
      CodeLen = 1;
    }
  }
}

static void DecodeAri(Word Code)
{
  Word DefaultDir = (Code >> 7) & 0x100;

  Code &= 0x7fff;
  if ((ArgCnt == 0) || (ArgCnt > 2)) WrError(1110);
  else
  {
    Boolean OK;
    Word AdrWord = EvalIntExpression(ArgStr[1], Int8, &OK);
    if (OK)
    {
      ChkSpace(SegData);
      WAsmCode[0] = Code + (AdrWord & 0xff);
      if (ArgCnt == 1)
      {
        CodeLen = 1;
        WAsmCode[0] += DefaultDir;
      }
      else if (strcasecmp(ArgStr[2], "W") == 0)
        CodeLen = 1;
      else if (strcasecmp(ArgStr[2], "F") == 0)
      {
        CodeLen = 1;
        WAsmCode[0] += 0x100;
      }
      else
      {
        AdrWord = EvalIntExpression(ArgStr[2], UInt1, &OK);
        if (OK)
        {
          CodeLen = 1;
          WAsmCode[0] += (AdrWord << 8);
        }
      }
    }
  }
}

static void DecodeBit(Word Code)
{
  if (ArgCnt != 2) WrError(1110);
  else
  {
    Boolean OK;
    Word AdrWord = EvalIntExpression(ArgStr[2], UInt3, &OK);
    if (OK)
    {
      WAsmCode[0] = EvalIntExpression(ArgStr[1], Int8, &OK);
       if (OK)
       {
         CodeLen = 1;
         WAsmCode[0] += Code + (AdrWord << 8);
         ChkSpace(SegData);
       }
    }
  }
}

static void DecodeF(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;
    Word AdrWord = EvalIntExpression(ArgStr[1], Int8, &OK);
    if (OK)
    {
      CodeLen = 1;
      WAsmCode[0] = Code + AdrWord;
      ChkSpace(SegData);
    }
  }
}

static void DecodeMOVFP_MOVPF(Word Code)
{
  if (ArgCnt != 2) WrError(1110);
  else
  {
    char *pArg1 = (Code & 0x2000) ? ArgStr[2] : ArgStr[1],
         *pArg2 = (Code & 0x2000) ? ArgStr[1] : ArgStr[2];
    Boolean OK;
    Word AdrWord;

    AdrWord = EvalIntExpression(pArg1, UInt5, &OK);
    if (OK)
    {
      WAsmCode[0] = EvalIntExpression(pArg2, Int8, &OK);
      if (OK)
      {
        WAsmCode[0] = Code + Lo(WAsmCode[0]) + (AdrWord << 8);
        CodeLen = 1;
      }
    }
  }
}

static void DecodeTABLRD_TABLWT(Word Code)
{
  if (ArgCnt != 3) WrError(1110);
  else
  {
    Boolean OK;

    WAsmCode[0] = Lo(EvalIntExpression(ArgStr[3], Int8, &OK));
    if (OK)
    {
      Word AdrWord = EvalIntExpression(ArgStr[2], UInt1, &OK);
      if (OK)
      {
        WAsmCode[0] += AdrWord << 8;
        AdrWord = EvalIntExpression(ArgStr[1], UInt1, &OK);
        if (OK)
        {
          WAsmCode[0] += Code + (AdrWord << 9);
          CodeLen = 1;
        }
      }
    }
  }
}

static void DecodeTLRD_TLWT(Word Code)
{
  if (ArgCnt != 2) WrError(1110);
  else
  {
    Boolean OK;

    WAsmCode[0] = Lo(EvalIntExpression(ArgStr[2], Int8, &OK));
    if (OK)
    {
      Word AdrWord = EvalIntExpression(ArgStr[1], UInt1, &OK);
      if (OK)
      {
        WAsmCode[0] += (AdrWord << 9) + Code;
        CodeLen = 1;
      }
    }
  }
  return;
}

static void DecodeCALL_GOTO(Word Code)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;

    Word AdrWord = EvalIntExpression(ArgStr[1], UInt16, &OK);
    if (OK)
    {
      if (((ProgCounter() ^ AdrWord) & 0xe000) != 0) WrError(1910);
      else
      {
        WAsmCode[0] = Code + (AdrWord & 0x1fff);
        CodeLen = 1;
      }
    }
  }
}

static void DecodeLCALL(Word Code)
{
  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;

    Word AdrWord = EvalIntExpression(ArgStr[1], UInt16, &OK);
    if (OK)
    {
      CodeLen = 3;
      WAsmCode[0] = 0xb000 + Hi(AdrWord);
      WAsmCode[1] = 0x0103;
      WAsmCode[2] = 0xb700 + Lo(AdrWord);
    }
  }
}

static void DecodeSFR(Word Code)
{
  UNUSED(Code);

  CodeEquate(SegData, 0, 0xff);
}

static void DecodeDATA_17C4x(Word Code)
{
  UNUSED(Code);

  DecodeDATA(Int16, Int8);
}

static void DecodeZERO(Word Code)
{
  Word Size;
  Boolean ValOK;

  UNUSED(Code);

  if (ArgCnt != 1) WrError(1110);
  else
  {
    FirstPassUnknown = False;
    Size = EvalIntExpression(ArgStr[1], Int16, &ValOK);
    if (FirstPassUnknown) WrError(1820);
    if ((ValOK) && (!FirstPassUnknown))
    {
      if (SetMaxCodeLen(Size << 1)) WrError(1920);
      else
      {
        CodeLen = Size;
        memset(WAsmCode, 0, 2 * Size);
      }
    }
  }
}

/*---------------------------------------------------------------------------*/

static void AddFixed(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeFixed);
}

static void AddLitt(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeLitt);
}

static void AddAri(char *NName, Word NDef, Word NCode)
{
  AddInstTable(InstTable, NName, NCode | (NDef << 15), DecodeAri);
}

static void AddBit(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeBit);
}

static void AddF(char *NName, Word NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeF);
}

static void InitFields(void)
{
  InstTable = CreateInstTable(203);
  AddInstTable(InstTable, "MOVFP", 0x6000, DecodeMOVFP_MOVPF);
  AddInstTable(InstTable, "MOVPF", 0x4000, DecodeMOVFP_MOVPF);
  AddInstTable(InstTable, "TABLRD", 0xa800, DecodeTABLRD_TABLWT);
  AddInstTable(InstTable, "TABLWT", 0xac00, DecodeTABLRD_TABLWT);
  AddInstTable(InstTable, "TLRD", 0xa000, DecodeTLRD_TLWT);
  AddInstTable(InstTable, "TLWT", 0xa400, DecodeTLRD_TLWT);
  AddInstTable(InstTable, "CALL", 0xe000, DecodeCALL_GOTO);
  AddInstTable(InstTable, "GOTO", 0xc000, DecodeCALL_GOTO);
  AddInstTable(InstTable, "LCALL", 0, DecodeLCALL);
  AddInstTable(InstTable, "SFR", 0, DecodeSFR);
  AddInstTable(InstTable, "RES", 0, DecodeRES);
  AddInstTable(InstTable, "DATA", 0, DecodeDATA_17C4x);
  AddInstTable(InstTable, "ZERO", 0, DecodeZERO);

  AddFixed("RETFIE", 0x0005);
  AddFixed("RETURN", 0x0002);
  AddFixed("CLRWDT", 0x0004);
  AddFixed("NOP"   , 0x0000);
  AddFixed("SLEEP" , 0x0003);

  AddLitt("MOVLB", 0xb800);
  AddLitt("ADDLW", 0xb100);
  AddLitt("ANDLW", 0xb500);
  AddLitt("IORLW", 0xb300);
  AddLitt("MOVLW", 0xb000);
  AddLitt("SUBLW", 0xb200);
  AddLitt("XORLW", 0xb400);
  AddLitt("RETLW", 0xb600);

  AddAri("ADDWF" , 0, 0x0e00);
  AddAri("ADDWFC", 0, 0x1000);
  AddAri("ANDWF" , 0, 0x0a00);
  AddAri("CLRF"  , 1, 0x2800);
  AddAri("COMF"  , 1, 0x1200);
  AddAri("DAW"   , 1, 0x2e00);
  AddAri("DECF"  , 1, 0x0600);
  AddAri("INCF"  , 1, 0x1400);
  AddAri("IORWF" , 0, 0x0800);
  AddAri("NEGW"  , 1, 0x2c00);
  AddAri("RLCF"  , 1, 0x1a00);
  AddAri("RLNCF" , 1, 0x2200);
  AddAri("RRCF"  , 1, 0x1800);
  AddAri("RRNCF" , 1, 0x2000);
  AddAri("SETF"  , 1, 0x2a00);
  AddAri("SUBWF" , 0, 0x0400);
  AddAri("SUBWFB", 0, 0x0200);
  AddAri("SWAPF" , 1, 0x1c00);
  AddAri("XORWF" , 0, 0x0c00);
  AddAri("DECFSZ", 1, 0x1600);
  AddAri("DCFSNZ", 1, 0x2600);
  AddAri("INCFSZ", 1, 0x1e00);
  AddAri("INFSNZ", 1, 0x2400);

  AddBit("BCF"  , 0x8800);
  AddBit("BSF"  , 0x8000);
  AddBit("BTFSC", 0x9800);
  AddBit("BTFSS", 0x9000);
  AddBit("BTG"  , 0x3800);

  AddF("MOVWF" , 0x0100);
  AddF("CPFSEQ", 0x3100);
  AddF("CPFSGT", 0x3200);
  AddF("CPFSLT", 0x3000);
  AddF("TSTFSZ", 0x3300);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
}

/*---------------------------------------------------------------------------*/

static void MakeCode_17c4x(void)
{
  CodeLen = 0;
  DontPrint = False;

  /* zu ignorierendes */

  if (Memo(""))
    return;

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200, OpPart);
}

static Boolean IsDef_17c4x(void)
{
  return Memo("SFR");
}

static void SwitchFrom_17c4x(void)
{
  DeinitFields();
}

static void SwitchTo_17c4x(void)
{
  TurnWords = False;
  ConstMode = ConstModeMoto;
  SetIsOccupied = False;

  PCSymbol = "*";
  HeaderID = 0x72;
  NOPCode = 0x0000;
  DivideChars = ",";
  HasAttrs = False;

  ValidSegs = (1 << SegCode) + (1 << SegData);
  Grans[SegCode] = 2; ListGrans[SegCode] = 2; SegInits[SegCode] = 0;
  SegLimits[SegCode] = 0xffff;
  Grans[SegData] = 1; ListGrans[SegData] = 1; SegInits[SegData] = 0;
  SegLimits[SegData] = 0xff;

  MakeCode = MakeCode_17c4x;
  IsDef = IsDef_17c4x;
  SwitchFrom = SwitchFrom_17c4x;
  InitFields();
}

void code17c4x_init(void)
{
  CPU17C42 = AddCPU("17C42", SwitchTo_17c4x);
}
