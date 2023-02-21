

/************************************************************************
 *									*
 *         Copyright (c) 1989 Mike Papazoglou and Willi Valder	        *
 *                      All rights reserved				*
 *									*
 *									*
 *    Permission to copy this program or any part of this program is	*
 *    granted in case of personal, non-commercial use only. Any use	*
 *    for profit or other commercial gain without written permissions	*
 *    of the authors is prohibited. 					*
 *									*
 *    You may freely distribute this program to friends, colleagues,	*
 *    etc.  As a courtesy we would appreciate it if you left our names	*
 *    on the code as the authors,                                       *
 *    (see the licensing agreement for further details).		*
 *    We also, of course, disavow any liabilities 			*
 *    for any bizarre things you may choose to do with the system.	*
 *									*
 *    We would appreciate though your letting us know about any 	*
 *    extensions, modifications, project suggestions, etc.		*
 *    Please mail all bugs, comments, complaints and suggestions to:	*
 *    									*
 *									*
 *    Mike P. Papazoglou						*
 *    ANU (Australian National University)	                        *
 *    Dept. of Computer Science,         				*
 *    G.P.O. Box 4							*
 *    Canberra, ACT 2601,						*
 *    Australia                          				*
 *									*
 *    email:    mike@anucsd.anu.edu.au           			*
 *    tel:      +61-6-2494725                            		*
 *    fax:      +61-6-2490010                            		*
 *									*
 ************************************************************************/


/* REQUIEM - main routine */

#include <signal.h>
#include "requ.h"

extern void requ_init(argc, argv);
extern void requ_header(void);

extern int error_code;
extern char *error_text();
extern int fpecatch();
extern BOOL   sys_flag;	 /* true for system defined relations */

/* ----------------------------------------------------------------- */

main(argc, argv)
     int argc;
     char *argv[];
{ 
  requ_header(); 
#ifndef UNIX
  init_username();
#endif
  s_init();
  
  requ_init(argc, argv); /* create & load system catatalogs */

  while (TRUE) {
    signal(SIGFPE, fpecatch);
    db_prompt("REQUIEM: ","\t# ");
    if (!parse((char *) 0, FALSE, FALSE)) {
      printf("***  ERROR : %s ***\n",error_text(error_code));
      cmd_kill(); /* clear command-line and icf environments */
    }

    if (sys_flag) {
      if (fexists("syskey"))
	sys_flag = FALSE;	 /* reset system flag */
    }
  }
  
} /* main */

/* -------------------------- EOF requ.c --------------------------- */
