#ifndef _TOOLUTILS_H
#define _TOOLUTILS_H
/* toolutils.c */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Unterroutinen fuer die AS-Tools                                           */
/*                                                                           */
/* Historie: 31. 5.1996 Grundsteinlegung                                     */
/*           30. 5.1999 Adresswildcard-Funktion                              */
/*           22. 1.2000 Funktion zum Lesen von RelocInfos                    */
/*           26. 6.2000 added exports                                        */
/*            4. 7.2000 ReadRecordHeader transports record type              */
/*                                                                           */
/*****************************************************************************/

#include "fileformat.h"

typedef struct
         {
           LargeInt Addr;
           LongInt Type;
           char *Name;
         } TRelocEntry, *PRelocEntry;

typedef struct
         {
           char *Name;
           LargeInt Value;
           LongInt Flags;
         } TExportEntry, *PExportEntry;

typedef struct
         {
          LongInt RelocCount, ExportCount;
          PRelocEntry RelocEntries;
          PExportEntry ExportEntries;
          char *Strings;
         } TRelocInfo, *PRelocInfo;

extern Word FileID;

extern char *OutName;


extern void WrCopyRight(char *Msg);

extern void DelSuffix(char *Name);

extern void AddSuffix(char *Name, char *Suff);

extern void FormatError(char *Name, char *Detail);

extern void ChkIO(char *Name);

extern Word Granularity(Byte Header);

extern void ReadRecordHeader(Byte *Header, Byte *Target, Byte* Segment,
                             Byte *Gran, char *Name, FILE *f);

extern void WriteRecordHeader(Byte *Header, Byte *Target, Byte* Segment,
                              Byte *Gran, char *Name, FILE *f);

extern void SkipRecord(Byte Header, char *Name, FILE *f);

extern PRelocInfo ReadRelocInfo(FILE *f);

extern void DestroyRelocInfo(PRelocInfo PInfo);

extern CMDResult CMD_FilterList(Boolean Negate, char *Arg);

extern Boolean FilterOK(Byte Header);

extern Boolean RemoveOffset(char *Name, LongWord *Offset);


extern void EraseFile(char *FileName, LongWord Offset);


extern Boolean AddressWildcard(char *addr);


extern void toolutils_init(char *ProgPath);

#include "asmerr.h"

#endif /* _TOOLUTILS_H */
