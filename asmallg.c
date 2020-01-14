/* codeallg.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* von allen Codegeneratoren benutzte Pseudobefehle                          */
/*                                                                           */
/* Historie:  10. 5.1996 Grundsteinlegung                                    */
/*            24. 6.1998 CODEPAGE-Kommando                                   */
/*            17. 7.1998 CHARSET ohne Argumente                              */
/*            17. 8.1998 BookKeeping-Aufruf bei ALIGN                        */
/*            18. 8.1998 RADIX-Kommando                                      */
/*             2. 1.1999 Standard-ChkPC-Routine                              */
/*             9. 1.1999 BINCLUDE gefixt...                                  */
/*            30. 5.1999 OUTRADIX-Kommando                                   */
/*            12. 7.1999 EXTERN-Kommando                                     */
/*             8. 3.2000 'ambigious else'-Warnungen beseitigt                */
/*             1. 6.2000 reset error flag before checks in BINCLUDE          */
/*                       added NESTMAX instruction                           */
/*            26. 6.2000 added EXPORT instruction                            */
/*             6. 7.2000 unknown symbol in EXPORT triggers repassing         */
/*            30.10.2000 EXTERN also works for symbols without a segment spec*/
/*             1.11.2000 added ASEG/RSEG                                     */
/*            14. 1.2001 silenced warnings about unused parameters           */
/*                       use SegInits also when segment hasn't been used up  */
/*                       to now                                              */
/*                                                                           */
/*****************************************************************************/
/* $Id: asmallg.c,v 1.9 2010/08/27 14:52:41 alfred Exp $                     */
/*****************************************************************************
 * $Log: asmallg.c,v $
 * Revision 1.9  2010/08/27 14:52:41  alfred
 * - some more overlapping strcpy() cleanups
 *
 * Revision 1.8  2010/04/17 13:14:19  alfred
 * - address overlapping strcpy()
 *
 * Revision 1.7  2008/11/23 10:39:15  alfred
 * - allow strings with NUL characters
 *
 * Revision 1.6  2007/11/24 22:48:02  alfred
 * - some NetBSD changes
 *
 * Revision 1.5  2007/04/30 18:37:51  alfred
 * - add weird integer coding
 *
 * Revision 1.4  2006/08/05 18:26:32  alfred
 * - remove static string
 *
 * Revision 1.3  2005/10/02 10:00:43  alfred
 * - ConstLongInt gets default base, correct length check on KCPSM3 registers
 *
 * Revision 1.2  2005/08/07 10:29:24  alfred
 * - remove mnemonic conflict with MICO8
 *
 * Revision 1.1  2003/11/06 02:49:18  alfred
 * - recreated
 *
 * Revision 1.12  2003/05/02 21:23:08  alfred
 * - strlen() updates
 *
 * Revision 1.11  2003/02/06 07:38:09  alfred
 * - add missing 'else'
 *
 * Revision 1.10  2003/02/02 12:05:01  alfred
 * - limit BINCLUDE transfer size to 256 bytes
 *
 * Revision 1.9  2002/11/23 15:53:27  alfred
 * - SegLimits are unsigned now
 *
 * Revision 1.8  2002/11/20 20:25:04  alfred
 * - added unions
 *
 * Revision 1.7  2002/11/17 16:09:12  alfred
 * - added DottedStructs
 *
 * Revision 1.6  2002/11/16 20:51:15  alfred
 * - adapted to structure changes
 *
 * Revision 1.5  2002/11/11 21:13:37  alfred
 * - add structure storage
 *
 * Revision 1.4  2002/11/11 19:24:57  alfred
 * - new module for structs
 *
 * Revision 1.3  2002/11/04 19:18:00  alfred
 * - add DOTS option for structs
 *
 * Revision 1.2  2002/07/14 18:39:57  alfred
 * - fixed TempAll-related warnings
 *
 *****************************************************************************/

#include "stdinc.h"
#include <string.h>
#include <ctype.h>

#include "nls.h"
#include "strutil.h"
#include "stringlists.h"
#include "bpemu.h"
#include "cmdarg.h"
#include "chunks.h"
#include "asmdef.h"
#include "asmsub.h"
#include "asmpars.h"
#include "asmmac.h"
#include "asmstructs.h"
#include "asmcode.h"
#include "asmrelocs.h"
#include "asmitree.h"
#include "codepseudo.h"

/*--------------------------------------------------------------------------*/

static char *PseudoStrs[3]={Nil,Nil,Nil};

static PInstTable PseudoTable=Nil,ONOFFTable;

/*--------------------------------------------------------------------------*/

	static Boolean DefChkPC(LargeWord Addr)
BEGIN
   if (((1 << ActPC) & ValidSegs) == 0) return False;
   else return (Addr <= SegLimits[ActPC]);
END

void SetCPU(CPUVar NewCPU, Boolean NotPrev)
{
  LongInt HCPU;
  char *z, *dest;
  Boolean ECPU;
  char s[11];
  PCPUDef Lauf;

  Lauf = FirstCPUDef;
  while ((Lauf) && (Lauf->Number != NewCPU))
    Lauf = Lauf->Next;
  if (!Lauf)
    return;

  strmaxcpy(MomCPUIdent, Lauf->Name, 11);
  MomCPU = Lauf->Orig;
  MomVirtCPU = Lauf->Number;
  strmaxcpy(s, MomCPUIdent, 11);
  for (z = dest = s; *z; z++)
    if (isxdigit(*z))
      *(dest++) = (*z);
  *dest = '\0';
  for (z = s; *z != '\0'; z++)
    if (isdigit(*z))
      break;
  if (*z) strmov(s, z);
  strprep(s, "$");
  HCPU = ConstLongInt(s, &ECPU, 10);
  if (ParamCount != 0)
  {
    EnterIntSymbol(MomCPUName, HCPU, SegNone, True);
    EnterStringSymbol(MomCPUIdentName, MomCPUIdent, True);
  }

  InternSymbol = Default_InternSymbol;
  ChkPC = DefChkPC;
  if (!NotPrev) SwitchFrom();
  Lauf->SwitchProc();

  DontPrint = True;
}

	Boolean SetNCPU(char *Name, Boolean NoPrev)
BEGIN
   PCPUDef Lauf;

   Lauf = FirstCPUDef;
   while ((Lauf != Nil) AND (strcmp(Name, Lauf->Name) != 0))
    Lauf = Lauf->Next;
   if (Lauf == Nil) WrXError(1430, Name);
   else SetCPU(Lauf->Number, NoPrev);
   return (Lauf != Nil);
END

	static void SetNSeg(Byte NSeg)
BEGIN
   if ((ActPC != NSeg) OR (NOT PCsUsed[ActPC]))
    BEGIN
     ActPC = NSeg;
     if (NOT PCsUsed[ActPC]) PCs[ActPC] = SegInits[ActPC];
     PCsUsed[ActPC] = True;
     DontPrint = True;
    END
END 

static void IntLine(char *pDest, LongInt Inp)
{
  switch (ConstMode)
  {
    case ConstModeIntel:
      sprintf(pDest, "%sH", HexString(Inp, 0));
      if (*pDest > '9') strmaxprep(pDest, "0", 255);
      break;
    case ConstModeMoto:
      sprintf(pDest, "$%s", HexString(Inp, 0));
      break;
    case ConstModeC:
      sprintf(pDest, "0x%s", HexString(Inp, 0));
      break;
    case ConstModeWeird:
      sprintf(pDest, "x'%s'", HexString(Inp, 0));
      break;
  }
}


	static void CodeSECTION(Word Index)
BEGIN
   PSaveSection Neu;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else if (ExpandSymbol(ArgStr[1]))
    BEGIN
     if (NOT ChkSymbName(ArgStr[1])) WrXError(1020,ArgStr[1]);
     else if ((PassNo==1) AND (GetSectionHandle(ArgStr[1],False,MomSectionHandle)!=-2)) WrError(1483);
     else
      BEGIN
       Neu=(PSaveSection) malloc(sizeof(TSaveSection));
       Neu->Next=SectionStack;
       Neu->Handle=MomSectionHandle;
       Neu->LocSyms=Nil; Neu->GlobSyms=Nil; Neu->ExportSyms=Nil;
       SetMomSection(GetSectionHandle(ArgStr[1],True,MomSectionHandle));
       SectionStack=Neu;
      END
    END
END


	static void CodeENDSECTION_ChkEmptList(PForwardSymbol *Root)
BEGIN
   PForwardSymbol Tmp;

   while (*Root!=Nil)
    BEGIN
     WrXError(1488,(*Root)->Name);
     free((*Root)->Name);
     Tmp=(*Root); *Root=Tmp->Next; free(Tmp);
    END
END

	static void CodeENDSECTION(Word Index)
BEGIN
   PSaveSection Tmp;
   UNUSED(Index);

   if (ArgCnt>1) WrError(1110);
   else if (SectionStack==Nil) WrError(1487);
   else if ((ArgCnt==0) OR (ExpandSymbol(ArgStr[1])))
    BEGIN
     if ((ArgCnt==1) AND (GetSectionHandle(ArgStr[1],False,SectionStack->Handle)!=MomSectionHandle)) WrError(1486);
     else
      BEGIN
       Tmp=SectionStack; SectionStack=Tmp->Next;
       CodeENDSECTION_ChkEmptList(&(Tmp->LocSyms));
       CodeENDSECTION_ChkEmptList(&(Tmp->GlobSyms));
       CodeENDSECTION_ChkEmptList(&(Tmp->ExportSyms));
       TossRegDefs(MomSectionHandle);
       if (ArgCnt==0)
        sprintf(ListLine,"[%s]",GetSectionName(MomSectionHandle));
       SetMomSection(Tmp->Handle);
       free(Tmp);
      END
    END
END


	static void CodeCPU(Word Index)
BEGIN
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else if (*AttrPart!='\0') WrError(1100);
   else
    BEGIN
     NLS_UpString(ArgStr[1]);
     if (SetNCPU(ArgStr[1], False))
       SetNSeg(SegCode);
    END
END


static void CodeSETEQU(Word Index)
{
  TempResult t;
  Boolean MayChange;
  Integer DestSeg;
  UNUSED(Index);

  FirstPassUnknown = False;
  MayChange = ((!Memo("EQU")) && (!Memo("=")));
  if (*AttrPart != '\0') WrError(1100);
  else if ((ArgCnt < 1) || (ArgCnt > 2)) WrError(1110);
  else
  {
    EvalExpression(ArgStr[1],&t);
    if (!FirstPassUnknown)
    {
      if (ArgCnt == 1) DestSeg = SegNone;
      else
      {
        NLS_UpString(ArgStr[2]);
        if (!strcmp(ArgStr[2], "MOMSEGMENT")) DestSeg = ActPC;
        else if (*ArgStr[2] == '\0') DestSeg = SegNone;
        else
        {
          for (DestSeg = 0; DestSeg <= PCMax; DestSeg++)
            if (!strcmp(ArgStr[2], SegNames[DestSeg]))
              break;
        }
      }
      if (DestSeg > PCMax) WrXError(1961,ArgStr[2]);
      else
      {
        SetListLineVal(&t);
        PushLocHandle(-1);
        switch (t.Typ)
        {
          case TempInt:
            EnterIntSymbol(LabPart, t.Contents.Int, DestSeg, MayChange);
            break;
          case TempFloat:
            EnterFloatSymbol(LabPart, t.Contents.Float, MayChange);
            break;
          case TempString:
            EnterDynStringSymbol(LabPart, &t.Contents.Ascii, MayChange);
            break;
          default:
            break;
        }
        PopLocHandle();
      }
    }
  }
}


	static void CodeORG(Word Index)
BEGIN
   LargeInt HVal;
   Boolean ValOK;
   UNUSED(Index);

   FirstPassUnknown=False;
   if (*AttrPart!='\0') WrError(1100);
   else if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
#ifndef HAS64
     HVal=EvalIntExpression(ArgStr[1],UInt32,&ValOK);
#else
     HVal=EvalIntExpression(ArgStr[1],Int64,&ValOK);
#endif
     if (FirstPassUnknown) WrError(1820);
     if ((ValOK) AND (NOT FirstPassUnknown))
      BEGIN
       PCs[ActPC]=HVal; DontPrint=True;
      END
    END
END


   	static void CodeSHARED_BuildComment(char *c)
BEGIN
   switch (ShareMode)
    BEGIN
     case 1: sprintf(c,"(* %s *)",CommPart); break;
     case 2: sprintf(c,"/* %s */",CommPart); break;
     case 3: sprintf(c,"; %s",CommPart); break;
   END
END

	static void CodeSHARED(Word Index)
BEGIN
   int z;
   Boolean ValOK;
   LargeInt HVal;
   Double FVal;
   String s,c;
   UNUSED(Index);

   if (ShareMode==0) WrError(30);
   else if ((ArgCnt==0) AND (*CommPart!='\0'))
    BEGIN
     CodeSHARED_BuildComment(c); 
     errno=0; fprintf(ShareFile,"%s\n",c); ChkIO(10004);
    END
   else
    for (z=1; z<=ArgCnt; z++)
     BEGIN
      if (IsSymbolString(ArgStr[z]))
       BEGIN
	ValOK=GetStringSymbol(ArgStr[z],s);
	if (ShareMode==1) 
         BEGIN
          strmaxprep(s,"\'",255); strmaxcat(s,"\'",255);
         END
	else
         BEGIN
          strmaxprep(s,"\"",255); strmaxcat(s,"\"",255);
         END
       END
      else if (IsSymbolFloat(ArgStr[z]))
       BEGIN
	ValOK=GetFloatSymbol(ArgStr[z],&FVal);
	sprintf(s,"%0.17g",FVal);
       END
      else
       BEGIN
	ValOK=GetIntSymbol(ArgStr[z], &HVal, NULL);
	switch (ShareMode)
         BEGIN
	  case 1: sprintf(s,"$%s",HexString(HVal,0)); break;
          case 2: sprintf(s,"0x%s",HexString(HVal,0)); break;
	  case 3: IntLine(s, HVal); break;
	 END
       END
      if (ValOK)
       BEGIN
        if ((z==1) AND (*CommPart!='\0'))
         BEGIN
          CodeSHARED_BuildComment(c); strmaxprep(c," ",255);
         END
        else *c='\0';
	errno=0;
	switch (ShareMode)
         BEGIN
          case 1: 
           fprintf(ShareFile,"%s = %s;%s\n",ArgStr[z],s,c); break;
          case 2: 
           fprintf(ShareFile,"#define %s %s%s\n",ArgStr[z],s,c); break;
          case 3:
           strmaxprep(s,IsSymbolChangeable(ArgStr[z])?"set ":"equ ",255);
           fprintf(ShareFile,"%s %s%s\n",ArgStr[z],s,c); break;
	 END
	ChkIO(10004);
       END
      else if (PassNo==1)
       BEGIN
        Repass=True;
	if ((MsgIfRepass) AND (PassNo>=PassNoForMessage)) WrXError(170,ArgStr[z]);
       END
     END
END

	static void CodeEXPORT(Word Index)
BEGIN
   int z;
   LargeInt Value;
   PRelocEntry Relocs;
   UNUSED(Index);

   for (z = 1; z <= ArgCnt; z++)
    BEGIN
     FirstPassUnknown = True;
     if (GetIntSymbol(ArgStr[z], &Value, &Relocs))
      BEGIN
       if (Relocs == NULL)
        AddExport(ArgStr[z], Value, 0);
       else if ((Relocs->Next != NULL) OR (strcmp(Relocs->Ref, RelName_SegStart) != 0))
        WrXError(1156, ArgStr[z]);
       else
        AddExport(ArgStr[z], Value, RelFlag_Relative);
      END
     else
      BEGIN
       Repass = True;
       if ((MsgIfRepass) AND (PassNo >= PassNoForMessage))
        WrXError(170,ArgStr[z]);
      END
    END
END

	static void CodePAGE(Word Index)
BEGIN
   Integer LVal,WVal;
   Boolean ValOK;
   UNUSED(Index);

   if ((ArgCnt!=1) AND (ArgCnt!=2)) WrError(1110);
   else if (*AttrPart!='\0') WrError(1100);
   else
    BEGIN
     LVal=EvalIntExpression(ArgStr[1],UInt8,&ValOK);
     if (ValOK)
      BEGIN
       if ((LVal<5) AND (LVal!=0)) LVal=5;
       if (ArgCnt==1)
        BEGIN
         WVal=0; ValOK=True;
        END
       else WVal=EvalIntExpression(ArgStr[2],UInt8,&ValOK);
       if (ValOK)
        BEGIN
         if ((WVal<5) AND (WVal!=0)) WVal=5;
         PageLength=LVal; PageWidth=WVal;
        END
      END
    END
END


	static void CodeNEWPAGE(Word Index)
BEGIN
   ShortInt HVal8;
   Boolean ValOK;
   UNUSED(Index);

   if (ArgCnt>1) WrError(1110);
   else if (*AttrPart!='\0') WrError(1100);
   else
    BEGIN
     if (ArgCnt==0)
      BEGIN
       HVal8=0; ValOK=True;
      END
     else HVal8=EvalIntExpression(ArgStr[1],Int8,&ValOK);
     if ((ValOK) OR (ArgCnt==0))
      BEGIN
       if (HVal8>ChapMax) HVal8=ChapMax;
       else if (HVal8<0) HVal8=0;
       NewPage(HVal8,True);
      END
    END
END


	static void CodeString(Word Index)
BEGIN
   String tmp;
   Boolean OK;

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     EvalStringExpression(ArgStr[1],&OK,tmp);
     if (NOT OK) WrError(1970); else strmaxcpy(PseudoStrs[Index],tmp,255);
    END
END


	static void CodePHASE(Word Index)
BEGIN
   Boolean OK;
   LongInt HVal;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else if (ActPC==StructSeg) WrError(1553);
   else
    BEGIN
     HVal=EvalIntExpression(ArgStr[1],Int32,&OK);
     if (OK) Phases[ActPC]=HVal-ProgCounter();
    END
END


	static void CodeDEPHASE(Word Index)
BEGIN
   UNUSED(Index);

   if (ArgCnt!=0) WrError(1110);
   else if (ActPC==StructSeg) WrError(1553);
   else Phases[ActPC]=0;
END


	static void CodeWARNING(Word Index)
BEGIN
   String mess;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     EvalStringExpression(ArgStr[1],&OK,mess);
     if (NOT OK) WrError(1970);
     else WrErrorString(mess,"",True,False);
    END
END


	static void CodeMESSAGE(Word Index)
BEGIN
   String mess;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     EvalStringExpression(ArgStr[1],&OK,mess);
     if (NOT OK) WrError(1970);
     printf("%s%s\n",mess,ClrEol);
     if (strcmp(LstName,"/dev/null")!=0) WrLstLine(mess);
    END
END


	static void CodeERROR(Word Index)
BEGIN
   String mess;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     EvalStringExpression(ArgStr[1],&OK,mess);
     if (NOT OK) WrError(1970);
     else WrErrorString(mess,"",False,False);
    END
END


	static void CodeFATAL(Word Index)
BEGIN
   String mess;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     EvalStringExpression(ArgStr[1],&OK,mess);
     if (NOT OK) WrError(1970);
     else WrErrorString(mess,"",False,True);
    END
END

	static void CodeCHARSET(Word Index)
BEGIN
   TempResult t;
   FILE *f;
   unsigned char tfield[256];
   LongWord Start,l,TStart,Stop,z;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt>3) WrError(1110);
   else if (ArgCnt==0)
    BEGIN
     for (z=0; z<256; z++) CharTransTable[z]=z;
    END
   else
    BEGIN
     FirstPassUnknown=False; EvalExpression(ArgStr[1],&t);
     switch (t.Typ)
      BEGIN
       case TempInt:
        if (FirstPassUnknown) t.Contents.Int&=255;
        if (ChkRange(t.Contents.Int,0,255))
         BEGIN
          if (ArgCnt<2) WrError(1110);
          else
           BEGIN
            Start=t.Contents.Int;
            FirstPassUnknown=False; EvalExpression(ArgStr[2],&t);
            switch (t.Typ)
             BEGIN
              case TempInt: /* �bersetzungsbereich als Character-Angabe */
               if (FirstPassUnknown) t.Contents.Int&=255;
               if (ArgCnt==2)
                BEGIN
                 Stop=Start; TStart=t.Contents.Int;
                 OK=ChkRange(TStart,0,255);
                END
               else
                BEGIN
                 Stop=t.Contents.Int; OK=ChkRange(Stop,Start,255);
                 if (OK) TStart=EvalIntExpression(ArgStr[3],UInt8,&OK);
                 else TStart=0;
                END
               if (OK)
                for (z=Start; z<=Stop; z++) CharTransTable[z]=TStart+(z-Start);
               break;
              case TempString:
               l = t.Contents.Ascii.Length; /* Uebersetzungsstring ab Start */
               if (Start +l > 256) WrError(1320);
               else
                for (z = 0; z < l; z++)
                  CharTransTable[Start + z] = t.Contents.Ascii.Contents[z];
               break;
              case TempFloat:
               WrError(1135);
               break;
              default:
               break;
             END
           END
         END
        break;
       case TempString:
        if (ArgCnt!=1) WrError(1110); /* Tabelle von Datei lesen */
        else
         BEGIN
          String Tmp;

          DynString2CString(Tmp, &t.Contents.Ascii, sizeof(Tmp));
          f=fopen(Tmp, OPENRDMODE);
          if (f==Nil) ChkIO(10001);
          if (fread(tfield,sizeof(char),256,f)!=256) ChkIO(10003);
          fclose(f); memcpy(CharTransTable,tfield,sizeof(char)*256);
         END
        break;
       case TempFloat:
        WrError(1135);
        break;
       default:
        break;
      END
    END
END

	static void CodePRSET(Word Index)
BEGIN
   int z,z2;
   UNUSED(Index);

   for (z=0; z<16; z++)
    BEGIN
     for (z2=0; z2<16; z2++) printf(" %02x",CharTransTable[z*16+z2]);
     printf("  ");
     for (z2=0; z2<16; z2++) printf("%c",CharTransTable[z*16+z2]>' ' ? CharTransTable[z*16+z2] : '.');
     putchar('\n');
    END
END

	static void CodeCODEPAGE(Word Index)
BEGIN
   PTransTable Prev,Run,New,Source;
   int erg=0;
   UNUSED(Index);

   if ((ArgCnt!=1) AND (ArgCnt!=2)) WrError(1110);
   else if (NOT ChkSymbName(ArgStr[1])) WrXError(1020,ArgStr[1]);
   else
    BEGIN
     if (NOT CaseSensitive)
      BEGIN
       UpString(ArgStr[1]);
       if (ArgCnt==2) UpString(ArgStr[2]);
      END

     if (ArgCnt==1) Source=CurrTransTable;
     else
      for (Source=TransTables; Source!=Nil; Source=Source->Next)
       if (strcmp(Source->Name,ArgStr[2])==0) break;

     if (Source==Nil) WrXError(1610,ArgStr[2]);
     else
      BEGIN
       for (Prev=Nil,Run=TransTables; Run!=Nil; Prev=Run,Run=Run->Next)
        if ((erg=strcmp(ArgStr[1],Run->Name))<=0) break;

       if ((Run==Nil) OR (erg<0))
        BEGIN
         New=(PTransTable) malloc(sizeof(TTransTable));
         New->Next=Run;
         New->Name=strdup(ArgStr[1]);
         New->Table=(unsigned char *) malloc(256*sizeof(char));
         memcpy(New->Table,Source->Table,256*sizeof(char));
         if (Prev==Nil) TransTables=New; else Prev->Next=New;
         CurrTransTable=New;
        END
       else CurrTransTable=Run;
      END
    END
END


	static void CodeFUNCTION(Word Index)
BEGIN
   String FName;
   Boolean OK;
   int z;
   UNUSED(Index);

   if (ArgCnt<2) WrError(1110);
   else
    BEGIN
     OK=True; z=1;
     do
      BEGIN
       OK=(OK AND ChkMacSymbName(ArgStr[z]));
       if (NOT OK) WrXError(1020,ArgStr[z]);
       z++;
      END
     while ((z<ArgCnt) AND (OK));
     if (OK)
      BEGIN
       strmaxcpy(FName,ArgStr[ArgCnt],255);
       for (z=1; z<ArgCnt; z++)
	CompressLine(ArgStr[z],z,FName);
       EnterFunction(LabPart,FName,ArgCnt-1);
      END
    END
END


	static void CodeSAVE(Word Index)
BEGIN
   PSaveState Neu;
   UNUSED(Index);

   if (ArgCnt!=0) WrError(1110);
   else
    BEGIN
     Neu=(PSaveState) malloc(sizeof(TSaveState));
     Neu->Next=FirstSaveState;
     Neu->SaveCPU=MomCPU;
     Neu->SavePC=ActPC;
     Neu->SaveListOn=ListOn;
     Neu->SaveLstMacroEx=LstMacroEx;
     Neu->SaveTransTable=CurrTransTable;
     FirstSaveState=Neu;
    END
END


	static void CodeRESTORE(Word Index)
BEGIN
   PSaveState Old;
   UNUSED(Index);

   if (ArgCnt!=0) WrError(1110);
   else if (FirstSaveState==Nil) WrError(1450);
   else
    BEGIN
     Old=FirstSaveState; FirstSaveState=Old->Next;
     if (Old->SavePC!=ActPC)
      BEGIN
       ActPC=Old->SavePC; DontPrint=True;
      END
     if (Old->SaveCPU!=MomCPU) SetCPU(Old->SaveCPU,False);
     EnterIntSymbol(ListOnName,ListOn=Old->SaveListOn,0,True); 
     SetFlag(&LstMacroEx,LstMacroExName,Old->SaveLstMacroEx);
     CurrTransTable=Old->SaveTransTable;
     free(Old);
    END
END


	static void CodeSEGMENT(Word Index)
BEGIN
   Byte SegZ;
   Word Mask;
   Boolean Found;
   UNUSED(Index);

   if (ArgCnt != 1) WrError(1110);
   else
    BEGIN
     Found = False; NLS_UpString(ArgStr[1]);
     for (SegZ = 1,Mask = 2; SegZ <= PCMax; SegZ++,Mask <<= 1)
      if (((ValidSegs&Mask) != 0) AND (strcmp(ArgStr[1], SegNames[SegZ]) == 0))
       BEGIN
        Found = True;
        SetNSeg(SegZ);
       END
     if (NOT Found) WrXError(1961, ArgStr[1]);
    END
END


	static void CodeLABEL(Word Index)
BEGIN
   LongInt Erg;
   Boolean OK;
   UNUSED(Index);

   FirstPassUnknown=False;
   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     Erg=EvalIntExpression(ArgStr[1],Int32,&OK);
     if ((OK) AND (NOT FirstPassUnknown))
      BEGIN
       char s[40];

       PushLocHandle(-1);
       EnterIntSymbol(LabPart,Erg,SegCode,False);
       IntLine(s,Erg);
       sprintf(ListLine,"=%s",s);
       PopLocHandle();
       END
    END
END


static void CodeREAD(Word Index)
{
  String Exp;
  TempResult Erg;
  Boolean OK;
  LongInt SaveLocHandle;
  UNUSED(Index);

  if ((ArgCnt != 1) && (ArgCnt != 2)) WrError(1110);
  else
  {
    if (ArgCnt == 2) EvalStringExpression(ArgStr[1], &OK, Exp);
    else
    {
      sprintf(Exp,"Read %s ? ",ArgStr[1]); OK=True;
    }
    if (OK)
    {
      setbuf(stdout, Nil); printf("%s", Exp); 
      fgets(Exp, 255, stdin); UpString(Exp);
      FirstPassUnknown = False;
      EvalExpression(Exp, &Erg);
      if (OK)
      {
        SetListLineVal(&Erg);
        SaveLocHandle = MomLocHandle; MomLocHandle = -1;
        if (FirstPassUnknown) WrError(1820);
        else switch (Erg.Typ)
        {
          case TempInt:
            EnterIntSymbol(ArgStr[ArgCnt], Erg.Contents.Int, SegNone, True);
            break;
          case TempFloat:
            EnterFloatSymbol(ArgStr[ArgCnt], Erg.Contents.Float, True);
            break;
          case TempString:
          {
            String Tmp;

            DynString2CString(Tmp, &Erg.Contents.Ascii, sizeof(Tmp));
            EnterStringSymbol(ArgStr[ArgCnt], Tmp, True);
            break;
          }
          default:
            break;
        }
        MomLocHandle = SaveLocHandle;
      }
    }
  }
}

	static void CodeRADIX(Word Index)
BEGIN
   Boolean OK;
   LargeWord tmp;

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     tmp = ConstLongInt(ArgStr[1], &OK, 10);
     if (NOT OK) WrError(1135);
     else if (ChkRange(tmp,2,36))
      BEGIN
       if (Index == 1) OutRadixBase = tmp;
       else RadixBase = tmp;
      END
    END
END

	static void CodeALIGN(Word Index)
BEGIN
   Word Dummy;
   Boolean OK;
   LongInt NewPC;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     FirstPassUnknown=False;
     Dummy=EvalIntExpression(ArgStr[1],Int16,&OK);
     if (OK)
      BEGIN
       if (FirstPassUnknown) WrError(1820);
       else
        BEGIN
         NewPC=ProgCounter()+Dummy-1;
         NewPC-=NewPC%Dummy;
         CodeLen=NewPC-ProgCounter();
         DontPrint=(CodeLen!=0);
         if (DontPrint) BookKeeping();
        END
      END
    END
END


	static void CodeENUM(Word Index)
BEGIN
   int z;
   char *p=Nil;
   Boolean OK;
   LongInt Counter,First=0,Last=0;
   String SymPart;
   UNUSED(Index);

   Counter=0;
   if (ArgCnt==0) WrError(1110);
   else
    for (z=1; z<=ArgCnt; z++)
     BEGIN
      p=QuotPos(ArgStr[z],'=');
      if (p!=Nil)
       BEGIN
        strmaxcpy(SymPart,p+1,255);
	FirstPassUnknown=False;
	Counter=EvalIntExpression(SymPart,Int32,&OK);
	if (NOT OK) return;
	if (FirstPassUnknown)
	 BEGIN
	  WrXError(1820,SymPart); return;
	 END
        *p='\0';
       END
      EnterIntSymbol(ArgStr[z],Counter,SegNone,False);
      if (z==1) First=Counter;
      else if (z==ArgCnt) Last=Counter;
      Counter++;
     END
   IntLine(SymPart, First);
   sprintf(ListLine, "=%s", SymPart);
   if (ArgCnt!=1)
    BEGIN
     strmaxcat(ListLine,"..",255);
     IntLine(SymPart, Last);
     strmaxcat(ListLine,SymPart,255);
    END
END


	static void CodeEND(Word Index)
BEGIN
   LongInt HVal;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt>1) WrError(1110);
   else
    BEGIN
     if (ArgCnt==1)
      BEGIN
       FirstPassUnknown=False;
       HVal=EvalIntExpression(ArgStr[1],Int32,&OK);
       if ((OK) AND (NOT FirstPassUnknown))
        BEGIN
         ChkSpace(SegCode);
         StartAdr=HVal;
         StartAdrPresent=True;
        END
      END
     ENDOccured=True;
    END
END


	static void CodeLISTING(Word Index)
BEGIN
   Byte Value=0xff;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt!=1) WrError(1110);
   else if (*AttrPart!='\0') WrError(1100);
   else
    BEGIN
     OK=True; NLS_UpString(ArgStr[1]);
     if (strcmp(ArgStr[1],"OFF")==0) Value=0;
     else if (strcmp(ArgStr[1],"ON")==0) Value=1;
     else if (strcmp(ArgStr[1],"NOSKIPPED")==0) Value=2;
     else if (strcmp(ArgStr[1],"PURECODE")==0) Value=3;
     else OK=False;
     if (NOT OK) WrError(1520);
     else EnterIntSymbol(ListOnName,ListOn=Value,0,True);
    END
END

        static void CodeBINCLUDE(Word Index)
BEGIN
   FILE *F;
   LongInt Len = (-1);
   LongWord Ofs = 0, Curr, Rest, FSize;
   Word RLen;
   Boolean OK, SaveTurnWords;
   LargeWord OldPC;
   String Name;
   UNUSED(Index);

   if ((ArgCnt < 1) OR (ArgCnt > 3)) WrError(1110);
   else if (ActPC == StructSeg) WrError(1940);
   else
    BEGIN
     if (ArgCnt == 1) OK = True;
     else
      BEGIN
       FirstPassUnknown = False;
       Ofs = EvalIntExpression(ArgStr[2], Int32, &OK);
       if (FirstPassUnknown)
        BEGIN
         WrError(1820); OK = False;
        END
       if (OK)
        BEGIN
         if (ArgCnt == 2) Len = (-1);
         else
          BEGIN
           Len = EvalIntExpression(ArgStr[3], Int32, &OK);
           if (FirstPassUnknown)
            BEGIN
             WrError(1820); OK = False;
            END
          END
        END
      END
     if (OK)
      BEGIN
       strmaxcpy(Name, ArgStr[1], 255);
       if (*Name == '"') strmov(Name, Name + 1);
       if ((*Name) && (Name[strlen(Name) - 1] == '"')) Name[strlen(Name) - 1] = '\0';
       strmaxcpy(ArgStr[1], Name, 255);
       strmaxcpy(Name, FExpand(FSearch(Name, IncludeList)), 255);
       if ((*Name) && (Name[strlen(Name) - 1] == '/')) strmaxcat(Name, ArgStr[1], 255);
       F = fopen(Name, OPENRDMODE);
       if (F == NULL) ChkIO(10001);
       errno = 0; FSize = FileSize(F); ChkIO(10003);
       if (Len == -1)
        if ((Len = FSize - Ofs) < 0)
         BEGIN
          fclose(F); WrError(1600); return;
         END
       if (NOT ChkPC(PCs[ActPC] + Len - 1)) WrError(1925);
       else
        BEGIN
         errno = 0; fseek(F, Ofs, SEEK_SET); ChkIO(10003);
         Rest = Len; SaveTurnWords = TurnWords; TurnWords = False;
         OldPC = ProgCounter();
         do
          BEGIN
           if (Rest <= 256) Curr = Rest; else Curr = 256;
           errno = 0; RLen = fread(BAsmCode, 1, Curr, F); ChkIO(10003);
           CodeLen = RLen;
           WriteBytes();
           PCs[ActPC] += CodeLen;
           Rest -= RLen;
          END
         while ((Rest != 0) AND (RLen == Curr));
         if (Rest != 0) WrError(1600);
         TurnWords = SaveTurnWords;
         DontPrint = True; CodeLen = ProgCounter() - OldPC;
         PCs[ActPC] = OldPC;
        END
       fclose(F);
      END
    END
END

        static void CodePUSHV(Word Index)
BEGIN
   int z;
   UNUSED(Index);

   if (ArgCnt<2) WrError(1110);
   else
    BEGIN
     if (NOT CaseSensitive) NLS_UpString(ArgStr[1]);
     for (z=2; z<=ArgCnt; z++)
      PushSymbol(ArgStr[z],ArgStr[1]);
    END
END

        static void CodePOPV(Word Index)
BEGIN
   int z;
   UNUSED(Index);

   if (ArgCnt<2) WrError(1110);
   else
    BEGIN
     if (NOT CaseSensitive) NLS_UpString(ArgStr[1]);
     for (z=2; z<=ArgCnt; z++)
      PopSymbol(ArgStr[z],ArgStr[1]);
    END
END

	static PForwardSymbol CodePPSyms_SearchSym(PForwardSymbol Root, char *Comp)
BEGIN
   PForwardSymbol Lauf=Root;
   UNUSED(Comp);

   while ((Lauf!=Nil) AND (strcmp(Lauf->Name,Comp)!=0)) Lauf=Lauf->Next;
   return Lauf;
END


        static void CodeSTRUCT(Word Index)
{
   PStructStack NStruct;
   int z;
   Boolean OK, DoExt;
   char ExtChar;
   UNUSED(Index);

   if (ArgCnt > 1)
     WrError(1110);
   else if (!ChkSymbName(LabPart))
     WrXError(1020,LabPart);
   else
   {
     if (!CaseSensitive)
       NLS_UpString(LabPart);

     if (StructStack)
       StructStack->CurrPC = ProgCounter();
     NStruct = (PStructStack) malloc(sizeof(TStructStack));
     NStruct->Name = strdup(LabPart);
     NStruct->CurrPC = 0; DoExt = True;
     ExtChar = DottedStructs ? '.' : '_';
     NStruct->Next = StructStack;
     OK = True;
     for (z = 1; z <= ArgCnt; z++)
       if (OK)
       {
         if (!strcasecmp(ArgStr[z], "EXTNAMES"))
           DoExt = True;
         else if (!strcasecmp(ArgStr[z],"NOEXTNAMES"))
           DoExt = False;
         else if (!strcasecmp(ArgStr[z], "DOTS"))
           ExtChar = '.';
         else if (!strcasecmp(ArgStr[z],"NODOTS"))
           ExtChar = '_';
         else
         {
           WrXError(1554,ArgStr[z]); OK = False;
         }
       }
     if (OK)
     {
       NStruct->StructRec = CreateStructRec();
       NStruct->StructRec->ExtChar = ExtChar;
       NStruct->StructRec->DoExt = DoExt;
       NStruct->StructRec->IsUnion = Memo("UNION");
       StructStack = NStruct;
       if (ActPC != StructSeg)
         StructSaveSeg = ActPC;
       ActPC = StructSeg;
       PCs[ActPC] = 0;
       Phases[ActPC] = 0;
       Grans[ActPC] = Grans[SegCode]; ListGrans[ActPC] = ListGrans[SegCode];
       ClearChunk(SegChunks + StructSeg);
       CodeLen = 0; DontPrint = True;
     }
     else
     {
       free(NStruct->Name); free(NStruct);
     }
   }
}

        static void CodeENDSTRUCT(Word Index)
{
   Boolean OK;
   PStructStack OStruct;
   TempResult t;
   String tmp;
   UNUSED(Index);

   if (ArgCnt>1) WrError(1110);
   else if (!StructStack) WrError(1550);
   else
   {
     if (*LabPart=='\0') OK=True;
     else
     {
       if (NOT CaseSensitive) NLS_UpString(LabPart);
       OK=(strcmp(LabPart, StructStack->Name)==0);
       if (NOT OK) WrError(1552);
     }
     if (OK)
     {
       OStruct = StructStack; StructStack = OStruct->Next;
       if (ArgCnt == 0)
         sprintf(tmp, "%s%clen", OStruct->Name, OStruct->StructRec->ExtChar);
       else
         strmaxcpy(tmp, ArgStr[1], 255);
       BumpStructLength(OStruct->StructRec, ProgCounter());
       EnterIntSymbol(tmp, OStruct->StructRec->TotLen, SegNone, False);
       t.Typ = TempInt; t.Contents.Int = OStruct->StructRec->TotLen;
       SetListLineVal(&t);

       AddStruct(OStruct->StructRec, OStruct->Name, True);

       free(OStruct->Name);
       free(OStruct);
       if (StructStack == Nil) ActPC = StructSaveSeg;
       else PCs[ActPC] += StructStack->CurrPC;
       ClearChunk(SegChunks + StructSeg);
       CodeLen = 0; DontPrint = True;
     }
   }
}

	static void CodeEXTERN(Word Index)
BEGIN
   char *Split;
   int i;
   Boolean OK;
   Byte Type;
   UNUSED(Index);

   if (ArgCnt < 1) WrError(1110);
   else
    BEGIN
     i = 1; OK = True;
     while ((OK) && (i <= ArgCnt))
      BEGIN
       Split = strrchr(ArgStr[i], ':');
       if (Split == NULL)
        Type = SegNone;
       else
        BEGIN
         *Split = '\0';
         for (Type = SegNone + 1; Type <= PCMax; Type++)
          if (strcasecmp(Split + 1, SegNames[Type]) == 0)
           break;
        END
       if (Type > PCMax) WrXError(1961, Split + 1);
       else
        BEGIN
         EnterExtSymbol(ArgStr[i], 0, Type, FALSE);
        END
       i++;
      END
    END
END

	static void CodeNESTMAX(Word Index)
BEGIN
   LongInt Temp;
   Boolean OK;
   UNUSED(Index);

   if (ArgCnt != 1) WrError(1110);
   else
    BEGIN
     FirstPassUnknown = False;
     Temp = EvalIntExpression(ArgStr[1], UInt32, &OK);
     if (OK)
      BEGIN
       if (FirstPassUnknown) WrError(1820);
       else NestMax = Temp;
      END
    END
END

	static void CodeSEGTYPE(Word Index)
BEGIN
   UNUSED(Index);

   if (ArgCnt != 0) WrError(1110);
   else
    RelSegs = (mytoupper(*OpPart) == 'R');
END

	static void CodePPSyms(PForwardSymbol *Orig,
                               PForwardSymbol *Alt1,
                               PForwardSymbol *Alt2)
BEGIN
   PForwardSymbol Lauf;
   int z;
   String Sym,Section;

   if (ArgCnt==0) WrError(1110);
   else
    for (z=1; z<=ArgCnt; z++)
     BEGIN
      SplitString(ArgStr[z],Sym,Section,QuotPos(ArgStr[z],':'));
      if (NOT ExpandSymbol(Sym)) return;
      if (NOT ExpandSymbol(Section)) return;
      if (NOT CaseSensitive) NLS_UpString(Sym);
      Lauf=CodePPSyms_SearchSym(*Alt1,Sym);
      if (Lauf!=Nil) WrXError(1489,ArgStr[z]);
      else
       BEGIN
	Lauf=CodePPSyms_SearchSym(*Alt2,Sym);
	if (Lauf!=Nil) WrXError(1489,ArgStr[z]);
	else
	 BEGIN
	  Lauf=CodePPSyms_SearchSym(*Orig,Sym);
	  if (Lauf==Nil)
	   BEGIN
	    Lauf=(PForwardSymbol) malloc(sizeof(TForwardSymbol)); 
            Lauf->Next=(*Orig); *Orig=Lauf;
	    Lauf->Name=strdup(Sym);
	   END
	  IdentifySection(Section,&(Lauf->DestSection));
	 END
       END
     END
END

/*------------------------------------------------------------------------*/

#define ONOFFMax 32
static int ONOFFCnt=0;
typedef struct
         {
          Boolean Persist,*FlagAddr;
          char *FlagName,*InstName;
         } ONOFFTab;
static ONOFFTab ONOFFList[ONOFFMax];

	static void DecodeONOFF(Word Index)
BEGIN
   ONOFFTab *Tab=ONOFFList+Index;
   Boolean OK;

   if (ArgCnt!=1) WrError(1110);
   else
    BEGIN
     NLS_UpString(ArgStr[1]);
     if (*AttrPart!='\0') WrError(1100);
     else if ((strcasecmp(ArgStr[1],"ON")!=0) AND (strcasecmp(ArgStr[1],"OFF")!=0)) WrError(1520);
     else
      BEGIN
       OK=(strcasecmp(ArgStr[1],"ON")==0);
       SetFlag(Tab->FlagAddr,Tab->FlagName,OK);
      END
    END
END

	void AddONOFF(char *InstName, Boolean *Flag, char *FlagName, Boolean Persist)
BEGIN
   if (ONOFFCnt==ONOFFMax) exit(255);
   ONOFFList[ONOFFCnt].Persist=Persist;
   ONOFFList[ONOFFCnt].FlagAddr=Flag;
   ONOFFList[ONOFFCnt].FlagName=FlagName;
   ONOFFList[ONOFFCnt].InstName=InstName;
   AddInstTable(ONOFFTable,InstName,ONOFFCnt++,DecodeONOFF);
END

	void ClearONOFF(void)
BEGIN
   int z,z2;

   for (z=0; z<ONOFFCnt; z++)
    if (NOT ONOFFList[z].Persist) break;

   for (z2=ONOFFCnt-1; z2>=z; z2--)
    RemoveInstTable(ONOFFTable,ONOFFList[z2].InstName);

   ONOFFCnt=z;
END

/*------------------------------------------------------------------------*/

typedef struct
         {
          char *Name;
          void (*Proc)(
#ifdef __PROTOS__
                       Word Index
#endif
                                 );
         } PseudoOrder;
static PseudoOrder Pseudos[]=
                   {{"ALIGN",      CodeALIGN     },
                    {"ASEG",       CodeSEGTYPE   },
                    {"BINCLUDE",   CodeBINCLUDE  },
                    {"CHARSET",    CodeCHARSET   },
                    {"CODEPAGE",   CodeCODEPAGE  },
                    {"CPU",        CodeCPU       },
                    {"DEPHASE",    CodeDEPHASE   },
                    {"END",        CodeEND       },
                    {"ENDS",       CodeENDSTRUCT },
                    {"ENDSECTION", CodeENDSECTION},
                    {"ENDSTRUCT",  CodeENDSTRUCT },
                    {"ENUM",       CodeENUM      },
                    {"ERROR",      CodeERROR     },
                    {"EXPORT_SYM", CodeEXPORT    },
                    {"EXTERN_SYM", CodeEXTERN    },
                    {"FATAL",      CodeFATAL     },
                    {"FUNCTION",   CodeFUNCTION  },
                    {"LABEL",      CodeLABEL     },
		    {"LISTING",    CodeLISTING   },
                    {"MESSAGE",    CodeMESSAGE   },
                    {"NEWPAGE",    CodeNEWPAGE   },
                    {"NESTMAX",    CodeNESTMAX   },
                    {"ORG",        CodeORG       },
                    {"PAGE",       CodePAGE      },
                    {"PHASE",      CodePHASE     },
                    {"POPV",       CodePOPV      },
                    {"PRSET",      CodePRSET     },
                    {"PUSHV",      CodePUSHV     },
                    {"RADIX",      CodeRADIX     },
                    {"READ",       CodeREAD      },
                    {"RESTORE",    CodeRESTORE   },
                    {"RSEG",       CodeSEGTYPE   },
                    {"SAVE",       CodeSAVE      },
                    {"SECTION",    CodeSECTION   },
                    {"SEGMENT",    CodeSEGMENT   },
                    {"SHARED",     CodeSHARED    },
                    {"STRUC",      CodeSTRUCT    },
                    {"STRUCT",     CodeSTRUCT    },
                    {"UNION",      CodeSTRUCT    },
                    {"WARNING",    CodeWARNING   },
                    {""       ,    Nil           }};

	Boolean CodeGlobalPseudo(void)
BEGIN
   switch (*OpPart)
    BEGIN
     case 'S': 
      if ((NOT SetIsOccupied) AND (Memo("SET")))
       BEGIN
        CodeSETEQU(0); return True;
       END
      break;
     case 'E':
      if ((SetIsOccupied) AND (Memo("EVAL")))
       BEGIN
        CodeSETEQU(0); return True;
       END
      break;
    END

   if (LookupInstTable(ONOFFTable,OpPart)) return True;

   if (LookupInstTable(PseudoTable,OpPart)) return True;

   if (SectionStack!=Nil)
    BEGIN
     if (Memo("FORWARD"))
      BEGIN
       if (PassNo<=MaxSymPass)
	CodePPSyms(&(SectionStack->LocSyms),
		   &(SectionStack->GlobSyms),
		   &(SectionStack->ExportSyms));
       return True;
      END
     if (Memo("PUBLIC"))
      BEGIN
       CodePPSyms(&(SectionStack->GlobSyms),
		  &(SectionStack->LocSyms),
		  &(SectionStack->ExportSyms));
       return True;
      END
     if (Memo("GLOBAL"))
      BEGIN
       CodePPSyms(&(SectionStack->ExportSyms),
		  &(SectionStack->LocSyms),
		  &(SectionStack->GlobSyms));
       return True;
      END
    END

   return False;
END


	void codeallg_init(void)
BEGIN
   PseudoOrder *POrder;

   PseudoStrs[0]=PrtInitString;
   PseudoStrs[1]=PrtExitString;
   PseudoStrs[2]=PrtTitleString;

   PseudoTable=CreateInstTable(201);
   for (POrder=Pseudos; POrder->Proc!=Nil; POrder++)
    AddInstTable(PseudoTable,POrder->Name,0,POrder->Proc);
   AddInstTable(PseudoTable,"OUTRADIX", 1, CodeRADIX);
   AddInstTable(PseudoTable,"EQU",0,CodeSETEQU);
   AddInstTable(PseudoTable,"=",0,CodeSETEQU);
   AddInstTable(PseudoTable,":=",0,CodeSETEQU);
   AddInstTable(PseudoTable,"PRTINIT",0,CodeString);
   AddInstTable(PseudoTable,"PRTEXIT",1,CodeString);
   AddInstTable(PseudoTable,"TITLE",2,CodeString);
   ONOFFTable=CreateInstTable(47);
   AddONOFF("MACEXP", &LstMacroEx, LstMacroExName, True);
   AddONOFF("RELAXED", &RelaxedMode, RelaxedName, True);
   AddONOFF("DOTTEDSTRUCTS", &DottedStructs, DottedStructsName,True);
END
