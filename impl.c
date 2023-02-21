

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


/* REQUIEM - command implementor */

#include "btree.h"
#include "requ.h"

extern int      lex_token;
extern int      sref_cntr;
extern char     lexeme[];
extern char     cformat[];
extern char     *join_on[];
extern char     *lptr;
extern BOOL     ask;
extern int      saved_ch;
extern BOOL     wh_signal;
extern BOOL     view_def;
extern BOOL     expose_exists;  /* expose operation flag */
extern BOOL     canned_commit;	/* db_commit called from within canned query */

extern struct  cmd_file *lex_cfp,*temp_cfp;   /* command file context ptrs */

char           *stck();
struct   trans_file *trans_hd = NULL;
char    **key_buf = NULL; /* indexed attributes to be inserted */
char    **del_buf = NULL; /* indexed attributes to be deleted */
static  char rel_fl[RNSIZE+5];

int help()
{
  FILE           *fp;
  int             ch;
  
  if ((fp = fopen("requ.hlp", "r")) != NULL) {
    while ((ch = getc(fp)) != EOF)
      putchar(ch);
    fclose(fp);
  } else
    printf("No online help available.  Read the manual\n");
  
  /* return successfully */
  return (TRUE);
  
} /* help */

/* ---------------------------------------------------------- */

int db_modify()
{
  struct scan    *db_ropen();
  struct scan    *sptr;
  struct attribute *aptr;
  char            a_size[ASIZE + 1];
  char            prompt[30];
  char           *buf_name;
  struct attribute *new_attrs[NATTRS];
  int             i;
  BOOL            change = TRUE;
  char *unfold();
  char *getnum();

  /* open the relation */
  if ((sptr = db_ropen(lexeme)) == NULL)
    return (FALSE);
  
  /* indicate that a store in header should take place */
  sptr->sc_store = TRUE;
  
  for (i = 0; i < NATTRS; i++) {
    /* get a pointer to the current attribute */
    aptr = &SCAN_HEADER.hd_attrs[i];
    
    /* check for the last attribute */
    if (aptr->at_name[0] == EOS)
      break;
    
    /* set up null prompt strings */
    db_prompt((char *) 0, (char *) 0);
    
    while (TRUE) {
      if (strlen(aptr->at_name) < 8)
     printf("%s\t\t###\n ", unfold(aptr->at_name));
      else
     printf("%s\t###\n ", unfold(aptr->at_name));
      
      /* if a change in size is required */
      sprintf(prompt,"old length = %d  new?\t:",aptr->at_size);
      if (getnum(prompt, a_size, LINEMAX, stdin) != "\n")
     if (isdigit(*a_size))
       change = TRUE;
      
      printf("------------------------------\n");
      break;
    }
    
    /* store the changes in an auxiliary attribute structure */
    if ((new_attrs[i] = MALLOC(struct attribute)) == NULL)
      return (error(INSMEM));
    
    strncpy(new_attrs[i]->at_name, aptr->at_name, 
	               MIN(strlen(aptr->at_name), ANSIZE));
    new_attrs[i]->at_name[MIN(strlen(aptr->at_name),ANSIZE)]=EOS;
    
    if (a_size[0] != EOS)
      new_attrs[i]->at_size = atoi(a_size);
    else
      new_attrs[i]->at_size = aptr->at_size;
    
    new_attrs[i]->at_key = aptr->at_key;
    new_attrs[i]->at_type = aptr->at_type;
  }
  
  /* close rel, write rel header into memory, free scan space */
  if ((buf_name = calloc(1, RNSIZE + 1)) == NULL) {
    db_rclose(sptr);
    return (error(INSMEM));
  }
  strcpy(buf_name, SCAN_RELATION->rl_name);
  db_rclose(sptr);
  
  if (change)               /* must perform a file transfer */
    if (!transfer(buf_name, new_attrs))
      return (FALSE);
  
  /* return successfully */
  return (TRUE);
} /* db_modify */

/* ---------------------------------------------------------- */

int db_focus()
{
  char            rname[RNSIZE + 1];
  struct scan    *sptr;
  struct scan    *db_ropen();
  
  /* save the relation file name */
  strcpy(rname, lexeme);
  
  /* make sure that the rest of the line is blank */
  if (!db_flush())
    return (FALSE);
  
  /* open the relation */
  if ((sptr = db_ropen(rname)) == NULL)
    return (FALSE);
  
  sprintf(cformat, "Current %s focus lies on tuple #%d#", rname,
       SCAN_HEADER.hd_cursor);
  rprint(cformat);
  cformat[0] = EOS;            /* clear command format */
  
  /* close file and clear relation structure */
  db_rclose(sptr);
  
  /* return successfully */
  return (TRUE);
} /* db_focus */

/* ---------------------------------------------------------- */

int db_display(slptr, c_width, offset)
     struct sel     *slptr;
     int             c_width, offset;
{
  struct relation *rc_create();
  struct relation *rptr;
  struct sattr   *saptr;
  char           *aname, *tbuf;
  int             i;
  char            filename[RNSIZE+3];
  
  /* create a new relation */
  sref_cntr++;
  if ((rptr = rc_create(stck(rel_fl, sref_cntr))) == NULL) {
    done(slptr);
    return (FALSE);
  }
  /* preserve file name */
  strcpy(filename, rptr->rl_name);
  
  /* create the selected attributes */
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    
    /* decide which attribute name to use */
    if ((aname = saptr->sa_name) == NULL)
      aname = saptr->sa_aname;
    
    /* add the attribute */
    if (!add_attr(rptr, aname, ATTRIBUTE_TYPE, 
		              ATTRIBUTE_SIZE, ATTRIBUTE_KEY)) {
      free((char *) rptr);
      done(slptr);
      return (FALSE);
    }
  }
  
  /* create the relation header */
  if (!rc_header(rptr, NOINDEX)) {
    done(slptr);
    return (FALSE);
  }

  /* allocate and initialize a tuple buffer */
  if ((tbuf = 
   calloc(1, (unsigned) RELATION_HEADER.hd_size + 1)) == NULL) {
    rc_done(rptr, TRUE);
    return (error(INSMEM));
  }

  /* reset cursor at current place, and seek tuple */
  SEL_HEADER.hd_cursor += offset;
  i = 0;
  if (!cseek(SEL_SCAN, SEL_HEADER.hd_cursor)) {
    /* reset cursor to original position */
    SEL_HEADER.hd_cursor = 0;
    SEL_SCAN->sc_store = TRUE;
    done(slptr);
    rc_done(rptr, TRUE);
    return (error(BADCURS));
  }
  /* read, write tuples where cursor was last positioned */
  do {
    /* read tuple from original relation */
    if (read(SEL_RELATION->rl_fd,
              tbuf, SEL_HEADER.hd_size) != SEL_HEADER.hd_size) {
      done(slptr);
      rc_done(rptr, TRUE);
      return (error(TUPINP));
    }
    /* write tuple to the temporary file */
    if (!write_tuple(rptr, tbuf, TRUE)) {
      rc_done(rptr, TRUE);
      done(slptr);
      return (error(INSBLK));
    }
    i++;                  /* increment index */
    
  } while (i < c_width && 
	   (SEL_HEADER.hd_cursor + i < SEL_HEADER.hd_tcnt));
  
  /* reset cursor if c_width > 0, 
     indicate that a store should take place in header */
  if (c_width)
    SEL_HEADER.hd_cursor += (i - 1);
  SEL_SCAN->sc_store = TRUE;
  
  /* finish the selection */
  done(slptr);
  nfree((char *) tbuf);
  
  /* finish the relation creation */
  if (!rc_done(rptr, TRUE))
    return (FALSE);
  
  /* print the resulting tuple on stdout */
  if (!parse("print", FALSE, FALSE))
    return (FALSE);
  
  /* return successfully */
  return (TRUE);
} /* db_display */

/* ---------------------------------------------------------- */

int db_commit(pixd, slptr, no_delete, cntr)
     struct ix_header *pixd;
     struct sel     *slptr;
     BOOL            no_delete;
     int            *cntr;
{
  char            response[5];
  char            prompt[80];
  BOOL            proceed = FALSE;
  BOOL            temp_wh_signal;
  char *getfld();

  if (*cntr >= 1 && wh_signal == TRUE && ! no_delete)
    proceed = TRUE;  /* no prompt in case of predicate-specified operations */
  else if (canned_commit) /* no prompt in case of canned query */
    proceed = TRUE;
  else if (strncmp(SEL_ATTRS->sa_rname, "sys", 3) != EQUAL) {
    sprintf(prompt, "Commit changes ? (<yes> or <no>)\t: ");
    getfld(prompt, response, 4, stdin);
  } else  		/*  no prompt in case of system applied deletions */
    proceed = TRUE;  
 
  
  temp_wh_signal = wh_signal;

  if (!proceed && strcmp(response, "yes") != EQUAL && wh_signal && !no_delete) 
    return(FAIL);
  
  if (strcmp(response, "yes") == EQUAL || proceed) {
    if (no_delete) {   /* update the tuple(s) only */
      if(!update_tuple(pixd, slptr->sl_rels, key_buf, del_buf))
	return(FAIL);
      else 
	(*cntr)++;
    } else {          /* delete the tuple (s) */
      if (!delete_tuple(pixd, SEL_SCAN, del_buf))
	return(FAIL);
      else
	(*cntr)++;
    }
  }
  
  if  (strcmp(response, "yes") == EQUAL && temp_wh_signal) {
    if (! no_delete)
      return(TRUE);
  } 
  
  if (proceed == FALSE || no_delete) { /* ask in case of updates */
    response[0] = EOS;
    sprintf(prompt, "%s%s",
	    "Press any character <return> to continue,",
	    " or <return> to exit : ");
    getfld(prompt, response, 4, stdin);
    
    if (response[0] == EOS || response[0] == NEWLINE)
      return (FAIL);
    else
      return(SUCCESS);
  }

  return(TRUE);
} /* db_commit */

/* ---------------------------------------------------------- */

db_remove()
{
  int             i;
  char            filename[RNSIZE+3];

  for (i = 1; i <= sref_cntr; i++) {
    sprintf(filename, "%s.db", stck(rel_fl, i));
    if (unlink(filename) == FAIL)
      continue;
  }

  sref_cntr = 1;	/* reset counter */

} /* db_remove */

/* ---------------------------------------------------------- */

remove_intermediate()
{
  char filename[RNSIZE+3];
  
  sprintf(filename, "%s.db", stck(rel_fl, sref_cntr--));
  unlink(filename); 
} /* remove_intermediate */

/* ---------------------------------------------------------- */

static int purge_trans(hd)
     struct   trans_file *hd;
{ 
  struct   trans_file *hd_prev;
  
  while (hd) {
    unlink(hd->t_file);
    hd_prev = hd;
    hd = hd->t_next;
    nfree((char *) hd_prev);
  }
} /* purge_trans */

/* ---------------------------------------------------------- */

clear_catalogs(rname)
     char *rname;
{
  char buf[70];
  char flat_file[RNSIZE+6], index_file[RNSIZE+6];
  BOOL temp_wh_signal = wh_signal;

  strcpy(flat_file, rname);
  strcat(flat_file, ".db");
  strcpy(index_file, rname);          
  strcat(index_file, ".indx");
  
  if (unlink(flat_file) == FAIL ||unlink(index_file) == FAIL)
    return(error(WRNRELREM));

  /* delete entries corresponding to catalog entries */
  sprintf(buf,
	  "delete systab where relname = \"%s\";", rname);
  if (!parse(buf, FALSE, FALSE))
      return(error(WRNRELREM));  
  saved_ch = '\n';
 
  sprintf(buf,
	  "delete sysatrs where relname = \"%s\";", rname);
     if (!parse(buf, FALSE, FALSE))
        return(error(WRNRELREM));   
  saved_ch = '\n';
 
  sprintf(buf, 
	  "delete sysview where base = \"%s\";", rname);
  if (!parse(buf, FALSE, FALSE))
    return(error(WRNRELREM));   
  saved_ch = '\n';
  
  sprintf(buf, 
	  "delete syskey where relname = \"%s\"", rname);

  if (!parse(buf, FALSE, FALSE))
    return(error(WRNRELREM));   
  
  sprintf(cformat, "#  %s and all catalog entries deleted  #", rname);
 
  wh_signal = temp_wh_signal;
  
  terminator();

  return(TRUE);

} /* clear_catalogs */   

/* ---------------------------------------------------------- */

int quit()
{
  db_remove();
  purge_trans(trans_hd);
  printf("\n ***   You have left the database    ***\n\n");
  exit(0);
  
} /* quit */

/* ---------------------------------------------------------- */

db_assign(trans_file)
     char trans_file[];
{
  static struct trans_file *trans_tail = NULL;
  char filename[RNSIZE+3];
  
  /* append this temp-rel name to the cur. transient file list */
  if (!trans_hd) {
    if ((trans_hd = CALLOC(1, struct trans_file)) == NULL)
      return(error(INSMEM));
    
    trans_hd->t_next = NULL;
    strcpy(trans_hd->t_file, trans_file);
    trans_tail = trans_hd;
  }
  else {
    if ((trans_tail->t_next = 
	 CALLOC(1, struct trans_file)) == NULL)
      return(error(INSMEM));
    
    trans_tail = trans_tail->t_next;
    strcpy(trans_tail->t_file, trans_file);
    trans_tail->t_next = NULL;
  }
  
  sprintf(filename, "%s.db", stck(rel_fl, sref_cntr));
  
  /* transfer files */
  if (!f_move(filename, trans_file))
    return(error(FILEMV));
  
  return(TRUE);
} /* db_assign */

/* ---------------------------------------------------------- */

int new_relation(sref_cntr, rptr, slptr, prptr, rel_fl)
     int            *sref_cntr;
     struct relation **rptr;
     struct sel     *slptr;
     struct pred    *prptr[];
     char           *rel_fl;
{
  struct relation *rc_create();

  (*sref_cntr)++;

  if ((*rptr = rc_create(stck(rel_fl, *sref_cntr))) == NULL) {
    done(slptr);
    free_pred(prptr);
    
    return (FALSE);
  }
  return (TRUE);
} /* new_relation */

/* ---------------------------------------------------------- */

int joined(relat, attrib, r_indx, a_indx, where_flag, recursive)
     char          **relat, **attrib;
     int             r_indx, a_indx;
     BOOL          where_flag, recursive;
{
  char    *rel1, *rel2, fmt[2*LINEMAX+1], *temp_buf = (char *)0;
  char    *temp_lptr, relop[5], wh_attr[25], constant[35];

  rel1 = calloc(1, (unsigned) strlen(relat[0]) + 1);
  rel2 = calloc(1, (unsigned) strlen(relat[1]) + 1);
  
  strcpy(rel1, relat[0]);
  strcpy(rel2, relat[1]);
  strcpy(join_on[0], attrib[0]);
  strcpy(join_on[1], attrib[1]);
  
  /* make qualified attributes */
  make_quattr(relat[0], attrib[0]);
  make_quattr(relat[1], attrib[1]);
  
  if (where_flag == FALSE)
    sprintf(fmt, "select from %s, %s where %s = %s", rel1, rel2,
         relat[0], relat[1]);
  else {
    if (!where_in_join(wh_attr, relop, constant))
      return(FALSE);
    
    sprintf(fmt, "select from %s, %s where %s = %s & %s %s %s",
      rel1, rel2, relat[0], relat[1], wh_attr, relop, constant);
    
    if (strlen(fmt) > 2*LINEMAX)
      return(error(LINELONG));
  }
  
  /* if internally formed query preserve pos of line pointer */
  if (lptr != NULL) {
    temp_buf = calloc(1, (unsigned) strlen(lptr));
    strcpy(temp_buf, lptr);  /* copy contents of line buffer */
    temp_lptr = lptr;        /* preserve old lptr address */
  }
  
  if (!parse(fmt, view_def, recursive)) {
    buf_free(relat, r_indx);
    buf_free(attrib, a_indx);
    nfree((char *) rel1);
    nfree((char *) rel2);
    return (FALSE);
  }
  
  nfree((char *) rel1);
  nfree((char *) rel2);
  
  /* restore line pointer if required */
  if (temp_buf != NULL) {
    strcpy(temp_lptr, temp_buf);  /* recover old buf contents */
    lptr = temp_lptr;             /* recover old address */
    nfree(temp_buf);              /* free temporary buffer */
  }

  return (TRUE);
} /* joined */

/* ---------------------------------------------------------- */

do_join(slptr, duplc_attr, ref_attr, d_indx, tbuf)
     struct sel *slptr;
     char **duplc_attr, **ref_attr, *tbuf;
     int  d_indx;
{
  char *name;
  int  i, j, diff, len, abase = 1;
  char **buff;
  struct sattr *saptr;
  BOOL set_dup = FALSE;
  BOOL intake = TRUE;
  
  if (!(buff = dcalloc(d_indx)))
    return (error(INSMEM));
  
  name = SEL_ATTRS->sa_rname; /* name of 1st relation */
  
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    if (strcmp(saptr->sa_rname, name) != EQUAL) { /*  2nd rel */
      for (j = 0; j < d_indx; j++)
	if (strcmp(saptr->sa_aname, duplc_attr[j]) == EQUAL) {
	  set_dup = TRUE; /* attribute names match */
	  break;
	}
      
      /* check if matching attributes hold same data */
      if (set_dup == TRUE) { /* check for data consistency */
	set_dup = FALSE;  /* reset duplicate attribute flag */
	if (ATTRIBUTE_TYPE == TCHAR && (strncmp(buff[j],
	       	saptr->sa_aptr, strlen(buff[j])) == EQUAL)) 
	  continue;  /* value of duplicate attr. already stored */
	else if ( ATTRIBUTE_TYPE != TCHAR) {
	  /* estimate the length difference of ints or reals */
	  diff = ATTRIBUTE_SIZE - strlen(buff[j]);
	  
	  if (diff < 0) {
	    if (strncmp(buff[j] - diff, saptr->sa_aptr, 
			ATTRIBUTE_SIZE) != EQUAL) {
              buf_free(buff, d_indx);
	      return(FAIL);    /* tuple failed test */
            }
	    else
	      intake = FALSE;  /* no entries permitted */
	  } else if (diff == 0) {
	    if ((strncmp(buff[j], saptr->sa_aptr,
			 strlen(buff[j])) != EQUAL)) {
              buf_free(buff, d_indx);
	      return(FAIL);   /* tuple failed test */
            }
	    else
	      intake = FALSE;   /* no entries permitted */
	  } else {  /* diff. > 0 */
	    if ((strncmp(buff[j], (saptr->sa_aptr) + diff,
			 strlen(buff[j])) != EQUAL)) {
              buf_free(buff, d_indx);
	      return(FAIL);    /* tuple failed test */
            }
	    else
	      intake = FALSE;  /* no entries permitted */
	  }
	} else         /* char strings with unequal values */
	  return(FAIL);
      } /* set_dup is FALSE */
    } else {  /* first relation processing */
      for (j = 0; j < d_indx; j++)
	if ((strcmp(saptr->sa_aname, duplc_attr[j]) == EQUAL) || 
	    (ref_attr[j] &&  
	     (strcmp(saptr->sa_aname, ref_attr[j]) == EQUAL))) {
	  len = ATTRIBUTE_SIZE;
	  buff[j] = calloc(1, (unsigned) len + 1);
	  strncpy(buff[j], saptr->sa_aptr, len); /* store duplc. attr values */
	  buff[j][len] = EOS;
	}
    }
    
    if (intake) {
      for (i = 0; i < ATTRIBUTE_SIZE; i++)
	tbuf[abase + i] = saptr->sa_aptr[i];
      
      abase += i;
    }
    intake = TRUE;         /* reset intake flag */
  }
  
  buf_free(buff, d_indx);
  
  return(TRUE);
  
} /* do_join */

/* ----------------------------------------------------------------- */

int add_selattrs(slptr,rptr,join_keys,duplc_attr,ref_attr,d_indx)
     struct sel     *slptr;
     struct relation *rptr;
     char          *join_keys, **duplc_attr, **ref_attr;
     int           *d_indx;
{
  struct sattr *saptr;
  char key, *aname, *name, **joined_attr;
  /* referenced attr in first join relation and its semantically
     equivalent counterpart in second join relation */
  char semeq_aname[ANSIZE + 1], ref_aname[ANSIZE + 1];
  BOOL entry, lock, equiv = FALSE;
  int  i, join_indexed = 0;
  int  indx = 0;
  BOOL first = TRUE;  /* check for semantically equivalent keys only once */
  
  if (!(joined_attr = dcalloc((int) SEL_NATTRS)))
    return(error(INSMEM));
  
  /* initialize join-keys */
  join_keys[0] = 'n';
  join_keys[1] = 'n';
  
  /* preserve name of first relation */
  name = SEL_ATTRS->sa_rname;
  
  /* create the selected attributes */
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    
    /* decide which attr name to use (alternate or normal) */
    if ((aname = saptr->sa_name) == NULL)
      aname = saptr->sa_aname;
    
    /* in case of join must ensure that no repetitive attrs are
     *   entered into the relation */
    if (join_on[0] == NULL)
      entry = TRUE;
    
    /* no locking required */
    lock = FALSE;
    
    /* in case of join, loop thro' the first relation members */
    if (join_on[0] && (strcmp(saptr->sa_rname, name) == EQUAL)) {
      
      /* still at first rel, if join-attribute is indexed */
      joined_attr[indx] = rmalloc(strlen(aname) + 1);
      
      if (strcmp(aname, join_on[0]) == EQUAL)
	if (ATTRIBUTE_KEY != 'n') {
	  join_indexed++;
	  
	  /* copy key-type */
	  join_keys[0] = ATTRIBUTE_KEY;
	}
      
      /* fill-in attributes of first relation */
      strcpy(joined_attr[indx++], aname);
      
      /* entry in resulting relation structure should be made */
      entry = TRUE;
   } else if (join_on[0]) {  /* second rel, assemble dupl names */
     
     /* look for potential semantically equivalent attrs */
     if (first) {
       if (fgn_join(name, saptr->sa_rname, aname, semeq_aname, ref_aname) 
	   == FALSE) 
	 return(FAIL);
       if (*semeq_aname)
	 first = FALSE;
     }    

     /* join on foreign keys */
     if (*semeq_aname && 
	 strcmp(semeq_aname, aname) == EQUAL) {
       equiv = TRUE;
       duplc_attr[*d_indx] = rmalloc(strlen(aname) + 1);
       strcpy(duplc_attr[*d_indx], aname);
       ref_attr[*d_indx] = rmalloc(strlen(ref_aname) + 1);
       strcpy(ref_attr[*d_indx], ref_aname);
       (*d_indx)++;
     }
     
      for (i = 0; i < indx; i++) { /* preserve atribute
	  names of 1st relation in aname and loop thro' 1st rel attrs 
	  in joined_attr buffer and try to locate any matching names */
	
	/* if join-attribute is indexed */
	 if ((lock == FALSE && 
	     strcmp(aname, join_on[0]) == EQUAL) || equiv) {
	  if (ATTRIBUTE_KEY != 'n') {
	    
	    /* lock any further entries */
	    lock = TRUE;
	    
	    join_keys[1] = ATTRIBUTE_KEY;
	    join_indexed++;
	  }
	  equiv = FALSE;
	}
	
	/* load duplicate attribute names in duplicate attr. buffer */
	if (strcmp(aname, joined_attr[i]) == EQUAL) {
	  entry = FALSE;      /* duplicate attr. flag */
	  duplc_attr[*d_indx] = rmalloc(strlen(aname) + 1);
	  strcpy(duplc_attr[*d_indx], aname);
	  ref_attr[*d_indx] = NULL;
	  (*d_indx)++;
	  break;
	} else 
	  entry= TRUE;    /* non-duplicate attr. */
      }
    }
    
    /* if the result is a "snapshot" file do not use indexes */
    if (strncmp(rptr->rl_name, SNAPSHOT, strlen(SNAPSHOT)) == EQUAL)
      key = 'n';
    else
      key =  ATTRIBUTE_KEY;
    
    /* do not add both semantically equivalent attributes */
    if (semeq_aname && strcmp(aname, semeq_aname) == EQUAL)
      entry = FALSE;

    /* add non-duplicate attribute names to form a new rel */
    if (entry)
      if (!add_attr(rptr,aname,ATTRIBUTE_TYPE,
		               ATTRIBUTE_SIZE,key)) {
	nfree((char *) rptr);
	done(slptr);
	return (FAIL);
      }
    
  }  /* exit from saptr loop */
  
  /* when join-attributes have different names and are no-indexed
     illegal join */
  if (join_on[0] && join_on[1])
    if (strcmp(join_on[0], join_on[1]) != EQUAL &&
	join_keys[0] == 'n' && join_keys[1] == 'n')
      return(ILLEGAL);

  /* create the relation header */
  if (!rc_header(rptr, NOINDEX)) {
    done(slptr);
    return (FAIL);
  }
  
  /* return successfully */
  buf_free(joined_attr, indx);
  return (join_indexed);

} /* add_selattrs */

/* ----------------------------------------------------------------- */

fgn_join(rname, tname, aname, fst_aname, secnd_aname)
     char *rname, *tname, *aname, *fst_aname, *secnd_aname;
{
  char fmt[200];
  struct sel *slptr, *db_select(), *retrieve();
  extern union code_cell code[];
  static union code_cell tmp_code[CODEMAX + 1];
  struct pred *tmp_pred[2];
  BOOL tmp_wh_signal;   /* save where-clause flag */

  save_pred(tmp_pred);  /* save predicate structure in case of canned query */

  /* preserve code array contents before retrieve operation */
  structcpy((char *) tmp_code, (char *) code, 
	    (CODEMAX + 1) * sizeof(union code_cell));
  tmp_wh_signal = wh_signal;

 /* the composite relation appears first in the join statement */
  sprintf(fmt, "from syskey where relname \
= \"%s\" & qualifier = \"%s\" & primary = \"%s\";",
       rname, tname, aname);

  if ((slptr = db_select(fmt)) == NULL) {
    fst_aname[0] = EOS;
    restore_pred(tmp_pred); /* restore pred. struct in case of canned query */
    structcpy((char *) code, (char *) tmp_code, 
	      (CODEMAX + 1) * sizeof(union code_cell));
    wh_signal = tmp_wh_signal;
    return(FALSE);
  }

  if (fetch(slptr, TRUE)) 
    assign_key_names(slptr, "primary", fst_aname, "fgnkey", secnd_aname);
  else {    /* target rel apears first in join statement */

    /* close composite relation */
    db_rclose(slptr->sl_rels->sr_scan);

    sprintf(fmt, "project syskey over fgnkey, primary where relname \
= \"%s\" & qualifier = \"%s\" & fgnkey = \"%s\";",
         tname, rname, aname);
    
    if (! parse(fmt, FALSE, FALSE)) {
      fst_aname[0] = EOS;
      restore_pred(tmp_pred);
      structcpy((char *) code, (char *) tmp_code, 
		(CODEMAX + 1) * sizeof(union code_cell));
      wh_signal = tmp_wh_signal;
      return(FALSE);
    }

    slptr = retrieve(NULL);
    
    remove_intermediate(); /* remove file generated by canned query */

    if (slptr == NULL) {
      fst_aname[0] = EOS;
      restore_pred(tmp_pred); /* restore pred. struct */
      structcpy((char *) code, (char *) tmp_code, 
		(CODEMAX + 1) * sizeof(union code_cell));
      wh_signal = tmp_wh_signal;
      return(FALSE);
    }
    
    if (fetch(slptr, TRUE)) 
      assign_key_names(slptr, "fgnkey", fst_aname, "primary", secnd_aname);
    else
      fst_aname[0] = EOS;
  }

  restore_pred(tmp_pred); /* restore pred. struct in case of canned query */
  db_rclose(slptr->sl_rels->sr_scan);

  /* restore code array contents after the retrieve operation */
  structcpy((char *) code, (char *) tmp_code, 
	    (CODEMAX + 1) * sizeof(union code_cell));
  wh_signal = tmp_wh_signal;

  if (expose_exists && temp_cfp) {
    lex_cfp = temp_cfp; 
    temp_cfp = NULL;
  }

  return(TRUE);
} /* fgn_join */

/* ---------------------------------------------------------- */

header_buffs(rptr, slptr, tbuf, aux_buf)
     struct sel *slptr;
     struct relation *rptr;
     char **tbuf, **aux_buf;
{
  
  if (!rc_header(rptr, NOINDEX)) {
    done(slptr);
    return (FALSE);
  }
  
  /* allocate and initialize a tuple buffer */
  if ((*tbuf = (char *)calloc(1, 
	    (unsigned) RELATION_HEADER.hd_size + 1)) == NULL) {
    rc_done(rptr, TRUE);
    return (error(INSMEM));
  }
  **tbuf = ACTIVE;
  
 /* allocate and initialize an auxilliary tuple buffer */
 if (aux_buf != NULL)
   if ((*aux_buf = (char *)calloc(1, 
	    (unsigned) RELATION_HEADER.hd_size + 1)) == NULL) {
    rc_done(rptr, TRUE);
    return (error(INSMEM));
  }
  
  /* initialize has table and word index */
  init_hash();
  
  return(TRUE);
} /* header_buffs */

/* ---------------------------------------------------------- */

create_attrs(rptr, slptr)
     struct relation *rptr;
     struct sel *slptr;
{
  char *aname;
  struct sattr *saptr;
  char key;
  
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    
    /* decide which attribute name to use */
    if ((aname = saptr->sa_name) == NULL)
      aname = saptr->sa_aname;
    
    /* if the result is a "snapshot" file do not use indexes */
    if (strncmp(rptr->rl_name, SNAPSHOT, strlen(SNAPSHOT)) == EQUAL)
      key = 'n';
    else
      key =  ATTRIBUTE_KEY;
    
    /* add the attribute */
    if (!add_attr(rptr, aname, ATTRIBUTE_TYPE, 
		  ATTRIBUTE_SIZE, key)) {
      nfree((char *) rptr);
      done(slptr);
      return (FALSE);
    }
  }
  
  return(TRUE);
} /* create_attrs */

/* ---------------------------------------------------------- */

int check_union(rptr, slptr, f_rel, name)
     struct relation *rptr;
     struct sel *slptr;
     struct unattrs  f_rel[];
     char **name;
{
  struct unattrs  s_rel[NATTRS];
  struct sattr *saptr;
  unsigned f_indx = 0, s_indx = 0;
  int i, j;
  char *aname;
  
  /* check if the relations have the same attribute names 
     and types */
  *name = SEL_ATTRS->sa_rname;
  
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    if (strcmp(saptr->sa_rname, *name) == EQUAL) {
      
      /* preserve names, sizes and types of 1st relation */
      f_rel[f_indx].size = ATTRIBUTE_SIZE;
      f_rel[f_indx].name = rmalloc(RNSIZE + ANSIZE + 1);
      sprintf(f_rel[f_indx++].name, "%s%c", saptr->sa_aname,
           ATTRIBUTE_TYPE);
    } else { /* preserve name and type of 2nd rel attribute */
      s_rel[s_indx].size = ATTRIBUTE_SIZE;
      s_rel[s_indx].name = rmalloc(RNSIZE + ANSIZE + 1);
      sprintf(s_rel[s_indx++].name, "%s%c", saptr->sa_aname,
           ATTRIBUTE_TYPE);
    }
  }
  
  /* compare two rels according to attribute names, and types */
  for (i = 0; i < s_indx; i++)
    for (j = 0; j < f_indx; j++)
      if (s_indx == f_indx && (strcmp(s_rel[j].name, 
                          f_rel[i].name) == EQUAL)) {
     
     /* get max relation size */
     f_rel[i].size = MAX(f_rel[i].size, s_rel[j].size);
     break;
      } /* if last attribute of 2nd relation reached, error */
      else if ( j == f_indx - 1)
     return (error(UNOP));
  
  /* create the selected attributes and decide about new 
   * relation size loop only thro' attrs of first relation */
  for (saptr = SEL_ATTRS, i = 0; saptr != NULL 
       && (strcmp(saptr->sa_rname, *name) == EQUAL); 
       saptr = saptr->sa_next) {
    
    /* decide which attribute name to use, in case of aliases */
    if ((aname = saptr->sa_name) == NULL)
      aname = saptr->sa_aname;
    
    /* add the attributes in the union-relation
     * allowing enough space for the longest common attribute */
    if (!add_attr(rptr, aname, ATTRIBUTE_TYPE, 
		      f_rel[i++].size, ATTRIBUTE_KEY)) {
      nfree((char *) rptr);
      done(slptr);
      return (FALSE);
    }
  }
  
  /* free union buffer space */
  free_union(s_rel, (int) SEL_NATTRS);
  
  return(TRUE);
} /* check_union */

/* ---------------------------------------------------------- */

free_union(data_struct, tcnt)
     struct unattrs data_struct[];
     int tcnt;
{
  int             i;
  
  for (i = 0; i < tcnt; i++)
    nfree(data_struct[i].name);
} /* free_union */

/* ---------------------------------------------------------- */

BOOL update_attrs(pixd, slptr, tcnt, doffset)
     struct ix_header *pixd;
     struct sel *slptr;
     int        *tcnt;
     long       doffset;
{
  struct sattr *saptr, *preserved_saptr;
  struct attribute *aptr, *preserved_aptr;
  char      avalue[STRINGMAX+1], old_value[STRINGMAX+1], 
            preserved_value[STRINGMAX+1];
  BOOL    insertion = FALSE;
  BOOL    updating = TRUE;
  char    attr_key, *rem_blanks();
  int     result;
  char *prompt_input();
  int fkeys_cnt = 0;	/* counts foreign key ocurrences */

  printf("-----------------------------\n");
  
  /* loop through the selected attributes */
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    
    /* get the attribute pointer */
    aptr = saptr->sa_attr;
    
    /* get and store the old attribute value */
    get_attr(aptr, saptr->sa_aptr, avalue);
    rem_blanks(old_value, avalue, aptr->at_size);
    
    /* prompt and input attribute values */
    if (prompt_input(aptr, insertion, avalue) == NULL)
      return(FALSE);
    
    attr_key = aptr->at_key;
    
    /* store the attribute value */
    if (avalue[0] != EOS) {
      store_attr(aptr, saptr->sa_aptr, avalue);
      saptr->sa_srel->sr_update = TRUE;
      if (doffset != 0)
	SEL_SCAN->sc_recpos = doffset;
      
      if ((attr_key != 'n')) 
	update_keys(pixd, slptr,saptr,aptr,&doffset, old_value, avalue);
      if (pixd != NULL) 
        if (pixd->dx.ixatts.ix_foreign == 't' && attr_key == 'p') {
	  fkeys_cnt++;
          /* NEW CHANGE */
          /* Something wrong with this part */
          /* composite relation, may need to preserve foreign key value */
          strcpy(preserved_value, old_value);
          preserved_saptr = saptr;
          preserved_aptr = aptr;
      }
    } else {
	if (pixd != NULL)
	  if (pixd->dx.ixatts.ix_foreign == 't' && attr_key == 'p') {
          /* composite relation, may need to preserve foreign key value */
          strcpy(preserved_value, old_value);
          preserved_saptr = saptr;
          preserved_aptr = aptr;
        }
    }
  }

  if (fkeys_cnt == 1)
    update_keys(pixd, slptr, preserved_saptr, preserved_aptr, &doffset, 
          /* NEW CHANGE */
          /* the last preserved_value should be changed to avalue */
		preserved_value, avalue);
    
  /* update the tuple, and commit changes */
  result = db_commit(pixd, slptr, updating, tcnt);
  
  /*  free  buffers */
  buf_free(key_buf, 4);
  key_buf = NULL;
  buf_free(del_buf, 4);
  del_buf = NULL;
  
  /* result obtains three values TRUE, FALSE and FAIL */
  return(result) ;
  
} /* update_attrs */

/* ---------------------------------------------------------- */

BOOL delete_attrs(pixd, slptr, tcnt, doffset)
     struct ix_header *pixd;
     struct sel *slptr;
     int        *tcnt;
     long       doffset;
{
  struct ix_header *ixd;
  struct sattr *saptr;
  struct attribute *aptr;
  char  *rem_blanks(), attr_key, 
         avalue[STRINGMAX+1], old_value[STRINGMAX+1];
  int result;              /* result of a commit operation */
  BOOL    update = FALSE;  /* perform deletion */
  
  if (! pixd)
    ixd = (struct ix_header *) calloc(1, sizeof (struct ix_header));
  else
    ixd = pixd;

  /* read tuple from data file if doffset is specified */
  if (!doffset)
    doffset = SEL_SCAN->sc_recpos;
  
  /* loop through the selected attributes */
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    attr_key = ATTRIBUTE_KEY;
    
    /* get the attribute pointer */
    aptr = saptr->sa_attr;
    
    /* get and store the old attribute value */
    get_attr(aptr, saptr->sa_aptr, avalue);
    rem_blanks(old_value, avalue, aptr->at_size);
    
    /* signifies tuple deletion */
    avalue[0] = EOS;
    
    if ((attr_key != 'n')) { /* if attribute indexed */
      if (!update_keys(ixd, slptr, saptr, aptr, 
		               &doffset, old_value, avalue))
     return(FALSE);
    }
  }
  
  /* preserve the adddress of the data tuple */
  if (doffset != 0)
    SEL_SCAN->sc_recpos = doffset;
  
  /* store changes into data header */
  SEL_SCAN->sc_store = TRUE;
  
  /* delete the tuple, and commit changes */
  result = db_commit(ixd, slptr, update, tcnt);
  
  /*  free delete buffer */
  buf_free(del_buf, 4);
  del_buf = NULL;
  
  /* result obtains three values TRUE, FALSE and FAIL */
  return(result) ;
  
} /* delete_attrs */

/* ---------------------------------------------------------- */

semid_attrs(comp_name, fgn_key, qualifier, aname)
     char *comp_name;
     char fgn_key[][ANSIZE+1], 
          qualifier[][RNSIZE+1], aname[][ANSIZE+1];
{
  int i, j, k;
  struct scan *target, *comp, *db_ropen();

  if ((comp = db_ropen(comp_name)) == NULL)
    return(FALSE);

  /* set flag for semantically identical attrs in composite rel */
  for (i = 0;comp->sc_relation->rl_header.hd_attrs[i].at_name[0]
                                                  != EOS; i++) {
    for (j = 0; j < 2; j++) {
      if (strcmp(comp->sc_relation->rl_header.hd_attrs[i].at_name, 
		 fgn_key[j]) == EQUAL) {
     comp->sc_relation->rl_header.hd_attrs[i].at_semid = 't';
     break;
      }
    }
  }
  comp->sc_store = TRUE;
  db_rclose(comp);
  free((char *) comp);

  /* set flag for semantically identical attrs in target rels */
  for (k = 0; k < 2; k++) {
    if ((target = db_ropen(qualifier[k])) == NULL)
      return(FALSE);
    
    for (i = 0;target->sc_relation->rl_header.hd_attrs[i].at_name[0] 
	 != EOS; i++) {
      for (j = 0; j < 2; j++) {
	if (strcmp(target->sc_relation->rl_header.hd_attrs[i].at_name, 
		   aname[j]) == EQUAL) {
	  target->sc_relation->rl_header.hd_attrs[i].at_semid = 't';
	  break;
	}
      }
    }
    target->sc_store = TRUE;
    db_rclose(target);
    free((char *) target);
  }
  
  return(TRUE);
} /* semid_attrs */

/* ---------------------------------------------------------- */

assign_key_names(slptr, fst_aname, fst_avalue, scnd_aname, scnd_avalue)
  struct sel *slptr;
  char *fst_aname, *fst_avalue, *scnd_aname, *scnd_avalue;
{
  struct sattr *saptr;

  for (saptr = SEL_ATTRS; saptr; saptr = saptr->sa_next) {
    if (strcmp(saptr->sa_aname, fst_aname) == EQUAL)
      strcpy(fst_avalue, saptr->sa_aptr);
    else if (strcmp(saptr->sa_aname, scnd_aname) == EQUAL)
      strcpy(scnd_avalue, saptr->sa_aptr);
  }
} /* assign_key_names */

/* --------------------- EOF  impl.c -------------------------- */

