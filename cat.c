

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


/* REQUIEM - catalog manipulation routines */

#include "requ.h"

extern int saved_ch;

/* ---------------------------------------------------------- */

/* prepare a catalog entry */
BOOL cat_entry(rptr, nattrs, rname, fgn_key)
     struct relation *rptr;
     int  nattrs;
     char *rname;
     char (*fgn_key)[ANSIZE + 1];
{
  char    creator[20];
  char    deriv[10], index[10];
  char    type[10], length[10];
  char    perms[10], degree[10];
  char    *path(), *status();
  char    **args;
  int     i, j, arg_index;
  BOOL    fgn;

  /* relation name catalog entry */
  if (strncmp(rname, "sys", 3) == EQUAL) {
    strcpy(creator, "sys");
    creator[3] = EOS;
  } else 
    path(creator);
  
  /* get status information from file descriptor */
  status(rname, perms);
  
  /* evaluate catalog entries */
  sprintf(degree, "%d", nattrs);
  strcpy(deriv, "base");
  
  /* insert catalog arguments in argument array */
  arg_index = 5;  /* no. of attributes for systab */
  args = dcalloc(arg_index);
  args[0] = rname;
  args[1] = creator;
  args[2] = perms;
  args[3] = deriv;
  args[4] = degree;
  
  /* insert systab catalog tuple */
  if (!cat_insert("systab", args, arg_index)) {
    clear_catalogs(rname);
    return(FALSE);
  }
  
  /* insert sysatrs catalog tuple */
  if (fexists("sysatrs")) {
    args[1] = rptr->rl_name;
    
    for (i = 0; ; i++) {
      
      if (RELATION_HEADER.hd_attrs[i].at_name[0] == EOS)
	break;
      
      args[0] = RELATION_HEADER.hd_attrs[i].at_name;
      
      if (RELATION_HEADER.hd_attrs[i].at_type == TCHAR)
	strcpy(type, "char");
      else if  (RELATION_HEADER.hd_attrs[i].at_type == TNUM)
	strcpy(type, "num");
      else if  (RELATION_HEADER.hd_attrs[i].at_type == TREAL)
	strcpy(type, "real");
      else
	return(error(UNDEFTYPE));
      args[2] = type;
      
      sprintf(length, "%d",RELATION_HEADER.hd_attrs[i].at_size);
      args[3] = length;
      
      clear_mem(index, 0, 10);
      
      for (j = 0, fgn = FALSE; j < 2 && fgn_key[j][0]; j++)
	if (strcmp(fgn_key[j], args[0]) == EQUAL) {
	  index_type(index, 'f');
	  fgn = TRUE;
	  break;
	}
      
      if (! fgn)
	index_type(index, RELATION_HEADER.hd_attrs[i].at_key);
      args[4] = index;
      
      if (!cat_insert("sysatrs", args, arg_index)) {
	clear_catalogs(rname);
	return(FALSE);
      }
    }
  }
  
  nfree((char *) args); 
  
  return(TRUE);
} /* cat_entry */

/* ---------------------------------------------------------- */

/* insert an entry into the catalog */
BOOL cat_insert(fname, args, arg_indx)
     char *fname;
     char **args;
     int  arg_indx;
{
  struct scan *sptr, *db_ropen();
  struct attribute *aptr, *taptr;
  char  **targs = NULL, **key_buf = NULL;
  char length[10], type[10], index[10], avalue[LINEMAX+1];
  int targ_index;
  int i, j, astart, p_cntr = 0, s_cntr = 2;
  
  /* open the relation, return a scan structure pointer */
  if ((sptr = db_ropen(fname)) == NULL)
    return (FALSE);
  
  
  /* systab entries in sysatrs catalog should be made first */
  if (strcmp(fname, "systab") == EQUAL && 
                 strcmp(args[0], "sysatrs") == EQUAL) {
    targ_index = 5;
    targs = dcalloc(targ_index);
    targs[1] = SCAN_RELATION->rl_name;
    
    for (j = 0; j < targ_index; j++) {
      taptr = &SCAN_HEADER.hd_attrs[j];
      targs[0] = taptr->at_name;
      if (taptr->at_type == TCHAR)
	strcpy(type, "char");
      else if (taptr->at_type == TNUM)
	strcpy(type, "num");
      else
	return(error(INCNST));
      
      targs[2] = type;
      sprintf(length, "%d", taptr->at_size);
      targs[3] = length;
      
      index_type(index, taptr->at_key);

      targs[4] = index;
      
      /* recurse */
      if (!cat_insert("sysatrs", targs, targ_index))
	return(FALSE);
    }
  }
  
  /* get attr values, astart points past tuple status code */
  astart = 1;
  for (i = 0; i < arg_indx; i++) {
    
    /* get a pointer to the current attribute */
    aptr = &SCAN_HEADER.hd_attrs[i];
    
    /* check for the last attribute */
    if (aptr->at_name[0] == EOS)
      break;
    
    strcpy(avalue, args[i]);
    avalue[strlen(avalue)] = EOS;
    
    /* place the attribute value in sc_tuple */
    store_attr(aptr, &sptr->sc_tuple[astart], avalue);
    
    /* update the attribute start */
    astart += aptr->at_size;
    
    /* check if indexing is required */
    if (aptr->at_key != 'n')
      check_ix(&key_buf, aptr, &p_cntr, &s_cntr, avalue); 
    
  }
  
  if (targs != NULL)
    nfree((char *) targs);
  
  /* store the new tuple */
  if (!store_tuple(sptr, key_buf)) {
    buf_free(key_buf, 4);
    key_buf = NULL;
    db_rclose(sptr); /* close relation file */
    saved_ch = NEWLINE;   /* force a newline */
    return (FALSE);
  }
  
  /* close relation and index files */
  db_rclose(sptr);
  buf_free(key_buf, 4);
  key_buf = NULL;
  return(TRUE);
  
} /* cat_insert */

/* ---------------------------------------------------------- */

