

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


/* REQUIEM - table output and file format routines */

#include "requ.h"

extern struct cmd_file *lex_cfp;
static char buffer[TABLEMAX+1];
int bndx;

int	table(fp, slptr)
     FILE *fp;
     struct sel *slptr;
{
  int	tcnt;
  
  /* loop through the selected tuples */
  for (tcnt = 0; fetch(slptr, FALSE); tcnt++) {
    
    /* print table head on first tuple selected */
    if (tcnt == 0)
      tab_head(fp, slptr);
    
    /* print the tuple */
    tab_entry(fp, slptr);
  }
  
  /* print table foot */
  if (tcnt != 0)
    tab_foot(fp, slptr);
  
  /* return the tuple count */
  return (tcnt);
} /* table */

/* ----------------------------------------------------------------- */

tab_head(fp,slptr)
     FILE *fp;
     struct sel *slptr;
{
  struct sattr *saptr;
  int twidth,fwidth,i;
  char *aname;
  
  /* compute the table width */
  printf("\n");
  twidth = 1;
  for (saptr = SEL_ATTRS; saptr != NULL; saptr = saptr->sa_next)
    twidth += ATTRIBUTE_SIZE + 3;
  
  /* print the top line of the table */
  bstart();
  binsert(' ');
  for (i = 0; i < twidth-2; i++)
    binsert('+');
  binsert(' ');
  print_tabline(fp);
  
  /* print the label line of the table */
  bstart();
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    fwidth = ATTRIBUTE_SIZE;
    binsert('|');
    binsert(' ');
    if ((aname = saptr->sa_name) == NULL)
      aname = saptr->sa_aname;
    for (i = 0; i < fwidth; i++)
      if (*aname != EOS) {
	if (is_alpha(*aname)) {
	  binsert(toupper(*aname));
	  aname++;
	} else 
	  binsert(*aname++);
      }
      else
	binsert(' ');
    binsert(' ');
  }
  binsert('|');
  print_tabline(fp);
  
  /* print the line under the labels */
  bstart();
  binsert(' ');
  for (i = 0; i < twidth-2; i++)
    binsert('-');
  binsert(' ');
  print_tabline(fp);

} /* tab_head */

/* ----------------------------------------------------------------- */

tab_foot(fp,slptr)
     FILE *fp;
     struct sel *slptr;
{
  struct sattr *saptr;
  int twidth, i, fwidth;
  
  /* compute the table width */
  twidth = 1;
  for (saptr = SEL_ATTRS; saptr != NULL; saptr = saptr->sa_next)
    twidth += ATTRIBUTE_SIZE + 3;
  
  /* print an empty line */
  bstart();
  for (saptr = SEL_ATTRS; saptr != NULL; saptr = saptr->sa_next) {
    fwidth = ATTRIBUTE_SIZE;
    binsert('|');
    for (i = 0; i < fwidth+2; i++)
      binsert(' ');
  }
  binsert('|');
  print_tabline(fp);
  
  /* print the line at the foot of the table */
  bstart();
  for (i = 0; i < twidth; i++)
    binsert('/');
  
  print_tabline(fp);
  printf("\n");
} /* tab_foot */

/* ----------------------------------------------------------------- */

tab_entry(fp,slptr)
     FILE *fp;
     struct sel *slptr;
{
  struct sattr *saptr;
  int fwidth,i;
  
  /* print a table entry */
  bstart();
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    fwidth = ATTRIBUTE_SIZE;
    binsert('|');
    binsert(' ');
    for (i = 0; i < fwidth; i++)
      if (saptr->sa_aptr[i] != 0)
	binsert(saptr->sa_aptr[i]);
      else
	binsert(' ');
    binsert(' ');
  }
  binsert('|');
  print_tabline(fp);

} /* tab_entry */

/* ----------------------------------------------------------------- */

bstart()
{
  bndx = 0;
} /* bstart */

/* ----------------------------------------------------------------- */

binsert(ch)
     int ch;
{
  if (bndx < TABLEMAX)
    buffer[bndx++] = ch;
} /* binsert */

/* ----------------------------------------------------------------- */

print_tabline(fp)
     FILE *fp;
{
  buffer[bndx] = EOS;
  fprintf(fp,"%s\n",buffer);
} /* print_tabline */

/* ----------------------------------------------------------------- */

int	form(ofp, slptr, ffp)
     FILE *ofp;
     struct sel *slptr;
     FILE *ffp;
{
  char	aname[ANSIZE+1];
  int	ch, tcnt;
  
  /* loop through the selected tuples */
  for (tcnt = 0; fetch(slptr, FALSE); tcnt++) {
    
    /* reposition the form definition file */
    fseek(ffp, 0L, 0);
    
    /* process the form */
    while ((ch = getc(ffp)) != -1)
      if (ch == '<') {
	get_aname(ffp, aname);
	put_avalue(ofp, slptr, aname);
      }
      else
	putc(ch, ofp);
  }
  
  /* return the tuple count */
  return (tcnt);
} /* form */

/* ----------------------------------------------------------------- */

get_aname(fp, aname)
     FILE *fp;
     char	*aname;
{
  int	ch;
  
  while ((ch = getc(fp)) != '>')
    if (!isspace(ch))
      *aname++ = ch;
  *aname = EOS;
} /* get_aname */

/* ----------------------------------------------------------------- */

put_avalue(fp, slptr, aname)
     FILE *fp;
     struct sel *slptr;
     char	*aname;
{
  struct sattr *saptr;
  char	*saname;
  int	i;
  
  /* loop through the selected attributes */
  for (saptr = SEL_ATTRS; saptr != NULL; saptr = saptr->sa_next) {
    
    /* check the selected attribute name */
    if ((saname = saptr->sa_name) == NULL)
      saname = saptr->sa_aname;
    if (strcmp(saname, aname) == EQUAL)
      break;
  }
  
  if (saptr == NULL) {
    fprintf(fp, "<error>");
    return;
  }
  
  /* get the attribute value */
  for (i = 0; i < ATTRIBUTE_SIZE; i++)
    if (saptr->sa_aptr[i] != 0)
      putc(saptr->sa_aptr[i], fp);
    else
      putc(' ', fp);
} /* put_avalue */

/* ----------------------------------------------------------------- */

char *prompt_input(aptr, insert, avalue)
     struct attribute *aptr;
     BOOL insert;
     char *avalue;
{
  char aname[ANSIZE+1];
  char *unfold(), *prompt_ninput();
  char *val_start;  /* points to bof attribute value */
  
  if (!insert) {
    
    for (val_start = avalue; isspace(*val_start); val_start++)
      ;
    
    /* print it */
    if (strlen(aname) < 8)
      printf("%s\t\t: %s\n", unfold(aptr->at_name), val_start);
    else
      printf("%s\t: %s\n", unfold(aptr->at_name), val_start);
    
  }
  
  /* save the attribute name */
  strncpy(aname,aptr->at_name,
	  MIN(strlen(aptr->at_name), ANSIZE));
  aname[MIN(strlen(aptr->at_name), ANSIZE)] = EOS;
  
  /* set up null prompt strings */
  db_prompt((char *) 0, (char *) 0);

  /* prompt and input attribute value */
  return(prompt_ninput(stdin, FALSE, aptr->at_type, 
		       aptr->at_size, aname, avalue));
} /* prompt_input */

/* ----------------------------------------------------------------- */

char *prompt_ninput(fp, no_prompt, type, size, aname, avalue)
register FILE *fp;
BOOL no_prompt;
int type, size;
char *aname, *avalue;
{
   char prompt[40];
   char *unfold(), *getnum(), *getfloat(), *getfld();

  while (TRUE) {
   if (no_prompt == FALSE) { /* prompt is required */
     if (lex_cfp == NULL)
       if (strlen(aname) < 8)
	 sprintf(prompt, "%s\t\t: ",unfold(aname));
       else
	 sprintf(prompt, "%s\t: ", unfold(aname));
    } else
      prompt[0] = EOS;

    if (type == TNUM) {
      if (getnum(prompt, avalue, size, fp) != NULL)
	break;
      else
	return(NULL);
    } else if (type == TREAL) {
      if (getfloat(prompt, avalue, size, fp) != NULL)
	break;
      else 
	return(NULL);
    } else if (type == TCHAR) {
      if (getfld(prompt, avalue, size, fp) != NULL)
	break;
      else 
	return(NULL);
    } else
      return(NULL);
  }

  /* return the attribute value */
  return(avalue);

} /* prompt_ninput */

/* ----------------------------------------------------------------- */

/* routine for printing the extract templates */
template(buff, fp)
     char buff[];
     FILE *fp;
{
  int i;
  bstart();
  binsert('|');
  binsert(' ');
  for (i = 0; i < 51; i++)
    if (*buff != EOS)
      binsert(*buff++);
    else
      binsert(' ');
  binsert(' ');
  binsert('|');
  print_tabline(fp);
} /* template */

/* ----------------------------------------------------------------- */

templ_line(fp, simple)
     FILE *fp;
     BOOL simple;
{
  int i;
  
  bstart();
  for (i = 0; i < 55; i++)
    if (simple) {
      if (i == 0 || i== 54)
	binsert(' ');
      else
	binsert('-');
    }
    else
      binsert('/');
  print_tabline(fp);
} /* templ_line */

/* ----------------------------------------------------------------- */
int rprint(format)
     char *format;
{
  int i;
  
  if (*format == EOS)
    return(FALSE);
  
  printf("\n\n");
  for (i = 0; i < strlen(format) + 6; i++)
    printf("+");
  
  printf("\n");
  printf("+");
  printf("  ");
  printf(format);
  printf("  ");
  printf("+\n");
  printf("+");
  
  for (i = 0; i < strlen(format) + 4 ; i++)
    printf(" ");
  printf("+\n");
  
  for (i = 0; i < strlen(format) + 6; i++)
    printf("/");
  
  printf("\n\n");
  
  return(TRUE);
} /* rprint */

/* ----------------------------------------------------------------- */

requ_header()
{
  int i;
  char format[65];
  
  printf("\n");
  for (i = 0; i <61; i++)
    printf("+");
  printf("\n");
  printf("+  ");
  printf("REQUIEM: Relational Query & Update Interactive System    +\n");
  sprintf(format, "VERSION 2.2");
  printf("+  ");
  printf(format);
  for (i = strlen(format) + 3; i < 60; i++)
    printf(" ");
  printf("+\n");
  
  for (i = 0; i <61; i++)
    printf("/");
  printf("\n\n");
} /* requ_header */

/* ----------------------------------------------------------------- */

tab_keys(cntr, buff, key_buff)
     int cntr;
     char *buff;
     char **key_buff;
{
  int i, j;
  
  for (i = 0, j = 0; i < cntr; i++) {
    
    if (i == 0) {
      sprintf(buff , "----- ");
      j = strlen(buff);
    }
    
    if (i+1 < cntr) {
      sprintf(buff+j, "%s,  ", key_buff[i]);
      j += 3;
    }
    else
      strcpy(buff+j, key_buff[i]);
    
    j += strlen(key_buff[i]);
  }
} /* tab_keys */

/* ----------------------------------------------------------------- */

message(cntr, format)
     int cntr;
     char *format;
{
  
  if (cntr == 0)
    sprintf(format, "<   no  tuples  found   >");
  else if (cntr == 1)
    sprintf(format, "<   only %d tuple found    >", cntr);
  else
    sprintf(format, "<   %d tuples found    >", cntr);
  
} /* message */

/* ------------------------- EOF tabl.c ---------------------------- */
