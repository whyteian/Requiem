

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



/* REQUIEM - decompose queries and retrieve data */

#include "requ.h"
extern int      sref_cntr;
extern int      lex_token;
extern char     lexeme[];
extern char     *stck();
extern int      xint_error;
extern BOOL     view_exists;
extern int      command;
char     errvar[ANSIZE];
static char     rel_fl[RNSIZE+5];
BOOL wh_signal;              /* selection predicate flag */
BOOL restore_paren = FALSE;  /* to be used with nested joins */


/* ----- select a set of tuples from a set of relations ----- */
struct sel *db_select(fmt)
     char     *fmt;
{
  struct sel *slptr;
  char *pt_error();

  /* check for a command line */
  if (fmt != NULL)
    q_scan(fmt);
  
  /* allocate a sel structure */
  if ((slptr = 
       (struct sel *)calloc(1, sizeof(struct sel ))) == NULL)
    return ((struct sel *) pt_error(INSMEM));
  
  /* initialize the structure */
  SEL_RELS = NULL;
  SEL_ATTRS = NULL;
  slptr->sl_bindings = NULL;
  wh_signal = FALSE;
  
  /* parse the list of selected attributes */
  if (!get_sel_attrs(slptr)) {
    done(slptr);
    return((struct sel *)NULL);
  }
  
  /* check for "from" clause */
  if (this_token() != FROM) {
    done(slptr);
    return((struct sel *) pt_error(SYNTAX));
  }
  
  next_token();
  
  /* expand view in case of view DML */
  if (view_stat() == FAIL)
    return((struct sel *) NULL); 
  
  if (this_token() == ID) {            
    if (!get_sel(slptr)) {
      done(slptr);
      return((struct sel *)NULL);
    }
    /* if view name, it was simply a view definition */
    view_exists = FALSE;
  }
  else {
    /* check for nested clauses */
    if (!nested_query()) 
      return((struct sel *)NULL);
    
    /* nested query processing */
    if (!find_relation(slptr, stck(rel_fl, sref_cntr), (char *) 0)) {
      done(slptr);
      return((struct sel *)NULL);
    }
  } 
  
  /* check the list of selected attributes */
  if (!check_attrs(slptr)) {
    done(slptr);
    return((struct sel *)NULL);
  }
  
  if (!predicate(slptr))
    return((struct sel *)NULL);

  /* return the new selection structure */
  return (slptr);
  
} /* db_select */

/* --- retrieve a set of tuples from a set of relations ----- */
struct sel *retrieve(fmt)
     char     *fmt;
{
  char *pt_error();
  struct sel *slptr;

  /* check for a command line */
  if (fmt != NULL)
    q_scan(fmt);
  
  /* allocate a sel structure */
  if ((slptr = 
       (struct sel *)calloc(1, sizeof(struct sel ))) == NULL)
    return ((struct sel *) pt_error(INSMEM));
  
  /* initialize the structure */
  SEL_RELS = NULL;
  SEL_ATTRS = NULL;
  slptr->sl_bindings = NULL;
  wh_signal = FALSE;
  
  /* check for selected relations clause */
  if (this_token() == ID) {
    if (!get_sel(slptr)) {
      done(slptr);
      return((struct sel *)NULL);
    }
    /* if view exists, it was simply a view definition  */
    view_exists = FALSE;
  }  else {
    /* check for nested clauses */
    if (!nested_query())
      return((struct sel *)NULL); 
    
    /* nested query processing */
    if (!find_relation(slptr, 
		       stck(rel_fl, sref_cntr), (char *) 0)) {
      done(slptr);
      return((struct sel *)NULL);
    }
  }
  
  /* check for "over" clause */
  if (this_token() == OVER)
    next_token();
  
  if (command <= UNION && command >= DIFFERENCE) {
    if (this_token() != ID) {     /* second relation name */
      done(slptr);
      return((struct sel *) pt_error(SYNTAX));
    }
  }

  /* parse the list of selected attributes */
  if (!get_sel_attrs(slptr)) {
    done(slptr);
    return((struct sel *)NULL);
  }
  
  /* check the list of selected attributes */
  if (!check_attrs(slptr)) {
    done(slptr);
    return((struct sel *)NULL);
  }
  
  /* check for "group" clause */
  if (this_token() == GROUP)
    return(slptr);
  
  /* check for the existance of a "where" clause */
  if (!predicate(slptr))
    return((struct sel *)NULL);

  /* return the new selection structure */
  return (slptr);
  
} /* retrieve */

/* ------ check whether this query is a nested query -------- */
static int     nested_query()
{
  int     status;
  BOOL    tmp_view_exists = view_exists;

  /* check first if this is a nested query 
     and then if it is a view call */
  if (this_token() == '(' || view_exists) {
    
    if (view_exists == FALSE) 
      next_token();
    
    switch (next_token()) {
    case DIFFERENCE:
      status = db_union(FALSE, TRUE);
      break;
    case EXPOSE:
      status = expose(TRUE);
      break;
    case INTERSECT:
      status = db_union(FALSE, FALSE);
      break;
    case JOIN:
      status = nat_join(TRUE, TRUE);
      break;
    case PROJECT:
      status = project(TRUE);
      break;
    case SELECT:
      status = requiem_select(TRUE);
      break;
    case UNION:
      status = db_union(TRUE, FALSE);
      break;
    default:
      return(FALSE);
    }
    
    view_exists = tmp_view_exists; /* restore view flag */

    if (!status)
      return(FALSE);
    
    if (view_exists && this_token() == ';') { /* a view call */
      
      /* clear query terminator  */
      next_token();
      
      view_exists = FALSE; /* reset flag */
      return(TRUE);
    }
    else if (view_exists) {
      view_exists = FALSE; /* reset flag */
      return(TRUE);        /* exit */
    } else 
      if (next_token() != ')') /* closing nested query paren. */
      return(error(NESTED));
  }
  return(TRUE);
} /* nested_query */

/* ----------- check for existence of a predicate ----------- */
static int     predicate(slptr)
     struct sel *slptr;
{
  /* check for the existance of a "where" clause */
  if (this_token() == WHERE) {
    next_token();
    
    wh_signal = TRUE;  /* predicate is present */
    
    /* parse the boolean expression */
    if (!xcompile(slptr)) {
      done(slptr);
      return (FALSE);
    }
  }
  
  return(TRUE);
  
} /* predicate */

/* -------------- predicate in a join query ----------------- */
where_in_join(wh_attr, relop, constant)
     char *wh_attr, *relop, *constant;
{
  int tkn;   
  
  if (next_token() != WHERE)
    return(error(SYNTAX));
  
  /* get the relation name */
  if (next_token() != ID)
    return(error(SYNTAX));
  
  strcpy(wh_attr, lexeme);
  
  if (next_token() != '.')   /* must be a qualified attribute */
    return(error(QUATTR));
  
  /* get the attribute name */
  if (next_token() != ID)
    return(error(SYNTAX));
  
  /* produce the qualified attribute */  
  make_quattr(wh_attr, lexeme);
  
  /* the next token must be a relational operator */
  tkn = next_token();
  if (relational_op(tkn, relop) == FALSE)
    return(error(SYNTAX));
  
  /* the next token must be a numeric or string constant */
  tkn = next_token();
  if (ns_constant(tkn, constant) == FALSE)
    return(error(SYNTAX));
  
  /* in case of a nested call, check for closing parenth. */
  if (this_token() == ')')
    restore_paren = TRUE;
  
  /* return successfully */
  return(TRUE);
  
} /* where_in_join */

/* --  check if next token is a rel operator and return it -- */
BOOL relational_op(tkn, relop)
     int tkn;   char *relop;
{
  char *rel_op;
  
  switch (tkn) {
  case LSS:
    rel_op = "<";	
    break;
  case LEQ:
    rel_op = "<=";	
    break;
  case EQL:
    rel_op = "=";	
    break;
  case PART:
    rel_op = "==";	
    break;
  case NEQ:
    rel_op = "<>";	
    break;
  case GEQ:
    rel_op = ">=";	
    break;
  case GTR:
    rel_op = ">";	
    break;
    
  default: return(FALSE);
  }
  strcpy(relop, rel_op);
  return(TRUE);
  
} /* relational_op */

/* ---------------- number or string constant --------------- */
BOOL ns_constant(tkn, constant) 
     int tkn;  char *constant;
{
  /* the next token must be a constant */
  if (tkn != NUMBER && tkn != REALNO && tkn != STRING)
    return(FALSE);
  
  if (tkn == STRING)
    sprintf(constant,"\"%s\"", lexeme);
  else
    strcpy(constant, lexeme);
  
  return(TRUE);
} /* ns_constant */

/*------------------  finish a selection -------------------- */
int done(slptr)
     struct sel *slptr;
{
  struct sattr *saptr, *nxtsa;
  struct srel *srptr, *nxtsr;
  struct binding *bdptr, *nxtbd;
  
  /* free the selected attribute blocks */
  for (saptr = SEL_ATTRS; saptr != NULL; saptr = nxtsa) {
    nxtsa = saptr->sa_next;
    if (saptr->sa_rname != NULL)
      free(saptr->sa_rname);
    free(saptr->sa_aname);
    if (saptr->sa_name != NULL)
      free(saptr->sa_name);
    free((char *) saptr);
  }
  
  /* close the scans and free the selected relation blocks */
  for (srptr = SEL_RELS; srptr != NULL; srptr = nxtsr) {
    nxtsr = SREL_NEXT;
    if (srptr->sr_name != NULL)
      free(srptr->sr_name);
    db_rclose(srptr->sr_scan);
    free((char *) srptr);
  }
  wh_signal = FALSE;
  /* free the user bindings */
  for (bdptr = slptr->sl_bindings;bdptr != NULL;bdptr = nxtbd) {
    nxtbd = bdptr->bd_next;
    free((char *) bdptr);
  }
  
  /* free the selection structure */
  free((char *) slptr);
  
} /* done */

/* ------- fetch the next tuple requested by a query -------- */
int     fetch(slptr, no_xprod)
     struct sel *slptr;
     BOOL no_xprod;
{
  struct srel *srptr;
  struct binding *bdptr;
  
  /* clear the update flags */
  for (srptr = SEL_RELS; srptr != NULL; srptr = SREL_NEXT)
    srptr->sr_update = FALSE;
  
  /* find a matching tuple */
  while (process(SEL_RELS, no_xprod)) {
    if (xint_error) /* interpretation error detected */
      break;
    if (xinterpret()) {
      for (bdptr = slptr->sl_bindings; bdptr != NULL; 
	                              bdptr = bdptr->bd_next)
	get_attr(bdptr->bd_attr,bdptr->bd_vtuple,bdptr->bd_vuser);
      return (TRUE);
    }
  }
  
  /* no matches, failure return */
  return (FALSE);
} /* fetch */

/* ---- get selected attribute type, pointer, and length ---- */
int     select_attr(slptr, rname, aname, ptype, val_ptr, plen)
     struct sel *slptr;
     char     *rname, *aname; /* rel and corresp. attr names */
     int     *ptype, *plen;   /* attribute type and length */
     char     **val_ptr;      /* buffer for attr value */
{
  struct srel *srptr;
  struct attribute *aptr;
  
  if (!find_attr(slptr, rname, aname, val_ptr, &srptr, &aptr))
    return (FALSE);
  *ptype = aptr->at_type;
  *plen = aptr->at_size;
  return (TRUE);
} /* select_attr */

/* ---------------  get selected attributes ----------------- */
static get_sel_attrs(slptr)
     struct sel *slptr;
{
  struct sattr *newsattr, *lastsattr;
  int     error();
  
  /* check for "all" or blank field meaning all attributes
   * are selected */
  if (this_token() == ALL) {
    next_token();
    return (TRUE);
  } else if (this_token() != ID)
    return (TRUE);
  
  /* parse a list of attribute names */
  lastsattr = NULL;
  while (TRUE) {
    
    /* get attribute name */
    if (next_token() != ID)
      return (error(SYNTAX));
    
    /* allocate a selected attribute structure */
    if ((newsattr = 
      (struct sattr *)calloc(1, sizeof(struct sattr ))) == NULL)
      return (error(INSMEM));
    
    /* initialize the selected attribute structure */
    newsattr->sa_next = NULL;
    
    /* save the attribute name */
    if ((newsattr->sa_aname = 
	        rmalloc(strlen(lexeme) + 1)) == NULL) {
      free((char *) newsattr);
      return (error(INSMEM));
    }
    strcpy(newsattr->sa_aname, lexeme);
    
    /* check for "." meaning "<rel-name>.<att-name>" */
    if (this_token() == '.') {
      next_token();
      
      /* the previous ID was really the relation name */
      newsattr->sa_rname = newsattr->sa_aname;
      
      /* check for attribute name */
      if (next_token() != ID) {
	free(newsattr->sa_aname);
	free((char *) newsattr);
	return (error(SYNTAX));
      }
      
      /* save the attribute name */
      if ((newsattr->sa_aname = 
	            rmalloc(strlen(lexeme) + 1)) == NULL) {
	free(newsattr->sa_aname);
	free((char *) newsattr);
	return (error(INSMEM));
      }
      strcpy(newsattr->sa_aname, lexeme);
    } else
      newsattr->sa_rname = NULL;
    
    /* check for secondary attribute name */
    if (this_token() == ID) {
      next_token();
      
      /* allocate space for the secondary name */
      if ((newsattr->sa_name = 
	           rmalloc(strlen(lexeme) + 1)) == NULL) {
	if (newsattr->sa_rname != NULL)
	  free(newsattr->sa_rname);
	free(newsattr->sa_aname);
	free((char *) newsattr);
	return (error(INSMEM));
      }
      strcpy(newsattr->sa_name, lexeme);
    } else
      newsattr->sa_name = NULL;
    
    /* set rname in structure sattr */
    if (SEL_RELS != NULL) {
      if (!set_rname(slptr, SEL_RELATION->rl_name))
	return(FALSE);
    }
    
    /* link the selected attribute structure into the list */
    if (lastsattr == NULL)
      SEL_ATTRS = newsattr;
    else
      lastsattr->sa_next = newsattr;
    lastsattr = newsattr;
    
    /* check for more attributes */
    if (this_token() != ',')
      break;
    next_token();
  }
  
  /* return successfully */
  return (TRUE);
} /* get_sel_attrs */

/* ------- insert a relation into sel structure ------------- */
static get_sel(slptr)
     struct sel *slptr;
{
  /* get the list of selected relations */
  while (TRUE) {
    
    /* get selected relation(s) */
    if (!get_sel_rel(slptr))
      return(FALSE);
    
    /* check for more selected relations */
    if (this_token() != ',')
      break;
    next_token();
  }
  
  /* return successfully */
  return (TRUE);
} /* get_sel */

/* ------------------- get selected relations --------------- */
static get_sel_rel(slptr)
     struct sel *slptr;
{
  char     rname[KEYWORDMAX+1], *aname;
  
  /* check for relation name */
  if (next_token() != ID)
    return (error(SYNTAX));
  strcpy(rname, lexeme);
  
  /* check for secondary relation name */
  if (this_token() == ID) {
    next_token();
    aname = lexeme;
  } else
    aname = NULL;
  
  /* set the relation name */
  if (!set_rname(slptr, rname))
    return(FALSE);
  
  /* add the relation name to the relation list */
  if (!find_relation(slptr, rname, aname))
    return (FALSE);
  
  /* return successfully */
  return(TRUE);
} /* get_sel_rel */

/* -------------------- find a relation --------------------- */
static find_relation(slptr, rname, aname)
     struct sel *slptr;
     char     *rname, *aname;
{
  struct scan *db_ropen();
  struct srel *srptr, *newsrel;
  
  /* allocate a new selected relation structure */
  if ((newsrel = 
       (struct srel *)calloc(1, sizeof(struct srel ))) == NULL)
    return (error(INSMEM));
  
  /* initialize the new selected relation structure */
  newsrel->sr_ctuple = FALSE;
  newsrel->sr_update = FALSE;
  newsrel->sr_next = NULL;
  
  /* open the relation */
  if ((newsrel->sr_scan = db_ropen(rname)) == NULL) {
    free((char *) newsrel);
    return (FALSE);
  }
  
  /* check for secondary relation name */
  if (aname != NULL) {
    
    /* allocate space for the secondary name */
    if ((newsrel->sr_name = 
	 malloc((unsigned) strlen(aname) + 1)) == NULL) {
      free((char *) newsrel);
      return (error(INSMEM));
    }
    strcpy(newsrel->sr_name, aname);
  } else
    newsrel->sr_name = NULL;
  
  /* initialise record position */
  newsrel->sr_scan->sc_recpos = 0; 
  
  /* find the end of the list of relation names */
  for (srptr = SEL_RELS; srptr != NULL; srptr = SREL_NEXT)
    if (SREL_NEXT == NULL)
      break;
  
  /* link the new selected relation structure into the list */
  if (srptr == NULL)
    SEL_RELS = newsrel;
  else
    SREL_NEXT = newsrel;
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* check the list of selected attributes */
static int     check_attrs(slptr)
     struct sel *slptr;
{
  struct sattr *saptr;
  
  /* check for all attributes selected */
  if (SEL_ATTRS == NULL)
    return (all_attrs(slptr));
  
  /* check each selected attribute */
  for (saptr = SEL_ATTRS; saptr != NULL; saptr = saptr->sa_next)
    if (!find_attr(slptr, saptr->sa_rname, saptr->sa_aname,
	    &saptr->sa_aptr, &saptr->sa_srel, &saptr->sa_attr))
      return (FALSE);
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* create a list of all attributes */
static int     all_attrs(slptr)
     struct sel *slptr;
{
  struct sattr *newsattr, *lastsattr;
  struct srel *srptr;
  struct attribute *aptr;
  int     i, astart;
  
  /* loop through each selected relation */
  lastsattr = NULL;
  for (srptr = SEL_RELS; srptr != NULL; srptr = SREL_NEXT) {
    
    /* loop through each attribute within the relation */
    astart = 1;
    for (i = 0; i < NATTRS; i++) {
      
      /* get a pointer to the current attribute */
      aptr = &SREL_HEADER.hd_attrs[i];
      
      /* check for last attribute */
      if (aptr->at_name[0] == EOS)
	break;
      
      /* allocate a new selected attribute structure */
      if ((newsattr = 
      (struct sattr *)calloc(1, sizeof(struct sattr ))) == NULL)
	return (error(INSMEM));
      
      /* initialize the new selected attribute structure */
      newsattr->sa_name = NULL;
      newsattr->sa_srel = srptr;
      newsattr->sa_aptr = srptr->sr_scan->sc_tuple + astart;
      newsattr->sa_attr = aptr;
      newsattr->sa_next = NULL;
      
      /* save the relation name */
      if ((newsattr->sa_rname = calloc(1, RNSIZE + 1)) == NULL) {
	free((char *) newsattr);
	return (error(INSMEM));
      }
      
      strncpy(newsattr->sa_rname, SREL_RELATION->rl_name,
	      MIN(strlen(SREL_RELATION->rl_name), RNSIZE));
      newsattr->sa_rname[MIN(strlen(SREL_RELATION->rl_name), 
			                       RNSIZE)] = EOS;
      
      /* save the attribute name */
      if ((newsattr->sa_aname = calloc(1, ANSIZE + 1)) == NULL) {
	free(newsattr->sa_rname);
	free((char *) newsattr);
	return (error(INSMEM));
      }
      strncpy(newsattr->sa_aname,
	      SREL_HEADER.hd_attrs[i].at_name,
	      ANSIZE);
      newsattr->sa_aname[MIN(ANSIZE, 
		  strlen(SREL_HEADER.hd_attrs[i].at_name))] = EOS;
      
      /* link the selected attribute into the list */
      if (lastsattr == NULL)
	SEL_ATTRS = newsattr;
      else
	lastsattr->sa_next = newsattr;
      lastsattr = newsattr;
      
      /* update the attribute start */
      astart += aptr->at_size;
    }
  }
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* find a named attribute */
int     find_attr(slptr, rname, aname, val_ptr, psrel, pattr)
     struct sel *slptr;
     char     *rname, *aname;
     char     **val_ptr;
     struct attribute **pattr;
     struct srel **psrel;
{
  /* check for unqualified or qualified attribute names */
  if (rname == NULL)
    return (unqu_attr(slptr, aname, val_ptr, psrel, pattr));
  else
    return (qual_attr(slptr,rname,aname,val_ptr,psrel,pattr));
}

/* ---------------------------------------------------------- */

/* find an unqualified attribute name */
static int     unqu_attr(slptr, aname, val_ptr, psrel, pattr)
     struct sel *slptr;
     char     *aname;
     char     **val_ptr;
     struct srel **psrel;
     struct attribute **pattr;
{
  struct srel *srptr;
  struct relation *relptr;
  struct attribute *aptr;
  int     i, astart;
  
  
  /* loop through each selected relation */
  *pattr = NULL;
  for (srptr = SEL_RELS; srptr != NULL; srptr = SREL_NEXT) {


      /* detect existence of multiple relations */
      if (SREL_NEXT) {
	relptr = SREL_NEXT->sr_scan->sc_relation;
	
	if (SEL_RELATION->rl_next == NULL && SEL_RELATION != relptr)
	  SEL_RELATION->rl_next = relptr;
	
	relptr->rl_next = NULL;
      } else {
	relptr =   SREL_RELATION;
	relptr->rl_next = NULL;
      }

    /* loop through each attribute within the relation */
    astart = 1;
    for (i = 0; i < NATTRS; i++) {
      
      /* get a pointer to the current attribute */
      aptr = &SREL_HEADER.hd_attrs[i];
      
      /* check for last attribute */
      if (aptr->at_name[0] == EOS)
	break;
      
      /* check for attribute name match */
      if (strncmp(aname, aptr->at_name, ANSIZE) == EQUAL) {
	if (*pattr != NULL)
	  return (error(ATAMBG));
	*val_ptr = srptr->sr_scan->sc_tuple + astart;
	*psrel = srptr;
	*pattr = aptr;
      }
      
      /* update the attribute start */
      astart += aptr->at_size;
    }
  }
  
  /* check whether attribute was found */
  if (*pattr == NULL) {
    strcpy(errvar, aname);
    errvar[strlen(aname)] = EOS;
    
    return (error(ATUNDF));
  }
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* find a qualified attribute name */
static int qual_attr(slptr, rname, aname, val_ptr, psrel, pattr)
     struct sel *slptr;
     char     *rname, *aname;
     char     **val_ptr;
     struct srel **psrel;
     struct attribute **pattr;
{
  struct srel *srptr;
  struct relation *relptr;
  struct attribute *aptr;
  char     *crname;
  int     i, astart;
  
  /* loop through each selected relation */
  for (srptr = SEL_RELS; srptr != NULL; srptr = SREL_NEXT) {

      /* detect existance of multiple relations */
      if (SREL_NEXT) {
	relptr =   SREL_NEXT->sr_scan->sc_relation;
	
	if (SEL_RELATION->rl_next == NULL)
	  SEL_RELATION->rl_next = relptr;
	
	relptr->rl_next = NULL;
      }
      else {
	relptr =   SREL_RELATION;
	relptr->rl_next = NULL;
      }

    /* get relation name */
    if ((crname = srptr->sr_name) == NULL)
      crname = SREL_RELATION->rl_name;
    
    /* check for relation name match */
    if (strncmp(rname, crname, RNSIZE) == EQUAL) {
      
      /* loop through each attribute within the relation */
      astart = 1;
      for (i = 0; i < NATTRS; i++) {
	
	/* get a pointer to the current attribute */
	aptr = &SREL_HEADER.hd_attrs[i];
	
	/* check for last attribute */
	if (aptr->at_name[0] == EOS)
	  break;
	
	/* check for attribute name match */
	if (strncmp(aname, aptr->at_name, ANSIZE) == EQUAL) {
	  *val_ptr = srptr->sr_scan->sc_tuple + astart;
	  *psrel = srptr;
	  *pattr = aptr;
	  return (TRUE);
	}
	
	/* update the attribute start */
	astart += aptr->at_size;
      }
      /* attribute name not found */
      strcpy(errvar, aname);
      errvar[strlen(aname)] = EOS;
      
      return (error(ATUNDF));
    }
  }
  
  /* relation name not found */
  return (error(RLUNDF));
} /* qual_attr */

/* ---------------------------------------------------------- */

/* process each tuple in a relation cross-product */
int     process(srptr, no_xprod)
     struct srel *srptr;
     BOOL no_xprod; /* signifies whether or not an x-product 
		      * between two rels should be formed */
{
  /* always get a new tuple if this is the last rel in list */
  if (SREL_NEXT == NULL || no_xprod)
    return(single_scan(srptr));
  
  /* check for the begining of a new scan */
  if (srptr->sr_ctuple == FALSE) {
    begin_scan(srptr->sr_scan);
    
    /* get the first tuple */
    if (!fetch_tuple(srptr->sr_scan))
      return (FALSE);
  }
  
  /* look for a match with the remaining relations in list */
  while (!process(SREL_NEXT, no_xprod))
    /* get the next tuple in the scan */
    if (!fetch_tuple(srptr->sr_scan))
      return (srptr->sr_ctuple = FALSE);
  
  /* found a match at this level */
  return (srptr->sr_ctuple = TRUE);
}

/* ---------------------------------------------------------- */

int     do_scan(srptr)
     struct srel *srptr;
{
  /* check for beginning of new scan,
     if no tuples have been fetched so far, begin scanning */
  if (srptr->sr_ctuple == FALSE)
    begin_scan(srptr->sr_scan);
  
  /* get the first tuple */
  if (!fetch_tuple(srptr->sr_scan))
    return (FALSE);
  
  /* return successfully */
  return(TRUE);
  
} /* do_scan */

/* ---------------------------------------------------------- */

begin_scan(sptr)    /* begin scan at first tuple in relation */
     struct scan *sptr;
{
  /* begin with the first tuple in the file */
  sptr->sc_dtnum = 0;
  
} /* begin_scan */

/* ---------------------------------------------------------- */

int  single_scan(srptr)
     struct srel *srptr;
{
  return (srptr->sr_ctuple = do_scan(srptr));
  
} /*scan_single */

/* ---------------------------------------------------------- */

/* get the value of an attribute */
get_attr(aptr, vptr, avalue)
     struct attribute *aptr;
     char     *vptr, *avalue;
{
  int     i;
  
  /* get the attribute value */
  for (i = 0; i < aptr->at_size; i++)
    *avalue++ = vptr[i];
  *avalue = EOS;
}

/* ---------------------------------------------------------- */

/* put the value of an attribute */
store_attr(aptr, vptr, avalue)
     struct attribute *aptr;
     char     *vptr, *avalue;
{
  int     i;
  
  /* initialize counter */
  i = 0;
  
  /* right justify numbers */
  if (aptr->at_type == TNUM || aptr->at_type == TREAL)
    for (; i < aptr->at_size - strlen(avalue); i++)
      vptr[i] = ' ';
  
  /* put the attribute value */
  for (; i < aptr->at_size; i++)
    if (*avalue == EOS)
      vptr[i] = EOS;
    else
      vptr[i] = *avalue++;
  
}

/* ---------------------------------------------------------- */

set_rname(slptr, rname)
     struct sel *slptr;
     char     *rname;
{
  if (SEL_ATTRS && SEL_ATTRS->sa_rname == NULL) {
    
    /* save the relation name */
    if ((SEL_ATTRS->sa_rname = rmalloc(RNSIZE + 1)) == NULL)
      return (error(INSMEM));
    
    
    strncpy(SEL_ATTRS->sa_rname, rname,
	    MIN(strlen(rname), RNSIZE));
    SEL_ATTRS->sa_rname[MIN(strlen(rname), RNSIZE)] = EOS;
  }
  return(TRUE);
}

/* ---------------------------------------------------------- */

