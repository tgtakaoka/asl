/* codemic8.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator LatticeMico8                                                */
/*                                                                           */
/*****************************************************************************/
/* $Id: codemic8.c,v 1.11 2010/12/11 22:52:39 alfred Exp $                   */
/*****************************************************************************
 * $Log: codemic8.c,v $
 * Revision 1.11  2010/12/11 22:52:39  alfred
 * - split up Mico 8 versions
 *
 * Revision 1.10  2010/01/01 14:31:49  alfred
 * - added some coding bugfixes
 *
 * Revision 1.9  2007/11/24 22:48:07  alfred
 * - some NetBSD changes
 *
 * Revision 1.8  2007/06/28 20:27:31  alfred
 * - silence some warnings on recent GNU C versions
 *
 * Revision 1.7  2005/10/02 10:00:46  alfred
 * - ConstLongInt gets default base, correct length check on KCPSM3 registers
 *
 * Revision 1.6  2005/09/30 12:53:49  alfred
 * - correct include statements
 *
 * Revision 1.5  2005/09/08 16:53:43  alfred
 * - use common PInstTable
 *
 * Revision 1.4  2005/08/07 10:29:44  alfred
 * - allow instruction aliases for (I) variants
 *
 * Revision 1.3  2005/08/06 14:19:28  alfred
 * - assure long unsigned constants on 16-bit-platforms
 *
 * Revision 1.2  2005/08/06 13:35:11  alfred
 * - added INC/DEC
 *
 * Revision 1.1  2005/07/30 13:57:02  alfred
 * - add LatticeMico8
 *
 *****************************************************************************/

#include "stdinc.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "nls.h"
#include "strutil.h"
#include "bpemu.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmitree.h"
#include "intpseudo.h"
#include "codevars.h"
#include "headids.h"

#include "codepseudo.h"
#include "codemic8.h"

#define ALUOrderCnt 14
#define FixedOrderCnt 9
#define ShortBranchOrderCnt 8
#define LongBranchOrderCnt 10
#define MemOrderCnt 6
#define RegOrderCnt 2

/* define as needed by address space */

#define CodeAddrInt UInt12
#define DataAddrInt UInt5

typedef struct
{
  LongWord Code;
} FixedOrder;

typedef struct
{
  LongWord Code;
  Boolean MayImm;
} ALUOrder;

typedef struct
{
  LongWord Code;
  Byte Space;
} MemOrder;

static FixedOrder *FixedOrders, *ShortBranchOrders, *RegOrders, *LongBranchOrders;
static MemOrder *MemOrders;
static ALUOrder *ALUOrders;

static CPUVar CPUMico8_05, CPUMico8_V3, CPUMico8_V31;

/*--------------------------------------------------------------------------
 * Address Expression Parsing
 *--------------------------------------------------------------------------*/

static Boolean IsWReg(char *Asc, LongWord *pErg)
{
  Boolean OK;
  char *s;

  if (FindRegDef(Asc, &s)) Asc = s;

  if ((strlen(Asc) < 2) || (mytoupper(*Asc) != 'R'))
  {
    *pErg = 0;
    return False;
  }

  *pErg = ConstLongInt(Asc + 1, &OK, 10);
  if (!OK)
    return False;

  return (*pErg < 32);
}

/*--------------------------------------------------------------------------
 * Code Handlers
 *--------------------------------------------------------------------------*/

static void DecodePort(Word Index)
{
  UNUSED(Index);

  CodeEquate(SegIO, 0, SegLimits[SegIO]);
}

static void DecodeRegDef(Word Index)
{
  UNUSED(Index);

  if (ArgCnt != 1) WrError(1110);
  else AddRegDef(LabPart, ArgStr[1]);
}

static void DecodeFixed(Word Index)
{
  FixedOrder *pOrder = FixedOrders + Index;

  if (ArgCnt != 0) WrError(1110);
  else
  {
    DAsmCode[0] = pOrder->Code;
    CodeLen = 1;
  }
}

static void DecodeALU(Word Index)
{
  ALUOrder *pOrder = ALUOrders + Index;
  LongWord Src, DReg;

  if (ArgCnt != 2) WrError(1110);
  else if (!IsWReg(ArgStr[1], &DReg)) WrXError(1445, ArgStr[1]);
  else if (IsWReg(ArgStr[2], &Src))
  {
    DAsmCode[0] = pOrder->Code | (DReg << 8) | (Src << 3);
    CodeLen = 1;
  }
  else if (!pOrder->MayImm) WrXError(1445, ArgStr[2]);
  else
  {
    Boolean OK;

    Src = EvalIntExpression(ArgStr[2], Int8, &OK);
    if (OK)
    {
      DAsmCode[0] = pOrder->Code | (1 << 13) | (DReg << 8) | (Src & 0xff);
      CodeLen = 1;
    }
  }
}

static void DecodeALUI(Word Index)
{
  ALUOrder *pOrder = ALUOrders + Index;
  LongWord Src, DReg;
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else if (!IsWReg(ArgStr[1], &DReg)) WrXError(1445, ArgStr[1]);
  else
  {
    Src = EvalIntExpression(ArgStr[2], Int8, &OK);
    if (OK)
    {
      DAsmCode[0] = pOrder->Code | (1 << 13) | (DReg << 8) | (Src & 0xff);
      CodeLen = 1;
    }
  }
}

static void DecodeShortBranch(Word Index)
{
  FixedOrder *pOrder = ShortBranchOrders + Index;
  LongInt Dest;
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else
  {   
    Dest = EvalIntExpression(ArgStr[1], CodeAddrInt, &OK);
    if (OK)
    {
      Dest -= EProgCounter();
      if (((Dest < -512) || (Dest > 511)) && (!SymbolQuestionable)) WrError(1370);
      else
      {
        DAsmCode[0] = pOrder->Code | (Dest & 0x3ff);
        CodeLen = 1;
      }
    }
  }
}

static void DecodeLongBranch(Word Index)
{
  FixedOrder *pOrder = LongBranchOrders + Index;
  LongInt Dest;
  Boolean OK;

  if (ArgCnt != 1) WrError(1110);
  else
  {
    Dest = EvalIntExpression(ArgStr[1], CodeAddrInt, &OK);
    if (OK)
    {
      Dest -= EProgCounter();
      if (((Dest < -2048) || (Dest > 2047)) && (!SymbolQuestionable)) WrError(1370);
      else
      {
        DAsmCode[0] = pOrder->Code | (Dest & 0xfff);
        CodeLen = 1;
      }
    }
  }
}

static void DecodeMem(Word Index)
{
  MemOrder *pOrder = MemOrders + Index;
  LongWord DReg, Src;
  Boolean OK;

  if (ArgCnt != 2) WrError(1110);
  else if (!IsWReg(ArgStr[1], &DReg)) WrXError(1445, ArgStr[1]);
  else if (IsWReg(ArgStr[2], &Src))
  {
    DAsmCode[0] = pOrder->Code | (DReg << 8) | ((Src & 0x1f) << 3) | 2;
    CodeLen = 1;
  }
  else
  {
    Src = EvalIntExpression(ArgStr[2], DataAddrInt, &OK);
    if (OK)
    {
      ChkSpace(pOrder->Space);
      DAsmCode[0] = pOrder->Code | (DReg << 8) | ((Src & 0x1f) << 3);
      CodeLen = 1;
    }
  }
}

static void DecodeMemI(Word Index)
{
  MemOrder *pOrder = MemOrders + Index;
  LongWord DReg, SReg;

  if (ArgCnt != 2) WrError(1110);
  else if (!IsWReg(ArgStr[1], &DReg)) WrXError(1445, ArgStr[1]);
  else if (!IsWReg(ArgStr[2], &SReg)) WrXError(1445, ArgStr[2]);
  else
  {
    DAsmCode[0] = pOrder->Code | (DReg << 8) | (SReg << 3) | 2;
    CodeLen = 1;
  }
}

static void DecodeReg(Word Index)
{
  FixedOrder *pOrder = RegOrders + Index;
  LongWord Reg = 0;

  if (ArgCnt != 1) WrError(1110);
  else if (!IsWReg(ArgStr[1], &Reg)) WrXError(1445, ArgStr[1]);
  {
    DAsmCode[0] = pOrder->Code | (Reg << 8);
    CodeLen = 1;
  }
}

/*--------------------------------------------------------------------------
 * Instruction Table Handling
 *--------------------------------------------------------------------------*/

static void AddFixed(char *NName, LongWord NCode)
{
  if (InstrZ >= FixedOrderCnt)
    exit(255);

  FixedOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ++, DecodeFixed);
}

static void AddALU(char *NName, char *NImmName, LongWord NCode)
{
  if (InstrZ >= ALUOrderCnt)
    exit(255);

  ALUOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ, DecodeALU);
  if ((ALUOrders[InstrZ].MayImm = (NImmName != NULL)))
    AddInstTable(InstTable, NImmName, InstrZ, DecodeALUI);
  InstrZ++;
}

static void AddShortBranch(char *NName, LongWord NCode)
{
  if (InstrZ >= ShortBranchOrderCnt)
    exit(255);

  ShortBranchOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ++, DecodeShortBranch);
}

static void AddLongBranch(char *NName, LongWord NCode)
{
  if (InstrZ >= LongBranchOrderCnt)
    exit(255);

  LongBranchOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ++, DecodeLongBranch);
}

static void AddMem(char *NName, char *NImmName, LongWord NCode, Byte NSpace)
{
  if (InstrZ >= MemOrderCnt)
    exit(255);

  MemOrders[InstrZ].Code = NCode;
  MemOrders[InstrZ].Space = NSpace;
  AddInstTable(InstTable, NName, InstrZ, DecodeMem);
  AddInstTable(InstTable, NImmName, InstrZ, DecodeMemI);
  InstrZ++;
}

static void AddReg(char *NName, LongWord NCode)
{
  if (InstrZ >= RegOrderCnt)
    exit(255);

  RegOrders[InstrZ].Code = NCode;
  AddInstTable(InstTable, NName, InstrZ++, DecodeReg);
}

static void InitFields(void)
{
  InstTable = CreateInstTable(97);

  InstrZ = 0;
  FixedOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * FixedOrderCnt);
  AddFixed("CLRC"  , 0x2c000);
  AddFixed("SETC"  , 0x2c001);
  AddFixed("CLRZ"  , 0x2c002);
  AddFixed("SETZ"  , 0x2c003);
  AddFixed("CLRI"  , 0x2c004);
  AddFixed("SETI"  , 0x2c005);
  if (MomCPU == CPUMico8_05)
  {
    AddFixed("RET"   , 0x3a000);
    AddFixed("IRET"  , 0x3a001);
  }
  else if (MomCPU == CPUMico8_V3)
  {
    AddFixed("RET"   , 0x38000);
    AddFixed("IRET"  , 0x39000);
  }
  else if (MomCPU == CPUMico8_V31)
  {
    AddFixed("RET"   , 0x39000);
    AddFixed("IRET"  , 0x3a000);
  }
  AddFixed("NOP"   , 0x10000);

  InstrZ = 0;
  ALUOrders = (ALUOrder*) malloc(sizeof(ALUOrder) * ALUOrderCnt);
  AddALU("ADD"    , "ADDI"  ,   2UL << 14);
  AddALU("ADDC"   , "ADDIC" ,   3UL << 14);
  AddALU("SUB"    , "SUBI"  ,   0UL << 14);
  AddALU("SUBC"   , "SUBIC" ,   1UL << 14);
  AddALU("MOV"    , "MOVI"  ,   4UL << 14);
  AddALU("AND"    , "ANDI"  ,   5UL << 14);
  AddALU("OR"     , "ORI"   ,   6UL << 14);
  AddALU("XOR"    , "XORI"  ,   7UL << 14);
  AddALU("CMP"    , "CMPI"  ,   8UL << 14);
  AddALU("TEST"   , "TESTI" ,   9UL << 14);
  AddALU("ROR"    , NULL    , (10UL << 14) | 0); /* Note: The User guide (Feb '08) differs  */
  AddALU("ROL"    , NULL    , (10UL << 14) | 1); /* from the actual implementation in */
  AddALU("RORC"   , NULL    , (10UL << 14) | 2); /* decoding the last 3 bits of the Rotate */
  AddALU("ROLC"   , NULL    , (10UL << 14) | 3); /* instructions. These values are correct. */

  InstrZ = 0;
  RegOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * RegOrderCnt);
  AddReg("INC"    , (2UL << 14)  | (1UL << 13) | 1);
  AddReg("DEC"    , (0UL << 14)  | (1UL << 13) | 1);

  InstrZ = 0;
  ShortBranchOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * ShortBranchOrderCnt);
  if (MomCPU != CPUMico8_V31)
  {
    AddShortBranch("BZ"    , 0x32000);
    AddShortBranch("BNZ"   , 0x32400);
    AddShortBranch("BC"    , 0x32800);
    AddShortBranch("BNC"   , 0x32c00);
    AddShortBranch("CALLZ" , 0x36000);
    AddShortBranch("CALLNZ", 0x36400);
    AddShortBranch("CALLC" , 0x36800);
    AddShortBranch("CALLNC", 0x36c00);
  }

  /* AcQ/MA: a group for unconditional branches, which can support
   *         larger branches then the conditional branches (not supported
   *         in the earliest versions of the Mico8 processor). The branch
   *         range is +2047 to -2048 instead of +511 to -512. */
  InstrZ = 0;
  LongBranchOrders = (FixedOrder*) malloc(sizeof(FixedOrder) * LongBranchOrderCnt);
  if (MomCPU != CPUMico8_05)
  {
    if (MomCPU == CPUMico8_V31)
    {
      AddLongBranch("BZ"    , 0x30000);
      AddLongBranch("BNZ"   , 0x31000);
      AddLongBranch("BC"    , 0x32000);
      AddLongBranch("BNC"   , 0x33000);
      AddLongBranch("CALLZ" , 0x34000);
      AddLongBranch("CALLNZ", 0x35000);
      AddLongBranch("CALLC" , 0x36000);
      AddLongBranch("CALLNC", 0x37000);
      AddLongBranch("CALL"  , 0x38000);
      AddLongBranch("B"     , 0x3b000);
    }
    else
    {
      AddLongBranch("B"     , 0x33000);
      AddLongBranch("CALL"  , 0x37000);
    }
  }

  InstrZ = 0;
  MemOrders = (MemOrder*) malloc(sizeof(MemOrder) * MemOrderCnt);
  if (MomCPU == CPUMico8_V31)
  {
    AddMem("INP"    , "INPI"   , (23UL << 13) | 1, SegIO);
    AddMem("IMPORT" , "IMPORTI", (23UL << 13) | 1, SegIO);
    AddMem("OUTP"   , "OUTPI"  , (23UL << 13) | 0, SegIO);
    AddMem("EXPORT" , "EXPORTI", (23UL << 13) | 0, SegIO);
    AddMem("LSP"    , "LSPI"   , (23UL << 13) | 5, SegData);
    AddMem("SSP"    , "SSPI"   , (23UL << 13) | 4, SegData);
  }
  else
  {
    if (MomCPU == CPUMico8_V3)
    {
      AddMem("INP"    , "INPI"   , (15UL << 14) | 1, SegIO);
      AddMem("OUTP"   , "OUTPI"  , (15UL << 14) | 0, SegIO);
    }
    AddMem("IMPORT" , "IMPORTI", (15UL << 14) | 1, SegIO);
    AddMem("EXPORT" , "EXPORTI", (15UL << 14) | 0, SegIO);
    AddMem("LSP"    , "LSPI"   , (15UL << 14) | 5, SegData);
    AddMem("SSP"    , "SSPI"   , (15UL << 14) | 4, SegData);
  }

  AddInstTable(InstTable, "REG", 0, DecodeRegDef);
  AddInstTable(InstTable, "PORT", 0, DecodePort);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
  free(FixedOrders);
  free(ALUOrders);
  free(LongBranchOrders);
  free(ShortBranchOrders);
  free(MemOrders);
  free(RegOrders);
}

/*--------------------------------------------------------------------------
 * Semipublic Functions
 *--------------------------------------------------------------------------*/

static Boolean IsDef_Mico8(void)
{
   return (Memo("REG")) || (Memo("PORT"));
}

static void SwitchFrom_Mico8(void)
{
   DeinitFields();
}

static void MakeCode_Mico8(void)
{
  CodeLen = 0; DontPrint = False;

  /* zu ignorierendes */

   if (Memo("")) return;

   /* Pseudoanweisungen */

   if (DecodeIntelPseudo(True)) return;

   if (NOT LookupInstTable(InstTable, OpPart))
     WrXError(1200, OpPart);
}

static void SwitchTo_Mico8(void)
{
   PFamilyDescr FoundDescr;

   FoundDescr = FindFamilyByName("Mico8");

   TurnWords = True; ConstMode = ConstModeC; SetIsOccupied = False;

   PCSymbol = "$"; HeaderID = FoundDescr->Id;

   /* NOP = mov R0,R0 */

   NOPCode = 0x10000;
   DivideChars = ","; HasAttrs = False;

   ValidSegs = (1 << SegCode) | (1 << SegData) | (1 << SegXData) | (1 << SegIO);
   Grans[SegCode] = 4; ListGrans[SegCode] = 4; SegInits[SegCode] = 0;
   SegLimits[SegCode] = IntMaxs[CodeAddrInt];
   Grans[SegData] = 1; ListGrans[SegData] = 1; SegInits[SegData] = 0;
   SegLimits[SegData] = IntMaxs[DataAddrInt];
   Grans[SegXData] = 1; ListGrans[SegXData] = 1; SegInits[SegXData] = 0;
   SegLimits[SegXData] = 0xff;
   Grans[SegIO] = 1; ListGrans[SegIO] = 1; SegInits[SegIO] = 0;
   SegLimits[SegIO] = 0xff;

   MakeCode = MakeCode_Mico8; IsDef = IsDef_Mico8;
   SwitchFrom = SwitchFrom_Mico8; InitFields();
}

/*--------------------------------------------------------------------------
 * Initialization
 *--------------------------------------------------------------------------*/

void codemico8_init(void)
{
   CPUMico8_05  = AddCPU("Mico8_05" , SwitchTo_Mico8);
   CPUMico8_V3  = AddCPU("Mico8_V3" , SwitchTo_Mico8);
   CPUMico8_V31 = AddCPU("Mico8_V31", SwitchTo_Mico8);
}
