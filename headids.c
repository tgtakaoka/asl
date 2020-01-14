/* headids.c */
/*****************************************************************************/
/* Makroassembler AS                                                         */
/*                                                                           */
/* Hier sind alle Prozessor-IDs mit ihren Eigenschaften gesammelt            */
/*                                                                           */
/* Historie: 29. 8.1998 angelegt                                             */
/*                      Intel i960                                           */
/*           30. 8.1998 NEC uPD7725                                          */
/*            6. 9.1998 NEC uPD77230                                         */
/*           30. 9.1998 Symbios SYM53C8xx                                    */
/*           29.11.1998 Intel 4004                                           */
/*            3.12.1998 Intel 8008                                           */
/*           25. 3.1999 National SC14xxx                                     */
/*            4. 7.1999 Fujitsu F2MC                                         */
/*           10. 8.1999 Fairchild ACE                                        */
/*                                                                           */
/*****************************************************************************/

#include "stdinc.h"

#include <string.h>

#include "headids.h"

/*---------------------------------------------------------------------------*/

static TFamilyDescr Descrs[]=
       {
        {"680x0"     , 0x0001, MotoS   },
        {"DSP56000"  , 0x0009, MotoS   },
        {"MPC601"    , 0x0005, MotoS   },
        {"M-CORE"    , 0x0003, MotoS   },
        {"68xx"      , 0x0061, MotoS   },
        {"6805/HC08" , 0x0062, MotoS   },
        {"6809"      , 0x0063, MotoS   },
        {"68HC12"    , 0x0066, MotoS   },
        {"68HC16"    , 0x0065, MotoS   },
        {"H8/300(H}" , 0x0068, MotoS   },
        {"H8/500"    , 0x0069, MotoS   },
        {"SH7x00"    , 0x006c, MotoS   },
        {"65xx"      , 0x0011, MOSHex  },
        {"MELPS-7700", 0x0019, MOSHex  },
        {"MELPS-4500", 0x0012, IntHex  },
        {"M16"       , 0x0013, IntHex32},
        {"M16C"      , 0x0014, IntHex  },
        {"MCS-48"    , 0x0021, IntHex  },
        {"MCS-(2)51" , 0x0031, IntHex  },
        {"MCS-96/196", 0x0039, IntHex  },
        {"4004/4040" , 0x003f, IntHex  },
        {"8008"      , 0x003e, IntHex  },
        {"8080/8085" , 0x0041, IntHex  },
        {"8086"      , 0x0042, IntHex16},
        {"i960"      , 0x002a, IntHex32},
        {"8X30x"     , 0x003a, IntHex  },
        {"XA"        , 0x003c, IntHex16},
        {"AVR"       , 0x003b, Atmel   },
        {"29xxx"     , 0x0029, IntHex32},
        {"80C166/167", 0x004c, IntHex16},
        {"Zx80"      , 0x0051, IntHex  },
        {"Z8"        , 0x0079, IntHex  },
        {"TLCS-900"  , 0x0052, MotoS   },
        {"TLCS-90"   , 0x0053, IntHex  },
        {"TLCS-870"  , 0x0054, IntHex  },
        {"TLCS-47xx" , 0x0055, IntHex  },
        {"TLCS-9000 ", 0x0056, MotoS   },
        {"16C8x"     , 0x0070, IntHex  },
        {"16C5x"     , 0x0071, IntHex  },
        {"17C4x"     , 0x0072, IntHex  },
        {"ST6"       , 0x0078, IntHex  },
        {"ST7"       , 0x0033, IntHex  },
        {"ST9"       , 0x0032, IntHex  },
        {"6804"      , 0x0064, MotoS   },
        {"TMS3201x"  , 0x0074, TiDSK   },
        {"TMS3202x"  , 0x0075, TiDSK   },
        {"TMS320C3x" , 0x0076, IntHex32},
        {"TMS320C5x" , 0x0077, TiDSK   },
        {"TMS320C6x" , 0x0047, IntHex32},
        {"TMS9900"   , 0x0048, IntHex  },
        {"TMS7000"   , 0x0073, IntHex  },
        {"TMS370xx"  , 0x0049, IntHex  },
        {"MSP430"    , 0x004a, IntHex  },
        {"SC/MP"     , 0x006e, IntHex  },
        {"COP8"      , 0x006f, IntHex  },
        {"SC14XXX"   , 0x006d, IntHex  },
        {"ACE"       , 0x0067, IntHex  },
        {"78(C)1x"   , 0x007a, IntHex  },
        {"75K0"      , 0x007b, IntHex  },
        {"78K0"      , 0x007c, IntHex  },
        {"7720"      , 0x007d, IntHex  },
        {"7725"      , 0x007e, IntHex  },
        {"77230"     , 0x007f, IntHex  },
        {"SYM53C8xx" , 0x0025, IntHex  },
        {"F2MC8"     , 0x0015, IntHex  },
        {Nil         , 0xffff, Default }
       };

/*---------------------------------------------------------------------------*/

	PFamilyDescr FindFamilyByName(char *Name)
BEGIN
   PFamilyDescr Run;

   for (Run=Descrs; Run->Name!=Nil; Run++)
    if (strcmp(Name,Run->Name)==0) return Run;

   return Nil;
END

	PFamilyDescr FindFamilyById(Word Id)
BEGIN
   PFamilyDescr Run;       

   for (Run=Descrs; Run->Name!=Nil; Run++)
    if (Id==Run->Id) return Run;

   return Nil;
END

/*---------------------------------------------------------------------------*/

	void headids_init(void)
BEGIN
END
