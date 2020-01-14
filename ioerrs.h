#ifndef _IOERRS_H
#define _IOERRS_H
/* ioerrs.h */
/*****************************************************************************/
/* AS-Portierung                                                             */
/*                                                                           */
/* Abliefern der I/O-Fehlermeldungen                                         */
/*                                                                           */
/* Historie: 11.10.1996 Grundsteinlegung                                     */
/*                                                                           */
/*****************************************************************************/

extern char *GetErrorMsg(int number);

extern void ioerrs_init(char *ProgPath);
#endif /* _IOERRS_H */
