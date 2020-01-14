/* codescmp.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Codegenerator National SC/MP                                              */
/*                                                                           */
/* Historie: 17. 2.1996 Grundsteinlegung                                     */
/*            2. 1.1998 ChkPC umgestellt                                     */
/*            9. 3.2000 'ambiguous else'-Warnungen beseitigt                 */
/*                                                                           */
/*****************************************************************************/
/* $Id: codescmp.c,v 1.6 2010/04/17 13:14:23 alfred Exp $                    */
/*****************************************************************************
 * $Log: codescmp.c,v $
 * Revision 1.6  2010/04/17 13:14:23  alfred
 * - address overlapping strcpy()
 *
 * Revision 1.5  2009/04/13 07:36:50  alfred
 * - clean up SC/MP target, correct PC-relative addressing
 *
 * Revision 1.4  2007/11/24 22:48:07  alfred
 * - some NetBSD changes
 *
 * Revision 1.3  2005/09/08 17:31:05  alfred
 * - add missing include
 *
 * Revision 1.2  2004/05/29 11:33:03  alfred
 * - relocated DecodeIntelPseudo() into own module
 *
 *****************************************************************************/

#include "stdinc.h"
#include <ctype.h>
#include <string.h>

#include "nls.h"
#include "strutil.h"
#include "chunks.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmitree.h"  
#include "intpseudo.h"
#include "codevars.h"

/*---------------------------------------------------------------------------*/

static CPUVar CPUSCMP;

/*---------------------------------------------------------------------------*/

static Boolean DecodeReg(const char *pAsc, Byte *pErg)
{
  if ((strlen(pAsc) != 2) || (mytoupper(*pAsc) != 'P'))
    return False;

  switch (mytoupper(pAsc[1]))
  {
    case 'C':
      *pErg = 0; break;
    case '0':
    case '1':
    case '2':
    case '3':
      *pErg = pAsc[1] - '0'; break;
    default:
      return False;
  }

  return True;
}

static Boolean DecodeAdr(char *Asc, Boolean MayInc, Byte PCDisp, Byte *Arg)
{
  Word Target;
  Boolean OK;
  int l = strlen(Asc);

  if ((l >= 4) && (Asc[l - 1] == ')') && (Asc[l-4] == '('))
  {
    Asc[l - 1] = '\0'; 
    if (DecodeReg(Asc + l - 3, Arg))
    {
      Asc[l - 4] = '\0';
      if (*Asc == '@')
      {
        if (!MayInc)
        {
          WrError(1350);
          return False;
        }
        strmov(Asc, Asc + 1); *Arg += 4;
      }
      if (!strcasecmp(Asc, "E"))
        BAsmCode[1] = 0x80;
      else if (*Arg == 0)
      {
        WrXError(1445, Asc + l - 3);
        return False;
      }
      else
      {
        BAsmCode[1] = EvalIntExpression(Asc, SInt8, &OK);
        if (!OK)
          return False;
      }
      return True;
    }
    else Asc[l - 1] = ')';
  }

  /* no carry in PC from bit 11 to 12; additionally handle preincrement */

  Target = EvalIntExpression(Asc, UInt16, &OK);
  if (OK)
  {
    Word PCVal = (EProgCounter() & 0xf000) + ((EProgCounter() + 1 + PCDisp) & 0xfff);
    Word Disp = (Target - PCVal) & 0xfff;

    if (SymbolQuestionable)
      Target = PCVal;

    if ((Target & 0xf000) != (PCVal & 0xf000)) WrError(1910);
    else if ((Disp > 0x7f) && (Disp <= 0xf80)) WrError(1370);
    else
    {
      BAsmCode[1] = Disp & 0xff;
      *Arg = 0;
      return True;
    }
  }
  return False;
}

static void ChkPage(void)
{
  if (((EProgCounter()) & 0xf000) != ((EProgCounter() + CodeLen) & 0xf000))
    WrError(250);
}

/*---------------------------------------------------------------------------*/

static void DecodeFixed(Word Index)
{
  if (ArgCnt != 0) WrError(1110);
  else
  {
    BAsmCode[0] = Index; CodeLen = 1;
  }
}

static void DecodeImm(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else
  {
    Boolean OK;

    BAsmCode[1] = EvalIntExpression(ArgStr[1], Int8, &OK);
    if (OK)
    {
      BAsmCode[0] = Index; CodeLen = 2; ChkPage();
    }
  }
}

static void DecodeRegOrder(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else if (!DecodeReg(ArgStr[1], BAsmCode+0)) WrXError(1445, ArgStr[1]);
  else
  {
    BAsmCode[0] |= Index; CodeLen = 1;
  }
}

static void DecodeMem(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else if (DecodeAdr(ArgStr[1], True, 1, BAsmCode + 0))
  {
    BAsmCode[0] |= Index; CodeLen = 2; ChkPage();
  }
}

static void DecodeJmp(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else if (DecodeAdr(ArgStr[1], False, 1, BAsmCode + 0))
  {
    BAsmCode[0] |= Index; CodeLen = 2; ChkPage();
  }
}

static void DecodeLD(Word Index)
{
  if (ArgCnt != 1) WrError(1110);
  else if (DecodeAdr(ArgStr[1], False, 0, BAsmCode + 0))
  {
    BAsmCode[0] |= Index; CodeLen = 2; ChkPage();
  }
}

/*---------------------------------------------------------------------------*/

static void AddFixed(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeFixed);
}

static void AddImm(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeImm);
}

static void AddReg(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeRegOrder);
}

static void AddMem(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeMem);
}

static void AddJmp(char *NName, Byte NCode)
{
  AddInstTable(InstTable, NName, NCode, DecodeJmp);
}

static void InitFields(void)
{
  InstTable = CreateInstTable(201);

  AddFixed("LDE" ,0x40); AddFixed("XAE" ,0x01); AddFixed("ANE" ,0x50);
  AddFixed("ORE" ,0x58); AddFixed("XRE" ,0x60); AddFixed("DAE" ,0x68);
  AddFixed("ADE" ,0x70); AddFixed("CAE" ,0x78); AddFixed("SIO" ,0x19);
  AddFixed("SR"  ,0x1c); AddFixed("SRL" ,0x1d); AddFixed("RR"  ,0x1e);
  AddFixed("RRL" ,0x1f); AddFixed("HALT",0x00); AddFixed("CCL" ,0x02);
  AddFixed("SCL" ,0x03); AddFixed("DINT",0x04); AddFixed("IEN" ,0x05);
  AddFixed("CSA" ,0x06); AddFixed("CAS" ,0x07); AddFixed("NOP" ,0x08);

  AddImm("LDI" , 0xc4); AddImm("ANI" , 0xd4); AddImm("ORI" , 0xdc);
  AddImm("XRI" , 0xe4); AddImm("DAI" , 0xec); AddImm("ADI" , 0xf4);
  AddImm("CAI" , 0xfc); AddImm("DLY" , 0x8f);

  AddReg("XPAL", 0x30); AddReg("XPAH", 0x34); AddReg("XPPC", 0x3c);

  AddMem("LD"  , 0xc0); AddMem("ST"  , 0xc8); AddMem("AND" , 0xd0);
  AddMem("OR"  , 0xd8); AddMem("XOR" , 0xe0); AddMem("DAD" , 0xe8);
  AddMem("ADD" , 0xf0); AddMem("CAD" , 0xf8);

  AddJmp("JMP" , 0x90); AddJmp("JP"  , 0x94); AddJmp("JZ"  , 0x98);
  AddJmp("JNZ" , 0x9c);

  AddInstTable(InstTable, "ILD", 0xa8, DecodeLD);
  AddInstTable(InstTable, "DLD", 0xb8, DecodeLD);
}

static void DeinitFields(void)
{
  DestroyInstTable(InstTable);
}

/*---------------------------------------------------------------------------*/

static void MakeCode_SCMP(void)
{
  CodeLen = 0; DontPrint = False;

  /* zu ignorierendes */

  if (Memo("")) return;

  /* Pseudoanweisungen */

  if (DecodeIntelPseudo(False)) return;

  if (!LookupInstTable(InstTable, OpPart))
    WrXError(1200,OpPart);
}

static Boolean IsDef_SCMP(void)
{
  return False;
}

static void SwitchFrom_SCMP(void)
{
  DeinitFields();
}

static void SwitchTo_SCMP(void)
{
  TurnWords = False; ConstMode = ConstModeC; SetIsOccupied = False;

  PCSymbol = "$"; HeaderID = 0x6e; NOPCode = 0x08;
  DivideChars = ","; HasAttrs = False;

  ValidSegs = 1 << SegCode;
  Grans[SegCode] = 1; ListGrans[SegCode] = 1; SegInits[SegCode] = 0;
  SegLimits[SegCode] = 0xffff;

  MakeCode = MakeCode_SCMP; IsDef = IsDef_SCMP;
  SwitchFrom = SwitchFrom_SCMP; InitFields();
}

void codescmp_init(void)
{
  CPUSCMP = AddCPU("SC/MP", SwitchTo_SCMP);
}


