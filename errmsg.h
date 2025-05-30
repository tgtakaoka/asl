#ifndef _ERRMSG_H
#define _ERRMSG_H
/* errmsg.h */
/*****************************************************************************/
/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only                     */
/*                                                                           */
/* Cross Assembler                                                           */
/*                                                                           */
/* Error message definition & associated checking                            */
/*                                                                           */
/*****************************************************************************/

#include "cpulist.h"
#include "symflags.h"
#include "datatypes.h"

typedef enum
{
  ErrNum_None = 0,
  ErrNum_UselessDisp = 5,
  ErrNum_ShortAddrPossible = 10,
  ErrNum_ShortJumpPossible = 20,
  ErrNum_RelJumpPossible = 25,
  ErrNum_NoShareFile = 30,
  ErrNum_BigDecFloat = 40,
  ErrNum_PrivOrder = 50,
  ErrNum_DistNull = 60,
  ErrNum_WrongSegment = 70,
  ErrNum_InAccSegment = 75,
  ErrNum_PhaseErr = 80,
  ErrNum_Overlap = 90,
  ErrNum_OverlapReg = 95,
  ErrNum_NoCaseHit = 100,
  ErrNum_InAccPage = 110,
  ErrNum_RMustBeEven = 120,
  ErrNum_Obsolete = 130,
  ErrNum_Unpredictable = 140,
  ErrNum_AlphaNoSense = 150,
  ErrNum_Senseless = 160,
  ErrNum_RepassUnknown = 170,
  ErrNum_AddrNotAligned = 180,
  ErrNum_IOAddrNotAllowed = 190,
  ErrNum_Pipeline = 200,
  ErrNum_DoubleAdrRegUse = 210,
  ErrNum_NotBitAddressable = 220,
  ErrNum_StackNotEmpty = 230,
  ErrNum_NULCharacter = 240,
  ErrNum_PageCrossing = 250,
  ErrNum_WUnderRange = 255,
  ErrNum_WOverRange = 260,
  ErrNum_NegDUP = 270,
  ErrNum_ConvIndX = 280,
  ErrNum_NullResMem = 290,
  ErrNum_BitNumberTruncated = 300,
  ErrNum_InvRegisterPointer = 310,
  ErrNum_MacArgRedef = 320,
  ErrNum_Deprecated = 330,
  ErrNum_SrcLEThanDest = 340,
  ErrNum_TrapValidInstruction = 350,
  ErrNum_PaddingAdded = 360,
  ErrNum_RegNumWraparound = 370,
  ErrNum_IndexedForIndirect = 380,
  ErrNum_NotInNormalMode = 390,
  ErrNum_NotInPanelMode = 400,
  ErrNum_ArgOutOfRange = 410,
  ErrNum_TrySkipMultiwordInstruction = 420,
  ErrNum_SignExtension = 430,
  ErrNum_MeansE = 440,
  ErrNum_NeedShortIO = 450,
  ErrNum_CaseWrongArgCnt = 460,
  ErrNum_ReplacedByNOP = 470,
  ErrNum_TreatedAsVector = 480,
  ErrNum_LargeIntAsFloat = 490,
  ErrNum_CodeNotInCodeSegment = 500,
  ErrNum_DoubleDef = 1000,
  ErrNum_SymbolUndef = 1010,
  ErrNum_InvSymName = 1020,
  ErrNum_RsvdSymName = 1030,
  ErrNum_InvFormat = 1090,
  ErrNum_UseLessAttr = 1100,
  ErrNum_TooLongAttr = 1105,
  ErrNum_UndefAttr = 1107,
  ErrNum_WrongArgCnt = 1110,
  ErrNum_CannotSplitArg = 1112,
  ErrNum_WrongOptCnt = 1115,
  ErrNum_OnlyImmAddr = 1120,
  ErrNum_InvOpSize = 1130,
  ErrNum_ConfOpSizes = 1131,
  ErrNum_UndefOpSizes = 1132,
  ErrNum_StringOrIntButFloat = 1133,
  ErrNum_IntButFloat = 1134,
  /* ErrNum_InvOpType = 1135, */
  ErrNum_FloatButString = 1136,
  ErrNum_OpTypeMismatch = 1137,
  ErrNum_StringButInt = 1138,
  ErrNum_StringButFloat = 1139,
  ErrNum_TooManyArgs = 1140,
  ErrNum_IntButString = 1141,
  ErrNum_IntOrFloatButString = 1142,
  ErrNum_ExpectString = 1143,
  ErrNum_ExpectInt = 1144,
  ErrNum_StringOrIntOrFloatButReg = 1145,
  ErrNum_ExpectIntOrString = 1146,
  ErrNum_ExpectReg = 1147,
  ErrNum_RegWrongTarget = 1148,
  ErrNum_FloatButInt = 1149,
  ErrNum_NoRelocs = 1150,
  ErrNum_IntOrFloatButReg = 1151,
  ErrNum_IntOrStringButReg = 1152,
  ErrNum_IntButReg = 1153,
  ErrNum_StringTooLong = 1154,
  ErrNum_UnresRelocs = 1155,
  ErrNum_Unexportable = 1156,
  ErrNum_UnknownInstruction = 1200,
  ErrNum_BrackErr = 1300,
  ErrNum_DivByZero = 1310,
  ErrNum_UnderRange = 1315,
  ErrNum_OverRange = 1320,
  ErrNum_NotPwr2 = 1322,
  ErrNum_InvalidDecDigit = 1323,
  ErrNum_DecStringTooLong = 1324,
  ErrNum_NotAligned = 1325,
  ErrNum_DistTooBig = 1330,
  ErrNum_TargOnDiffPage = 1331,
  ErrNum_InAccReg = 1335,
  ErrNum_NoShortAddr = 1340,
  ErrNum_InvAddrMode = 1350,
  ErrNum_AddrMustBeEven = 1351,
  ErrNum_AddrMustBeAligned = 1352,
  ErrNum_InvParAddrMode = 1355,
  ErrNum_UndefCond = 1360,
  ErrNum_IncompCond = 1365,
  ErrNum_UnknownFlag = 1366,
  ErrNum_DuplicateFlag = 1367,
  ErrNum_UnknownInt = 1368,
  ErrNum_DuplicateInt = 1369,
  ErrNum_JmpDistTooBig = 1370,
  ErrNum_JmpDistIsZero = 1371,
  ErrNum_DistIsOdd = 1375,
  ErrNum_SkipTargetMismatch = 1376,
  ErrNum_InvShiftArg = 1380,
  ErrNum_Range18 = 1390,
  ErrNum_Only1 = 1391,
  ErrNum_ShiftCntTooBig = 1400,
  ErrNum_InvRegList = 1410,
  ErrNum_InvCmpMode = 1420,
  ErrNum_InvCPUType = 1430,
  ErrNum_InvFPUType = 1431,
  ErrNum_InvPMMUType = 1432,
  ErrNum_InvCtrlReg = 1440,
  ErrNum_InvPMMUReg = 1444,
  ErrNum_InvReg = 1445,
  ErrNum_DoubleReg = 1446,
  ErrNum_RegBankMismatch = 1447,
  ErrNum_UndefRegSize = 1448,
  ErrNum_InvOpOnReg = 1449,
  ErrNum_NoSaveFrame = 1450,
  ErrNum_NoRestoreFrame = 1460,
  ErrNum_UnknownMacArg = 1465,
  ErrNum_MissEndif = 1470,
  ErrNum_InvIfConst = 1480,
  ErrNum_DoubleSection = 1483,
  ErrNum_InvSection = 1484,
  ErrNum_MissingEndSect = 1485,
  ErrNum_WrongEndSect = 1486,
  ErrNum_NotInSection = 1487,
  ErrNum_UndefdForward = 1488,
  ErrNum_ContForward = 1489,
  ErrNum_InvFuncArgCnt = 1490,
  ErrNum_DupFuncArgName = 1491,
  ErrNum_MsgMissingLTORG = 1495,
  ErrNum_InstructionNotSupported = 1500,
  ErrNum_FPUNotEnabled = 1501,
  ErrNum_PMMUNotEnabled = 1502,
  ErrNum_FullPMMUNotEnabled = 1503,
  ErrNum_Z80SyntaxNotEnabled = 1504,
  ErrNum_AddrModeNotSupported = 1505,
  ErrNum_Z80SyntaxExclusive = 1506,
  ErrNum_FPUInstructionNotSupported = 1507,
  ErrNum_CustomNotEnabled = 1508,
  ErrNum_InvBitPos = 1510,
  ErrNum_OnlyOnOff = 1520,
  ErrNum_StackEmpty = 1530,
  ErrNum_NotOneBit = 1540,
  ErrNum_MissingStruct = 1550,
  ErrNum_OpenStruct = 1551,
  ErrNum_WrongStruct = 1552,
  ErrNum_PhaseDisallowed = 1553,
  ErrNum_InvStructDir = 1554,
  ErrNum_DoubleStruct = 1555,
  ErrNum_UnresolvedStructRef = 1556,
  ErrNum_DuplicateStructElem = 1557,
  ErrNum_NotRepeatable = 1560,
  ErrNum_ShortRead = 1600,
  ErrNum_UnknownCodepage = 1610,
  ErrNum_RomOffs063 = 1700,
  ErrNum_InvFCode = 1710,
  ErrNum_InvFMask = 1720,
  ErrNum_InvMMUReg = 1730,
  ErrNum_Level07 = 1740,
  ErrNum_InvBitMask = 1750,
  ErrNum_InvRegPair = 1760,
  ErrNum_OpenMacro = 1800,
  ErrNum_OpenIRP = 1801,
  ErrNum_OpenIRPC = 1802,
  ErrNum_OpenREPT = 1803,
  ErrNum_OpenWHILE = 1804,
  ErrNum_EXITMOutsideMacro = 1805,
  ErrNum_TooManyMacParams = 1810,
  ErrNum_UndefKeyArg = 1811,
  ErrNum_NoPosArg = 1812,
  ErrNum_DoubleMacro = 1815,
  ErrNum_FirstPassCalc = 1820,
  ErrNum_TooManyNestedIfs = 1830,
  ErrNum_MissingIf = 1840,
  ErrNum_RekMacro = 1850,
  ErrNum_UnknownFunc = 1860,
  ErrNum_InvFuncArg = 1870,
  ErrNum_FloatOverflow = 1880,
  ErrNum_InvArgPair = 1890,
  ErrNum_NotOnThisAddress = 1900,
  ErrNum_NotFromThisAddress = 1905,
  ErrNum_JmpTargOnDiffPage = 1910,
  ErrNum_TargOnDiffSection = 1911,
  ErrNum_CodeOverflow = 1920,
  ErrNum_AdrOverflow = 1925,
  ErrNum_MixDBDS = 1930,
  ErrNum_NotInStruct = 1940,
  ErrNum_ParNotPossible = 1950,
  ErrNum_InvSegment = 1960,
  ErrNum_UnknownSegment = 1961,
  ErrNum_UnknownSegReg = 1962,
  ErrNum_InvString = 1970,
  ErrNum_InvRegName = 1980,
  ErrNum_InvArg = 1985,
  ErrNum_NoIndir = 1990,
  ErrNum_NotInThisSegment = 1995,
  ErrNum_NotInMaxmode = 1996,
  ErrNum_OnlyInMaxmode = 1997,
  ErrNum_PackCrossBoundary = 2000,
  ErrNum_UnitMultipleUsed = 2001,
  ErrNum_MultipleLongRead = 2002,
  ErrNum_MultipleLongWrite = 2003,
  ErrNum_LongReadWithStore = 2004,
  ErrNum_TooManyRegisterReads = 2005,
  ErrNum_OverlapDests = 2006,
  ErrNum_TooManyBranchesInExPacket = 2008,
  ErrNum_CannotUseUnit = 2009,
  ErrNum_InvEscSequence = 2010,
  ErrNum_InvPrefixCombination = 2020,
  ErrNum_ConstantRedefinedAsVariable = 2030,
  ErrNum_VariableRedefinedAsConstant = 2035,
  ErrNum_StructNameMissing = 2040,
  ErrNum_EmptyArgument = 2050,
  ErrNum_Unimplemented = 2060,
  ErrNum_FreestandingUnnamedStruct = 2070,
  ErrNum_STRUCTEndedByENDUNION = 2080,
  ErrNum_AddrOnDifferentPage = 2090,
  ErrNum_UnknownMacExpMod = 2100,
  ErrNum_TooManyMacExpMod = 2105,
  ErrNum_ConflictingMacExpMod = 2110,
  ErrNum_InvalidPrepDir = 2120,
  ErrNum_ExpectedError = 2130,
  ErrNum_NoNestExpect = 2140,
  ErrNum_MissingENDEXPECT = 2150,
  ErrNum_MissingEXPECT = 2160,
  ErrNum_NoDefCkptReg = 2170,
  ErrNum_InvBitField = 2180,
  ErrNum_ArgValueMissing = 2190,
  ErrNum_UnknownArg = 2200,
  ErrNum_IndexRegMustBe16Bit = 2210,
  ErrNum_IOAddrRegMustBe16Bit = 2211,
  ErrNum_SegAddrRegMustBe32Bit = 2212,
  ErrNum_NonSegAddrRegMustBe16Bit = 2213,
  ErrNum_InvStructArgument = 2220,
  ErrNum_TooManyArrayDimensions = 2221,
  ErrNum_InvIntFormat = 2230,
  ErrNum_InvIntFormatList = 2231,
  ErrNum_InvScale = 2240,
  ErrNum_ConfStringOpt = 2250,
  ErrNum_UnknownStringOpt = 2251,
  ErrNum_InvCacheInvMode = 2252,
  ErrNum_InvCfgList = 2253,
  ErrNum_ConfBitBltOpt = 2254,
  ErrNum_UnknownBitBltOpt = 2255,
  ErrNum_InvCBAR = 2260,
  ErrNum_InAccPageErr = 2270,
  ErrNum_InAccFieldErr = 2280,
  ErrNum_TargInDiffField = 2281,
  ErrNum_InvCombination = 2290,
  ErrNum_UnmappedChar = 2300,
  ErrNum_MultiCharInvLength = 2310,
  ErrNum_NoTarget = 2320,
  ErrNum_InvDispLen = 2330,
  ErrNum_UserError = 9990,
  ErrNum_InternalError = 10000,
  ErrNum_OpeningFile = 10001,
  ErrNum_ListWrError = 10002,
  ErrNum_FileReadError = 10003,
  ErrNum_FileWriteError = 10004,
  ErrNum_HeapOvfl = 10006,
  ErrNum_StackOvfl = 10007,
  ErrNum_MaxIncLevelExceeded = 10008,
  ErrNum_InvListHeadFormat = 10010,
  ErrNum_ListHeadFormatElemTooOften = 10011
} tErrorNum;

struct sLineComp;
struct sStrComp;

extern Boolean ChkRangePos(LargeInt Value, LargeInt Min, LargeInt Max, const struct sStrComp *p_comp);
#define ChkRange(Value, Min, Max) ChkRangePos(Value, Min, Max, NULL)
extern Boolean ChkRangeWarnPos(LargeInt Value, LargeInt Min, LargeInt Max, const struct sStrComp *p_comp);
#define ChkRangeWarn(Value, Min, Max) ChkRangeWarnPos(Value, Min, Max, NULL)

extern Boolean ChkArgCntExtPos(int ThisCnt, int MinCnt, int MaxCnt, const struct sLineComp *pComp);
#define ChkArgCnt(MinCnt, MaxCnt) ChkArgCntExtPos(ArgCnt, MinCnt, MaxCnt, NULL)
#define ChkArgCntExt(ThisCnt, MinCnt, MaxCnt) ChkArgCntExtPos(ThisCnt, MinCnt, MaxCnt, NULL)
extern Boolean ChkArgCntExtEitherOr(int ThisCnt, int EitherCnt, int OrCnt);

extern Boolean ChkMinCPUExt(CPUVar MinCPU, tErrorNum ErrorNum);
#define ChkMinCPU(MinCPU) ChkMinCPUExt(MinCPU, ErrNum_InstructionNotSupported)

extern Boolean AChkMinCPUExtPos(CPUVar MinCPU, tErrorNum ErrorNum, const struct sStrComp *pComp);
#define AChkMinCPUPos(MinCPU, pComp) AChkMinCPUExtPos(MinCPU, ErrNum_AddrModeNotSupported, pComp)

extern Boolean ChkMaxCPUExt(CPUVar MaxCPU, tErrorNum ErrorNum);
#define ChkMaxCPU(MaxCPU) ChkMaxCPUExt(MaxCPU, ErrNum_InstructionNotSupported)

extern Boolean ChkExactCPUExt(CPUVar CheckCPU, tErrorNum ErrorNum);
#define ChkExactCPU(CheckCPU) ChkExactCPUExt(CheckCPU, ErrNum_InstructionNotSupported)

extern Boolean ChkRangeCPUExt(CPUVar MinCPU, CPUVar MaxCPU, tErrorNum ErrorNum);
#define ChkRangeCPU(MinCPU, MaxCPU) ChkRangeCPUExt(MinCPU, MaxCPU, ErrNum_InstructionNotSupported)

extern Boolean ChkExcludeCPUExt(CPUVar CheckCPU, tErrorNum ErrorNum);
#define ChkExcludeCPU(CheckCPU) ChkExcludeCPUExt(CheckCPU, ErrNum_InstructionNotSupported)

extern int ChkExactCPUList(int ErrorNum, ...);
extern int ChkExcludeCPUList(int ErrorNum, ...);

extern int ChkExactCPUMaskExt(Word CPUMask, CPUVar FirstCPU, tErrorNum ErrorNum);
#define ChkExactCPUMask(CPUMask, FirstCPU) ChkExactCPUMaskExt(CPUMask, FirstCPU, ErrNum_InstructionNotSupported)

extern Boolean ChkSamePage(LargeWord CurrAddr, LargeWord DestAddr, unsigned PageBits, tSymbolFlags DestFlags);

#endif /* _ERRMSG_H */
