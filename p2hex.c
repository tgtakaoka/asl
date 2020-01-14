/* p2hex.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Konvertierung von AS-P-Dateien nach Hex                                   */
/*                                                                           */
/* Historie:  1. 6.1996 Grundsteinlegung                                     */
/*           29. 8.1998 HeadIds verwendet fuer Default-Hex-Format            */
/*           30. 5.1999 0x statt $ erlaubt                                   */
/*            6. 7.1999 minimal S-Record-Adresslaenge setzbar                */
/*                      Fehlerabfrage in CMD_Linelen war falsch              */
/*           12.10.1999 Startadresse 16-Bit-Hex geaendert                    */
/*           13.10.1999 Startadressen 20+32 Bit Intel korrigiert             */
/*           24.10.1999 Relokation von Adressen (Thomas Eschenbach)          */
/*                                                                           */
/*****************************************************************************/

#include "stdinc.h"
#include <ctype.h>
#include <string.h>

#include "version.h"
#include "endian.h"
#include "bpemu.h"
#include "hex.h"
#include "nls.h"
#include "nlmessages.h"
#include "p2hex.rsc"
#include "ioerrs.h"
#include "strutil.h"
#include "chunks.h"
#include "cmdarg.h"

#include "toolutils.h"
#include "headids.h"

static char *HexSuffix=".hex";
#define MaxLineLen 254

typedef void (*ProcessProc)(
#ifdef __PROTOS__
char *FileName, LongWord Offset
#endif
);

static CMDProcessed ParProcessed;
static int z;
static FILE *TargFile;
static String SrcName, TargName;

static LongWord StartAdr, StopAdr, LineLen;
static LongWord StartData, StopData, EntryAdr;
static LargeInt Relocate;
static Boolean StartAuto, StopAuto, AutoErase, EntryAdrPresent;
static Word Seg, Ofs;
static LongWord Dummy;
static Byte IntelMode;
static Byte MultiMode;   /* 0=8M, 1=16, 2=8L, 3=8H */
static Byte MinMoto;
static Boolean Rec5;
static Boolean SepMoto;

static Boolean RelAdr, MotoOccured, IntelOccured, MOSOccured, DSKOccured;
static Byte MaxMoto, MaxIntel;

static THexFormat DestFormat;

static ChunkList UsedList;

        static void ParamError(Boolean InEnv, char *Arg)
BEGIN
   printf("%s%s\n",getmessage(InEnv?Num_ErrMsgInvEnvParam:Num_ErrMsgInvParam),Arg);
   printf("%s\n",getmessage(Num_ErrMsgProgTerm));
   exit(1);
END

        static void OpenTarget(void)
BEGIN
   TargFile=fopen(TargName,"w");
   if (TargFile==Nil) ChkIO(TargName);
END

	static void CloseTarget(void)
BEGIN
   errno=0; fclose(TargFile); ChkIO(TargName);
   if (Magic!=0) unlink(TargName);
END

        static void ProcessFile(char *FileName, LongWord Offset)
BEGIN
   FILE *SrcFile;
   Word TestID;
   Byte InpHeader,InpSegment,InpGran,BSwap;
   LongInt InpStart,SumLen;
   int z2;
   Word InpLen,TransLen;
   Boolean doit,FirstBank=0;
   Byte Buffer[MaxLineLen];
   Word *WBuffer=(Word *) Buffer;
   LongWord ErgStart,
#ifdef __STDC__
            ErgStop=0xffffffffu,
#else
            ErgStop=0xffffffff,
#endif
            NextPos,IntOffset=0,MaxAdr;
   Word ErgLen=0,ChkSum=0,RecCnt,Gran,SwapBase,HSeg;

   LongInt z;

   Byte MotRecType=0;

   THexFormat ActFormat;
   PFamilyDescr FoundDscr;

   SrcFile=fopen(FileName,OPENRDMODE); 
   if (SrcFile==Nil) ChkIO(FileName);

   if (NOT Read2(SrcFile,&TestID)) ChkIO(FileName);
   if (TestID!=FileMagic) FormatError(FileName,getmessage(Num_FormatInvHeaderMsg));

   errno=0; printf("%s==>>%s",FileName,TargName); ChkIO(OutName);

   SumLen=0;

   do
    BEGIN
     ReadRecordHeader(&InpHeader,&InpSegment,&InpGran,FileName,SrcFile);
     if (InpHeader==FileHeaderStartAdr)
      BEGIN
       if (NOT Read4(SrcFile,&ErgStart)) ChkIO(FileName);
       if (NOT EntryAdrPresent)
        BEGIN
         EntryAdr=ErgStart; EntryAdrPresent=True;
        END
      END
     else if (InpHeader!=FileHeaderEnd)
      BEGIN
       Gran=InpGran;
       
       if ((ActFormat=DestFormat)==Default)
        BEGIN
         FoundDscr=FindFamilyById(InpHeader);
         if (FoundDscr==Nil)
          FormatError(FileName,getmessage(Num_FormatInvRecordHeaderMsg));
         else ActFormat=FoundDscr->HexFormat;
        END

       switch (ActFormat)
        BEGIN
         case MotoS:
         case IntHex32:
#ifdef __STDC__
          MaxAdr=0xffffffffu; break;
#else
          MaxAdr=0xffffffff; break;
#endif
         case IntHex16:
          MaxAdr=0xffff0+0xffff; break;
         case Atmel:
          MaxAdr=0xffffff; break;
         default:
          MaxAdr=0xffff;
        END

       if (NOT Read4(SrcFile,&InpStart)) ChkIO(FileName);
       if (NOT Read2(SrcFile,&InpLen)) ChkIO(FileName);

       NextPos=ftell(SrcFile)+InpLen;
       if (NextPos>=FileSize(SrcFile)-1)
        FormatError(FileName,getmessage(Num_FormatInvRecordLenMsg));

       doit=(FilterOK(InpHeader)) AND (InpSegment==SegCode);

       if (doit)
        BEGIN
         InpStart+=Offset;
 	 ErgStart=max(StartAdr,InpStart);
 	 ErgStop=min(StopAdr,InpStart+(InpLen/Gran)-1);
 	 doit=(ErgStop>=ErgStart);
 	 if (doit)
 	  BEGIN
 	   ErgLen=(ErgStop+1-ErgStart)*Gran;
           if (AddChunk(&UsedList,ErgStart,ErgStop-ErgStart+1,True))
            BEGIN
             errno=0; printf(" %s\n",getmessage(Num_ErrMsgOverlap)); ChkIO(OutName);
            END
          END
        END

       if (ErgStop>MaxAdr)
        BEGIN
         errno=0; printf(" %s\n",getmessage(Num_ErrMsgAdrOverflow)); ChkIO(OutName);
        END

       if (doit)
        BEGIN
 	 /* an Anfang interessierender Daten */

 	 if (fseek(SrcFile,(ErgStart-InpStart)*Gran,SEEK_CUR)==-1) ChkIO(FileName);

 	 /* Statistik, Anzahl Datenzeilen ausrechnen */

         RecCnt=ErgLen/LineLen;
         if ((ErgLen%LineLen)!=0) RecCnt++;

 	 /* relative Angaben ? */

 	 if (RelAdr) ErgStart -= StartAdr;

         /* Auf Zieladressbereich verschieben */

         ErgStart += Relocate;

 	 /* Kopf einer Datenzeilengruppe */

  	 switch (ActFormat)
          BEGIN
   	   case MotoS:
  	    if ((NOT MotoOccured) OR (SepMoto))
  	     BEGIN
  	      errno=0; fprintf(TargFile,"S0030000FC\n"); ChkIO(TargName);
  	     END
  	    if ((ErgStop >> 24) != 0) MotRecType=2;
  	    else if ((ErgStop>>16)!=0) MotRecType=1;
  	    else MotRecType=0;
            if (MotRecType < (MinMoto - 1)) MotRecType = (MinMoto - 1);
            if (MaxMoto<MotRecType) MaxMoto=MotRecType;
  	    if (Rec5)
  	     BEGIN
  	      ChkSum=Lo(RecCnt)+Hi(RecCnt)+3;
              errno=0;
  	      fprintf(TargFile,"S503%s%s\n",HexWord(RecCnt),HexByte(Lo(ChkSum^0xff)));
  	      ChkIO(TargName);
  	     END
  	    MotoOccured=True;
  	    break;
  	   case MOSHex:
            MOSOccured=True;
            break;
  	   case IntHex:
  	    IntelOccured=True;
  	    IntOffset=0;
  	    break;
  	   case IntHex16:
  	    IntelOccured=True;
#ifdef __STDC__
  	    IntOffset=ErgStart&0xfffffff0u;
#else
            IntOffset=ErgStart&0xfffffff0;
#endif
  	    HSeg=IntOffset>>4; ChkSum=4+Lo(HSeg)+Hi(HSeg);
            errno=0;
  	    fprintf(TargFile,":02000002%s%s\n",HexWord(HSeg),HexByte(0x100-ChkSum));
            if (MaxIntel<1) MaxIntel=1;
  	    ChkIO(TargName);
  	    break;
           case IntHex32:
  	    IntelOccured=True;
#ifdef __STDC__
            IntOffset=ErgStart&0xffff0000u;
#else
            IntOffset=ErgStart&0xffff0000;
#endif
            HSeg=IntOffset>>16; ChkSum=6+Lo(HSeg)+Hi(HSeg);
            fprintf(TargFile,":02000004%s%s\n",HexWord(HSeg),HexByte(0x100-ChkSum));
            if (MaxIntel<2) MaxIntel=2;
  	    ChkIO(TargName);
            FirstBank=False;
            break;
           case TekHex:
            break;
           case Atmel:
            break;
  	   case TiDSK:
  	    if (NOT DSKOccured)
  	     BEGIN
  	      DSKOccured=True;
              errno=0; fprintf(TargFile,"%s%s\n",getmessage(Num_DSKHeaderLine),TargName); ChkIO(TargName);
  	     END
            break;
           default: 
            break;
 	  END

 	 /* Datenzeilen selber */

 	 while (ErgLen>0)
 	  BEGIN
           /* evtl. Folgebank fuer Intel32 ausgeben */

           if ((ActFormat==IntHex32) AND (FirstBank))
            BEGIN
             IntOffset+=0x10000;
             HSeg=IntOffset>>16; ChkSum=6+Lo(HSeg)+Hi(HSeg);
             errno=0;
             fprintf(TargFile,":02000004%s%s\n",HexWord(HSeg),HexByte(0x100-ChkSum));
             ChkIO(TargName);
             FirstBank=False;
            END

           /* Recordlaenge ausrechnen, fuer Intel32 auf 64K-Grenze begrenzen
              bei Atmel nur 2 Byte pro Zeile! */

           TransLen=min(LineLen,ErgLen);
           if ((ActFormat==IntHex32) AND ((ErgStart&0xffff)+(TransLen/Gran)>=0x10000))
            BEGIN
             TransLen=Gran*(0x10000-(ErgStart&0xffff));
             FirstBank=True;
            END
           else if (ActFormat==Atmel) TransLen=min(2,TransLen);

 	   /* Start der Datenzeile */

 	   switch (ActFormat)
            BEGIN
 	     case MotoS:
              errno=0;
              fprintf(TargFile,"S%c%s",'1'+MotRecType,HexByte(TransLen+3+MotRecType));
 	      ChkIO(TargName);
 	      ChkSum=TransLen+3+MotRecType;
 	      if (MotRecType>=2)
 	       BEGIN
 	        errno=0; fprintf(TargFile,"%s",HexByte((ErgStart>>24)&0xff)); ChkIO(TargName);
 	        ChkSum+=((ErgStart>>24)&0xff);
 	       END
 	      if (MotRecType>=1)
 	       BEGIN
 	        errno=0; fprintf(TargFile,"%s",HexByte((ErgStart>>16)&0xff)); ChkIO(TargName);
 	        ChkSum+=((ErgStart>>16)&0xff);
 	       END
 	      errno=0; fprintf(TargFile,"%s",HexWord(ErgStart&0xffff)); ChkIO(TargName);
 	      ChkSum+=Hi(ErgStart)+Lo(ErgStart);
 	      break;
 	     case MOSHex:
 	      errno=0; fprintf(TargFile,";%s%s",HexByte(TransLen),HexWord(ErgStart AND 0xffff)); ChkIO(TargName);
 	      ChkSum+=TransLen+Lo(ErgStart)+Hi(ErgStart);
 	      break;
             case IntHex:
             case IntHex16:
             case IntHex32:
 	      errno=0; fprintf(TargFile,":"); ChkIO(TargName); ChkSum=0;
 	      if (MultiMode==0)
 	       BEGIN
 	        errno=0; fprintf(TargFile,"%s",HexByte(TransLen)); ChkIO(TargName);
 	        errno=0; fprintf(TargFile,"%s",HexWord((ErgStart-IntOffset)*Gran)); ChkIO(TargName);
 	        ChkSum+=TransLen;
 	        ChkSum+=Lo((ErgStart-IntOffset)*Gran);
 	        ChkSum+=Hi((ErgStart-IntOffset)*Gran);
 	       END
 	      else
 	       BEGIN
 	        errno=0; fprintf(TargFile,"%s",HexByte(TransLen/Gran)); ChkIO(TargName);
 	        errno=0; fprintf(TargFile,"%s",HexWord(ErgStart-IntOffset)); ChkIO(TargName);
 	        ChkSum+=TransLen/Gran;
 	        ChkSum+=Lo(ErgStart-IntOffset);
 	        ChkSum+=Hi(ErgStart-IntOffset);
 	       END
 	      errno=0; fprintf(TargFile,"00"); ChkIO(TargName);
 	      break;
 	     case TekHex:
 	      errno=0; 
              fprintf(TargFile,"/%s%s%s",HexWord(ErgStart),HexByte(TransLen),
 		                         HexByte(Lo(ErgStart)+Hi(ErgStart)+TransLen));
 	      ChkIO(TargName);
 	      ChkSum=0;
 	      break;
 	     case TiDSK:
              errno=0; fprintf(TargFile,"9%s",HexWord(/*Gran**/ErgStart));
 	      ChkIO(TargName);
 	      ChkSum=0;
 	      break;
             case Atmel:
              errno=0; fprintf(TargFile,"%s%s:",HexByte(ErgStart >> 16),HexWord(ErgStart & 0xffff));
              ChkIO(TargName);
             default:
              break;
 	    END

 	   /* Daten selber */

 	   if (fread(Buffer,1,TransLen,SrcFile)!=TransLen) ChkIO(FileName);
 	   if ((Gran!=1) AND (MultiMode==1))
 	    for (z=0; z<(TransLen/Gran); z++)
 	     BEGIN
 	      SwapBase=z*Gran;
 	      for (z2=0; z2<(Gran/2); z++)
 	       BEGIN
 	        BSwap=Buffer[SwapBase+z2];
 	        Buffer[SwapBase+z2]=Buffer[SwapBase+Gran-1-z2];
 	        Buffer[SwapBase+Gran-1-z2]=BSwap;
 	       END
 	     END
 	   if (ActFormat==TiDSK)
            BEGIN
             if (BigEndian) WSwap(WBuffer,TransLen);
 	     for (z=0; z<(TransLen/2); z++)
 	      BEGIN
               errno=0;
 	       if ((ErgStart+z >= StartData) AND (ErgStart+z <= StopData))
 	        fprintf(TargFile,"M%s",HexWord(WBuffer[z]));
 	       else
 	        fprintf(TargFile,"B%s",HexWord(WBuffer[z]));
 	       ChkIO(TargName);
 	       ChkSum+=WBuffer[z];
 	       SumLen+=Gran;
 	      END
            END
           else if (ActFormat==Atmel)
            BEGIN
             if (TransLen>=2)
              BEGIN
               fprintf(TargFile,"%s",HexWord(WBuffer[0])); SumLen+=2;
              END
            END
 	   else
 	    for (z=0; z<(LongInt)TransLen; z++)
 	     if ((MultiMode<2) OR (z%Gran==MultiMode-2))
 	      BEGIN
 	       errno=0; fprintf(TargFile,"%s",HexByte(Buffer[z])); ChkIO(TargName);
 	       ChkSum+=Buffer[z];
 	       SumLen++;
 	      END

 	   /* Ende Datenzeile */

 	   switch (ActFormat)
            BEGIN
 	     case MotoS:
 	      errno=0;
 	      fprintf(TargFile,"%s\n",HexByte(Lo(ChkSum^0xff)));
 	      ChkIO(TargName);
 	      break;
 	     case MOSHex:
 	      errno=0;
              fprintf(TargFile,"%s\n",HexWord(ChkSum));
              break;
             case IntHex:
             case IntHex16:
             case IntHex32:
              errno=0;
 	      fprintf(TargFile,"%s\n",HexByte(Lo(1+(ChkSum^0xff))));
 	      ChkIO(TargName);
 	      break;
 	     case TekHex:
 	      errno=0;
              fprintf(TargFile,"%s\n",HexByte(Lo(ChkSum)));
 	      ChkIO(TargName);
 	      break;
 	     case TiDSK:
 	      errno=0;
              fprintf(TargFile,"7%sF\n",HexWord(ChkSum));
 	      ChkIO(TargName);
 	      break;
             case Atmel:
              errno=0;
              fprintf(TargFile,"\n");
              ChkIO(TargName);
              break;
             default:
              break;
 	    END

 	   /* Zaehler rauf */

 	   ErgLen-=TransLen;
 	   ErgStart+=TransLen/Gran;
 	  END

         /* Ende der Datenzeilengruppe */

         switch (ActFormat)
          BEGIN
           case MotoS:
            if (SepMoto)
             BEGIN
              errno=0;
 	      fprintf(TargFile,"S%c%s",'9'-MotRecType,HexByte(3+MotRecType));
              ChkIO(TargName);
              for (z=1; z<=2+MotRecType; z++)
               BEGIN
 	        errno=0; fprintf(TargFile,"%s",HexByte(0)); ChkIO(TargName);
               END
              errno=0;
              fprintf(TargFile,"%s\n",HexByte(0xff-3-MotRecType)); 
              ChkIO(TargName);
             END
            break;
           case MOSHex:
            break;
           case IntHex:
           case IntHex16:
           case IntHex32:
            break;
           case TekHex:
            break;
           case TiDSK:
            break;
           case Atmel:
            break;
           default:
            break;
          END;
        END
       if (fseek(SrcFile,NextPos,SEEK_SET)==-1) ChkIO(FileName);
      END
    END
   while (InpHeader!=0);

   errno=0; printf("  (%d Byte)\n",SumLen); ChkIO(OutName);

   errno=0; fclose(SrcFile); ChkIO(FileName);
END

static ProcessProc CurrProcessor;
static LongWord CurrOffset;

	static void Callback(char *Name)
BEGIN
   CurrProcessor(Name,CurrOffset);
END

	static void ProcessGroup(char *GroupName_O, ProcessProc Processor)
BEGIN
   String Ext,GroupName;

   CurrProcessor=Processor;
   strmaxcpy(GroupName,GroupName_O,255); strmaxcpy(Ext,GroupName,255);
   if (NOT RemoveOffset(GroupName,&CurrOffset)) ParamError(False,Ext);
   AddSuffix(GroupName,getmessage(Num_Suffix));

   if (NOT DirScan(GroupName,Callback))
    fprintf(stderr,"%s%s%s\n",getmessage(Num_ErrMsgNullMaskA),GroupName,getmessage(Num_ErrMsgNullMaskB));
END

        static void MeasureFile(char *FileName, LongWord Offset)
BEGIN
   FILE *f;
   Byte Header,Segment,Gran;
   Word Length,TestID;
   LongWord Adr,EndAdr,NextPos;

   f=fopen(FileName,OPENRDMODE); if (f==Nil) ChkIO(FileName);

   if (NOT Read2(f,&TestID)) ChkIO(FileName); 
   if (TestID!=FileMagic) FormatError(FileName,getmessage(Num_FormatInvHeaderMsg));

   do
    BEGIN 
     ReadRecordHeader(&Header,&Segment,&Gran,FileName,f);

     if (Header==FileHeaderStartAdr)
      BEGIN
       if (fseek(f,sizeof(LongWord),SEEK_CUR)==-1) ChkIO(FileName);
      END
     else if (Header!=FileHeaderEnd)
      BEGIN
       if (NOT Read4(f,&Adr)) ChkIO(FileName);
       if (NOT Read2(f,&Length)) ChkIO(FileName);
       NextPos=ftell(f)+Length;
       if (NextPos>FileSize(f))
        FormatError(FileName,getmessage(Num_FormatInvRecordLenMsg));

       if (FilterOK(Header))
        BEGIN
         Adr+=Offset;
 	 EndAdr=Adr+(Length/Gran)-1;
         if (StartAuto) if (StartAdr>Adr) StartAdr=Adr;
 	 if (StopAuto) if (EndAdr>StopAdr) StopAdr=EndAdr;
        END

       fseek(f,NextPos,SEEK_SET);
      END
    END
   while(Header!=0);

   fclose(f);
END

	static CMDResult CMD_AdrRange(Boolean Negate, char *Arg)
BEGIN
   char *p,Save;
   Boolean err;

   if (Negate)
    BEGIN
     StartAdr=0; StopAdr=0x7fff;
     return CMDOK;
    END
   else
    BEGIN
     p=strchr(Arg,'-'); if (p==Nil) return CMDErr;

     Save = (*p); *p = '\0'; 
     if ((StartAuto = AddressWildcard(Arg))) err = True;
     else StartAdr = ConstLongInt(Arg, &err);
     *p = Save;
     if (NOT err) return CMDErr;

     if ((StopAuto = AddressWildcard(p + 1))) err = True;
     else StopAdr = ConstLongInt(p + 1, &err);
     if (NOT err) return CMDErr;

     if ((NOT StartAuto) AND (NOT StopAuto) AND (StartAdr>StopAdr)) return CMDErr;

     return CMDArg;
    END
END

	static CMDResult CMD_RelAdr(Boolean Negate, char *Arg)
BEGIN
   RelAdr=(NOT Negate);
   return CMDOK;
END

       static CMDResult CMD_AdrRelocate(Boolean Negate, char *Arg)
BEGIN
   Boolean err;

   if (Negate)
    BEGIN
     Relocate = 0;
     return CMDOK;
    END
   else
    BEGIN
     Relocate = ConstLongInt(Arg,&err);
     if (NOT err) return CMDErr;

     return CMDArg;
    END
END
   
        static CMDResult CMD_Rec5(Boolean Negate, char *Arg)
BEGIN
   Rec5=(NOT Negate);
   return CMDOK;
END

        static CMDResult CMD_SepMoto(Boolean Negate, char *Arg)
BEGIN
   SepMoto=(NOT Negate);
   return CMDOK;
END

        static CMDResult CMD_IntelMode(Boolean Negate, char *Arg)
BEGIN
   int Mode;
   Boolean err;

   if (*Arg=='\0') return CMDErr;
   else
    BEGIN
     Mode=ConstLongInt(Arg,&err);
     if ((NOT err) OR (Mode<0) OR (Mode>2)) return CMDErr;
     else
      BEGIN
       if (NOT Negate) IntelMode=Mode;
       else if (IntelMode==Mode) IntelMode=0;
       return CMDArg;
      END
    END
END

	static CMDResult CMD_MultiMode(Boolean Negate, char *Arg)
BEGIN
   int Mode;
   Boolean err;

   if (*Arg=='\0') return CMDErr;
   else
    BEGIN
     Mode=ConstLongInt(Arg,&err);
     if ((NOT err) OR (Mode<0) OR (Mode>3)) return CMDErr;
     else
      BEGIN
       if (NOT Negate) MultiMode=Mode;
       else if (MultiMode==Mode) MultiMode=0;
       return CMDArg;
      END
    END
END

        static CMDResult CMD_DestFormat(Boolean Negate, char *Arg)
BEGIN
#define NameCnt 9

   static char *Names[NameCnt]={"DEFAULT","MOTO","INTEL","INTEL16","INTEL32","MOS","TEK","DSK","ATMEL"};
   static THexFormat Format[NameCnt]={Default,MotoS,IntHex,IntHex16,IntHex32,MOSHex,TekHex,TiDSK,Atmel};
   int z;

   for (z=0; z<strlen(Arg); z++) Arg[z]=toupper(Arg[z]);

   z=0;
   while ((z<NameCnt) AND (strcmp(Arg,Names[z])!=0)) z++;
   if (z>=NameCnt) return CMDErr;

   if (NOT Negate) DestFormat=Format[z];
   else if (DestFormat==Format[z]) DestFormat=Default;

   return CMDArg;
END

	static CMDResult CMD_DataAdrRange(Boolean Negate, char *Arg)
BEGIN
   char *p,Save;
   Boolean err;

   if (Negate)
    BEGIN
     StartData=0; StopData=0x1fff;
     return CMDOK;
    END
   else
    BEGIN
     p=strchr(Arg,'-'); if (p==Nil) return CMDErr;

     Save=(*p); *p='\0';
     StartData=ConstLongInt(Arg,&err);
     *p=Save;
     if (NOT err) return CMDErr;

     StopData=ConstLongInt(p+1,&err);
     if (NOT err) return CMDErr;

     if (StartData>StopData) return CMDErr;

     return CMDArg;
    END
END

	static CMDResult CMD_EntryAdr(Boolean Negate, char *Arg)
BEGIN
   Boolean err;

   if (Negate)
    BEGIN
     EntryAdrPresent=False;
     return CMDOK;
    END
   else
    BEGIN
     EntryAdr=ConstLongInt(Arg,&err);
     if ((NOT err) OR (EntryAdr>0xffff)) return CMDErr;
     return CMDArg;
    END
END

        static CMDResult CMD_LineLen(Boolean Negate, char *Arg)
BEGIN
   Boolean err;

   if (Negate)
    if (*Arg!='\0') return CMDErr;
    else
     BEGIN
      LineLen=16; return CMDOK;
     END
   else if (*Arg=='\0') return CMDErr;
   else
    BEGIN
     LineLen=ConstLongInt(Arg,&err);
     if ((NOT err) OR (LineLen<1) OR (LineLen>MaxLineLen)) return CMDErr;
     else
      BEGIN
       LineLen+=(LineLen&1); return CMDArg;
      END
    END
END

        static CMDResult CMD_MinMoto(Boolean Negate, char *Arg)
BEGIN
   Boolean err;

   if (Negate)
    if (*Arg != '\0') return CMDErr;
    else
     BEGIN
      MinMoto = 0; return CMDOK;
     END
   else if (*Arg == '\0') return CMDErr;
   else
    BEGIN
     MinMoto = ConstLongInt(Arg,&err);
     if ((NOT err) OR (MinMoto < 1) OR (MinMoto > 3)) return CMDErr;
     else return CMDArg;
    END
END

	static CMDResult CMD_AutoErase(Boolean Negate, char *Arg)
BEGIN
   AutoErase=NOT Negate;
   return CMDOK;
END

#define P2HEXParamCnt 14
static CMDRec P2HEXParams[P2HEXParamCnt]=
	       {{"f", CMD_FilterList},
		{"r", CMD_AdrRange},
                {"R", CMD_AdrRelocate},
		{"a", CMD_RelAdr},
		{"i", CMD_IntelMode},
		{"m", CMD_MultiMode},
		{"F", CMD_DestFormat},
		{"5", CMD_Rec5},
		{"s", CMD_SepMoto},
		{"d", CMD_DataAdrRange},
                {"e", CMD_EntryAdr},
                {"l", CMD_LineLen},
                {"k", CMD_AutoErase},
                {"M", CMD_MinMoto}};

static Word ChkSum;

	int main(int argc, char **argv)
BEGIN
   char *ph1,*ph2;
   String Ver;

   ParamCount=argc-1; ParamStr=argv;

   nls_init(); NLS_Initialize();

   hex_init();
   endian_init();
   bpemu_init();
   hex_init();
   chunks_init();
   cmdarg_init(*argv);
   toolutils_init(*argv);
   nlmessages_init("p2hex.msg",*argv,MsgId1,MsgId2); ioerrs_init(*argv);

   sprintf(Ver,"P2HEX/C V%s",Version);
   WrCopyRight(Ver);

   InitChunk(&UsedList);

   if (ParamCount==0)
    BEGIN
     errno=0; printf("%s%s%s\n",getmessage(Num_InfoMessHead1),GetEXEName(),getmessage(Num_InfoMessHead2)); ChkIO(OutName);
     for (ph1=getmessage(Num_InfoMessHelp),ph2=strchr(ph1,'\n'); ph2!=Nil; ph1=ph2+1,ph2=strchr(ph1,'\n'))
      BEGIN
       *ph2='\0';
       printf("%s\n",ph1);
       *ph2='\n';
      END
     exit(1);
    END

   StartAdr = 0; StopAdr = 0x7fff;
   StartAuto = False; StopAuto = False;
   StartData = 0; StopData = 0x1fff;
   EntryAdr = (-1); EntryAdrPresent = False; AutoErase = False;
   RelAdr = False; Rec5 = True; LineLen = 16;
   IntelMode = 0; MultiMode = 0; DestFormat = Default; MinMoto = 1;
   *TargName = '\0';
   Relocate = 0;
   ProcessCMD(P2HEXParams, P2HEXParamCnt, ParProcessed, "P2HEXCMD", ParamError);

   if (ProcessedEmpty(ParProcessed))
    BEGIN
     errno=0; printf("%s\n",getmessage(Num_ErrMsgTargMissing)); ChkIO(OutName);
     exit(1);
    END

   z=ParamCount;
   while ((z>0) AND (NOT ParProcessed[z])) z--;
   strmaxcpy(TargName,ParamStr[z],255);
   if (NOT RemoveOffset(TargName,&Dummy)) ParamError(False,ParamStr[z]);
   ParProcessed[z]=False;
   if (ProcessedEmpty(ParProcessed))
    BEGIN
     strmaxcpy(SrcName,ParamStr[z],255); DelSuffix(TargName);
    END
   AddSuffix(TargName,HexSuffix);

   if ((StartAuto) OR (StopAuto))
    BEGIN
#ifdef __STDC__
     if (StartAuto) StartAdr=0xffffffffu;
#else
     if (StartAuto) StartAdr=0xffffffff;
#endif
     if (StopAuto) StopAdr=0;
     if (ProcessedEmpty(ParProcessed)) ProcessGroup(SrcName,MeasureFile);
     else for (z=1; z<=ParamCount; z++)
      if (ParProcessed[z]) ProcessGroup(ParamStr[z],MeasureFile);
     if (StartAdr>StopAdr)
      BEGIN
       errno=0; printf("%s\n",getmessage(Num_ErrMsgAutoFailed)); ChkIO(OutName); exit(1);
      END
    END

   OpenTarget();
   MotoOccured=False; IntelOccured=False;
   MOSOccured=False;  DSKOccured=False;
   MaxMoto=0; MaxIntel=0;

   if (ProcessedEmpty(ParProcessed)) ProcessGroup(SrcName,ProcessFile);
   else for (z=1; z<=ParamCount; z++)
    if (ParProcessed[z]) ProcessGroup(ParamStr[z],ProcessFile);

   if ((MotoOccured) AND (NOT SepMoto))
    BEGIN
     errno=0; fprintf(TargFile,"S%c%s",'9'-MaxMoto,HexByte(3+MaxMoto)); ChkIO(TargName);
     ChkSum=3+MaxMoto;
     if (NOT EntryAdrPresent) EntryAdr=0;
     if (MaxMoto>=2)
      BEGIN
       errno=0; fprintf(TargFile,HexByte((EntryAdr>>24)&0xff)); ChkIO(TargName);
       ChkSum+=(EntryAdr>>24)&0xff;
      END
     if (MaxMoto>=1)
      BEGIN
       errno=0; fprintf(TargFile,HexByte((EntryAdr>>16)&0xff)); ChkIO(TargName);
       ChkSum+=(EntryAdr>>16)&0xff;
      END
     errno=0; fprintf(TargFile,"%s",HexWord(EntryAdr&0xffff)); ChkIO(TargName);
     ChkSum+=(EntryAdr>>8)&0xff;
     ChkSum+=EntryAdr&0xff;
     errno=0; fprintf(TargFile,"%s\n",HexByte(0xff-(ChkSum&0xff))); ChkIO(TargName);
    END

   if (IntelOccured)
    BEGIN
     if (EntryAdrPresent)
      BEGIN
       if (MaxIntel == 2)
        BEGIN
         errno = 0; fprintf(TargFile, ":04000005"); ChkIO(TargName); ChkSum = 4 + 5;
         errno = 0; fprintf(TargFile, "%s", HexLong(EntryAdr)); ChkIO(TargName);
         ChkSum+=((EntryAdr >> 24)& 0xff) +
                 ((EntryAdr >> 16)& 0xff) +
                 ((EntryAdr >> 8) & 0xff) +
                 ( EntryAdr       & 0xff);
        END
       else if (MaxIntel == 1)
        BEGIN
         Seg = (EntryAdr >> 4) & 0xffff;
         Ofs = EntryAdr & 0x000f;
         errno = 0; fprintf(TargFile, ":04%s03%s", HexWord(Ofs), HexWord(Seg));
         ChkIO(TargName); ChkSum = 4 + 3 + Lo(Seg) + Hi(Seg) + Ofs;
        END
       else
        BEGIN
         errno = 0; fprintf(TargFile, ":00%s03", HexWord(EntryAdr & 0xffff));
         ChkIO(TargName); ChkSum = 3 + Lo(EntryAdr) + Hi(EntryAdr);
        END
       errno = 0; fprintf(TargFile, "%s\n", HexByte(0x100 - ChkSum)); ChkIO(TargName);
      END
     errno=0;
     switch (IntelMode)
      BEGIN
       case 0: fprintf(TargFile,":00000001FF\n"); break;
       case 1: fprintf(TargFile,":00000001\n"); break;
       case 2: fprintf(TargFile,":0000000000\n"); break;
      END
     ChkIO(TargName);
    END

   if (MOSOccured)
    BEGIN
     errno=0; fprintf(TargFile,";0000040004\n"); ChkIO(TargName);
    END

   if (DSKOccured)
    BEGIN
     if (EntryAdrPresent)
      BEGIN
       errno=0;
       fprintf(TargFile,"1%s7%sF\n",HexWord(EntryAdr),HexWord(EntryAdr));
       ChkIO(TargName);
      END
     errno=0; fprintf(TargFile,":\n"); ChkIO(TargName);
    END

   CloseTarget();

   if (AutoErase)
    BEGIN
     if (ProcessedEmpty(ParProcessed)) ProcessGroup(SrcName,EraseFile);
     else for (z=1; z<=ParamCount; z++)
      if (ParProcessed[z]) ProcessGroup(ParamStr[z],EraseFile);
    END

   return 0;
END
