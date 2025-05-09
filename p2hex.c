/* p2hex.c */
/*****************************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only                     */
/*                                                                           */
/* AS-Portierung                                                             */
/*                                                                           */
/* Konvertierung von AS-P-Dateien nach Hex                                   */
/*                                                                           */
/*****************************************************************************/

#include "stdinc.h"
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "version.h"
#include "be_le.h"
#include "bpemu.h"
#include "nls.h"
#include "nlmessages.h"
#include "p2hex.rsc"
#ifdef _USE_MSH
# include "p2hex.msh"
#endif
#include "ioerrs.h"
#include "strutil.h"
#include "chunks.h"
#include "stringlists.h"
#include "msg_level.h"
#include "cmdarg.h"

#include "toolutils.h"
#include "headids.h"

static const char *HexSuffix = ".hex";
#define MaxLineLen 254
#define AVRLEN_DEFAULT 3
#define DefaultCFormat "dSEl"

typedef void (*ProcessProc)(
#ifdef __PROTOS__
const char *FileName, LongWord Offset
#endif
);

static FILE *TargFile;
static String TargName, CFormat;
static Byte ForceSegment;

static LongWord StartAdr[SegCount], StopAdr[SegCount], LineLen, EntryAdr;
static LargeInt Relocate;
static Boolean StartAuto, StopAuto, AutoErase, EntryAdrPresent;
static Word Seg, Ofs;
static LongWord Dummy;
static Byte IntelMode;
static Byte MultiMode;   /* 0=8M, 1=16, 2=8L, 3=8H */
static Byte MinMoto;
static Boolean Rec5;
static Boolean SepMoto;
static LongWord AVRLen;

static Boolean RelAdr;

static unsigned FormatOccured;
enum
{
  eMotoOccured = (1 << 0),
  eIntelOccured = (1 << 1),
  eMOSOccured = (1 << 2),
  eDSKOccured = (1 << 3),
  eMico8Occured = (1 << 4)
};
static Byte MaxMoto, MaxIntel;

static tHexFormat DestFormat;

static ChunkList UsedList;

static String CTargName;
static unsigned NumCBlocks;

static void DefStartStopAdr(LongWord SegMask)
{
  Byte Seg;

  for (Seg = 0; Seg < SegCount; Seg++)
    if (SegMask & (1 << Seg))
    {
      StartAdr[Seg] = 0;
      StopAdr[Seg] = (Seg == SegData) ? 0x1fff : 0x7fff;
    }
}

static void Filename2CName(char *pDest, const char *pSrc)
{
  char Trans;
  const char *pPos;
  Boolean Written = False;

  pPos = strrchr(pSrc, PATHSEP);
#ifdef DRSEP
  if (!pPos)
    pPos = strchr(pSrc, DRSEP);
#endif
  if (pPos)
    pSrc = pPos + 1;

  for (; *pSrc; pSrc++)
  {
    Trans = (*pSrc == '-') ? '_' : *pSrc;
    if (isalnum(*pSrc) || (*pSrc == '_'))
    {
      *pDest++ = Trans;
      Written = True;
    }
    else if (Written)
      break;
  }
  *pDest = '\0';
}

static void GetCBlockName(char *pDest, size_t DestSize, unsigned Num)
{
  if (Num > 0)
    as_snprintf(pDest, DestSize, "_%u", Num + 1);
  else
    *pDest = '\0';
}

static void ParamError(Boolean InEnv, char *Arg)
{
  fprintf(stderr, "%s%s\n", getmessage(InEnv ? Num_ErrMsgInvEnvParam : Num_ErrMsgInvParam), Arg);
  fprintf(stderr, "%s\n", getmessage(Num_ErrMsgProgTerm));
}

static void OpenTarget(void)
{
  TargFile = fopen(TargName, "w");
  if (!TargFile)
    ChkIO(TargName);
}

static void CloseTarget(void)
{
  if (EOF == fclose(TargFile))
    ChkIO(TargName);
  if (Magic != 0)
    unlink(TargName);
}

static void PrCData(FILE *pTargFile, char Ident, const char *pName,
                    const char *pCTargName, const char *pCBlockName, LongWord Value)
{
  const char *pFormat;

  if (strchr(CFormat, toupper(Ident)))
    pFormat = "ul";
  else if (strchr(CFormat, tolower(Ident)))
    pFormat = "u";
  else
    return;

  chkio_fprintf(pTargFile, TargName, "#define %s%s_%s 0x%08lX%s\n",
                pCTargName, pCBlockName, pName,
                LoDWord(Value), pFormat);
}

static void ProcessFile(const char *FileName, LongWord Offset)
{
  FILE *SrcFile;
  Word TestID;
  Byte InpHeader, InpCPU, InpSegment, InpGran;
  LongWord InpStart, SumLen;
  Word InpLen, TransLen;
  Boolean doit, FirstBank = 0;
  Boolean CDataLower = !!strchr(CFormat, 'd'),
          CDataUpper = !!strchr(CFormat, 'D');
  Byte Buffer[MaxLineLen];
  Word *WBuffer = (Word *) Buffer;
  LongWord ErgStart,
           ErgStop = 0xfffffffful,
           IntOffset = 0, MaxAdr;
  LongInt NextPos;
  LongWord ValidSegs;
  Word ErgLen = 0, ChkSum, RecCnt, Gran, HSeg;
  String CBlockName;

  LongInt z;

  Byte MotRecType = 0;

  tHexFormat ActFormat;
  const TFamilyDescr *FoundDscr;

  SrcFile = fopen(FileName, OPENRDMODE);
  if (!SrcFile) ChkIO(FileName);

  if (!Read2(SrcFile, &TestID))
    chk_wr_read_error(FileName);
  if (TestID !=FileMagic)
    FormatError(FileName, getmessage(Num_FormatInvHeaderMsg));

  if (msg_level >= e_msg_level_normal)
    chkio_printf(OutName, "%s==>>%s", FileName, TargName);

  SumLen = 0;

  do
  {
    ReadRecordHeader(&InpHeader, &InpCPU, &InpSegment, &InpGran, FileName, SrcFile);

    if (InpHeader == FileHeaderStartAdr)
    {
      if (!Read4(SrcFile, &ErgStart))
        chk_wr_read_error(FileName);
      if (!EntryAdrPresent)
      {
        EntryAdr = ErgStart;
        EntryAdrPresent = True;
      }
    }

    else if (InpHeader == FileHeaderDataRec)
    {
      Gran = InpGran;

      if ((ActFormat = DestFormat) == eHexFormatDefault)
      {
        FoundDscr = FindFamilyById(InpCPU);
        if (!FoundDscr)
          FormatError(FileName, getmessage(Num_FormatInvRecordHeaderMsg));
        else
          ActFormat = FoundDscr->HexFormat;
      }

      if (ForceSegment != SegNone)
        ValidSegs = (1 << ForceSegment);
      else
      {
        ValidSegs = (1 << SegCode);
        if (eHexFormatTiDSK == ActFormat)
          ValidSegs |= (1 << SegData);
      }

      switch (ActFormat)
      {
        case eHexFormatMotoS:
        case eHexFormatIntel32:
        case eHexFormatC:
          MaxAdr = 0xfffffffful;
          break;
        case eHexFormatIntel16:
          MaxAdr = 0xffff0ul + 0xffffu;
          break;
        case eHexFormatAtmel:
          MaxAdr = (1 << (AVRLen << 3)) - 1;
          break;
        case eHexFormatMico8:
          MaxAdr = 0xfffffffful;
          break;
        default:
          MaxAdr = 0xffffu;
      }

      if (!Read4(SrcFile, &InpStart))
        chk_wr_read_error(FileName);
      if (!Read2(SrcFile, &InpLen))
        chk_wr_read_error(FileName);

      NextPos = ftell(SrcFile) + InpLen;
      if (NextPos >= FileSize(SrcFile) - 1)
        FormatError(FileName, getmessage(Num_FormatInvRecordLenMsg));

      doit = FilterOK(InpCPU) && (ValidSegs & (1 << InpSegment));

      if (doit)
      {
        InpStart += Offset;
        ErgStart = max(StartAdr[InpSegment], InpStart);
        ErgStop = min(StopAdr[InpSegment], InpStart + (InpLen/Gran) - 1);
        doit = (ErgStop >= ErgStart);
        if (doit)
        {
          ErgLen = (ErgStop + 1 - ErgStart) * Gran;
          if (AddChunk(&UsedList, ErgStart, ErgStop - ErgStart + 1, True))
            chkio_fprintf(stderr, OutName, " %s\n", getmessage(Num_ErrMsgOverlap));
        }
      }

      if (doit && (ErgStop > MaxAdr))
        chkio_fprintf(stderr, OutName, " %s\n", getmessage(Num_ErrMsgAdrOverflow));

      if (doit)
      {
        /* an Anfang interessierender Daten */

        if (fseek(SrcFile, (ErgStart - InpStart) * Gran, SEEK_CUR) == -1)
          ChkIO(FileName);

        /* Statistik, Anzahl Datenzeilen ausrechnen */

        RecCnt = ErgLen / LineLen;
        if ((ErgLen % LineLen) !=0)
          RecCnt++;

        /* relative addresses? */

        if (RelAdr)
          ErgStart -= StartAdr[InpSegment];

        /* move to target address range */

        ErgStart += Relocate;

        /* head of group of data lines */

        switch (ActFormat)
        {
          case eHexFormatMotoS:
            if ((!(FormatOccured & eMotoOccured)) || (SepMoto))
              chkio_fprintf(TargFile, TargName, "S0030000FC\n");
            if ((ErgStop >> 24) != 0)
              MotRecType = 2;
            else if ((ErgStop >> 16) !=0)
              MotRecType = 1;
            else
              MotRecType = 0;
            if (MotRecType < (MinMoto - 1))
              MotRecType = (MinMoto - 1);
            if (MaxMoto < MotRecType)
              MaxMoto = MotRecType;
            if (Rec5)
            {
              ChkSum = Lo(RecCnt) + Hi(RecCnt) + 3;
              chkio_fprintf(TargFile, TargName, "S503%04X%02X\n", LoWord(RecCnt), Lo(ChkSum ^ 0xff));
            }
            FormatOccured |= eMotoOccured;
            break;
          case eHexFormatMOS:
            FormatOccured |= eMOSOccured;
            break;
          case eHexFormatIntel:
            FormatOccured |= eIntelOccured;
            IntOffset = 0;
            break;
          case eHexFormatIntel16:
            FormatOccured |= eIntelOccured;
            IntOffset = (ErgStart * Gran);
            IntOffset -= IntOffset & 0x0f;
            HSeg = IntOffset >> 4;
            ChkSum = 4 + Lo(HSeg) + Hi(HSeg);
            IntOffset /= Gran;
            chkio_fprintf(TargFile, TargName, ":02000002%04X%02X\n", LoWord(HSeg), Lo(0x100 - ChkSum));
            if (MaxIntel < 1)
              MaxIntel = 1;
            break;
          case eHexFormatIntel32:
            FormatOccured |= eIntelOccured;
            IntOffset = (ErgStart * Gran);
            IntOffset -= IntOffset & 0xffff;
            HSeg = IntOffset >> 16;
            ChkSum = 6 + Lo(HSeg) + Hi(HSeg);
            IntOffset /= Gran;
            chkio_fprintf(TargFile, TargName, ":02000004%04X%02X\n", LoWord(HSeg), Lo(0x100 - ChkSum));
            if (MaxIntel < 2)
              MaxIntel = 2;
            FirstBank = False;
            break;
          case eHexFormatTek:
            break;
          case eHexFormatAtmel:
            break;
          case eHexFormatMico8:
            break;
          case eHexFormatTiDSK:
            if (!(FormatOccured & eDSKOccured))
            {
              FormatOccured |= eDSKOccured;
              chkio_fprintf(TargFile, TargName, "%s%s\n", getmessage(Num_DSKHeaderLine), TargName);
            }
            break;
          case eHexFormatC:
            GetCBlockName(CBlockName, sizeof(CBlockName), NumCBlocks);
            PrCData(TargFile, 's', "start", CTargName, CBlockName, ErgStart);
            PrCData(TargFile, 'l', "len", CTargName, CBlockName, ErgLen);
            PrCData(TargFile, 'e', "end", CTargName, CBlockName, ErgStart + ErgLen - 1);
            if (CDataLower || CDataUpper)
              chkio_fprintf(TargFile, TargName, "static const unsigned char %s%s_data[] =\n{\n",
                            CTargName, CBlockName);
            break;
          default:
            break;
        }

        /* data lines themselves */

        while (ErgLen > 0)
        {
          /* next bank for Intel32? */

          if ((ActFormat == eHexFormatIntel32) && (FirstBank))
          {
            IntOffset += (0x10000 / Gran);
            HSeg = IntOffset >> 16;
            ChkSum = 6 + Lo(HSeg) + Hi(HSeg);
            chkio_fprintf(TargFile, TargName, ":02000004%04X%02X\n", LoWord(HSeg), Lo(0x100 - ChkSum));
            FirstBank = False;
          }

          /* Recordlaenge ausrechnen, fuer Intel32 auf 64K-Grenze begrenzen
             Bei Atmel nur 2 Byte pro Zeile!
             Bei Mico8 nur 4 Byte (davon ein Wort=18 Bit) pro Zeile! */

          TransLen = min(LineLen, ErgLen);
          if ((ActFormat == eHexFormatIntel32) && ((ErgStart & 0xffff) + (TransLen/Gran) >= 0x10000))
          {
            TransLen = Gran * (0x10000 - (ErgStart & 0xffff));
            FirstBank = True;
          }
          else if (ActFormat == eHexFormatAtmel)
            TransLen = min(2, TransLen);
          else if (ActFormat == eHexFormatMico8)
            TransLen = min(4, TransLen);

          /* start of data line */

          ChkSum = 0;
          switch (ActFormat)
          {
            case eHexFormatMotoS:
              chkio_fprintf(TargFile, TargName, "S%c%02X", '1' + MotRecType, Lo(TransLen + 3 + MotRecType));
              ChkSum += TransLen + 3 + MotRecType;
              if (MotRecType >= 2)
              {
                chkio_fprintf(TargFile, TargName, "%02X", Lo(ErgStart >> 24));
                ChkSum += ((ErgStart >> 24) & 0xff);
              }
              if (MotRecType >= 1)
              {
                chkio_fprintf(TargFile, TargName, "%02X", Lo(ErgStart >> 16));
                ChkSum += ((ErgStart >> 16) & 0xff);
              }
              chkio_fprintf(TargFile, TargName, "%04X", LoWord(ErgStart));
              ChkSum += Hi(ErgStart) + Lo(ErgStart);
              break;
            case eHexFormatMOS:
              chkio_fprintf(TargFile, TargName, ";%02X%04X", Lo(TransLen), LoWord(ErgStart));
              ChkSum += TransLen + Lo(ErgStart) + Hi(ErgStart);
              break;
            case eHexFormatIntel:
            case eHexFormatIntel16:
            case eHexFormatIntel32:
            {
              Word WrTransLen;
              LongWord WrErgStart;

              WrTransLen = (MultiMode < 2) ? TransLen : (TransLen / Gran);
              WrErgStart = (ErgStart - IntOffset) * ((MultiMode < 2) ? Gran : 1);
              chkio_fprintf(TargFile, TargName, ":%02X%04X00", Lo(WrTransLen), LoWord(WrErgStart));
              ChkSum += Lo(WrTransLen) + Hi(WrErgStart) + Lo(WrErgStart);

              break;
            }
            case eHexFormatTek:
              chkio_fprintf(TargFile, TargName, "/%04X%02X%02X", LoWord(ErgStart), Lo(TransLen),
                            Lo(Lo(ErgStart) + Hi(ErgStart) + TransLen));
              break;
            case eHexFormatTiDSK:
              chkio_fprintf(TargFile, TargName, "9%04X", LoWord(/*Gran**/ErgStart));
              break;
            case eHexFormatAtmel:
              for (z = (AVRLen - 1) << 3; z >= 0; z -= 8)
                chkio_fprintf(TargFile, TargName, "%02X", Lo(ErgStart >> z));
              if (EOF == fputc(':', TargFile))
                ChkIO(TargName);
              break;
            case eHexFormatMico8:
              break;
            case eHexFormatC:
              chkio_fprintf(TargFile, TargName, "  ");
              break;
            default:
              break;
          }

          /* data itself */

          if (fread(Buffer, 1, TransLen, SrcFile) !=TransLen)
            chk_wr_read_error(FileName);
          if (MultiMode == 1)
            switch (Gran)
            {
              case 4:
                DSwap(Buffer, TransLen);
                break;
              case 2:
                WSwap(Buffer, TransLen);
                break;
              case 1:
                break;
            }
          switch (ActFormat)
          {
            case eHexFormatTiDSK:
              if (HostBigEndian)
                WSwap(WBuffer, TransLen);
              for (z = 0; z < (TransLen / 2); z++)
              {
                if (((ErgStart + z >= StartAdr[SegData]) && (ErgStart + z <= StopAdr[SegData]))
                 || (InpSegment == SegData))
                  chkio_fprintf(TargFile, TargName, "M%04X", LoWord(WBuffer[z]));
                else
                  chkio_fprintf(TargFile, TargName, "B%04X", LoWord(WBuffer[z]));
                ChkSum += WBuffer[z];
                SumLen += Gran;
              }
              break;
            case eHexFormatAtmel:
              if (TransLen >= 2)
              {
                chkio_fprintf(TargFile, TargName, "%04X", LoWord(WBuffer[0]));
                SumLen += 2;
              }
              else if (TransLen >= 1)
              {
                chkio_fprintf(TargFile, TargName, "%04X", Lo(WBuffer[0]));
                SumLen++;
              }
              break;
            case eHexFormatMico8:
              if (TransLen >= 4)
              {
                chkio_fprintf(TargFile, TargName, "%01X%02X%02X", Buffer[1] & 0x0f, Buffer[2] % 0xff, Buffer[3] & 0xff);
                SumLen += 4;
              }
              break;
            case eHexFormatC:
              if (CDataLower || CDataUpper)
                for (z = 0; z < (LongInt)TransLen; z++)
                  if ((MultiMode < 2) || (z % Gran == MultiMode - 2))
                  {
                    chkio_fprintf(TargFile, TargName, CDataLower ? "0x%02x%s" : "0x%02X%s", (unsigned)Buffer[z],
                                  (ErgLen - z > 1) ? "," : "");
                    ChkSum += Buffer[z];
                    SumLen++;
                  }
              break;
            default:
              for (z = 0; z < (LongInt)TransLen; z++)
                if ((MultiMode < 2) || (z % Gran == MultiMode - 2))
                {
                  chkio_fprintf(TargFile, TargName, "%02X", Lo(Buffer[z]));
                  ChkSum += Buffer[z];
                  SumLen++;
                }
          }

          /* end of data line */

          switch (ActFormat)
          {
            case eHexFormatMotoS:
              chkio_fprintf(TargFile, TargName, "%02X\n", Lo(ChkSum ^ 0xff));
              break;
            case eHexFormatMOS:
              chkio_fprintf(TargFile, TargName, "%04X\n", LoWord(ChkSum));
              break;
            case eHexFormatIntel:
            case eHexFormatIntel16:
            case eHexFormatIntel32:
              chkio_fprintf(TargFile, TargName, "%02X\n", Lo(1 + (ChkSum ^ 0xff)));
              break;
            case eHexFormatTek:
              chkio_fprintf(TargFile, TargName, "%02X\n", Lo(ChkSum));
              break;
            case eHexFormatTiDSK:
              chkio_fprintf(TargFile, TargName, "7%04XF\n", LoWord(ChkSum));
              break;
            case eHexFormatAtmel:
            case eHexFormatMico8:
            case eHexFormatC:
              chkio_fprintf(TargFile, TargName, "\n");
              break;
            default:
              break;
          }

          /* Zaehler rauf */

          ErgLen -= TransLen;
          ErgStart += TransLen/Gran;
        }

        /* Ende der Datenzeilengruppe */

        switch (ActFormat)
        {
          case eHexFormatMotoS:
            if (SepMoto)
            {
              chkio_fprintf(TargFile, TargName, "S%c%02X", '9' - MotRecType, Lo(3 + MotRecType));
              for (z = 1; z <= 2 + MotRecType; z++)
                chkio_fprintf(TargFile, TargName, "%02X", 0);
              chkio_fprintf(TargFile, TargName, "%02X\n", Lo(0xff - 3 - MotRecType));
            }
            break;
          case eHexFormatMOS:
            break;
          case eHexFormatIntel:
          case eHexFormatIntel16:
          case eHexFormatIntel32:
            break;
          case eHexFormatTek:
            break;
          case eHexFormatTiDSK:
            break;
          case eHexFormatAtmel:
            break;
          case eHexFormatMico8:
            break;
          case eHexFormatC:
            if (CDataLower || CDataUpper)
            {
              chkio_fprintf(TargFile, TargName, "};\n\n");
              NumCBlocks++;
            }
            break;
          default:
            break;
        };
      }
      if (fseek(SrcFile, NextPos, SEEK_SET) == -1)
        ChkIO(FileName);
    }
    else
      SkipRecord(InpHeader, FileName, SrcFile);
  }
  while (InpHeader !=0);

  if (msg_level >= e_msg_level_normal)
  {
    chkio_printf(OutName, " (");
    chkio_printf(OutName, Integ32Format, SumLen);
    chkio_printf(OutName, " %s)\n", getmessage((SumLen == 1) ? Num_Byte : Num_Bytes));
  }
  if (!SumLen)
  {
    if (fputs(getmessage(Num_WarnEmptyFile), stderr) == EOF) ChkIO(OutName);
  }

  if (EOF == fclose(SrcFile))
    ChkIO(FileName);
}

static ProcessProc CurrProcessor;
static LongWord CurrOffset;

static void Callback(char *Name)
{
  CurrProcessor(Name, CurrOffset);
}

static void ProcessGroup(const char *GroupName_O, ProcessProc Processor)
{
  String Ext, GroupName;

  CurrProcessor = Processor;
  strmaxcpy(GroupName, GroupName_O, STRINGSIZE);
  strmaxcpy(Ext, GroupName, STRINGSIZE);
  if (!RemoveOffset(GroupName, &CurrOffset))
  {
    ParamError(False, Ext);
    exit(1);
  }
  AddSuffix(GroupName, STRINGSIZE, getmessage(Num_Suffix));

  if (!DirScan(GroupName, Callback))
    fprintf(stderr, "%s%s%s\n", getmessage(Num_ErrMsgNullMaskA), GroupName, getmessage(Num_ErrMsgNullMaskB));
}

static void MeasureFile(const char *FileName, LongWord Offset)
{
  FILE *f;
  Byte Header, InpCPU, InpSegment, Gran;
  Word Length, TestID;
  LongWord Adr, EndAdr;
  LongInt NextPos;
  LongWord ValidSegs;
  Boolean doit;

  f = fopen(FileName, OPENRDMODE);
  if (!f)
    ChkIO(FileName);

  if (!Read2(f, &TestID))
    chk_wr_read_error(FileName);
  if (TestID !=FileMagic)
    FormatError(FileName, getmessage(Num_FormatInvHeaderMsg));

  do
  {
    ReadRecordHeader(&Header, &InpCPU, &InpSegment, &Gran, FileName, f);

    if (Header == FileHeaderDataRec)
    {
      if (!Read4(f, &Adr))
        chk_wr_read_error(FileName);
      if (!Read2(f, &Length))
        chk_wr_read_error(FileName);
      NextPos = ftell(f) + Length;
      if (NextPos > FileSize(f))
        FormatError(FileName, getmessage(Num_FormatInvRecordLenMsg));

      if (ForceSegment != SegNone)
        ValidSegs = (1 << ForceSegment);
      else
        ValidSegs = (1 << SegCode) | (1 << SegData);

      doit = FilterOK(InpCPU) && (ValidSegs & (1 << InpSegment));

      if (doit)
      {
        Adr += Offset;
        EndAdr = Adr + (Length/Gran) - 1;
        if (StartAuto)
          if (StartAdr[InpSegment] > Adr)
            StartAdr[InpSegment] = Adr;
        if (StopAuto)
          if (EndAdr > StopAdr[InpSegment])
            StopAdr[InpSegment] = EndAdr;
      }

      fseek(f, NextPos, SEEK_SET);
    }
    else
     SkipRecord(Header, FileName, f);
  }
  while(Header !=0);

  fclose(f);
}

/* ------------------------------------------- */

static as_cmd_result_t CMD_AdrRange(Boolean Negate, const char *Arg)
{
  if (Negate)
  {
    DefStartStopAdr(1 << SegCode);
    return e_cmd_ok;
  }
  else
    return CMD_Range(&StartAdr[SegCode], &StopAdr[SegCode],
                     &StartAuto, &StopAuto, Arg);
}

static as_cmd_result_t CMD_RelAdr(Boolean Negate, const char *Arg)
{
  UNUSED(Arg);

  RelAdr = (!Negate);
  return e_cmd_ok;
}

static as_cmd_result_t CMD_AdrRelocate(Boolean Negate, const char *Arg)
{
  if (Negate)
  {
    Relocate = 0;
    return e_cmd_ok;
  }
  else
  {
    const char *p_end;

    Relocate = as_cmd_strtol(Arg, &p_end);
    return *p_end ? e_cmd_err : e_cmd_arg;
  }
}

static as_cmd_result_t CMD_Rec5(Boolean Negate, const char *Arg)
{
  UNUSED(Arg);

  Rec5 = (!Negate);
  return e_cmd_ok;
}

static as_cmd_result_t CMD_SepMoto(Boolean Negate, const char *Arg)
{
  UNUSED(Arg);

  SepMoto = !Negate;
  return e_cmd_ok;
}

static as_cmd_result_t CMD_IntelMode(Boolean Negate, const char *Arg)
{
  if (!*Arg)
    return e_cmd_err;
  else
  {
    const char *p_end;
    int Mode = as_cmd_strtol(Arg, &p_end);

    if (*p_end || (Mode < 0) || (Mode > 2))
      return e_cmd_err;
    else
    {
      if (!Negate)
        IntelMode = Mode;
      else if (IntelMode == Mode)
        IntelMode = 0;
      return e_cmd_arg;
    }
  }
}

static as_cmd_result_t CMD_MultiMode(Boolean Negate, const char *Arg)
{
  if (*Arg == '\0')
    return e_cmd_err;
  else
  {
    const char *p_end;
    int Mode = as_cmd_strtol(Arg, &p_end);

    if (*p_end || (Mode < 0) || (Mode > 3))
      return e_cmd_err;
    else
    {
      if (!Negate)
        MultiMode = Mode;
      else if (MultiMode == Mode)
        MultiMode = 0;
      return e_cmd_arg;
    }
  }
}

static as_cmd_result_t CMD_DestFormat(Boolean Negate, const char *pArg)
{
#define NameCnt (sizeof(Names) / sizeof(*Names))

  static const char *Names[] =
  {
    "DEFAULT", "MOTO", "INTEL", "INTEL16", "INTEL32", "MOS", "TEK", "DSK", "ATMEL", "MICO8", "C"
  };
  static tHexFormat Format[] =
  {
    eHexFormatDefault, eHexFormatMotoS, eHexFormatIntel, eHexFormatIntel16,
    eHexFormatIntel32, eHexFormatMOS, eHexFormatTek, eHexFormatTiDSK,
    eHexFormatAtmel, eHexFormatMico8, eHexFormatC
  };
  unsigned z;
  String Arg;

  strmaxcpy(Arg, pArg, STRINGSIZE);
  NLS_UpString(Arg);

  z = 0;
  while ((z < NameCnt) && (strcmp(Arg, Names[z])))
    z++;
  if (z >= NameCnt)
    return e_cmd_err;

  if (!Negate)
    DestFormat = Format[z];
  else if (DestFormat == Format[z])
    DestFormat = eHexFormatDefault;

  return e_cmd_arg;
}

static as_cmd_result_t CMD_ForceSegment(Boolean Negate,  const char *Arg)
{
  int z = addrspace_lookup(Arg);

  if (z >= SegCount)
    return e_cmd_err;

  if (!Negate)
    ForceSegment = z;
  else if (ForceSegment == z)
    ForceSegment = SegNone;

  return e_cmd_arg;
}

static as_cmd_result_t CMD_DataAdrRange(Boolean Negate,  const char *Arg)
{
  fputs(getmessage(Num_WarnDOption), stderr);
  fflush(stdout);

  if (Negate)
  {
    DefStartStopAdr(1 << SegData);
    return e_cmd_ok;
  }
  else
  {
    Boolean StartDataAuto, StopDataAuto;
    as_cmd_result_t Ret = CMD_Range(&StartAdr[SegData], &StopAdr[SegData],
                              &StartDataAuto, &StopDataAuto, Arg);

    if (StartDataAuto || StopDataAuto)
      Ret = e_cmd_err;
    return Ret;
  }
}

static as_cmd_result_t CMD_EntryAdr(Boolean Negate, const char *Arg)
{
  if (Negate)
  {
    EntryAdrPresent = False;
    return e_cmd_ok;
  }
  else if (!*Arg)
    return e_cmd_err;
  else
  {
    const char *p_end;
    EntryAdr = as_cmd_strtol(Arg, &p_end);
    if (*p_end || (EntryAdr > 0xffff))
      return e_cmd_err;
    EntryAdrPresent = True;
    return e_cmd_arg;
  }
}

static as_cmd_result_t CMD_LineLen(Boolean Negate, const char *Arg)
{
  if (Negate)
  {
    if (*Arg)
      return e_cmd_err;
    else
    {
      LineLen = 16;
      return e_cmd_ok;
    }
  }
  else if (!*Arg)
    return e_cmd_err;
  else
  {
    const char *p_end;
    LineLen = as_cmd_strtol(Arg, &p_end);
    if (*p_end || (LineLen < 1) || (LineLen > MaxLineLen))
      return e_cmd_err;
    else
    {
      LineLen += LineLen & 1;
      return e_cmd_arg;
    }
  }
}

static as_cmd_result_t CMD_MinMoto(Boolean Negate, const char *Arg)
{
  if (Negate)
  {
    if (*Arg)
      return e_cmd_err;
    else
    {
      MinMoto = 0;
      return e_cmd_ok;
    }
  }
  else if (!*Arg)
    return e_cmd_err;
  else
  {
    const char *p_end;
    MinMoto = as_cmd_strtol(Arg, &p_end);
    if (*p_end || (MinMoto < 1) || (MinMoto > 3))
      return e_cmd_err;
    else
      return e_cmd_arg;
  }
}

static as_cmd_result_t CMD_AutoErase(Boolean Negate, const char *Arg)
{
  UNUSED(Arg);

  AutoErase = !Negate;
  return e_cmd_ok;
}

static as_cmd_result_t CMD_AVRLen(Boolean Negate, const char *Arg)
{
  if (Negate)
  {
    AVRLen = AVRLEN_DEFAULT;
    return e_cmd_ok;
  }
  else
  {
    const char *p_end;
    Word Temp = as_cmd_strtol(Arg, &p_end);
    if (*p_end || (Temp < 2) || (Temp > 3))
      return e_cmd_err;
    else
    {
      AVRLen = Temp;
      return e_cmd_arg;
    }
  }
}

static as_cmd_result_t CMD_CFormat(Boolean Negate, const char *pArg)
{
  if (Negate)
  {
    strcpy(CFormat, DefaultCFormat);
    return e_cmd_ok;
  }
  else
  {
    int NumData = 0, NumStart = 0, NumLen = 0, NumEnd = 0;
    const char *pFormat;

    for (pFormat = pArg; *pFormat; pFormat++)
      switch (toupper(*pFormat))
      {
        case 'S': NumStart++; break;
        case 'D': NumData++; break;
        case 'L': NumLen++; break;
        case 'E': NumEnd++; break;
        default: return e_cmd_err;
      }
    if ((NumData > 1) || (NumStart > 1) || (NumLen > 1) || (NumEnd > 1))
      return e_cmd_err;
    strcpy(CFormat, pArg);
    return e_cmd_arg;
  }
}

static const as_cmd_rec_t P2HEXParams[] =
{
  { "f"        , CMD_FilterList },
  { "r"        , CMD_AdrRange },
  { "R"        , CMD_AdrRelocate },
  { "a"        , CMD_RelAdr },
  { "i"        , CMD_IntelMode },
  { "m"        , CMD_MultiMode },
  { "F"        , CMD_DestFormat },
  { "5"        , CMD_Rec5 },
  { "s"        , CMD_SepMoto },
  { "d"        , CMD_DataAdrRange },
  { "e"        , CMD_EntryAdr },
  { "l"        , CMD_LineLen },
  { "k"        , CMD_AutoErase },
  { "M"        , CMD_MinMoto },
  { "SEGMENT"  , CMD_ForceSegment },
  { "AVRLEN"   , CMD_AVRLen },
  { "CFORMAT"  , CMD_CFormat }
};

static Word ChkSum;

int main(int argc, char **argv)
{
  char *p_target_name;
  const char *p_src_name;
  as_cmd_results_t cmd_results;
  StringRecPtr p_src_run;

  nls_init();
  if (!NLS_Initialize(&argc, argv))
    exit(4);

  be_le_init();
  bpemu_init();
  chunks_init();
  as_cmdarg_init(*argv);
  msg_level_init();
  toolutils_init(*argv);
#ifdef _USE_MSH
  nlmessages_init_buffer(p2hex_msh_data, sizeof(p2hex_msh_data), MsgId1, MsgId2);
#else
  nlmessages_init_file("p2hex.msg", *argv, MsgId1, MsgId2);
#endif
  ioerrs_init(*argv);

  InitChunk(&UsedList);

  DefStartStopAdr(0xffff);
  StartAuto = True;
  StopAuto = True;
  EntryAdr = -1;
  EntryAdrPresent = False;
  AutoErase = False;
  RelAdr = False;
  Rec5 = True;
  LineLen = 16;
  AVRLen = AVRLEN_DEFAULT;
  IntelMode = 0;
  MultiMode = 0;
  DestFormat = eHexFormatDefault;
  MinMoto = 1;
  *TargName = '\0';
  Relocate = 0;
  ForceSegment = SegNone;
  strcpy(CFormat, DefaultCFormat);

  as_cmd_register(P2HEXParams, as_array_size(P2HEXParams));
  if (e_cmd_err == as_cmd_process(argc, argv, "P2HEXCMD", &cmd_results))
  {
    ParamError(cmd_results.error_arg_in_env, cmd_results.error_arg);
    exit(1);
  }

  if ((msg_level >= e_msg_level_verbose) || cmd_results.write_version_exit)
  {
    String Ver;

    as_snprintf(Ver, sizeof(Ver), "P2HEX V%s", Version);
    WrCopyRight(Ver);
  }

  if (cmd_results.write_help_exit)
  {
    char *ph1, *ph2;

    chkio_printf(OutName, "%s%s%s\n", getmessage(Num_InfoMessHead1), as_cmdarg_get_executable_name(), getmessage(Num_InfoMessHead2));
    for (ph1 = getmessage(Num_InfoMessHelp), ph2 = strchr(ph1, '\n'); ph2; ph1 = ph2 + 1, ph2 = strchr(ph1, '\n'))
    {
      *ph2 = '\0';
      chkio_printf(OutName, "%s\n", ph1);
      *ph2 = '\n';
    }
  }

  if (cmd_results.write_version_exit || cmd_results.write_help_exit)
    exit(0);

  if (StringListEmpty(cmd_results.file_arg_list))
  {
    fprintf(stderr, "%s: %s\n", as_cmdarg_get_executable_name(), getmessage(Num_ErrMessNoInputFiles));
    exit(1);
  }

  p_target_name = MoveAndCutStringListLast(&cmd_results.file_arg_list);
  if (!p_target_name || !*p_target_name)
  {
    chkio_fprintf(stderr, OutName, "%s\n", getmessage(Num_ErrMsgTargMissing));
    if (p_target_name) free(p_target_name);
    p_target_name = NULL;
    exit(1);
  }

  strmaxcpy(TargName, p_target_name, STRINGSIZE);
  if (!RemoveOffset(TargName, &Dummy))
  {
    strmaxcpy(TargName, p_target_name, STRINGSIZE);
    free(p_target_name); p_target_name = NULL;
    ParamError(False, TargName);
  }

  /* special case: only one argument <name> treated like <name>.p -> <name).hex */

  if (StringListEmpty(cmd_results.file_arg_list))
  {
    AddStringListLast(&cmd_results.file_arg_list, p_target_name);
    DelSuffix(TargName);
  }
  free(p_target_name); p_target_name = NULL;
  AddSuffix(TargName, STRINGSIZE, HexSuffix);
  Filename2CName(CTargName, TargName);
  NumCBlocks = 0;

  if (StartAuto || StopAuto)
  {
    int z;
    Byte ChkSegment = ForceSegment ? ForceSegment : (Byte)SegCode;

    if (StartAuto)
      for (z = 0; z < SegCount; z++)
        StartAdr[z] = 0xfffffffful;
    if (StopAuto)
      for (z = 0; z < SegCount; z++)
        StopAdr[z] = 0;

    for (p_src_name = GetStringListFirst(cmd_results.file_arg_list, &p_src_run);
         p_src_name; p_src_name = GetStringListNext(&p_src_run))
      if (*p_src_name)
        ProcessGroup(p_src_name, MeasureFile);

    if (StartAdr[ChkSegment] > StopAdr[ChkSegment])
    {
      chkio_fprintf(stderr, OutName, "%s\n", getmessage(Num_ErrMsgAutoFailed));
      exit(1);
    }
    if (msg_level >= e_msg_level_normal)
    {
      printf("%s: 0x%08lX-", getmessage(Num_InfoMessDeducedRange), LoDWord(StartAdr[ChkSegment]));
      printf("0x%08lX\n", LoDWord(StopAdr[ChkSegment]));
    }
  }

  OpenTarget();
  FormatOccured = 0;
  MaxMoto = 0;
  MaxIntel = 0;

  if (DestFormat == eHexFormatC)
  {
    chkio_fprintf(TargFile, TargName, "#ifndef _%s_H\n#define _%s_H\n\n", CTargName, CTargName);
    NumCBlocks = 0;
  }

  for (p_src_name = GetStringListFirst(cmd_results.file_arg_list, &p_src_run);
       p_src_name; p_src_name = GetStringListNext(&p_src_run))
    if (*p_src_name)
      ProcessGroup(p_src_name, ProcessFile);

  if ((FormatOccured & eMotoOccured) && (!SepMoto))
  {
    chkio_fprintf(TargFile, TargName, "S%c%02X", '9' - MaxMoto, Lo(3 + MaxMoto));
    ChkSum = 3 + MaxMoto;
    if (!EntryAdrPresent)
      EntryAdr = 0;
    if (MaxMoto >= 2)
    {
      chkio_fprintf(TargFile, TargName, "%02X", Lo(EntryAdr >> 24));
      ChkSum += (EntryAdr >> 24) & 0xff;
    }
    if (MaxMoto >= 1)
    {
      chkio_fprintf(TargFile, TargName, "%02X", Lo(EntryAdr >> 16));
      ChkSum += (EntryAdr >> 16) & 0xff;
    }
    chkio_fprintf(TargFile, TargName, "%04X", LoWord(EntryAdr & 0xffff));
    ChkSum += (EntryAdr >> 8) & 0xff;
    ChkSum += EntryAdr & 0xff;
    chkio_fprintf(TargFile, TargName, "%02X\n", Lo(0xff - (ChkSum & 0xff)));
  }

  if (FormatOccured & eIntelOccured)
  {
    Word EndRecAddr = 0;

    if (EntryAdrPresent)
    {
      switch (MaxIntel)
      {
        case 2:
          chkio_fprintf(TargFile, TargName, ":04000005");
          ChkSum = 4 + 5;
          chkio_fprintf(TargFile, TargName, "%08lX", LoDWord(EntryAdr));
          ChkSum += ((EntryAdr >> 24) & 0xff) +
                    ((EntryAdr >> 16) & 0xff) +
                    ((EntryAdr >>  8) & 0xff) +
                    ( EntryAdr        & 0xff);
          goto WrChkSum;

        case 1:
          Seg = (EntryAdr >> 4) & 0xffff;
          Ofs = EntryAdr & 0x000f;
          chkio_fprintf(TargFile, TargName, ":04000003%04X%04X", LoWord(Seg), LoWord(Ofs));
          ChkSum = 4 + 3 + Lo(Seg) + Hi(Seg) + Ofs;
          goto WrChkSum;

        default: /* == 0 */
          EndRecAddr = EntryAdr & 0xffff;
          break;

        WrChkSum:
          chkio_fprintf(TargFile, TargName, "%02X\n", Lo(0x100 - ChkSum));
      }
    }
    errno = 0;
    switch (IntelMode)
    {
      case 0:
      {
        ChkSum = 1 + Hi(EndRecAddr) + Lo(EndRecAddr);
        chkio_fprintf(TargFile, TargName, ":00%04X01%02X\n", LoWord(EndRecAddr), Lo(0x100 - ChkSum));
        break;
      }
      case 1:
        chkio_fprintf(TargFile, TargName, ":00000001\n");
        break;
      case 2:
        chkio_fprintf(TargFile, TargName, ":0000000000\n");
        break;
    }
  }

  if (FormatOccured & eMOSOccured)
    chkio_fprintf(TargFile, TargName, ";0000040004\n");

  if (FormatOccured & eDSKOccured)
  {
    if (EntryAdrPresent)
      chkio_fprintf(TargFile, TargName, "1%04X7%04XF\n", LoWord(EntryAdr), LoWord(EntryAdr));
    chkio_fprintf(TargFile, TargName, ":\n");
  }

  if (DestFormat == eHexFormatC)
  {
    unsigned ThisCBlock;
    String CBlockName;
    const char *pFormat;

    errno = 0;
    fprintf(TargFile,
            "typedef struct\n"
            "{\n");
    for (pFormat = CFormat; *pFormat; pFormat++)
      switch (*pFormat)
      {
        case 'd':
        case 'D':
          chkio_fprintf(TargFile, TargName, "  const char *data;\n"); break;
        case 's':
          chkio_fprintf(TargFile, TargName, "  unsigned start;\n"); break;
        case 'S':
          chkio_fprintf(TargFile, TargName, "  unsigned long start;\n"); break;
        case 'l':
          chkio_fprintf(TargFile, TargName, "  unsigned len;\n"); break;
        case 'L':
          chkio_fprintf(TargFile, TargName, "  unsigned long len;\n"); break;
        case 'e':
          chkio_fprintf(TargFile, TargName, "  unsigned end;\n"); break;
        case 'E':
          chkio_fprintf(TargFile, TargName, "  unsigned long end;\n"); break;
        default:
          break;
      }
    fprintf(TargFile,
            "} %s_blk;\n"
            "static const %s_blk %s_blks[] =\n"
            "{\n",
            CTargName, CTargName, CTargName);
    for (ThisCBlock = 0; ThisCBlock < NumCBlocks; ThisCBlock++)
    {
      GetCBlockName(CBlockName, sizeof(CBlockName), ThisCBlock);
      chkio_fprintf(TargFile, TargName, "  {");
      for (pFormat = CFormat; *pFormat; pFormat++)
      {
        chkio_fprintf(TargFile, TargName, (pFormat != CFormat) ? ", " : " ");
        switch (toupper(*pFormat))
        {
          case 'D': chkio_fprintf(TargFile, TargName, "%s%s_data", CTargName, CBlockName); break;
          case 'S': chkio_fprintf(TargFile, TargName, "%s%s_start", CTargName, CBlockName); break;
          case 'L': chkio_fprintf(TargFile, TargName, "%s%s_len", CTargName, CBlockName); break;
          case 'E': chkio_fprintf(TargFile, TargName, "%s%s_end", CTargName, CBlockName); break;
          default: break;
        }
      }
      chkio_fprintf(TargFile, TargName, " },\n");
    }
    chkio_fprintf(TargFile, TargName, "  {");
    for (pFormat = CFormat; *pFormat; pFormat++)
      chkio_fprintf(TargFile, TargName, (pFormat != CFormat) ? ", 0" : " 0");
    chkio_fprintf(TargFile, TargName, " }\n"
                      "};\n\n");

    if (EntryAdrPresent)
      chkio_fprintf(TargFile, TargName, "#define %s_entry 0x%08lXul\n\n",
                    CTargName, LoDWord(EntryAdr));
    chkio_fprintf(TargFile, TargName,
                  "#endif /* _%s_H */\n",
                  CTargName);
  }
  CloseTarget();

  if (AutoErase)
  {
    for (p_src_name = GetStringListFirst(cmd_results.file_arg_list, &p_src_run);
         p_src_name; p_src_name = GetStringListNext(&p_src_run))
      if (*p_src_name)
        ProcessGroup(p_src_name, EraseFile);
  }

  ClearStringList(&cmd_results.file_arg_list);

  return 0;
}
