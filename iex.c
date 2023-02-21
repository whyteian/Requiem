

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


/* REQUIEM - import/export command routines */

#include "requ.h"

extern int lex_token;
extern char lexeme[];
extern char cformat[];

/* ---------------------------------------------------------- */

int db_import(fmt)
     char *fmt;
{
  struct scan *db_ropen(), *fget_string();
  struct scan *sptr;
  struct attribute *aptr;
  char fname[STRINGMAX+1],avalue[STRINGMAX+1], start_value[STRINGMAX+1];
  char **key_buf = NULL;
  int tcnt,astart,i;
  int p_cntr = 0, s_cntr = 2;
  BOOL eofile = FALSE;
  FILE *fp;
  int error();
  
  /* check for a command line */
  if (fmt != NULL)
    q_scan(fmt);
  
  /* checks for "<filename> into <relation-name>" */
  if (next_token() == ID)
    strcat(lexeme,".dat");
  else if (lex_token != STRING)
    return (error(SYNTAX));
  strcpy(fname,lexeme);
  if (next_token() != INTO)
    return (error(SYNTAX));
  if (next_token() != ID)
    return (error(SYNTAX));
  
  /* open the relation */
  if ((sptr = db_ropen(lexeme)) == NULL)
    return (FALSE);
  
  /* open the input file */
  if ((fp = fopen(fname,"r")) == NULL)
    return (error(INPFNF));
  
  /* import tuples */
   for (tcnt = 0; ; tcnt++) { /* count the inserted tuples */
    
    /* get attribute values */
    astart = 1;
    for (i = 0; i < NATTRS; i++) {
      
      /* get a pointer to the current attribute */
      aptr = &SCAN_HEADER.hd_attrs[i];
      
      /* check for the last attribute */
      if (aptr->at_name[0] == 0)
	break;
      
      /* input the tuple  */
      if (fget_string(aptr->at_name, avalue, 
                           aptr->at_type, aptr->at_size, fp) == NULL) {
	eofile = TRUE;
	break;
      }
      avalue[strlen(avalue)] = EOS;
      
      /* store the attribute value */
      if (avalue[0])
        store_attr(aptr,&SCAN_TUPLE[astart],avalue);
      else {
        fclose(fp);
        print_message(tcnt, "import");
        rprint(cformat);
        return(error(INPERR));
      }

      /* update the attribute start */
      astart += aptr->at_size;

      /* remove trailing blanks */
      rm_lead_blks(avalue, start_value);

      /* check if indexing is required */
      if (aptr->at_key != 'n')
	check_ix(&key_buf, aptr, &p_cntr, &s_cntr, start_value);
    }
     
    p_cntr = 0; s_cntr =2; 

    /* store the new tuple */
    if (!eofile) {
      if (!store_tuple(sptr, key_buf)) {
        buf_free(key_buf, 4);
	key_buf = NULL;
	db_rclose(sptr);
	return (FALSE);
      }
    }
    else
      break;
  }
  
  /* close the relation */
  db_rclose(sptr);
  
  /* close the input file */
  fclose(fp);
  
  /* read terminator */
  if (this_token() != ';')
     return(error(SYNTAX)); 

  print_message(tcnt, "import");

  /* return successfully */
  return (TRUE);
} /* import */

/* ---------------------------------------------------------- */

print_message(tcnt, text)
int tcnt;
char *text;
{
/* check number of tuples imported */
  if (tcnt != 0) {
    
    /* print tuple count */
    sprintf(cformat, "   %d tuples %sed   ",tcnt, text);
  }
  else
    sprintf(cformat, "   no tuples %sed   ", text);
} /* print_message */

/* ---------------------------------------------------------- */

int db_export(fmt)
     char *fmt;
{
  struct scan *db_ropen();

  struct scan *sptr;
  struct attribute *aptr;
  char rname[RNSIZE+1],avalue[STRINGMAX+1],temp_value[STRINGMAX+1];
  int tcnt,astart,i,j;
  FILE *fp;
  int error();
    
  /* check for a command line */
  if (fmt != NULL)
    q_scan(fmt);
  
  /* checks for "<relation-name> [ into <filename> ]" */
  if (next_token() != ID)
    return (error(SYNTAX));
  strcpy(rname,lexeme);
  if (!redirect_to(&fp,".dat"))
    return (FALSE);
  
  /* open the relation */
  if ((sptr = db_ropen(rname)) == NULL)
    return (FALSE);
  
  /* export tuples */
  for (tcnt = 0; fetch_tuple(sptr); tcnt++) {
    
    /* get attribute values */
    astart = 1;
    for (i = 0; i < NATTRS; i++) {
      
      /* get a pointer to the current attribute */
      aptr = &SCAN_HEADER.hd_attrs[i];
      
      /* check for the last attribute */
      if (aptr->at_name[0] == 0)
	break;
      
      /* get the attribute value */
      get_attr(aptr, &SCAN_TUPLE[astart], avalue);
      
      /* remove any trailing blanks */
      rm_lead_blks(avalue, temp_value);

      /* output the tuple */
      for (j=0;!temp_value[j] && j < aptr->at_size;j++)
	;
      fprintf(fp,"%s\n",&temp_value[j]);
      
      /* update the attribute start */
      astart += aptr->at_size;
    }
  }
  
  /* close the relation */
  db_rclose(sptr);
  
  /* close the output file */
  if (fp != stdout)
    fclose(fp);

  /* read terminator */
  if (this_token() != ';')
     return(error(SYNTAX));  

  print_message(tcnt, "export");
  
  /* return successfully */
  return (TRUE);
} /* db_export */

/* ---------------------------------------------------------- */

int db_extract(fmt)
     char *fmt;
{
  struct scan *db_ropen();
  struct scan *sptr;
  struct attribute *aptr;
  char rname[RNSIZE+1],aname[ANSIZE+1],*atype;
  char  buff[TABLEMAX+1];
  char **prim_buf = NULL;
  char **sec_buf = NULL;
  int i, p_cntr = 0, s_cntr = 0;
  FILE *fp;
  int error();
  char *unfold();
  
  /* check for a command line */
  if (fmt != NULL)
    q_scan(fmt);
  
  /* checks for "<relation-name> [ into <filename> ]" */
  if (next_token() != ID)
    return (error(SYNTAX));
  strcpy(rname,lexeme);
  /* check for file name provided by user or return stdout */
  if (!redirect_to(&fp,".def"))
    return (FALSE);
  
  /* open the relation */
  if ((sptr = db_ropen(rname)) == NULL)
    return (FALSE);
  
  /* output the relation definition */
  printf("\n");
  strcpy(rname, unfold(rname));
  templ_line(fp, TRUE);
  sprintf(buff,"Relation name:%12s ",rname);
  template(buff, fp);
  templ_line(fp, TRUE);
  sprintf(buff,"Attribute names:");
  template(buff, fp);
  
  /* get attribute values */
  for (i = 0; i < NATTRS; i++) {
    
    /* get a pointer to the current attribute */
    aptr = &SCAN_HEADER.hd_attrs[i];
    
    /* check for the last attribute */
    if (aptr->at_name[0] == EOS)
      break;
    
    /* save the attribute name */
    strncpy(aname,aptr->at_name,
	    MIN(strlen(aptr->at_name), ANSIZE));
    aname[MIN(strlen(aptr->at_name), ANSIZE)] = EOS;
    
    /* determine the attribute type */
    switch (aptr->at_type) {
    case TCHAR:
      atype = "char";
      break;
    case TNUM:
      atype = "num";
      break;
    case TREAL:
      atype = "real";
      break;
    default:
      atype = "<error>";
      break;
    }
    
    strcpy(aname, unfold(aname));
    templ_line(fp, TRUE);
    
    /* check for primary key */
    if (aptr->at_key == 'p' && p_cntr < 2)  {
      
      /* create a key buffer */
      if (prim_buf == NULL) {
	if ((prim_buf = 
	     (char **)calloc(4, sizeof(char *))) == NULL)
	  return (error(INSMEM));
      }
      prim_buf[p_cntr] = rmalloc((int) aptr->at_size + 1);
      strcpy(prim_buf[p_cntr++], aname);
    }
    
    /* check for secondary key */
    if (aptr->at_key == 's' && s_cntr < 4)  {
      
      /* create a key buffer */
      if (sec_buf == NULL) {
	if ((sec_buf = 
	     (char **)calloc(4, sizeof(char *))) == NULL)
	  return (error(INSMEM));
      }
      sec_buf[s_cntr] = rmalloc((int) aptr->at_size + 1);
      strcpy(sec_buf[s_cntr++], aname);
    }
    
    /* output the attribute definition */
    sprintf(buff,"   %s   of type %s  and size  %d",
	                         aname, atype, aptr->at_size - 1);
    template(buff, fp);
  }
  
  templ_line(fp, TRUE);
  
  /* print out primary and secondary keys */
  tab_keys(p_cntr, buff, prim_buf);
  
  if (p_cntr == 1)
    template("Primary key is: ", fp);
  else 
    template("Combination of foreign keys: ", fp);
  
  template(buff, fp);
  templ_line(fp, TRUE);
  
  if (sec_buf)
    tab_keys(s_cntr, buff, sec_buf);
  
  template("Secondary keys: ", fp);
  
  if (sec_buf)
    template(buff, fp);
  else {
    buff[0] = EOS;
    template(buff, fp);
  }
  
  templ_line(fp, TRUE);
  
  sprintf(buff,"No. of tuples currently filled:   %d", 
	                              SCAN_HEADER.hd_tcnt);
  template(buff, fp);
  sprintf(buff,"Size of tuple in bytes   %d", 
	                              (SCAN_HEADER.hd_size)-1);
  template(buff, fp);
  
  /* force the printing of a blank line */
  *buff = EOS;
  template(buff, fp);
  templ_line(fp, FALSE);
  printf("\n");
  
  /* close the relation, free buffers */
  db_rclose(sptr);
  buf_free(prim_buf, 4); 
  buf_free(sec_buf, 4);
  
  /* close the output file */
  if (fp != stdout)
    fclose(fp);
  
  /* return successfully */
  return (TRUE);
} /* db_extract */

/* ---------------------------------------------------------- */

int	redirect_to(pfp, ext)
     FILE **pfp;
     char	*ext;
{
  /* assume no into clause */
  *pfp = stdout; /* output redirected to stdout */
  
  /* check for "into <fname>" */
  if (this_token() != INTO)
    return (TRUE);
  next_token();
  if (next_token() == ID)
    strcat(lexeme, ext);
  else if (lex_token != STRING)
    return (error(SYNTAX));
  
  /* open the output file */
  *pfp = fopen(lexeme, "w");
  if (*pfp == NULL)
    return (error(OUTCRE));
  
  /* return successfully */
  return (TRUE);
  
} /* redirect_to */

/* ---------------------------------------------------------- */

int	using(pfp, ext)
     FILE **pfp;
     char	*ext;
{
  /* assume no using clause */
  *pfp = NULL;
  
  if (next_token() == ID)
    strcat(lexeme, ext);
  else if (lex_token != STRING)
    return (error(SYNTAX));
  
  /* open the input file */
  if ((*pfp = fopen(lexeme, "r")) == NULL)
    return (error(INPFNF));
  
  /* return successfully */
  return (TRUE);
} /* using */

/* ------------------------ EOF iex.c ------------------------ */
