

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


/* REQUIEM - error messages */

#include <signal.h>
#include "requ.h"
extern char lexeme[];
extern char *rmalloc();
extern char errvar[];
int error_code;            /* global error code variable */

char *error_text(msg)      /* error text declarations */
     int msg;
{
  char *txt;
  char s[40];

  switch (msg) {
    
  case AGGRPARAM:
    txt = "wrong aggregate parameter type";
    break;
  case ATAMBG:
    txt = "ambiguous attribute name";
    break;
  case ATUNDF:
    sprintf(s, " undefined attribute #%s#", errvar);
    txt = rmalloc(sizeof(s));
    strcpy(txt, s);
    break;
  case BADCURS:
    txt = "cursor mispositioned, reset to origin";
    break;
  case BADHDR:
    txt = "bad relation header";
    break;
  case BADSET:
    txt = "bad set parameter";
    break;
  case CDSIZE:
    txt = "boolean expression too complex";
    break;
  case DUPATT: 
    sprintf(s, " duplicate attribute #%s#", lexeme);
    txt = rmalloc(sizeof(s));
    strcpy(txt, s);
    break;
  case DUPTUP:
    txt = "wrong insertion operation primary key already exists";
    break;
  case FILEMV:
    txt = "error in moving files";
    break;
  case HASHSIZ: 
    txt = "hash table size exceeded";
    break;
  case ILLJOIN:
    txt = "unmatched attribute names in join operation";
    break;
  case ILLREL:
    txt = "illegal relation name start: 'sys'";
    break;
  case INCNST:
    txt = "input entry holds inconsistent information";
    break;
  case INDFNF:
    txt = "indirect command file not found";
    break;
  case INDXCRE:
    txt= "error creating index file";
    break;
  case INPFNF:
    txt = "input file not found";
    break;
  case INPERR:
    txt = "erroneous input data from external file";
    break;
  case INSBLK:
    txt = "insufficient disk space";
    break;
  case INSMEM:
    txt = "insufficient memory";
    break;
  case IXATRNF:
    txt = "indexed attribute not found";
    break;
  case IXDELFAIL:
    txt = "index delete failed";
    break;
  case IXFAIL:
    txt = "indexing failed";
    break;
  case IXFLNF:
    txt = "index file not found";
    break;
  case KEYEXCD:
    sprintf(s, " size of key attribute #%s# exceeded", errvar);
    txt = rmalloc(sizeof(s));
    strcpy(txt, s);
    break;
  case LINELONG:
    txt = "length of command/viewline line exceeded";
    break;
  case LINK:
    txt = "error in renaming file";
    break;
  case MAXATT:
    txt = "too many attributes in relation create";
    break;
  case MAX5:
    txt = "no more than 5 attributes are allowed in expose";
    break;
  case NESTED:
    txt = "wrong nested command syntax";
    break;
  case NJOIN:
    txt = "join operation failed";
    break;
  case NOALIAS:
    txt = " no aliases allowed  in group clause ";
    break;
  case NOEXSTUP:
    txt = "wrong deletion operation tuple does not exist";
    break;
  case NOKEY:
    txt= "at least one primary key attribute must be specified";
    break;
  case OUTCRE:
    txt = "error creating output file";
    break;
  case QUATTR:
    txt = "qualified attribute is required";
    break;
  case RELCRE:
    txt = "error creating relation file";
    break;
  case RELFNF:
    if ((strncmp(errvar,SNAPSHOT, strlen(SNAPSHOT)) == EQUAL))
      txt = "no intermediate results have been derived !";
    else {
      sprintf(s, " relation file #%s# not found", errvar);
      txt = rmalloc(sizeof(s));
      strcpy(txt, s);
    }
    break;
  case RELFUL:
    txt = "relation file full";
    break;
  case RLUNDF:
    sprintf(s, " undefined relation #%s#", lexeme);
    txt = rmalloc(sizeof(s));
    strcpy(txt, s);
    break;
  case STACKOVRFL:
    txt = "stack overflow";
    break;
  case STACKUNDFL:
    txt = "stack underflow";  
    break;
  case SYNTAX:
    sprintf(s, "syntax error at or near %s", lexeme);
    txt = rmalloc(sizeof(s));
    strcpy(txt, s);
    break;
  case TOOMANY:
    txt = "you may specify 2 indexed attributes of a kind at most";
    break;
  case TUPINP:
    txt= "redundant <from>, or forgotten <over> clause";
    break;
  case TUPOUT:
    txt = "tuple output error";
    break;
  case UNATTR:
    txt = "inapplicable  attribute name encountered";
    break;
  case UNDEFTYPE:
    txt = "undefined attribute type encountered";
    break;
  case UNDEFVAR:
    txt = "undefined variable encountered";
    break;
  case UNIQEXCD:        
    txt =  "no. of unique keys exceeded";
    break;
  case UNLINK:
    txt = "error in purging file";
    break;
  case UNOP:
    txt = "no union-intersect operation is allowed";
    break;
  case UPDTERR:
    txt = "error in  updating indexed atributes";
    break;
  case VIEWDEF:
    txt = "unacceptable view name specifier in view definition";
    break;
  case VIEWDEL:
    txt = "unacceptable view name specifier in view deletion";
    break;
  case WRIXCOM:
    txt = "error in compilation of indexed attributes";
    break;
  case WRLEN:
    txt = "max length of name should not exceed 10 chars";
    break;
  case WRONGID:
    txt = "operation meaningless, operand mismatch encountered!";
    break;
  case WRNGCOMB:
    txt = "wrong foreign key combination, primary already exists";
    break;
  case WRNGFGNKEY:
    txt = "wrong foreign key declaration";
    break;
  case  WRNGINS:
    txt =  "wrong insertion operation";
    break; 
  case WRNRELREM:
    txt = " non-existent relation-file";
    break;
  case WRPREDID:
    txt = "wrong identifier in predicate expression";
    break;
  default:
    txt = "undefined error";
    break;
  }
  /* return the message text */
  return (txt);
} /* error_text */

char *pt_error(errcode) /* store error code and return NULL */
     int errcode;
{
  error_code = errcode;
  return (NULL);
}/* pt_error */

int error(errcode)  /* store the error code and return FALSE */
     int errcode;
{
  error_code = errcode;
  return (FALSE);
} /* error */

int fpecatch()  /* catch floating point exceptions */
{
  signal(SIGFPE, fpecatch);
  printf("*** CATASTROPHIC-ERROR: overflow, ");
  printf("please check length of operators ! \n");
} /* fpecatch */
