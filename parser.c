

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

/* REQUIEM - command parser */

#include "requ.h"
#include "btree.h"

extern int    lex_token;
extern char   lexeme[];
extern int    saved_ch;
extern float  lex_value;
extern int    xint_error;         /* interpretation error */
extern char   errvar[];           /* error  variable */
extern struct cmd_file *lex_cfp;   /* command file pointer */
extern struct ix_header *pixd;
extern struct hash **Hash_Table;
extern int    HTIndex;
extern BOOL   view_exists, restore_paren;
extern int    saved_tkn;
extern BOOL   wh_signal;
extern BOOL   non_ixattr;        /* index predicate flag */
extern BOOL   canned_commit;

extern int    help(), db_import(), drop(), quit(), 
              view_define(), view_show(), db_extract(); 
struct  cmd_file *temp_cfp = NULL;         /* command file context ptr */
struct pred   *prptr[2] = {NULL, NULL};   /* predicate attribute structure */
char          *join_on[2] = {NULL, NULL}; /* names of join-attributes */
int           sref_cntr = 0;    /* snapshot reference counter */
int           command;		/* holds the type of the current command */
static assign(), s_count(), create(), difference(), delete(), 
       display(), export(), extract(),  focus(), import(), 
       insert(), intersect(), modify(), print(), c_project(), 
       purge(), c_select(), c_expose(), update(),
       join(), c_union();

int    terminator();

char    cformat[65];           /* print control format */
BOOL    no_print = FALSE;      /* print control flag */
BOOL    view_def = FALSE;      /* view definition control flag */
BOOL    ask = TRUE;	       /* interactive purge flag */
BOOL    expose_exists = FALSE; /* expose operation in progress */

static char rel_fl[RNSIZE+5];

static struct command {   /* command parsing structure */
  int  cmd_token;         /* token definition of command */
  int  (*cmd_function)(); /* command function pointer */
} ;
static struct command despatch_table[] = {
  { ASSIGN, assign },
  { COUNT, s_count },
  { CREATE, create },
  { DEFINE, view_define },
  { DELETE, delete },
  { DIFFERENCE, difference },
  { DISPLAY, display },
  { DROP, drop},
  { EXIT, quit },
  { EXPORT, export },
  { EXPOSE, c_expose },
  { EXTRACT, extract },
  { FOCUS, focus },
  { HELP, help },
  { IMPORT, import },
  { INSERT, insert },
  { INTERSECT,  intersect },
  { JOIN, join },
  { MODIFY, modify },
  { PRINT, print },
  { PROJECT, c_project }, 
  { PURGE, purge },
  { QUIT, quit },
  { SELECT, c_select },
  { TERMINATOR, terminator },
  { SHOW, view_show },
  { UNION,  c_union },
  { UPDATE, update }
};

/* ---------------------------------------------------------- */

/* parse - parse a command */
int     parse(q_fmt, def_view, recursive)
     char     *q_fmt;
     BOOL     def_view;
     BOOL    recursive;    /* TRUE if parse is called recursive */
{    
  BOOL   status;
  int    c_type;
  
  /* estimate the number of table entries */
  int table_entries = 
    sizeof(despatch_table) / sizeof(struct command);
  
  /* check for a command line */
  if (q_fmt)
    q_scan(q_fmt);
  
  /* set view definition flag */
  view_def = def_view;
 
  /* reset index predicate flag */
  non_ixattr = FALSE;

  /* determine the current statement type */
  c_type = next_token();
  
  /* find command and call the corresponding function */
  status = evaluate(c_type, despatch_table, table_entries);
  
  /* clear command line */
  if (view_def)
    saved_ch = '\n';

  if (q_fmt && ! recursive)
    cmd_clear();    

  /* free predicate attribute structure */
  free_pred(prptr);

  if (!q_fmt && temp_cfp) {
    lex_cfp = temp_cfp;           /* restore cf pointer */
    temp_cfp = NULL;
  }

  /* returns true or false from evaluation of parse routines */
  return (status);
}

/* ---------------------------------------------------------- */

/* evaluate type of command and call parser function */
BOOL evaluate(cmd_type, despatch, cmd_cntr)
     int cmd_type, cmd_cntr;
     struct command despatch[];
{    
  struct command *cmd_ptr;
  int func_res = FAIL;     /* result from a function call */
  
  for (cmd_ptr = despatch; 
       cmd_ptr <= &despatch[cmd_cntr-1]; cmd_ptr++)
    
    if (cmd_type == (*cmd_ptr).cmd_token) {
      func_res = (*(*cmd_ptr).cmd_function)();
      break;
    }
  
  if (func_res == FAIL)
    return(error(SYNTAX));
  else 
    return(func_res);
  
} /* evaluate */

/* ---------------------------------------------------------- */

static int difference()
{
  command = DIFFERENCE;
  return(db_union(FALSE, TRUE));
}

/* ---------------------------------------------------------- */

static int export()
{
  command = EXPORT;
  return(db_export((char *) 0));
}

/* ---------------------------------------------------------- */

static int c_expose()
{
  command = EXPOSE;
  return(expose(no_print));
}

/* ---------------------------------------------------------- */

static int extract()
{
  command = EXTRACT;
  return(db_extract((char *) 0));
}

/* ---------------------------------------------------------- */

static int import()
{
  command = IMPORT;
  return(db_import((char *) 0));
}

/* ---------------------------------------------------------- */

static int intersect()
{
  command = INTERSECT;
  return(db_union(FALSE, FALSE));
}

/* ---------------------------------------------------------- */

static int join()
{
  command = JOIN;
  return(nat_join(no_print, FALSE));
}

/* ---------------------------------------------------------- */

static int c_project()
{
  command = PROJECT;
  return(project(no_print));
}

/* ---------------------------------------------------------- */

static int c_select()
{
  command = SELECT;
  return(requiem_select(no_print));
}

/* ---------------------------------------------------------- */

static int c_union()
{
  command = UNION;
  return(db_union(TRUE, FALSE));
}

/* ---------------------------------------------------------- */

terminator()
{  
  if (cformat[0] != EOS) {
    rprint(cformat);
    clear_mem(cformat, 0, 65);
  }
  
  return(TRUE);
} /* terminator */

/* ---------------------------------------------------------- */

/* create - create a new relation */
static int     create()
{
  struct relation *rc_create();
  struct relation *rptr, *temp_rptr;
  char  key, aname[2][ANSIZE+1], fgn_key[2][ANSIZE+1];
  char  qualifier[2][RNSIZE+1], rname[RNSIZE+1], 
        **args, **dcalloc();
  int     atype, i, j, arg_index, nattrs = 0, pc = 0, sc = 0;
  BOOL fgn_exists = FALSE;
  
  fgn_key[0][0] = EOS;
  fgn_key[1][0] = EOS;

  /* get relation name and start relation creation */
  if (next_token() != ID)
    return (error(SYNTAX));
  
  /* check size of relation id */
  if (strlen(lexeme) >= RNSIZE)
    return (error(WRLEN));
  strcpy(rname, lexeme);
  
  if ((rptr = rc_create(rname)) == NULL)
    return (FALSE); /* create a new relation structure */
  
  if (next_token() != '(')      /* check for attribute list */
    return(rc_error(rptr));
  
  /* parse the attributes */
  while (TRUE) {
    if (next_token() != ID)  /* get the attribute name */
      return(rc_error(rptr));
    
    /* check size of attribute id */
    if (strlen(lexeme) >= ANSIZE)
      return (error(WRLEN));
    strcpy(aname[0], lexeme);
    
    if (next_token() != '(') /* line opening parenthesis */
      return(rc_error(rptr));
    
    /* get the attribute type */
    next_token();
    if (lex_token == CHAR) {
      atype = TCHAR;
    }
    else if (lex_token == NUM) {
      atype = TNUM;
    }
    else if (lex_token == REAL){
      atype = TREAL;
    }
    else
      return(error(UNDEFTYPE));
    
    if (next_token() != '(')   /* size opening parenthesis */
      return(rc_error(rptr));
    
    if (next_token() != NUMBER) /* get the attribute size */
      return(rc_error(rptr));
    
    if (next_token() != ')')    /* size right parenthesis */
      return(rc_error(rptr));
    
    key = 'n'; /* non-key attribute */
    if (this_token() == ',') {  /* check for key specs */
      next_token();
      
      if (!chk_key_defs(aname[0], &key, rptr,&pc, &sc, atype))
	return(rc_error(rptr));
    }
    
    if (next_token() != ')')    /* line right parenthesis */
      return(rc_error(rptr));
    
    /* add the attribute to created relation structure */
    nattrs++;

    /* increment the size by one to cater for EOS */
    if (!add_attr(rptr, aname[0], atype, (int)lex_value + 1, key)) {
      nfree((char *) rptr);
      return(FALSE);
    }
    
    /* check for comma or final closing paren. */
    switch (this_token()) {
    case ',':
      next_token();
      break;
    case ')':
      break;
      
    default:
      next_token();
      return(rc_error(rptr));
    }
    
    /* check for end of attributes */
    if (this_token() != ID)
      break;
  }

  /* if foreign keys exist */
  for (i = 0; (this_token() == FOREIGN) && i < 2; i++) {
    next_token();
    fgn_exists = TRUE; /* foreign key exists */
    
    /* get the name of the foreign key */
    if (next_token() != KEY)
      return(rc_error(rptr));
    
    if (next_token() != ID)
      return(rc_error(rptr));
    else
      strcpy(fgn_key[i], lexeme);
    
    if (next_token() != REFERENCES)
      return(rc_error(rptr));
    
    /* get the name of the primary key */
    if (next_token() != ID)
      return(rc_error(rptr));
    strcpy(aname[i], lexeme);  
    
    if (next_token() != IN)
      return(rc_error(rptr));
    
    if (next_token() != ID)
      return(rc_error(rptr));
    
    /* preserve name of qualifying relation */           
    strcpy(qualifier[i], lexeme);                           
    
    if (next_token() != ';')
      return(rc_error(rptr));
    
    for (j = 0; j < nattrs; j++) /* get type of foreign key */
      if (strcmp(fgn_key[i], rptr->rl_header.hd_attrs[j].at_name) == EQUAL) {
	atype = rptr->rl_header.hd_attrs[j].at_type;
	break;
      }

    if (!chk_fgn_key(rptr,fgn_key[i],qualifier[i],aname[i],&pc, atype))
      return(error(WRNGFGNKEY));
    else                /* change key specs to primary */
      change_key(rptr, fgn_key[i], 'p');
    
  } 
  
  if (next_token() != ')')   /* check for attribute list end */
    return(rc_error(rptr));
  
  
  /* retain a temporary copy of structure rptr */
  if ((temp_rptr = 
       CALLOC(1, struct relation)) == (struct relation *) NULL)
    return(FALSE);

  structcpy((char *) temp_rptr, 
	    (char *) rptr, sizeof(struct relation));

  /* finish the relation creation and construct index */
  if (!cre_finish(rptr)) {
    free((char *) rptr);
    return(FALSE);
  }
  
  /* make appropriate catalog entries */
  if (!cat_entry(temp_rptr, nattrs, rname, fgn_key)) {
    cre_finish(rptr);
    free((char *) rptr); free((char *) temp_rptr);
    return(FALSE);
  }
  
  if (fgn_exists) {
    
    /* allocate argument array to hold catalague parameters */
    arg_index = 4;
    args = dcalloc(arg_index);

    for (i = 0; i < 2; i++) {
      /* insert key catalog arguments in argument array */
      args[0] = fgn_key[i];
      args[1] = rname;
      args[2] = qualifier[i];
      args[3] = aname[i];
      
      if (!cat_insert("syskey", args, arg_index)) {
	nfree(args);
	return(FALSE);
      }
    }
    /* decide about the semantically equivalent attributes */
    if (! semid_attrs(rptr->rl_name, fgn_key, qualifier, aname))
      return(FALSE);

    nfree(args); 
  }
  
  free((char *) rptr); 
  free((char *) temp_rptr);
  sprintf(cformat, " Relation %s created  ", rname);
  rprint(cformat);
  cformat[0] = EOS;
  return (TRUE);
  
}/* create */

/* ---------------------------------------------------------- */

/* insert - insert a tuple into a relation */
static int     insert()
{
  struct scan *sptr, *db_ropen();
  struct attribute *aptr;
  char     avalue[STRINGMAX+1], base_name[ANSIZE];
  char    **key_buf = NULL;
  int     tcnt, astart, i;
  int     p_cntr = 0, /* primary key counter */
          s_cntr = 2; /* secondary key counter */
  BOOL    insertion = TRUE;
  char *prompt_input();

  if (nxtoken_type() != ID)
    return(error(SYNTAX));
  
  /* preserve the relation-view name */
  strcpy(base_name, lexeme);
  
  /* make sure that the rest of the line is blank */
  if (!db_flush())
    return (FALSE);
  
  if ((which_view(base_name)) == FAIL)
    return(error(SYNTAX));
  
  /* open the relation, return a scan structure pointer */
  if ((sptr = db_ropen(base_name)) == NULL)
    return (FALSE);
  
  /* insert tuples */
  for (tcnt = 0; ; tcnt++) { /* count inserted tuples */
    
    if (tcnt != 0)    /* print separator if not first tuple */
      printf("-------------------------------\n");
    
    /* get attr values, astart points past tuple status code */
    astart = 1;
    for (i = 0; i < sptr->sc_nattrs; i++) {
      
      /* get a pointer to the current attribute */
      aptr = &SCAN_HEADER.hd_attrs[i];
      
      /* check for the last attribute */
      if (aptr->at_name[0] == EOS)
	break;
      
      /* prompt and input attribute values */
      if (prompt_input(aptr, insertion, avalue) == NULL)
	return(FALSE);
      
      /* check for last insert */
      if (avalue[0] == EOS)
	break;
      
      /* place the attribute value in sc_tuple */
      store_attr(aptr, &SCAN_TUPLE[astart], avalue);
      
      /* update the attribute start */
      astart += aptr->at_size;
      
      /* check if indexing is required */
      if (aptr->at_key != 'n')
	check_ix(&key_buf, aptr, &p_cntr, &s_cntr, avalue);
    }
    
    /* all attr values for this tuple have been processed */
    if (avalue[0] == EOS)       /* check for last insert */
      break;
    
    /* store the new tuple */
    if (!store_tuple(sptr, key_buf)) {
      buf_free(key_buf, 4);
      key_buf = NULL;
      db_rclose(sptr); /* close relation file */
      saved_ch = '\n';   /* force a newline */
      return (FALSE);
    }
    
    /* reset  counters and free buffer */
    p_cntr = 0;
    s_cntr = 2;
  }
  
  /* close the relation and the index */
  db_rclose(sptr);
  
  /* check number of tuples inserted */
  if (tcnt != 0)
    sprintf(cformat, "# %d tuples inserted #", tcnt);
  else
    sprintf(cformat, "# no tuples inserted #");
  
  /* return successfully */
  buf_free(key_buf, 4);
  key_buf = NULL;
  rprint(cformat);
  cformat[0] = EOS;     /* print result of insertion */
  return (TRUE);
  
} /* insert */

/* ---------------------------------------------------------- */

static int     modify()
{
  
  /* get relation name */
  if (next_token() != ID)
    return(error(SYNTAX));
  
  /* make sure that the rest of the line is blank */
  if (!db_flush())
    return (FALSE);
  
  if (!db_modify())
    return(FALSE);
  
  /* return successfully */
  return(TRUE);
  
} /* modify */

/* ---------------------------------------------------------- */

static int assign()
{
  char trans_file[RNSIZE+5];
  
  if (next_token() != ID)
    return (error(SYNTAX));
  
  /* keep transient relation name */
  strcpy(trans_file, lexeme);
  strcat(trans_file, ".db");
  
  if (next_token() != TO)
    return (error(SYNTAX));
  
  /* parse the assignment statement */
  if (!parse((char *)NULL, FALSE, FALSE))
    return(FALSE);
  
  if (!db_assign(trans_file))
    return(FALSE);
  else
    return(TRUE);
  
} /* assign */

/* ---------- purge a base or transient relation ------------ */
static int purge()
{
  struct sel  *retrieve(), *slptr = NULL;
  char response[5], prompt[65]; 
  char rname[RNSIZE + 1], fmt[70];
  char *getfld();

  if (nxtoken_type() != ID)
    return (error(SYNTAX));
  
  if (strncmp(lexeme, "sys", 3) == EQUAL || 
                        fexists(lexeme) == FALSE)
    return(error(WRNRELREM));
  
  /* keep transient relation name */
  strcpy(rname, lexeme);

  if (ask) {
    sprintf(prompt,
      "Irrecoverable relation deletion should I go ahead?\t: ");
    getfld(prompt, response, 4, stdin);
  }

  if (! ask || strcmp(response, "yes") == EQUAL)
    if(!clear_catalogs(rname))
       return(FALSE);  
   
  /* project syskey over current relation, see if it is a target relation */
  sprintf(fmt, 
	  "syskey over relname where qualifier = \"%s\";", rname);

  if ((slptr = retrieve(fmt)) == NULL)
    return(FALSE);
  else
    cmd_kill();   /* clear command line */
 
  /* loop through each relation name projected from SYSKEY */
  while (fetch(slptr, TRUE)) {
    /* purge composite relation */
    if (!clear_catalogs(SEL_ATTRS->sa_aptr)) {
      ask = TRUE;
      return(error(WRNRELREM));  
    }
  }
  
  if (slptr)
    db_rclose(slptr->sl_rels->sr_scan);

  if (temp_cfp) {
    lex_cfp = temp_cfp;           /* restore cf pointer */
    temp_cfp = NULL;
  }
  
  return(TRUE);
} /* purge */

/* ------------ delete tuples from a relation --------------- */
static int     delete()
{
  struct sel *retrieve(), *delete_view();
  struct sel *slptr;
  int     cntr = 0;  /* deleted tuple counter */
  BOOL hash = FALSE, del;
  static del_cnt = 0;
  static char comp_rlname[RNSIZE + 1];

  /* delete operation performed through a view ? */
  if (token_type() != ID)
    return(FALSE);
  
  if (!fexists(lexeme)) {   /* it is a view call */     
    if ((slptr = delete_view()) == (struct sel *)NULL)
      return (FALSE);				
  } else {
    /* parse the retrieval clause */
    if ((slptr = retrieve((char *)NULL)) == (struct sel *)NULL)
      return (FALSE);
  }

  if (SEL_RELATION->rl_header.hd_tcnt == 0) {
    sprintf(cformat, "#   no tuples to delete    #");
    /* finish the selection, clear and reset buffers */
    done(slptr);
    return(TRUE);
  }

  if (! wh_signal) {/* no predicate specified */
    /* finish the selection, clear and reset buffers */
    done(slptr);
    return(error(SYNTAX));
  }

  if (!prptr[0]) {   /* if predicate non-indexed */
    /* loop through the selected tuples */
    for ( ; fetch(slptr, TRUE); ) {
      del = delete_attrs(NULL, slptr, &cntr, 0L);
      if (del == FALSE) {
	done(slptr);
	return(FALSE);
      } else if (del == FAIL)
	break;
    }
  } else { /* predicate attribute indexed */
    
    /* implement deletion algorithm */
    if (!access_plan(prptr[0], (struct relation *)NULL, 
		        slptr, &cntr, hash, DELETION)) {
      done(slptr);
      return(FALSE);
    }
  }
    
  /* detect existence of interpretation error */
  if (xint_error) {
    xint_error = FALSE;
    done(slptr);
    return(FALSE);
  }
  
  if (canned_commit) {
    del_cnt += cntr;
    strcpy(comp_rlname, SEL_RELATION->rl_name);
  } else {
    /* check number of tuples deleted */
    if (cntr > 0)
      sprintf(cformat, "#  %d deleted in relation %s  #", cntr,
	      SEL_RELATION->rl_name);
      else
	sprintf(cformat, "#   no tuples deleted in relation %s   #",
		SEL_RELATION->rl_name);

    if (del_cnt > 0) {
      terminator();
      sprintf(cformat, "#  %d deleted in relation %s  #", 
	      del_cnt, comp_rlname); 
      del_cnt = 0;
    }
  }

  /* finish the selection, clear and reset buffers */
  done(slptr);
  
  terminator();
  
  /* return successfully */
  return (TRUE);
  
} /* delete */

/* ------------ update tuples from a relation -------------- */
static int     update()
{
  struct sel *db_select(), *update_view(), *slptr;
  struct relation *rptr;
  BOOL    updt, updating = TRUE, hash = FALSE, res = TRUE;
  int     cntr = 0;
  
  /* parse the select clause */
  if ((slptr = db_select((char *) 0)) == (struct sel *) 0)
    return (FALSE);
  
  if (! wh_signal) /* no predicate specified */
    return(error(SYNTAX));

  /* make sure that the rest of the line is blank */
  if (!db_flush()) {
    done(slptr);
    return (FALSE);
  }
  
  if (!prptr[0]) {   /* if predicate non-indexed */
    /* loop through the selected tuples */
    for ( ; fetch(slptr, TRUE); ) {
      updt = update_attrs(NULL, slptr, &cntr, 0L);
      if (updt == FALSE) {
	done(slptr);
	return(FALSE);
      } else if (updt == FAIL)
	break;
    }
  } else { /* predicate attribute indexed */
    /* create a new relation */
    if (!new_relation(&sref_cntr, &rptr, slptr, prptr, rel_fl)) {
      done(slptr);
      return(FALSE);
    }

   if ((res = access_plan(prptr[0], rptr, slptr, &cntr, hash, updating)) < 1) {
      done(slptr);
      saved_ch = '\n';   /* force a newline */
    }
  }

  if (res == TRUE) {
    /* finish the selection, clear and reset buffers */
    done(slptr);
  
    /* detect existence of interpretation error */
    if (xint_error) {
      xint_error = FALSE;
      return(FALSE);
    }
  }
  
  /* check number of tuples updated */
  if (cntr > 0)
    sprintf(cformat, "#  %d  tuples updated   #", cntr);
  else
    sprintf(cformat, "#  no tuples updated    #");
  
  
  if (res == FAIL)
    terminator();

  /* return successfully */
  return (TRUE);
  
} /* update */

/* --------- print tuples from a set of relations ----------- */
static int     print()
{
  struct sel *retrieve(), *db_select(), *slptr;
  FILE *ffp = NULL, *ofp;
  
  /* reset view existence flag */
  view_exists = FALSE;
  
  /* check for "using <fname>" */
  if (this_token() == USING) {
    next_token();
    
    /* parse the using clause */
    if (!using(&ffp, ".form"))
      return (FALSE);
    
    /* parse the select clause */
    if ((slptr = db_select((char *) 0)) == (struct sel *) 0)
      return (FALSE);
  }
  else if  /* parse the retrieval clause */
    ((slptr = retrieve((char *) 0)) == (struct sel *) 0)
      return (FALSE);
  
  /* parse the into clause */
  if (!redirect_to(&ofp, ".txt")) {
    done(slptr);
    return (FALSE);
  }
  
  /* check for normal or formatted output */
  if (ffp == NULL)
    table(ofp, slptr);
  else
    form(ofp, slptr, ffp);
  
  /* finish the selection */
  done(slptr);
  
  /* close the form definition file */
  if (ffp != NULL)
    fclose(ffp);
  
  /* close the output file */
  if (ofp != stdout)
    fclose(ofp);
  
  /* free command line */
  next_token();
  
  /* return successfully */
  db_remove();          /* remove all temporary files */
  return (TRUE);
  
} /* print */

/* ---------------------------------------------------------- */

/* count the no. of tuples in a relation */
static int s_count()
{
  struct relation *rlptr;
  struct relation *rfind();
  char relname[RNSIZE];
  
  if (next_token() != ID)
    return (error(SYNTAX));
  
  strcpy(relname, lexeme);
  if ((rlptr = rfind(relname)) == NULL)
    return (FALSE);
  
  if (this_token() != ';')
    return(error(SYNTAX));
  
  sprintf(cformat, "<   %d tuples counted    >", 
	                     rlptr->rl_header.hd_tcnt);
  
  return(TRUE);
  
}

/* ---------------------------------------------------------- */

/* db_focus - focus on current tuple where focus lies */
static int     focus()
{
  
  if (next_token() != ON)
    return (error(SYNTAX));
  
  /* parse the relation file clause */
  if (next_token() != ID)
    return (error(SYNTAX));
  
  if (!db_focus()) /* implement focus */
    return(FALSE);
  
  /* return successfully */
  return(TRUE);
  
} /* focus */

/* ---------------------------------------------------------- */

/* display the current tuple of a specified relation */
static int     display()
{
  struct sel *retrieve();
  struct sel *slptr;
  int     c_width = 0, offset = 0;
  int     reset = FALSE;
  
  /* compute the displacement */
  if (this_token() == SIGNEDNO) {
    offset = atoi(lexeme); /* compute a signed number */
    next_token();
    
    if (this_token() == ',' ) {
      next_token();
      if (next_token() != NUMBER)
	error(SYNTAX);
      else
	c_width = (int) lex_value;
    }
  } else
    reset = TRUE;
  
  /* parse the "using" clause */
  if (next_token() != USING)
    return (error(SYNTAX));
  
  /* parse the select clause */
  if ((slptr = retrieve((char *) 0)) == (struct sel *) 0)
    return (FALSE);
  if (reset)
    SEL_HEADER.hd_cursor = 0;
  
  if (!db_display(slptr, c_width, offset)) {
    done(slptr);
    return(FALSE);
  }

  done(slptr);
  /* return successfully */
  return(TRUE);
  
} /* display */

/* ---------------------------------------------------------- */

/* select - select tuples from a set of relations */
int     requiem_select(supress)
     BOOL     supress;
{
  struct sel *db_select();
  struct sel *slptr;
  struct relation *rptr;
  struct sattr *saptr;
  char     *tbuf;
  int      abase;
  int rc;
  char     **duplc_attr, **ref_attr, join_keys[2];
  BOOL    no_product = TRUE; 
  int     join_cntr = 0;
  int     d_indx = 0;
  int tcnt = 0;
  int decr = 0;

  /* parse the select clause */
  if ((slptr = db_select((char *) 0)) == (struct sel *) 0) {
    
    free_pred(prptr);
    
    return (FALSE);
  }
  
  /* if view definition exit */
  if (view_def) {
    done(slptr);
    return(TRUE);
  }

  /* create a new relation */
  if (!new_relation(&sref_cntr, &rptr, slptr, prptr, rel_fl)) {
    done(slptr);
    return(FALSE);
  }

  /* create a joined attribute buffer if required */
  if (join_on[0] != (char *) 0) {
    no_product = FALSE; /* x-product required */
    if (!(duplc_attr = dcalloc(10)))  {
      done(slptr);
      return (error(INSMEM));
    }

    if (!(ref_attr = dcalloc(10))) {
      done(slptr);
      return (error(INSMEM));
    }
  }

  
  /* add the selected attributes to the newly created relation,
     in case of join eliminate attribute duplication; return the number
     of indexed join attributes */
  join_cntr = add_selattrs(slptr, rptr, join_keys, 
			     duplc_attr, ref_attr, &d_indx);
  
  if (join_on[0] && join_cntr == ILLEGAL)
    return(error(ILLJOIN));

  if (join_on[0] != (char *) 0) {
    if (join_cntr == FAIL) {
      free_pred(prptr);
      done(slptr);
      return(error(NJOIN));
    }
  }
  
  /* allocate and initialize a tuple buffer */
  if ((tbuf = (char *) calloc(1, (unsigned)RELATION_HEADER.hd_size + 1)) 
                                                == (char *) 0) {
    rc_done(rptr, TRUE);
    return (error(INSMEM));
  }
  tbuf[0] = ACTIVE;
  
  /* if no index for predicate or joined - attrs
                     loop through the selected tuples sequentially */
  if (! prptr[0] && ! prptr[1]) {
    for ( ; fetch(slptr, no_product); tcnt++) {
      /* create the tuple from the selected attributes */
      if (join_on[0]) { /* non-indexed join-operation */
	if ((rc = do_join(slptr, duplc_attr,
			   ref_attr, d_indx, tbuf)) == FALSE) {
	  done(slptr);
	  return(FALSE);
	}
	else if (rc == FAIL){ /* tuple failed consistency test */
	  decr++;   /* tcnt must be decremented to reflect final result */
	  continue;
	}
      }
      else { /* non-indexed select-operation */
	abase = 1;
	for (saptr = SEL_ATTRS; saptr != (struct sattr *) 0; 
	     saptr = saptr->sa_next) {
	  bcopy(saptr->sa_aptr, &tbuf[abase], ATTRIBUTE_SIZE);
	  abase += ATTRIBUTE_SIZE;
	}
      }
      
      /* in case of join write only one joined attribute */
      if (!write_tuple(rptr, tbuf, TRUE)) {
	rc_done(rptr, TRUE);
	return(error(INSBLK));
      }
    }
  }
  else if (prptr[0] && ! prptr[1] && ! join_on[0]) { /* indexed select-operation */
    if (!access_plan(prptr[0], rptr, slptr, &tcnt, FALSE, FALSE)) {
      done(slptr);
      return(FALSE);
    }

  }     /* indexed attributes in join-query */
  else if (join_on[0]) {
    if (!join_ix(rptr, slptr, &tcnt, join_keys, 
	      join_cntr, duplc_attr, ref_attr, d_indx, tbuf)) {
      done(slptr);
      return(FALSE);
    }
  }

  
  /* finish the selection, clear and reset buffers */
  done(slptr);
  
  /* finish relation creation */
  if (!rc_done(rptr, TRUE)) {
    buf_free(duplc_attr, d_indx);
    buf_free(ref_attr, d_indx);
    nfree(tbuf);
    buf_free(join_on, 2);
    join_on[0] = NULL;
    join_on[1] = NULL;
    return (FALSE);
  }
  
  if (join_on[0] != (char *) 0)  { /* do house keeping */
    close(rptr->rl_fd);
    buf_free(duplc_attr, d_indx);
    nfree(tbuf);
    buf_free(join_on, 2);
    join_on[0] = NULL;
    join_on[1] = NULL;
  }
  
  /* detect existence of interpretation error */
  if (xint_error) {
    xint_error = FALSE;
    return(FALSE);
  }
  
  if (!supress)  /* check no. of tuples selected, print message */
    message(tcnt-decr, cformat);
  
  /* return successfully */
  return (TRUE);
  
  
} /* select */

/* ---------------------------------------------------------- */

/* join - join selected tuples from a set of relations */
int     nat_join(supress, recursive)
     int     supress;
     BOOL   recursive;
{
  char     **relat, **attrib;
  int     c, i,  r_indx = 0, a_indx = 0;
  
  /* allocate and initialize pointers to relations and 
     attributes */
  if (   !(relat = dcalloc(2))
      || !(attrib =  dcalloc(2))
      || ! (join_on[0] = (char *) calloc(1, ANSIZE+1))
      || ! (join_on[1] = (char *) calloc(1, ANSIZE+1)))
    return (error(INSMEM));
  
  /* parse the expression */
  for( i= 0; i < 2; i++) {   /* if name a view do not expand */
    if (next_token() != ID)
      return (error(SYNTAX));
    else {
      relat[r_indx] = rmalloc(2 * RNSIZE + 2);
      strcpy(relat[r_indx++], lexeme);
    }
    
    if (next_token() != ON)
      return(error(SYNTAX));
    
    if (next_token() != ID)
      return(error(SYNTAX));
    else {
      attrib[a_indx] = rmalloc(ANSIZE + 1);
      strcpy(attrib[a_indx++], lexeme);
    }
    
    if (( c = this_token()) == ';' || c == WHERE || c == ')')
      break;
    else if (c == WITH)
      next_token();
    else
      return (error(SYNTAX));
  }
  
  if (c == WHERE) {  /* join involves predicate */
    if (!joined(relat, attrib, r_indx, a_indx, TRUE, recursive)) 
      return(FALSE);
    
  } else {
    if (c != ')') /* it is not a nested query */
      next_token(); /* clear the comma */
    
    if (!joined(relat, attrib, r_indx, a_indx, FALSE, recursive)) 
      return(FALSE);
    
  }
  
  /* check for a join in a nested query */     
  if (c == ')') {
    saved_ch = ')';   /* pushback character and then */
    saved_tkn = EOS;  /* force a read from input buffer */
  } else if (restore_paren) {
    saved_ch = ')';  /* push back a ")" character */
    saved_tkn = EOS;
    restore_paren = FALSE;
  }
  
  if (!supress) 
    terminator();
  
  /* free buffers and return successfully */
  buf_free(relat, 2);
  buf_free(attrib, 2);
  
  return(TRUE);
  
} /* join */

/* ---------------------------------------------------------- */

int     expose(supress)
     BOOL     supress;
{
  struct sel *retrieve();
  struct sel *slptr;
  struct sattr *saptr; 
  char  a_buffer[65], a_name[15], common[ANSIZE+1], relop[5];
  char  relation2[RNSIZE+1], qualifier[25], fmt[2*LINEMAX+1], 
        constant[35];
  int      tkn, cntr = 0;
  char fst_aname[ANSIZE + 1], scnd_aname[ANSIZE + 1], tmp_aname[ANSIZE + 1];
  
  /* parse the retrieve clause */
  if ((slptr = retrieve((char *) 0)) == (struct sel *) 0)
    return (FALSE);
  
  /* if view definition exit */
  if (view_def) {
    done(slptr);
    return(TRUE);
  }
  
  if (next_token() != WHEN) {
    done(slptr);
    return(error(SYNTAX));
  }

  a_buffer[0] = EOS;
  
  /* get the exposed attribute names, fill attribute buffer */
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    cntr++;
    
    if (cntr > 5) {
      done(slptr);
      return(error(MAX5));
    }

    if (a_buffer[0] != EOS) {
      sprintf(a_name, ", %s", saptr->sa_aname);
      strcat(a_buffer, a_name);
    } else
      strcpy(a_buffer, saptr->sa_aname);
  }
  
  /* next token must be the common attribute name */
  if (next_token() != ID) {
    done(slptr);
    return(error(SYNTAX));
  }

  /* preserve common attribute */
  strcpy(common, lexeme);
  
  if (next_token() != IS) {
    done(slptr);
    return(error(SYNTAX));
  }

  if (next_token() != IN) {
    done(slptr);
    return(error(SYNTAX));
  }

  if (next_token() != PROJECT) {
    done(slptr);
    return(error(SYNTAX));
  }

  /* next token must be the second relation name */
  if (next_token() != ID) {
    done(slptr);
    return(error(SYNTAX));
  }

  /* preserve the relation name */
  strcpy(relation2, lexeme);
  
  /* relations must be different */
  if (strcmp(SEL_RELATION->rl_name, relation2) == EQUAL) {
    done(slptr);
    return(error(SYNTAX));
  }

  if (next_token() != OVER) {
    done(slptr);
    return(error(SYNTAX));
  }

  /* common attribute */
  if (next_token() != ID) {
    done(slptr);
    return(error(SYNTAX));   
  }

  strcpy(tmp_aname, lexeme);  /* store second relation attribute name */
  /* compare the current lexeme with the common atribute */
  if (strncmp(common, tmp_aname, strlen(tmp_aname)) != EQUAL) {
    expose_exists = TRUE;	 /* restore lex_cfp in recursive 
				    join-call that follows */

    if (! fgn_join(relation2, SEL_RELATION->rl_name, common, fst_aname, 
		   scnd_aname)) {
      done(slptr);
      return(error(UNATTR));
    }
    expose_exists = FALSE;
    cmd_clear_nested(); /* clear lineptr and lookahead token */

    if (fst_aname[0] == EOS) {
      done(slptr);
      return(error(UNATTR));
    }
    if (strcmp(tmp_aname, scnd_aname) != EQUAL) {
      done(slptr);
      return(error(UNATTR));
    }
  } else 
    strcpy(scnd_aname, tmp_aname);  /* attribute names are identical */
  
  if ((tkn = next_token()) == WHERE) {
    
    if (next_token() != ID) {
      done(slptr);
      expose_exists = FALSE;
      return(error(SYNTAX));
    }

    /* create qualified attribute */
    strcpy(qualifier, relation2);
    make_quattr(qualifier, lexeme);
    
    /* the next token must be a relational operator */
    tkn = next_token();
    if (relational_op(tkn, relop) == FALSE) {
      done(slptr);
      expose_exists = FALSE;
      return(error(SYNTAX));
    }

    /* the next token must be a numeric or string constant */
    tkn = next_token();
    if (ns_constant(tkn, constant) == FALSE) {
      done(slptr);
      expose_exists = FALSE;
      return(error(SYNTAX));
    }

    /* form the query to be processed */
    sprintf(fmt, 
       "project (join %s on %s with %s on %s where %s %s %s) \
              over %s", SEL_RELATION->rl_name, common, relation2,
	      scnd_aname, qualifier, relop, constant, a_buffer);
  } else {
    /* form the query to be processed */
    sprintf(fmt, 
	"project (join %s on %s with %s on %s) over %s",
	    SEL_RELATION->rl_name, common, relation2, 
	    scnd_aname, a_buffer);
  }
  
#ifndef UNIX
  close(SEL_RELATION->rl_fd);  /* cannot re-open an already opened file */
#endif

  if (!parse(fmt, FALSE, FALSE)) {
    done(slptr);
    expose_exists = FALSE;
    return(FALSE);
  }

  /* print the results */
  if (!supress) 
    terminator();
  
  done(slptr);
  expose_exists = FALSE;
  return(TRUE);
  
} /* expose */

/* ---------------------------------------------------------- */

/* project - project selected attributes from a relation */
int     project(supress)
     BOOL     supress;
{
  struct sel *retrieve();
  struct sel *slptr;
  struct relation *rptr;
  struct sattr *saptr;
  char     *tbuf, *aux_buf;
  int     cntr = 0, abase;
  BOOL    res;
  
  /* parse the retrieve clause */
  if ((slptr = retrieve((char *) 0)) == NULL)
    return (FALSE);
  
  /* if view definition exit */
  if (view_def) {
    done(slptr);
    return(TRUE);
  }

  /* check for group clause */
  if (this_token() == GROUP) {
    next_token();
    res = group(slptr);
    done(slptr);
    
    return(res);
  }
  
  /* create a new relation */
  if (!new_relation(&sref_cntr, &rptr, slptr, prptr, rel_fl)) {
    done(slptr);
    return(FALSE);
  }

  /* create the selected attributes */
  if (!create_attrs(rptr, slptr)) {
    done(slptr);
    return(FALSE);
  }

  /* create the relation header and appropriate buffers */
  if (!header_buffs(rptr, slptr, &tbuf, &aux_buf)) {
    done(slptr);
    return(FALSE);
  }

  /* if no indexed attrs, loop through the selected tuples */
  if (!prptr[0]) {
    clear_mem(aux_buf, 0, RELATION_HEADER.hd_size + 1);
    for ( ; fetch(slptr, TRUE); ) {
      /* create the tuple from the selected attributes */
      abase = 1;
      for (saptr = SEL_ATTRS; saptr != NULL;
	                          saptr = saptr->sa_next) {

	bcopy(saptr->sa_aptr, &tbuf[abase], ATTRIBUTE_SIZE);
	/* strip off blanks */
	strcat(aux_buf, tbuf+abase);
	/* reset */
	abase += ATTRIBUTE_SIZE;
      }
      
      /* eliminate redundant duplicate tuples if necessary
       * and write tuples into relation file */
      cntr += db_hsplace(aux_buf, rptr->rl_name, tbuf, rptr);
      clear_mem(aux_buf, 0, RELATION_HEADER.hd_size + 1);
      clear_mem(&tbuf[1], 0, RELATION_HEADER.hd_size);
    }
  }
  else
    if (!access_plan(prptr[0], rptr, slptr, &cntr, TRUE, FALSE)) {
      done(slptr);
      return(FALSE);
    }

  /* finish the selection, clear and reset buffers */
  done(slptr);
  
  /* finish relation creation */
  if (!rc_done(rptr, TRUE))
    return (FALSE);
  
  /* free hash table and buffers */
  nfree(tbuf);
  hash_done();
  
  if (!supress) /* check no. of tuples selected, print message */
    message(cntr, cformat);
  
  /* return successfully */
  return (TRUE);
  
} /* project */

/* ---------------------------------------------------------- */

/* group - group sets of attributes */
int group(slptr)
     struct sel *slptr;
{    
  extern  double  atof(), count(), average(), minimum(), 
          maximum(), sum();
  struct relation *rptr;
  struct sattr *saptr;
  char    *tbuf, *aux_tbuf, *name, *aggr_attr, format[20];
  char    partition[LINEMAX+1], *s_copy();
  int     cntr, i, tkn, length, type;
  double  num, (*funptr)();
  BOOL pred_flag = FALSE;
  
  /* initialize static variables in functions avg and count */
  init_group();
  
  /* check existence of "by" clause */
  if (next_token() != BY)
    return(error(SYNTAX));
  
  /* check existence of qualifier attribute */
  if (next_token() != ID)
    return(error(SYNTAX));
  
  /* create a new relation */
  if (!new_relation(&sref_cntr, &rptr, slptr, prptr, rel_fl))
    return(FALSE);
  
  /* create the selected attributes */
  if (!create_attrs(rptr, slptr))
    return(FALSE);
  
  /* create relation header */
  if (!rc_header(rptr, NOINDEX))
    return (FALSE);
  
  /* initialize hash table */
  init_hash();  
  
  /* check coherance of 1st attribute with group field attr */
  if (strcmp(SEL_ATTRS->sa_attr->at_name, lexeme) != EQUAL)
    return(error(WRPREDID));
  
  /* get the name of base relation */
  if (SEL_ATTRS->sa_rname == NULL && SEL_ATTRS->sa_name != NULL)
    return(error(NOALIAS));
  else 
    name = s_copy(SEL_ATTRS->sa_rname);
  
  tkn = next_token();
  
  /* check for predicate expression */
  if (tkn == WHERE) {
    pred_flag = TRUE;
    tkn = next_token();
  }
  
  /* check for aggregate function and its parameters */
  if (next_token() != '(')
    return(error(SYNTAX));
  
  if (next_token() != ID)
    return(error(SYNTAX));
  
  aggr_attr = s_copy(lexeme);
  
  if (next_token() != ')')
    return(error(SYNTAX));
  
  
  /* determine type of aggregate function & define 
   * appropriate function */
  switch(tkn) {
  case AVG:
    funptr = average;
    strcpy(format, "avg_");
    break;
  case COUNT:
    funptr = count;
    strcpy(format, "cnt_");
    break;
  case MAXM:
    funptr = maximum;
    strcpy(format, "max_");
    break;
  case MINM:
    funptr = minimum;
    strcpy(format, "min_");
    break;
  case  SUM:
    funptr = sum;
    strcpy(format, "sum_");
    break;
  default:
    return(error(SYNTAX));
  }
  
  /* increment size of second attribute */
  RELATION_HEADER.hd_size += 4;
  
  /* length of first attribute in query */
  length = SEL_ATTRS->sa_attr->at_size;
  
  /* allocate and initialize a tuple buffer */
  if ((tbuf=(char *)calloc(1,(unsigned)RELATION_HEADER.hd_size+1)) 
                                         == NULL) {
    rc_done(rptr, TRUE);
    return (error(INSMEM));
  }
  tbuf[0] = ACTIVE;
  
  aux_tbuf = (char *) 
    calloc(1, (unsigned) SEL_ATTRS->sa_next->sa_attr->at_size+4);
  /* check for name coherence between 2nd attribute and
   *  aggregate function parameter */
  if (strcmp(SEL_ATTRS->sa_next->sa_attr->at_name, 
	                                aggr_attr) != EQUAL)
    return(error(WRPREDID));
  
  if (((type = SEL_ATTRS->sa_next->sa_attr->at_type) != TNUM) &&
                                        (type != TREAL))
    return(error(AGGRPARAM));
  
  /* fetch the values of the selected attributes,
     one tuple at a time */
  while (fetch(slptr, TRUE)) {
    
    /* get the first attribute in the query */
    saptr = SEL_ATTRS;
    
    /* create the tuple from the selected attributes 
     *  store first attribute value in buffer */
    for (i = 0; i < length; i++)
      partition[i] = saptr->sa_aptr[i];
    partition[length] = EOS;
    
    /* get the next attribute in the query */
    saptr = saptr->sa_next;
    
    /* store the second attribute's contents in buffer */
    bcopy(saptr->sa_aptr, aux_tbuf, ATTRIBUTE_SIZE);
    aux_tbuf[ATTRIBUTE_SIZE] = EOS; 
    
    num = atof(aux_tbuf);
    
    if (db_hash(partition, name, num)) {
      
      /*  call built-in function */
      Hash_Table[HTIndex]->aggr_param = 
	(*funptr)(num, Hash_Table[HTIndex]->aggr_param);
    } else {
      if (funptr == count)
	Hash_Table[HTIndex]->aggr_param = 1;
      else if (funptr == average)
	Hash_Table[HTIndex]->aggr_param = num;
    }
  }
  
  /* adjust resulting relation parameters */
  strcat(format, SEL_ATTRS->sa_next->sa_attr->at_name);
  free(RELATION_HEADER.hd_attrs[1].at_name);
  strncpy(RELATION_HEADER.hd_attrs[1].at_name, format, ANSIZE);
  RELATION_HEADER.hd_attrs[1].at_name[MIN(strlen(format),ANSIZE)] = EOS;
  RELATION_HEADER.hd_attrs[1].at_size += 4;
  RELATION_HEADER.hd_attrs[1].at_type = TREAL;

  if (pred_flag) { 
    tkn = next_token();
    switch (tkn) {
    case LSS:
      break;
    case LEQ:
      break;
    case EQL:
      break;
    case GEQ:
      break;
    case GTR:
      break;
    case NEQ:
      break;
    default:
      return(error(SYNTAX));
    }
    
    if (next_token() != NUMBER)
      return(error(SYNTAX));
  }
  
  
  /* write resulting relation into file */ 
  if (pred_flag)  /* if predicate was encountered */
    cntr = write_hash(length, rptr, tkn, tbuf, aux_tbuf);
  else
    cntr = write_hash(length, rptr, FALSE, tbuf, aux_tbuf);  
  
  /* finish relation creation */
  if (!rc_done(rptr, TRUE))
    return (FALSE);
  
  /* free hash table and buffers */
  nfree((char *) rptr);  
  nfree(aux_tbuf);
  nfree(tbuf);  
  nfree(name); 
  nfree(aggr_attr);
  hash_done();
  
  /* free predicate structure */
  free_pred(prptr);
  
  /* check number of tuples selected, print message */
  message(cntr, cformat);
  
  /* return successfully */
  return(TRUE);
} /* group */

/* ---------------------------------------------------------- */

/* union - unites all tuples from two relations */
int     db_union(unite, diff)
     int     unite, diff;
{
  struct sel *slptr;
  struct sel *retrieve(), *minus();
  struct relation *rptr;
  struct sattr *saptr, *aux_saptr, *next_saptr;
  struct unattrs  f_rel[NATTRS];
  char     *tbuf, *name;
  int     tcnt, lcnt, tup_offset, incr, i, k;
  int      size_diff, hs_cnt = 0;
  unsigned     hp_cnt = 0;
  
  if (diff) {
    /* parse the difference clause */
    if ((slptr = minus()) == NULL)
      return(FALSE);
  }
  else {
    /* parse the retrieve clause */
    if ((slptr = retrieve((char *) 0)) == NULL)
      return (FALSE);
  }
  
  /* create a new relation */
  if (!new_relation(&sref_cntr, &rptr, slptr, prptr, rel_fl)) {
    done(slptr);
    return(FALSE);
  }

  /* check uninon relations for validity and 
   * fill temporary relation */
  if (!check_union(rptr, slptr, f_rel, &name)) {
    done(slptr);
    return(FALSE);
  }

  /* create the relation header and the required buffers */
  if (!header_buffs(rptr, slptr, &tbuf, NULL)) {
    done(slptr);
    return(FALSE);
  }

  aux_saptr = NULL;
  /* loop through the selected tuples */
  for (tcnt = 0; single_scan(SEL_RELS); tcnt++) {
    /* create the tuple from the selected attributes */
    tup_offset = 1;
    k = 0;
    
    for (saptr=SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
      /* first relation tuples should be first written,
       * disgard momentarily tuples of second relation */
      
      if (strcmp(saptr->sa_rname, name) != EQUAL) {
	/* reached 1st attribute of 2nd relation */
	if (tcnt == 0)
	  aux_saptr = saptr;
	
	/* exit loop and write the created tuple */
	break;
      } else  if (k < SEL_NATTRS) {
	
	/* in first relation, f_rel contains the attribute lengths
	   of the new relation; hence adjust displacements */
	size_diff = f_rel[k].size - ATTRIBUTE_SIZE;
	
	if (!(incr = adjust(saptr, tbuf, size_diff, tup_offset))) {
	  done(slptr);
	  return(FALSE);
	}

	k++;  /* increment index for next attribute */
	
	tup_offset += incr;
      }
    }
    /* hash and write the tuples */
    if (unite)  /* union operation */
      hs_cnt  += 
	db_hsplace(&tbuf[1], rptr->rl_name, tbuf, rptr);
    else
      db_hash(&tbuf[1], rptr->rl_name, (double) 0);
  }
  
  /* insert the tuples of the second relation but in the order
   * as specified from their sequence in the first relation */
  lcnt = aux_saptr->sa_srel->sr_scan->sc_relation
    ->rl_header.hd_tcnt; /* no. of tuples in second relation */
  for (i = 0; i < lcnt && single_scan(SEL_RELS->sr_next); i++) {
    tup_offset = 1;
    k = 0;
    
    for (saptr = SEL_ATTRS; saptr != aux_saptr; 
	 saptr = saptr->sa_next, k++)
      for (next_saptr = aux_saptr; next_saptr != NULL; 
	   next_saptr = next_saptr->sa_next)
	if ((strcmp(next_saptr->sa_aname,saptr->sa_aname)==EQUAL)
	    && k < SEL_NATTRS) {
	  size_diff = f_rel[k].size - next_saptr->sa_attr->at_size;
	  if (!(incr=adjust(next_saptr,tbuf,size_diff,tup_offset))) {
	    done(slptr);
	    return(FALSE);
	  }

	  tup_offset += incr;
	  break; /* match has been found */
	}
    
    /* hash and write the tuples */
    if (unite || diff )  /* union or difference is required */
      hs_cnt += 
	db_hsplace(&tbuf[1], rptr->rl_name, tbuf, rptr);
    else /* intersection is required */
      hp_cnt += 
	db_hplace(&tbuf[1], rptr->rl_name, tbuf, rptr);
  }
  
  /* finish the selection */
  done(slptr);
  
  /* release auxilliary buffer storage */
  free_union(f_rel, (int) SEL_NATTRS);
  
  /* finish relation creation */
  if (!rc_done(rptr, TRUE))
    return (FALSE);
  
  /* free hash table and buffers */
  hash_done();
  hs_cnt += hp_cnt;
  
  message(hs_cnt, cformat);	
  
  /* return successfully */
  return (TRUE);
} /* db_union */

/* ---------------------------------------------------------- */

static struct sel *minus()
{
  struct sel *slptr;
  struct sel *retrieve();
  char *pt_error();
  char **relat;
  int r_indx = 0;
  char fmt[30];
  
  /* allocate and initialize pointers to relations */
  if (!(relat = dcalloc(2)))
    return ((struct sel *)pt_error(INSMEM));
  
  /* parse the expression */
  if (next_token() != ID)
    return ((struct sel *)pt_error(SYNTAX));
  else {
    relat[r_indx] = rmalloc(RNSIZE + 1);
    strcpy(relat[r_indx++], lexeme);
  }
  
  if (this_token() == ',')
    next_token();
  else
    return ((struct sel *)pt_error(SYNTAX));
  
  if (next_token() != ID)
    return ((struct sel *)pt_error(SYNTAX));
  else {
    relat[r_indx] = rmalloc(RNSIZE + 1);
    strcpy(relat[r_indx], lexeme);
  }
  
  next_token();
  
  /* reverse the order of evaluation to cater for internal
   * representation, and to comply with semantics of difference */
  sprintf(fmt, "%s, %s", relat[1], relat[0]);
   
  if ((slptr = retrieve(fmt)) == NULL)
    return ((struct sel *)NULL);
  
  if (temp_cfp) {
    lex_cfp = temp_cfp;           /* restore cf pointer */
    temp_cfp = NULL;
  }
  
  return(slptr);
  
} /* minus */

/* ---------------------------------------------------------- */

