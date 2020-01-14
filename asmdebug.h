#ifndef _ASMDEBUG_H
#define _ASMDEBUG_H
/* asmdebug.h */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Verwaltung der Debug-Informationen zur Assemblierzeit                     */
/*                                                                           */
/* Historie: 16. 5.1996 Grundsteinlegung                                     */
/*                                                                           */
/*****************************************************************************/

extern void AddLineInfo(Boolean InMacro, LongInt LineNum, char *FileName, 
                        ShortInt Space, LargeInt Address, LargeInt Len);

extern void InitLineInfo(void);

extern void ClearLineInfo(void);

extern void DumpDebugInfo(void);

extern void asmdebug_init(void);
#endif /* _ASMDEBUG_H */
