

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

/* REQUIEM - access path module */
#include "btree.h"
#include "requ.h"

extern struct pred *prptr[];       /* predicate structure for relations */
extern union code_cell code[];
extern char    **key_buf;
extern char    **del_buf;
extern char    *join_on[];
extern BOOL    wh_signal;

	/* non-indexed attr flag set during compilation */
extern BOOL non_ixattr;  
extern int xand(), xor(), xnot(), xlss(), xleq(), xeql(), 
           xgeq(), xgtr(), xneq(), xpart(), xstop(), 
           compf(), sizef();
BOOL stop_flag = FALSE;
BOOL canned_commit = FALSE;  /* db_commit called from within canned query */

/* ---------------------------------------------------------- *
   |              index inserting routines                  |
   * -------------------------------------------------------- */
struct ix_header *db_index(rname, tup_pos, ix_buf)
     long tup_pos;
     char *rname, **ix_buf;
{
  struct ix_header *ixd;
  BOOL insert = TRUE;
  char *pt_error();

  if (!(ixd = (struct ix_header *) calloc(1, sizeof(struct ix_header))))
      return(NULL);

  /* open  index file and assign index header structure */
  if (!assign_pixd(rname, ixd)) {
    if (ixd->ixfile > 0)
      close(ixd->ixfile);
    return((struct ix_header *)pt_error(IXFLNF));
  }

  /* index the attributes according to their key specs */
  if (!index_attrs(tup_pos, ixd, ix_buf, insert)) {
    if (ixd->ixfile > 0) {
      close(ixd->ixfile);
    }
    free(ixd);
    ixd = NULL;
    return((struct ix_header *)pt_error(WRNGINS));
  }

  return(ixd);   /* indexing was successful */
} /* db_index */

/* ---------------------------------------------------------- */

int    index_attrs(tup_pos, pixd, ix_buf, insert)
     long tup_pos;
     struct ix_header *pixd;
     char    **ix_buf;
     BOOL insert;
{
  struct entry entr;
  char    prefix[5];
  int    i;
  int ret;

  entr.lower_lvl = tup_pos; /* point to tuple pos in datafile */
  
  /* form a primary key as combination of foreign keys, 
     if required in case of pure inserts only */
  if (insert && pixd->dx.ixatts.ix_foreign == 't' && !del_buf) {
    if (!combine_fkeys(pixd, ix_buf, &entr))
      return(FALSE);
  }

  /* in case of update determine whether new prim. key already exists */
  if (pixd->dx.ixatts.ix_foreign == 'f' && !insert && key_buf[0]) {
    clear_mem(entr.key, 0, MAXKEY);
    cat_strs(entr.key, "P0*", key_buf[0]);
    ret = find_ix(&entr, pixd);
    if (ret == 0)  /* if entry found in record */
      return(error(DUPATT));
  }
  
  for ( i = 0; i < 2 * MAXKEYS ; i++) {
    /* index primary keys first */
    if (i < 2 && 
	*(pixd->dx.ixatts.ix_prim[i]) != EOS && 
	ix_buf[i]) {
      clear_mem(entr.key, 0, MAXKEY);
      sprintf(prefix, "P%d*", i);
      cat_strs(entr.key, prefix, ix_buf[i]);
      
      if (insert) {
        if (find_ins(&entr, pixd) == IX_FAIL)
          return(error(DUPATT)); /* entry already there */
      } else if (find_del(&entr, pixd) == IX_FAIL)
        return(error(IXATRNF)); /* entry not there */
    }
    
    /* index secondary keys now */
    if (i >= 2 && 
	*(pixd->dx.ixatts.ix_secnd[i-2]) != EOS && 
	ix_buf[i]) {
      clear_mem(entr.key, 0, MAXKEY);
      sprintf(prefix, "S%d*", i - 2);
      cat_strs(entr.key, prefix, ix_buf[i]);
      
      if (insert) {
        if (find_ins(&entr, pixd) == IX_FAIL)
          return(error(DUPATT)); /* entry already there */
      } else if (find_del(&entr, pixd) == IX_FAIL)
        return(error(IXATRNF)); /* entry not there */
    }
    
  }
  
  return(TRUE);    /* return success */
} /* index_attrs */


/* ---------------------------------------------------------- 
   |           index update, delete routines                |
   * -------------------------------------------------------- */
int    update_ix(tup_pos, ix_buf, pixd)
     long tup_pos;
     char    **ix_buf;
     struct ix_header *pixd;
{
  /* index the attributes according to their key specs */
  if (!index_attrs(tup_pos, pixd, ix_buf, TRUE)) {
    close((pixd)->ixfile);
    return(error(UPDTERR));
  }
  
  return(TRUE);   /* indexing was successful */
} /* update_ix */

/* -------------- update an indexed entry ------------------- */
update_keys(pixd, slptr, saptr, aptr, offset, old_value, new_value)
     struct ix_header *pixd;
     struct sel *slptr;
     struct sattr *saptr;
     struct attribute *aptr;
     long    *offset;
     char    *old_value, *new_value;
{
  
  struct srel *srptr;
  int p_cntr = 0, s_cntr = 2;      /* key counters */
  struct relation *rptr;
  struct ix_header *ixd;
  struct entry e, e1;

  char    fname[RNSIZE+6];
  int    i, scntr = 0;
  
  if (! pixd) {
    if (!(ixd = (struct ix_header *) calloc(1, sizeof (struct ix_header))))
      return(FALSE);
  } else
    ixd = pixd;

  rptr = SEL_RELATION;
  srptr = SEL_RELS;
  
  for (i = 0; i < srptr->sr_scan->sc_nattrs; i++) {
    if (RELATION_HEADER.hd_attrs[i].at_key == 's')
      scntr++;
    else  if (RELATION_HEADER.hd_attrs[i].at_key == 'p')
      p_cntr++;
    
    /* locate the selected attribute and estimate its prefix */
    if (strcmp(saptr->sa_aname,
	       RELATION_HEADER.hd_attrs[i].at_name) == EQUAL)
      break;
    
  }
  /* first two positions reserved for foreign keys */
  s_cntr = 2 + scntr;  
  
  /* if predicate struct not available, form index key and 
   * locate key-attribute value in index file */
  if (!prptr[0]) {
    /* if index file not open, open it */
    if (!form_ix(fname, ixd, SEL_RELATION))
      return(FALSE);
    
    /* form index-key to look for */
    if (scntr > 0)
      sprintf(e.key, "S%d*%s", scntr - 1, old_value);
    else if (p_cntr > 0)
      sprintf(e.key, "P%d*%s", p_cntr - 1, old_value);
    
    /* position index file at an entry >= to search key */
    find_ix(&e, ixd);
    
    /* get current entry */
    get_current(&e1, ixd);
    rm_trailblks(e1.key);
    
    /* compare entries */
    if (strcmp(e.key, e1.key) == EQUAL)
      *offset = e1.lower_lvl;
    else
      return(FALSE);
  }
  
  /* fill delete index buffer with attr values to be deleted */
  fill_ix(&del_buf, aptr, p_cntr, s_cntr, old_value);
  
  /* fill insert index buffer with attr values to be inserted */
  fill_ix(&key_buf, aptr, p_cntr, s_cntr, new_value);
  
  return(TRUE);
  
} /* update_keys */

/* ---------------------------------------------------------- */

BOOL del_ixattr(pixd, sptr, del_buf)
     struct ix_header **pixd;
     struct scan *sptr;
     char    **del_buf;
{
  BOOL insert = FALSE;
  
  if (*pixd == NULL) {
    if ((*pixd = CALLOC(1, struct ix_header)) == NULL)
      return(error(INSMEM));
  
    /* open index file and assign index header structure */
    if (!assign_pixd(sptr->sc_relation->rl_name, *pixd))
      return(error(IXFLNF));
  }

  /* index the attributes according to their key specs */
  if (!index_attrs(sptr->sc_recpos, *pixd, del_buf, insert)) {
    close((*pixd)->ixfile);
    return(error(IXDELFAIL));
  }
  
  return(TRUE);
} /* del_ixattr */

/* ---------------------------------------------------------- */

BOOL updt_ixattr(pixd, sptr, del_buf, key_buf)
     struct ix_header **pixd;
     struct scan *sptr;
     char    **del_buf, **key_buf;
{
  BOOL insert = FALSE;
  
  if (*pixd == NULL) {
    if ((*pixd = CALLOC(1, struct ix_header)) == NULL)
      return(error(INSMEM));
  
    /* open index file and assign index header structure */
    if (!assign_pixd(sptr->sc_relation->rl_name, *pixd))
      return(error(IXFLNF));
  }

  /* check if new entry is a viable entry for foreign keys */
  if (!f_consistency(*pixd, key_buf))
    return(error(INCNST));
  
  /* index the attributes according to their key specs */
  if (!index_attrs(sptr->sc_recpos, *pixd, del_buf, insert)) {
      close((*pixd)->ixfile);
    if (key_buf)
      return(FALSE);
    else
      return(error(IXDELFAIL));
  }
  
  return(TRUE);
} /* updt_ixattr */

/* ---------------------------------------------------------- */

/* fill in attribute to be updated, deleted */
fill_ix(key_buf, aptr, p_cntr, s_cntr, avalue)
     char    ***key_buf;
     struct attribute *aptr;
     int    p_cntr, s_cntr;     /* primary and secondary keys */
     char    *avalue;           /* value to be indexed */
{
  char    key;     /* key type */
  
  /* get key type */
  key = aptr->at_key;
  
  /* create a key buffer */
  if (*key_buf == NULL)
    if ((*key_buf = (char **)calloc(4, sizeof(char *))) == NULL)
      return (error(INSMEM));
  
  /* check for primary key */
  if (key == 'p' && p_cntr - 1 < MAXKEYS)  {
    if (*avalue == EOS) {  /* in case of deletetion */
      (*key_buf)[p_cntr-1] = (char *)NULL;
      return(TRUE);
    }
    
    (*key_buf)[p_cntr-1] = 
      calloc(1, (unsigned) aptr->at_size + 1);
    strcpy((*key_buf)[p_cntr-1], avalue);
    return(TRUE);
  }
  
  /* check for secondary key */
  if (key == 's' && s_cntr - 1 < 2 * MAXKEYS)  {
    if (*avalue == EOS) {  /* in case of deletetion */
      (*key_buf)[s_cntr-1] = (char *)NULL;
      return(TRUE);
    }
    
    (*key_buf)[s_cntr-1] = 
      calloc(1, (unsigned) aptr->at_size + 1);
    strcpy((*key_buf)[s_cntr-1], avalue);
    return(TRUE);
  }
  
  return(FALSE);
} /* fill_ix */

/* ---------------------------------------------------------- * 
   |            foreign key support functions               |
   * -------------------------------------------------------- */

/* combine fgn keys to create a prim key for a composite rel */
combine_fkeys(pixd, keys, entr)
     struct ix_header *pixd;
     struct entry *entr;
     char    **keys;
{
  char *temp_1, *temp_2, *comb_key, prefix[5];
  int len;
  char *rem_blanks();
  
  /* check whether this relation is a composite relation 
   * and both foreign key values are present */
  if ((pixd->dx.ixatts.ix_foreign == 't') && 
                               keys[0] && keys[1]) {
    temp_1 = (char *)calloc(1, (unsigned) strlen(keys[0]) + 1);
    temp_2 = (char *)calloc(1, (unsigned) strlen(keys[1]) + 1);
    
    /* copy keys to temp buffers by discarding any 
     * trailing blanks in keys */
    rem_blanks(temp_1, keys[0], strlen(keys[0]));
    rem_blanks(temp_2, keys[1], strlen(keys[1]));
    
    comb_key =  (char *)calloc(1, (unsigned) strlen(temp_1) + 
			       (unsigned) strlen(temp_2) + 1);
    
    /* see if combination of foreign keys already exists */
    clear_mem(entr->key, 0, MAXKEY);
    /* denotes primary key composed of two foreign keys */
    sprintf(prefix, "P2*");
    
    /* form the composite key for this entry */
    if (strlen(temp_1) + strlen(temp_2) <= MAXKEY-3) {
      cat_strs(comb_key, temp_1, temp_2);
      cat_strs(entr->key, prefix, comb_key);
    } else {
      /* compute how many chars will actually fit */
      len = (MAXKEY-3) - strlen(temp_1);
      strcat(comb_key, temp_1);
      strncat(comb_key, temp_2, len);
      cat_strs(entr->key, prefix, comb_key);
      printf("The combination of your foreign keys is too ");
      printf("long and this may lead into trouble!\n");
    }
    
    /* check if each foreign key exists individually, 
     * if not so report error */
    if (!chk_keycat(pixd, temp_1, temp_2)) {
      nfree(temp_1); 
      nfree(temp_2); 
      nfree(comb_key);
      return(FALSE);
    }
    
    /* then insert the composite key formed by the merging 
     * of the  two foreign keys */
     if (find_ins(entr, pixd) == IX_FAIL) { /* already there */
      nfree(temp_1); 
      nfree(temp_2); 
      nfree(comb_key);
      return(error(WRNGCOMB));
    }
    /* free buffers */
    nfree(temp_1); 
    nfree(temp_2); 
    nfree(comb_key);
  }
  
  return(TRUE);
} /* combine_fkeys */

/* ---------------------------------------------------------- */

/*check for foreign key concistency */
f_consistency(pixd, keys)
     struct ix_header *pixd;
     char    **keys;
{
  char *temp_1 = (char *)NULL, *temp_2 = (char *)NULL;
  
  /* check whether this relation is a composite relation */
  if (pixd->dx.ixatts.ix_foreign == 't') {
    if (keys[0]) {
      temp_1 = (char *)calloc(1, (unsigned) strlen(keys[0])); 
      /* copy key to temp buffer */
      rem_blanks(temp_1, keys[0], strlen(keys[0])); 
    }
    
    if (keys[1]) {
      temp_2 = (char *)calloc(1, (unsigned) strlen(keys[1]));
      rem_blanks(temp_2, keys[1], strlen(keys[1]));
    }
    
    /* check if each foreign key exists individually, 
     * if not report error */
    if (!chk_keycat(pixd, temp_1, temp_2)) {
      if (keys[0])
        nfree(temp_1); 
      
      if (keys[1])
        nfree(temp_2); 
      
      return(FALSE);
    }
    
    /* free buffers */
    if (keys[0])
      nfree(temp_1); 
    
    if (keys[1])
      nfree(temp_2); 
  }
  
  return(TRUE);
} /* f_consistency */

/* ---------------------------------------------------------- */

/* check whether foreign key values exist individually 
 * in its qualifying relation */
BOOL chk_keycat(pixd, f_key1, f_key2)
     struct ix_header *pixd;
     char *f_key1, *f_key2;
{
  struct ix_header *ixd;
  struct entry entr;
  char prefix[5], fname[RNSIZE+6], qua_rel[15];
  int ret, tcnt;
  
  if (!(ixd = (struct ix_header *) calloc(1, sizeof (struct ix_header))))
    return(FALSE);

  clear_mem(ixd, 0, sizeof(struct ix_header));
  ixd->ixfile = -1;

  /* loop through referenced key array in index file header */
  for (tcnt = 0; tcnt < MAXKEYS; tcnt++)  {
    
    /* make sure that fgn key buffers contain non-null values */
    if ((tcnt == 0) && !f_key1)
      continue;
    
    if ((tcnt == 1) && !f_key2)
      continue;
    
    /* find names of the qualifying rels and referenced keys */
    l_substring(pixd->dx.ixatts.ix_refkey[tcnt], qua_rel);
    
    /* form indexed value of primary key of the qualifier rel */
    clear_mem(entr.key, 0, MAXKEY);
    entr.lower_lvl = 0;
    sprintf(prefix, "P0*");
    
    /* form entry for the foreign keys */
    if (tcnt == 0)
      cat_strs(entr.key, prefix, f_key1);
    else
      cat_strs(entr.key, prefix, f_key2);
    
    /* form the index file name for the qualifying relation */
    cat_strs(fname, qua_rel, ".indx");
    
    /* open the qualifier indexed file */
    if (!make_ix(ixd, fname)) {
      finish_ix(&ixd);
      return(FALSE);
    }
    
    /* find if value of primary key exists in qualifier */
    ret = find_ix(&entr, ixd);
    
    /* close qualifier index */
    close(ixd->ixfile);
    
    clear_mem(ixd, 0, sizeof(struct ix_header));
    ixd->ixfile = -1;

    if (ret != 0) {
      printf("WRONG INSERTION:  ");
      printf("value %s does not exist in relation # %s # !\n", 
                                        (entr.key)+3, qua_rel);
      free(ixd);
      ixd = NULL;
      return(FALSE);
    }
  }
  
  /* return successfully */
  free(ixd);
  ixd = NULL;
  return(TRUE);  
  
} /* chk_keycat */

/* ---------------------------------------------------------- */

BOOL change_fgnkey(pixd, sptr, deleted)
     struct ix_header *pixd;
     struct scan *sptr;
     BOOL deleted;
{
  struct ix_header *ixd;    /* composite index header */
  struct sel *slptr, *db_select(), *retrieve();
  struct sattr *saptr;
  struct sel *slptr_comp;  /* composite rel select structure */
  char  fname[RNSIZE+6], fmt[200], comp_rel[RNSIZE + 1];
  int tcnt, i;
  BOOL tmp_wh_signal = wh_signal;
  static union code_cell tmp_code[CODEMAX + 1];
  struct pred *tmp_pred[2];
  
  /* if this is a system catalog, return */
  if (strncmp(SCAN_RELATION->rl_name, "sys", 3) == EQUAL)
    return(TRUE);
  
  if (!(ixd = CALLOC(1, struct ix_header)))
    return(FALSE);

  fname[0] = EOS; 
  
  /* if a target rel make changes in all composite rels */
  if ( pixd->dx.ixatts.ix_foreign == 'f') {
    
    save_pred(tmp_pred);  /* save predicate in case of canned query */

    /* preserve code array contents before retrieve operation */
    structcpy((char *) tmp_code, (char *) code, 
	      (CODEMAX+1) * sizeof(union code_cell));

    /* project name of composite rel in the SYSKEY catalog */
    sprintf(fmt, "syskey over relname where  qualifier \
                               = \"%s\" & primary = \"%s\"; ", 
            pixd->dx.ixatts.ix_rel, pixd->dx.ixatts.ix_prim[0]);
    
    /* selection pointer to SYSKEY relation */
    if ((slptr = retrieve(fmt)) == NULL) {
      restore_pred(tmp_pred); /* restore pred. in case of canned query */
      structcpy((char *) code, (char *) tmp_code, 
		(CODEMAX+1) * sizeof(union code_cell));
      wh_signal = tmp_wh_signal;
      return(FALSE);
    } else
      cmd_kill();   /* clear command line */
    
    /* loop through each relation name projected fom SYSKEY */
    for (tcnt = 0; fetch(slptr, TRUE); tcnt++)  {
      saptr = SEL_ATTRS;
      
      /* do not process same relation twice */
      if (strncmp(fname, saptr->sa_aptr, strlen(saptr->sa_aptr))
	  					== EQUAL)
        break;
      
      /* form the name of the composite index file */
      cat_strs(fname, saptr->sa_aptr, ".indx");
      
      /* open the composite indexed file */
      if (!make_ix(ixd, fname)) {
        finish_ix(&ixd);
	db_rclose(slptr->sl_rels->sr_scan);
	restore_pred(tmp_pred); /* restore pred. in case of canned query */
	structcpy((char *) code, (char *) tmp_code, 
		  (CODEMAX+1) * sizeof(union code_cell));
	wh_signal = tmp_wh_signal;
        return(FALSE);
      }
      
      /* open composite data file, return a select 
       * structure pointer */
      sprintf(fmt, "from %s", saptr->sa_aptr); 
      if ((slptr_comp = db_select(fmt)) == (struct sel *)NULL) {
	db_rclose(slptr->sl_rels->sr_scan);
	restore_pred(tmp_pred); /* restore pred. in case of canned query */
	structcpy((char *) code, (char *) tmp_code, 
		  (CODEMAX+1) * sizeof(union code_cell));
	wh_signal = tmp_wh_signal;
        return (FALSE);
      } else
        cmd_kill();   /* clear command line */
      
      /* check if displacement in index buffer is required */
      strcpy(comp_rel, ixd->dx.ixatts.ix_rel); 
      displace_ixbufs(ixd, pixd->dx.ixatts.ix_rel);
      
      /* delete all foreign key occurrences in composite rel */
      if(!update_entry(ixd, slptr_comp, pixd->dx.ixatts.ix_prim[0])) {
	db_rclose(slptr->sl_rels->sr_scan);
	restore_pred(tmp_pred); /* restore pred. in case of canned query */
	structcpy((char *) code, (char *) tmp_code, 
		  (CODEMAX+1) * sizeof(union code_cell));
	wh_signal = tmp_wh_signal;
        return(FALSE);
      }
      
      /* return successfully */
      for (i = 0; i < MAXKEYS; i++)
	if (key_buf[i] && del_buf[i] && !deleted)
	 printf("Replaced all # %s # entries with # %s # in relation %s !\n\n",
		   del_buf[i],
		   key_buf[i],
		   comp_rel
		   );

      db_rclose(slptr->sl_rels->sr_scan);		/* close syskey */

      /* NEW */
      /* close composite index file */
      if (ixd) {
        close_ix(ixd);
      };
    }
    /* current relation is not a target relation */
    if (tcnt == 0) {
      restore_pred(tmp_pred); /* restore pred. in case of canned query */
      structcpy((char *) code, (char *) tmp_code, 
		(CODEMAX+1) * sizeof(union code_cell));
      wh_signal = tmp_wh_signal;
      return(TRUE);
    }
  } else  /* relation is a composite relation */
    return (update_composite(pixd, deleted));
  
  restore_pred(tmp_pred); /* restore pred. struct in case of canned query */
  structcpy((char *) code, (char *) tmp_code, 
	    (CODEMAX + 1) * sizeof(union code_cell));
  wh_signal = tmp_wh_signal;
  return(TRUE); 
  
} /* change_fgnkey */

/* ---------------------------------------------------------- */

del_fgn(pixd, pentr, cntr)   /* delete foreign key entry */
     struct ix_header *pixd;
     struct entry *pentr;
     int cntr;
{  
  char prefix[20];
  
  /* form indexed value of each fgn key of the composite rel */
  clear_mem(pentr->key, 0, MAXKEY);
  sprintf(prefix, "P%d*", cntr);
  cat_strs(pentr->key, prefix, del_buf[cntr]);
  
  /* delete old foreign key entry in composite relation */
  find_ix(pentr, pixd);  /* find pos of old key in B-tree */
  
  if (find_del(pentr, pixd) == IX_FAIL)  /* then delete it */
    return(FALSE);
  
  /* if an update operation on foreign keys is required */
  if (key_buf[cntr] && *key_buf[cntr])
    /* form new key entry */
    cat_strs(pentr->key, prefix, key_buf[cntr]);
  
  return(TRUE);
} /* del_fgn */

/* ---------------------------------------------------------- */

BOOL update_composite(pixd, deleted)
     struct ix_header *pixd;
     BOOL deleted;
{
  BOOL result;

  if (pixd->dx.ixatts.ix_foreign != 't') 
    return(FALSE);
  
  /* curent relation is not a composite relation */
  if (pixd->dx.ixatts.ix_refkey[0] == NULL) 
    return(TRUE);
  
  if (! deleted)   /* for updates only */
    result = replace_foreign(pixd, (struct sel *) NULL, FALSE);
  else {
    /* check whether new composite key already exists */
    if (!check_replace(pixd))  
      return(FALSE); 
    else
      /* replace  old composite primary key entry with 
       * new key values */
      result = replace_foreign(pixd, (struct sel *) NULL, FALSE);
  }  

  return(result);
} /* update_composite */

/* ---------------------------------------------------------- */

BOOL update_entry(pixd, slptr_comp, ref_key)
     struct ix_header *pixd;
     struct sel *slptr_comp;
     char *ref_key;
{
  int i, j;                   /*  index counter of foreign key */
  BOOL is_done = FALSE; 
  BOOL tmp_wh_signal = wh_signal;
  int result;
  char fmt[200];
  char fgn_key[ANSIZE + 1];

  /* check whether new composite key already exists */
  if (!check_replace(pixd))  
    return(FALSE); 
  
  /* loop through all keys in the index buffers */
  for (i = 0; i < MAXKEYS; i++)  
    if ((key_buf[i] && *key_buf[i]) || 
	(del_buf[i] && *del_buf[i])) { 
      
      /* if an update operation on foreign keys is required */
      if (key_buf[i] && *key_buf[i]) { 
        
        /* update fgn key in data tuple only once per entry */
        if (!is_done) {
          /* replace composite primimary key entries
	   * with new key values */
          replace_foreign(pixd, slptr_comp, TRUE);
          
          is_done = TRUE;   /* tuple modified */
        }
      } else {
	if (! r_substring(pixd->dx.ixatts.ix_refkey[0], fgn_key))
	  return(FALSE);
	
	if (strcmp(fgn_key, ref_key) == EQUAL) 
	  j = 0;
	else 
	  j = 1;

	strcpy(fgn_key, pixd->dx.ixatts.ix_prim[j]);
	  
        /* delete foreign key occurrences */
	canned_commit = TRUE;
	sprintf(fmt, "delete %s where %s = %s", 
		pixd->dx.ixatts.ix_rel, fgn_key, del_buf[j]);
	close_ix(pixd);
	db_rclose(slptr_comp->sl_rels->sr_scan);
	result = parse(fmt, FALSE, FALSE);
	wh_signal = tmp_wh_signal;
	canned_commit = FALSE;
	return(result);
      }
    }
  
  return(TRUE);
} /* update_entry */   

/* ---------------------------------------------------------- */

displace_ixbufs(pixd, r_name)
     struct ix_header *pixd;
     char *r_name;
{   
  int p_cntr;
  
  /* find relative position of target relation in composite 
     indexed attributes structure */
  for (p_cntr = 0; p_cntr < MAXKEYS; p_cntr++)
    if (strncmp(pixd->dx.ixatts.ix_refkey[p_cntr], 
		r_name, strlen(r_name)) == EQUAL)
      break;
  
  /* this is the second fgn key of the composite relation, 
     displace insert & delete index buffer contents one place 
     downwards */
  if (p_cntr == 1) {  
    if (key_buf[p_cntr-1]) {
      key_buf[p_cntr] = 
	calloc(1, (unsigned) strlen(key_buf[p_cntr-1]) + 1);
      strcpy(key_buf[p_cntr], key_buf[p_cntr-1]);
    nfree(key_buf[p_cntr-1]);
    key_buf[p_cntr-1]  = (char *)NULL;
    }
    
    if (del_buf[p_cntr-1]) {
      del_buf[p_cntr] = 
	calloc(1, (unsigned) strlen(del_buf[p_cntr-1]) + 1);
      strcpy(del_buf[p_cntr], del_buf[p_cntr-1]);
    nfree(del_buf[p_cntr-1]);
    del_buf[p_cntr-1]  = (char *)NULL;
    }
  }
} /* displace_ixbufs */

/* ---------------------------------------------------------
   | replace the part of composite key holding the old     |
   | value with its corresponding new value                |
   * ------------------------------------------------------- */

BOOL check_replace(pixd)
     struct ix_header *pixd;
{ struct entry  entr;
  int ret;
  char temp[100];
  
  if (key_buf) {
    /* if both parts of composite key must be changed */
    if ((key_buf[0] && *key_buf[0]) && 
	        (key_buf[1] && *key_buf[1])) {
      
      /* form composite key to be inserted */
      cat_strs(temp, key_buf[0], key_buf[1]);
      cat_strs(entr.key, "P2*", temp);
      
      /* find entry >= than the 1st foreign entry key  */ 
      ret = find_ix(&entr, pixd);
      
      if (ret == 0)  /* if entry found in record */
        return(error(WRNGCOMB));
    }  /* only part of the composite key must be changed */
    else if ((key_buf[0] && *key_buf[0]) || 
	                    (key_buf[1] && *key_buf[1])) 
      if (!check_part(pixd))
        return(FALSE);
  }
  
  return(TRUE);
} /* check_replace */

/* ---------------------------------------------------------- */

check_part(pixd) /* check if composite key exists in B-tree */
     struct ix_header *pixd;
{
  struct entry  comp_entr, temp_entr;
  int i, j, loc, ret;
  
  clear_mem(comp_entr.key, 0, MAXKEY);
  strcpy(comp_entr.key, "P2*");   /* set up composite prefix */
  
  for (j = 0; ;j++) { /* if foreign key exists more than once */
    
    for (i = 0; i < MAXKEYS && (key_buf[i]); i++) { 
      
      if (j == 0) { /* locate the first value of comp. key */
        find_ix(&comp_entr, pixd); 
        
        /* get the current entry there */ 
        get_current(&comp_entr, pixd);  
      }
      
      /* stop if past composite key entries in B-tree  */
      if (strncmp(comp_entr.key, "P2*", 3) != EQUAL)
        break;
      
      /* return location of old value in B-tree entry, make 
       * sure whether foreign key contained in left or right 
       * part of composite key */
      if (i == 0)   
	/* left substring */
        loc = lsubstring_index(&comp_entr.key[3], del_buf[i]); 
      else
	/* right substring */
        loc =  rsubstring_index(&comp_entr.key[3], del_buf[i]);
      
      /* replace old fgn key part with new fgn key in comp key */
      if (loc != FAIL) {
        /* copy value of composite entry */
        structcpy((char *) &temp_entr, 
		  (char *)&comp_entr, sizeof(struct entry));
        
        remove_substring(&temp_entr.key[3], loc, strlen(del_buf[i]));
        insert_string(&temp_entr.key[3], key_buf[i], loc);
        
        /* try to locate new composite key */
        ret = find_ix(&temp_entr, pixd);
        
        /* if entry already there report error */
        if (ret == 0)
          return(error(WRNGCOMB));
      }
    }
    
    /* get further composite entries, unless at EOF */
    find_ix(&comp_entr, pixd); 
    /* reset current pos in index file */
    get_current(&comp_entr, pixd);
    if (get_next(&comp_entr, pixd) == EOF_IX)
      break; 
  }
  
  return(TRUE);
  
} /* check_part */

/* --------------------------------------------------------- */

replace_foreign(pixd, slptr, target)
     struct ix_header *pixd;
     struct sel *slptr;  /* composite rel selection structure */
     BOOL target;        /* on when we have a target rel */
{
  struct entry  comp_entr, temp_entr, first_entr;
  int i, loc;
  BOOL found = FALSE, done = FALSE; /* single iteration flag */
  BOOL insert_comp = FALSE;

  clear_mem(comp_entr.key, 0, MAXKEY);

  strcpy(comp_entr.key, "P2*");

  /* form composite key to be deleted */
  if (del_buf[0] && *del_buf[0]) {
    strcat(comp_entr.key, del_buf[0]);
    if (del_buf[1] && *del_buf[1])
      strcat(comp_entr.key, del_buf[1]);
  }

  /* find entry >= the composite entry key (primary key ) */ 
  find_ix(&comp_entr, pixd);
  get_current(&comp_entr, pixd);  /* get the cur. entry there */ 
  
  /* NEW ADDED */
  if (key_buf[0] && *key_buf[0] && !target)   /* for updates only */
     while(comp_entr.lower_lvl != pixd->ix_recpos)
        get_next(&comp_entr, pixd);

  /* in case of delete remove old composite key in composite relation */
  if (!key_buf[0] && !key_buf[1]) {
     if (find_del(&comp_entr, pixd) == IX_FAIL)      
       return(error(IXDELFAIL));
     else 
       return(TRUE);
  }

  /* replace old value in those entries in primary key 
   * that include the foreign key, if foreign key exists 
   * more than once replace all their occurrences */
  while(TRUE) {
    for (i = 0; i < MAXKEYS; i++)  {
      if (key_buf[i] && *key_buf[i]) {  /* for updates only */
        /* return location of old value in B-tree entry, make 
	 * sure whether foreign key contained in left or right 
	 * part of composite key */
        if (i == 0)   
          loc = lsubstring_index(&comp_entr.key[3], 
			   del_buf[i]); /* left substring */
        else
          loc =  rsubstring_index(&comp_entr.key[3], 
			    del_buf[i]); /* right substring */
        
        /* old value cannot be found in remaining composite 
	 * key entries */
        if (found == TRUE && loc == FAIL && i == 0)
          break;
        
        found = FALSE;       /* reset found flag */
        
        if (loc != FAIL) {   /* if entry located */
          
          if (target && !done) {/* more than 1 key occurrence */
            
            /* find position of 1st foreign key within composite index file */
            find_foreign(i, loc, pixd, &first_entr, &comp_entr, &temp_entr);

            /* if current index positions do not coincide, 
                    then key is already deleted */
            if (temp_entr.lower_lvl !=  first_entr.lower_lvl && !target)       
              return(TRUE); /* if this is a composite relation return */

            /* del fgn key entry and form new key if required */
            if (!del_fgn(pixd, &first_entr, i)) 
              return(FALSE);
            
            /* insert new foreign key entry */
            if(find_ins(&first_entr, pixd) == IX_FAIL) 
                break;                  
              
            if (!modify_tuple(pixd, slptr, 
				pixd->dx.ixatts.ix_prim[i], 
                                key_buf[i], 
				first_entr.lower_lvl)) 
                return(error(UPDTERR));                       
          }
          
          if (!insert_comp) { /* make appropriate insertions */
            
            /* preserve old composite key entry */
            structcpy((char *)&temp_entr, 
		      (char *)&comp_entr, sizeof(struct entry));
            
            /* replace old substring with new substring in 
	     * composite key */
            remove_substring(temp_entr.key, 
			     loc + 3, 
			     strlen(del_buf[i]));
            insert_string(temp_entr.key, key_buf[i], loc + 3);
            
	    if (i == 0 && key_buf[1] && *key_buf[1]) {
	      insert_string(temp_entr.key,key_buf[1],loc + 3 + strlen(key_buf[0]));
	      temp_entr.key[loc + 3 + strlen(key_buf[0]) + strlen(key_buf[1])] = EOS;
	      insert_comp = TRUE;
	    }

            /* insert new composite key */
            if (find_ins(&temp_entr, pixd) == IX_FAIL)      
              return(error(IXFAIL));
            
	    /* remove old composite key in composite relation */
	    if (!done) {
	      if (find_del(&comp_entr, pixd) == IX_FAIL)      
		return(error(IXDELFAIL));

 	      found = TRUE;   /* primary subkey found */
	    }

            /* if it is a composite relation return */
            if (!target)
              return(TRUE);
	    else 
	      break;
          }	/* end insertions */

        } /* if loc */
      } /* if key_buf */
     } /* for i */
    
    
    if (!found) {   /* get next entry in the index */
      if (get_next(&comp_entr, pixd) == EOF_IX)
           break; 

      /* stop if past composite key entries in B-tree  */
      if (strncmp(comp_entr.key, "P2*", 3) != EQUAL)
        break;
    }
    else { 
      /* after a previous delete the index 
	           * position is in front of current */
      get_current(&comp_entr, pixd);
  
      /* if past primary sub-key exit loop */
      if (del_buf[0] && 
	  strncmp(del_buf[0], 
		  comp_entr.key + 3, 
		  strlen(del_buf[0])) != EQUAL)
	break;
      
      if (key_buf[0]) {   /* if first fgn key was replaced */
      /* if first part of comp. key and inserted fgn key match */
        if (strncmp((comp_entr.key)+3, 
		    del_buf[0], 
		    strlen(del_buf[0])) > 0)
          break;   /* exit the loop */
      }
      else if (key_buf[1]) { /* if second fgn key was replaced */
        if (strncmp((comp_entr.key)+3 + strlen(comp_entr.key+3)
                    - strlen(key_buf[1]), 
		    key_buf[1], strlen(key_buf[1])) == EQUAL)
          if (get_next(&comp_entr, pixd) == EOF_IX)
            break; 
      }
    }
    
    /* stop if past composite key entries in B-tree  */
    if (strncmp(comp_entr.key, "P2*", 3) != EQUAL)
      break;
    else
      done = FALSE;     
   
  } /* while-true loop */
  
  /* return succesfully */
  return(TRUE);
  
} /* replace_foreign */

/* ---------------------------------------------------------- */

find_foreign(cntr, loc, pixd, fgn_entr, comp_entr, temp_entr)
   int cntr, loc;
   struct ix_header *pixd;
   struct entry  *fgn_entr, *comp_entr, *temp_entr;
{
  char fgn_key[MAXKEY];

  clear_mem(fgn_entr->key, 0, MAXKEY);
            
  /* preserve old composite key entry */
  structcpy((char *) temp_entr, 
		     (char *) comp_entr, sizeof(struct entry));
            
  /* remove from composite key its foreign part */ 
  remove_substring(temp_entr->key, loc + 3, strlen(del_buf[cntr]));

  /* form the  foreign key */
  if (cntr == 0)   
      cat_strs(fgn_key, "P1*", &temp_entr->key[3]);
  else
      cat_strs(fgn_key, "P0*", &temp_entr->key[3]);
            
  /* locate this key */
  strcpy(fgn_entr->key, fgn_key);
  fgn_entr->lower_lvl = temp_entr->lower_lvl;

  /* find entry in B-tree */
  find_ix(fgn_entr, pixd);

} /* find_foreign */

/* ---------------------------------------------------------- */

BOOL modify_tuple(pixd, slptr, fkey_name, new_value, data_level)
     struct ix_header *pixd;
     struct sel *slptr; /* composite relation sel pointer */
     char *fkey_name;   /* name of fgn key in composite rel */
     char *new_value;   /* new value of foreign key */
     long data_level;   /* pos of fgn key in comp rel file */
{
  struct relation *rptr;
  struct sattr *saptr;
  
  rptr = SEL_RELATION;
  
  /* read original relation from data file */
  if (read_tuple(rptr, data_level, SEL_TUPLE) == FALSE) 
    return(FALSE); 
  
  /* look for fgn key within selected attributes structure */
  for (saptr = SEL_ATTRS; saptr != NULL; saptr = saptr->sa_next)
    if (strcmp(fkey_name, saptr->sa_aname) == EQUAL) {  
      /* store its new value */
      store_attr(saptr->sa_attr, saptr->sa_aptr, new_value);
      saptr->sa_srel->sr_update = TRUE;
      /* data tuple address */
      saptr->sa_srel->sr_scan->sc_recpos = data_level;
      break;
    }
  
  /* write changes back to file */
  if(!update_tuple(pixd, slptr->sl_rels,(char **)NULL,(char **)NULL))
    return(FALSE);
  
  /* return successfully */
  return(TRUE);
  
} /* modify_tuple */

/* --------------------------------------------------------- 
   |                index searching routines                |
   * -------------------------------------------------------- */

int access_plan(prptr, rel_ptr, slptr, cntr, hash, updt)
     struct pred *prptr;
     struct relation *rel_ptr;
     struct sel *slptr;
     int    *cntr;
     BOOL hash, updt;
{
  struct ix_header *ixd;
  struct entry e, e2, upper_entry, last_entry; /* NEW */
  struct relation *rptr;
  char    *buf;
  char    fname[RNSIZE+6];
  char    upper[35], lower[35];
  int    path_res = FAIL, i = 0;
  long    data_level;
  BOOL opt = FALSE, temp_ixattr;
  BOOL first_change = TRUE; /* NEW FLAG to indicate whether it's first change */
 
  if (!(ixd = (struct ix_header *) calloc(1, sizeof (struct ix_header))))
    return(FALSE);

  /* initialize local variables */
  *cntr = 0;
  clear_mem(lower, 0, 35);
  clear_mem(upper, 0, 35);
  upper[0] = lower[0] = EOS;
  rptr = SEL_RELATION;

  /* form an index file name and open the corresponding file */
  if (!form_ix(fname, ixd, rptr))
    return(FALSE);
  
  /* allocate and initialize a tuple buffer */
  if ((buf = calloc(1, (unsigned) RELATION_HEADER.hd_size + 1))
      					 == NULL) {
    rc_done(rptr, TRUE);
    return (error(INSMEM));
  }
  
  /* if only one, or the same, indexed attr in predicate */
  if (prptr->pr_ixtype[i] && prptr->pr_ixtype[i+1] == NULL && 
                                        !non_ixattr) {
    /* locate the indexed attribute */
    /* if two indexed predicates start from b.o.ix */
    if ((opt = gtr_equ(4, lower, 0, upper)) == FALSE)
      opt = lss_equ(4, lower, 0, upper);
    
    if (opt == FALSE)  /* no optimization possible */
      lower[0] = upper[0] = EOS;
    
    /* form the B-tree search entry */
    cat_strs(e.key, prptr->pr_ixtype[i], lower);
    
    /* locate the lower search key entry in B-tree */ 
    if (!locate_ix(prptr, ixd, &e, &e2, &data_level)) {
      finish_ix(&ixd);
      return(FALSE);
    }
  } else if (prptr->pr_ixtype[0] == NULL) {
    finish_ix(&ixd);
    return(error(WRIXCOM));
  }
  
  /* if there is a 2nd  or a non-indexed attr in predicate */
  if (non_ixattr || prptr->pr_ixtype[i+1]) {
    
    /*  find the start of associated indexed key */
    strncpy(e.key, prptr->pr_ixtype[i], 3);
    e.key[3] = EOS;

    if (!locate_ix(prptr, ixd, &e, &e2, &data_level)) {
      finish_ix(&ixd);
      return(FALSE);
    }
    
    /* read curent index and get other attrs in predicate */
    read_get(rptr, slptr, buf, data_level);
  }

  /* NEW ADDED */
  last_entry.lower_lvl = e2.lower_lvl; /* get the last entry */  
  strcpy(last_entry.key, e2.key);

  /* preserve value of global index flag in the case 
   * of composite deletes, updates */
  temp_ixattr = non_ixattr;
  clear_mem(upper_entry.key, 0, MAXKEY);
  cat_strs(upper_entry.key, prptr->pr_ixtype[i], upper);

  while (TRUE) {    /* interpret the results of the search */
    /* 1 indexed attr in predicate with prefix in ix_type[0] */
    if (temp_ixattr == FALSE && prptr->pr_ixtype[i+1] == NULL) {
      
      /* if search limit has been exceeded exit */
      if (upper_entry.key[3] != EOS) {
	if (compf(&e2, &upper_entry, get_datatype(ixd, &e2)) > 0)
	  break;
      }
      
      /* in case of only one indexed attribute, if 
       * interpretation successful or new search  key value
       * same with the old search key value, keep on searching */

      if (xinterpret()) {

        if ((path_res = sel_path(ixd, updt, hash, cntr,
               buf, rptr, rel_ptr, slptr, data_level)) == FALSE)
          return(FALSE);
        else if (path_res == FAIL) {
          finish_ix(&ixd);
          return(path_res);
        }
      } else {
	path_res = FAIL;  /* reset flag in case of failure */
	if (opt  && strcmp(e.key, e2.key) != EQUAL)
	  break;
      }
    } else {  /* inside 2nd indexed or non-indexed attr loop */
      
      /* read current index and get other attrs in predicate */
      read_get(rptr, slptr, buf, data_level);

      if (xinterpret()) {  /* evaluate the result */

        if ((path_res = sel_path(ixd, updt, hash, cntr,
              buf, rptr, rel_ptr, slptr, data_level)) == FALSE)
          return(FALSE);
        else if (path_res == FAIL) {
          finish_ix(&ixd);
          return(path_res);
        }
      } else
	path_res = FAIL;   /* reset flag in case of failure */
    }

    /* NEW ADDED */
    /* in case of failure deletion and update, keep the last entry pos. */
    if ((updt == DELETION && path_res == FAIL)  
	       || (updt == TRUE && path_res == FAIL)) {    
       if (first_change == TRUE)
           first_change = FALSE;
       last_entry.lower_lvl = e2.lower_lvl;  
       strcpy(last_entry.key, e2.key);
    }
  
    /* in case of updates or deletions get back where we were */
    if (updt == DELETION || updt == TRUE) { /* for delete and update cases */
       if (first_change == TRUE) {         /* the first one to be changed */
          find_ix(&e2, ixd); 
          get_current(&e2, ixd);
       } else {
          find_exact(&last_entry, ixd);     /* find the last one entry */
          if (get_next(&e2, ixd) == EOF_IX) /* start from next entry */
	     break;
         } 
       if (updt == TRUE && e2.lower_lvl == data_level) {
          if (first_change == TRUE)
             first_change = FALSE;       /* reset the first_change */
          last_entry.lower_lvl = e2.lower_lvl; /* get the last entry */  
          strcpy(last_entry.key, e2.key);
          if (get_next(&e2, ixd) == EOF_IX) 
	     break;
       }
     } else {
         /* normal selection path */
         if (get_next(&e2, ixd) == EOF_IX) 
	   break;
       }
    /* END OF NEW CHANGED */

    rm_trailblks(e2.key);  /* remove any trailing blanks */
    
    if (strncmp(e2.key, prptr->pr_ixtype[i], 3) != EQUAL)
      break;
    
    assign_pred(prptr->pr_opd[i], &e2, &data_level);
  }
  
  nfree(buf);
  non_ixattr = FALSE;  /* reset non-indexed flag */
  finish_ix(&ixd);
  
  return(TRUE);
} /* acces_plan */

/* ---------------------------------------------------------- */

BOOL locate_ix(prptr, pixd, entr1, entr2, data_level)
     struct pred   *prptr;   
     struct ix_header *pixd;
     struct entry *entr1, *entr2;
     long    *data_level;
{
  /* find entry containing a key value >= to that of entr1 */ 
  find_ix(entr1, pixd);
  
  /* get current entry from index file */
  get_current(entr2, pixd);
  rm_trailblks(entr2->key);  /* remove any trailing blanks */
  
  /* if key prefixes are not identical */
  if (strncmp(entr2->key, prptr->pr_ixtype[0], 3) != EQUAL) {
    strncpy(entr1->key, prptr->pr_ixtype[0],  3);
    entr1->key[3] = EOS;

    find_ix(entr1, pixd);
    get_current(entr2, pixd);
    rm_trailblks(entr2->key);  /* remove any trailing blanks */
  }
  
  /* assign new values to the 1st operand structure in the 
   * predicate structure */
  assign_pred(prptr->pr_opd[0], entr2, data_level);
  
  return(TRUE);
  
} /* locate_ix */


/* ---------------------------------------------------------- */

sel_path(pixd, updt, hash, cntr, buf, 
	 rptr, rel_ptr, slptr, data_level)
     struct ix_header *pixd;
     struct sel *slptr;
     struct relation *rptr, /* original relation file */
       *rel_ptr;            /* transient relation files */
     BOOL updt, hash;
     char    *buf;
     long    data_level;
     int    *cntr;
{    
  int res;   /* obtains three values TRUE, FALSE, and FAIL */

  /* read original relation from data file */
  if (read_tuple(rptr, data_level, SEL_TUPLE) == FALSE) {
    finish_ix(&pixd);
    return(FALSE);
  }

  if (rel_ptr == NULL) {  /* deletion must be performed */
    res = delete_attrs(pixd, slptr, cntr, data_level);
    return(res);   /* if res = FAIL, no futher deletions */
  }
  
  if (updt)  {     /* update must be performed */
    res = update_attrs(pixd, slptr, cntr, data_level);
    return(res);      /* if res = FAIL, no futher updates */
    
  } else {
    if (hash == FALSE) {
      /* copy tuple in buffer */
      structcpy(buf, SEL_TUPLE,  RELATION_HEADER.hd_size);
      write_data(rel_ptr, buf, cntr); /* write data back */
    } else
      hash_data(rel_ptr, rptr, slptr, buf, cntr);
  }
  return(TRUE);
  
} /* sel_path */

/* ---------------------------------------------------------- */

/* check if attr to be inserted or updated requires indexing */
check_ix(key_buf, aptr, p_cntr, s_cntr, avalue)
     char    ***key_buf;
     struct attribute *aptr;
     int    *p_cntr, 		/* primary key pointer */
       *s_cntr; 		/* secondary key pointer */
     char    *avalue;           /* value to be indexed */
{
  char    key;     		/* key type */
  
  /* get key type */
  key = aptr->at_key;
  
  /* create a key buffer */
  if (*key_buf == NULL)
    if ((*key_buf = (char **)calloc(4, sizeof(char *))) == NULL)
      return (error(INSMEM));
  
  /* check for primary key */
  if (key == 'p' && *p_cntr < MAXKEYS)  {
    (*key_buf)[*p_cntr] = calloc(1, aptr->at_size + 1);
    strcpy((*key_buf)[(*p_cntr)++], avalue);
    return(TRUE);
  }
  
  /* check for secondary key */
  if (key == 's' && *s_cntr < 2 * MAXKEYS)  {
    (*key_buf)[*s_cntr] = calloc(1, aptr->at_size + 1);
    strcpy((*key_buf)[(*s_cntr)++], avalue);
    return(TRUE);
  }
  
  return(FALSE);
} /* check_ix */

/* ---------------------------------------------------------- */

BOOL write_data(tmprel_ptr, buf, cntr)
     struct relation  *tmprel_ptr;   /* snapshot rel pointer */
     char    *buf;
     int    *cntr;
{
  (*cntr)++;  /* increment tuple counter */
  
  /* write tuple in the temporary data file */
  if (!write_tuple(tmprel_ptr, buf, TRUE)) {
    rc_done(tmprel_ptr, TRUE);
    return(error(INSBLK));
  }
  return(TRUE);
} /* write_data */

/* ---------------------------------------------------------- */

read_get(rptr, slptr, buf, data_level)
     struct relation *rptr;
     struct sel *slptr;
     char    *buf;
     long    data_level;
{
  int    i;
  char    a_value[LINEMAX], *pr_value;
  struct sattr *saptr;
  char *rem_blanks();

  /* fill the scan buffer with the data tuple */
  read_tuple(rptr, data_level, buf);
  structcpy(SEL_TUPLE, buf, RELATION_HEADER.hd_size);
  
  /* get the other attributes in the predicate */
  for (i = 1; prptr[0]->pr_ixtype[i]; i++)
    for (saptr = SEL_ATTRS; saptr != NULL; 
	                       saptr = saptr->sa_next) {
      if (strcmp(prptr[0]->pr_atname[i],saptr->sa_aname)== EQUAL) {
        pr_value = prptr[0]->pr_opd[i]->o_value.ov_char.ovc_string;
        get_attr(saptr->sa_attr, saptr->sa_aptr, a_value);
        rem_blanks(pr_value, a_value, strlen(a_value));
        prptr[0]->pr_opd[i]->o_value.ov_char.ovc_length =
	                               strlen(pr_value);
      }
    }
  
} /* read_get */

/* ----------------------------------------------------------
   |              auxilliary index routines                 |
   - -------------------------------------------------------- */

BOOL make_ix(pixd, fname)
     struct ix_header *pixd;
     char    *fname;
{
  
  if (open_ix(fname, pixd, compf, sizef) < 0)
    return(error(IXFLNF));
  
  return(TRUE);
  
} /* make_ix */

/* ---------------------------------------------------------- */

/* form  index file and open associated file */
form_ix(fname, pixd, rptr)
     struct relation *rptr;
     struct ix_header *pixd;
     char    *fname;
{
  cat_strs(fname, rptr->rl_name, ".indx");
  
  if (!make_ix(pixd, fname)) {
    finish_ix(&pixd);
    return(FALSE);
  }
  
  return(TRUE);
} /* form_ix */

/* ---------------------------------------------------------- */

/* assign predicate structure memebers */
assign_pred(p_opd, entr, d_level)
     struct operand *p_opd;
     struct entry *entr;
     long    *d_level;
{
  strcpy(p_opd->o_value.ov_char.ovc_string, entr->key + 3);
  p_opd->o_value.ov_char.ovc_string[strlen( entr->key+3)] = EOS;
  p_opd->o_value.ov_char.ovc_length = 
                strlen(p_opd->o_value.ov_char.ovc_string);  
  *d_level =  entr->lower_lvl;
  
} /* assign_pred */

/* ---------------------------------------------------------- */

BOOL    assign_pixd(rname, pixd)
     char *rname;
     struct ix_header *pixd;
{
  char    fname[RNSIZE+6];
  
  cat_strs(fname, rname, ".indx");
  
  if (!open_ix(fname, pixd, compf, sizef))
    return(FALSE);
  else
    return(TRUE);
  
} /* assign_pixd */

/* --------------------- finish indexing -------------------- */

finish_ix(pixd)
     struct ix_header **pixd;
{
  close_ix(*pixd);
  free(*pixd);
  *pixd = NULL;
  free_pred(prptr);
  
} /* finish_ix */

/* ----------------------------------------------------------
   |            predicate optimising routines               |
   ---------------------------------------------------------- */

gtr_equ(ix, lower, op, upper)
     int    ix, op;
     char    *lower;
     char    *upper;
{
  int    (*code_op)();
  int    disp = 6;
  int    ix1;
  
  code_op = code[ix].c_operator;
  if (code_op == xgtr || code_op == xgeq 
      || code_op == xeql || code_op == xpart) {
    
    if (lower[0] != EOS) {
      if (strcmp(code[ix-1].OPERAND_STRING, lower) > 0) {
        strcpy(lower, code[ix-1].OPERAND_STRING);
        lower[strlen(code[ix-1].OPERAND_STRING)] = EOS;
      }
    } else {
      strcpy(lower, code[ix-1].OPERAND_STRING);
      lower[strlen(code[ix-1].OPERAND_STRING)] = EOS;
    }
    
    if (code[ix+1+op].c_operator == xstop) {
      stop_flag = TRUE;
      return(TRUE);
    } else if (code[ix+disp+op].c_operator == xand) {
      ix1 = ix + disp + op - 1;
      if (lss_equ(ix1, lower, op = 1, upper)) {
        
        if (upper[0] != EOS) {
          
          if (stop_flag)
            return(TRUE);
          
          if (strcmp(code[ix1+disp-2].OPERAND_STRING,
		                                 upper)< 0) {
            strcpy(upper, code[ix1+disp-2].OPERAND_STRING);
            upper[strlen(code[ix1+disp-2].OPERAND_STRING)] = EOS;
          }
        } else {
          strcpy(upper, code[ix+disp-2].OPERAND_STRING);
          upper[strlen(code[ix+disp-2].OPERAND_STRING)] = EOS;
        }
        
        return(TRUE);
      } else if (gtr_equ(ix1 + disp - 1, lower, op = 1, upper))
        return(TRUE);
    }
  }
  
  return(FALSE);
} /*gtr_equ */

/* ---------------------------------------------------------- */

lss_equ(ix, lower, op, upper)
     int    ix, op;
     char    *lower, *upper;
{
  int    (*code_op)();
  int    disp = 6;
  int    ix1;
  
  code_op = code[ix].c_operator;
  if (code_op == xlss || code_op == xleq 
      || code_op == xpart || code_op == xeql) {
    
    if (code[ix+1+op].c_operator == xstop) {
      if (!op) {
        strcpy(upper, 
               code[ix-1].OPERAND_STRING);
        upper[strlen(code[ix-1].OPERAND_STRING)] = EOS;
      }
      stop_flag = TRUE;
      return(TRUE);
    } else if (code[ix+disp+op].c_operator == xand) {
      
      if (upper[0] != EOS) {
        
        if (stop_flag)
          return(TRUE);
        
        if (strcmp(code[ix-1].OPERAND_STRING, upper) < 0) {
          strcpy(upper, code[ix-1].OPERAND_STRING);
          upper[strlen(code[ix-1].OPERAND_STRING)] = EOS;
        }
      } else {
        strcpy(upper, code[ix-1].OPERAND_STRING);
        upper[strlen(code[ix-1].OPERAND_STRING)] = EOS;
      }
      
      ix1 = ix + disp + op - 1;
      
      if (gtr_equ(ix1, lower, op = 1, upper)) {
        if (lower[0] != EOS) {
          
          if (stop_flag)
            return(TRUE);
          
          if (strcmp(code[ix1+disp-2].OPERAND_STRING, lower) > 0) {
            strcpy(lower,
                   code[ix1+disp-2].OPERAND_STRING);
            lower[strlen(code[ix1+disp-2].OPERAND_STRING)] = EOS;
          }
        } else {
          strcpy(lower,
                 code[ix1+disp-2].OPERAND_STRING);
          lower[strlen(code[ix1+disp-2].OPERAND_STRING)] = EOS;
        }
        
        return(TRUE);
      }
    }
  }
  return(FALSE);
}

/* ---------------------------------------------------------
   |              index predicate routines                  |
   ---------------------------------------------------------- */

get_ixattr(secnd_rel, rlptr, opr, atr_name, atr_key, atr_len)
     BOOL secnd_rel;
     struct operand **opr;
     struct relation *rlptr;
     char   atr_key, *atr_name;
     int    atr_len;
{ int i;
  struct pred   *tmp_prptr;   

  if (!prptr[0] && !secnd_rel) { /* if pred. struct for 1st rel non existent */
    if (set_pred(&prptr[0]) == FALSE)
      return (error(INSMEM));
  } else if (secnd_rel && !prptr[1]) { /* pred struct needed for 2nd rel */
    if (set_pred(&prptr[1]) == FALSE)
      return (error(INSMEM));
  }

  if (secnd_rel)
    tmp_prptr = prptr[1];
  else
    tmp_prptr = prptr[0];

  /* increment the key indexes */
  if (atr_key == 'p') /* place new value into operand struct */
    check_pred(tmp_prptr, rlptr, opr, atr_name, atr_key, atr_len);
  else if (atr_key == 's') 
    check_pred(tmp_prptr, rlptr, opr, atr_name, atr_key, atr_len);
  
  /* loop thro' the predicate attributes  */
  /* more than one different attributes exist */
  for (i = 0; tmp_prptr->pr_ixtype[i]; i++)
    if (strcmp(tmp_prptr->pr_atname[i], atr_name) != EQUAL) 
      return(FAIL); 
  
  /* return successfully */
  return(TRUE);
} /* get_ixattr */

/* ---------------------------------------------------------- */

check_pred(tmp_prptr, rlptr, opr, atr_name, atr_key, atr_len)
     struct pred   *tmp_prptr;   
     struct relation *rlptr;
     struct operand **opr;
     char    *atr_name;
     char    atr_key;
     int    atr_len;
{
  int    j = 0;  /* key type counter */
  int    ct, i, temp;
  BOOL  opr_set = FALSE;  /* operand structure set flag */
  
  if (tmp_prptr->pr_rlname == NULL) {
    tmp_prptr->pr_rlname = calloc(1, RNSIZE + 1);
    strcpy(tmp_prptr->pr_rlname, rlptr->rl_name);
    tmp_prptr->pr_rlname[strlen(rlptr->rl_name)] = EOS;
  }
  
  /* loop thro' the atributes in the relation structure */
  for (i = 0; *(rlptr->rl_header.hd_attrs[i].at_name); i++) {
    if (rlptr->rl_header.hd_attrs[i].at_key == atr_key) {
      if (strcmp(atr_name, 
		 rlptr->rl_header.hd_attrs[i].at_name) != EQUAL)
        j++;   /* same key type but different atribute names */
      else {
        /* go to next empty position in the predicate array */
        for (ct = 0; tmp_prptr->pr_ixtype[ct]; ct++)
          ;
        
        /* preserve the relative location of next entry */
        temp = ct;
        
        /* loop thro' the predicate attributes once again */
	/* if there is at least one entry */
        for (ct = 0; tmp_prptr->pr_ixtype[ct]; ct++)
          if (temp && 
	      strcmp(tmp_prptr->pr_atname[ct], atr_name) == EQUAL) {
            tmp_prptr->pr_opd[temp] = tmp_prptr->pr_opd[ct];
            *opr = tmp_prptr->pr_opd[temp];
            opr_set = TRUE;
          }
        
        /* discriminate between the entries requiring hashing 
	 * and those that do not */
        if (!opr_set) { 
          if (rlptr->rl_header.hd_attrs[i].at_key == 'p')
            put_pred(tmp_prptr, *opr, atr_name, atr_len, ct, j, TRUE);
          else
            put_pred(tmp_prptr, *opr, atr_name, atr_len, ct, j, FALSE);
        }
      }
    }
  }
} /* check_pred */

/* ---------------------------------------------------------- */

put_pred(tmp_prptr, opr, atr_name, len, ct, indx, flag)
     struct pred   *tmp_prptr;   
     struct operand *opr;
     BOOL flag;
     int    len, ct, indx;
     char    *atr_name;
{
  tmp_prptr->pr_ixtype[ct] = (char *) calloc(1, (unsigned) len + 5);
  tmp_prptr->pr_atname[ct] = 
    (char *) calloc(1, (unsigned) strlen(atr_name) + 1);
  
  /* initialize the indexed predicate attribute types */
  if (flag)  /* primary keys include the prefix P0* */
    sprintf(tmp_prptr->pr_ixtype[ct], "P%d*", indx);
  else      /* secondary keys include the prefix S(indx)* */
    sprintf(tmp_prptr->pr_ixtype[ct], "S%d*", indx);
  
  strcpy(tmp_prptr->pr_atname[ct], atr_name);
  
  if ((tmp_prptr->pr_opd[ct] = 
       CALLOC(1, struct operand )) == NULL) {
    free_opr(tmp_prptr->pr_opd[ct]);
    return (error(INSMEM));
  }
  
  if ((opr->o_value.ov_char.ovc_string = 
       calloc(1, len + 1)) == NULL) {
    free_opr(opr);
    return (error(INSMEM));
  }
  
  tmp_prptr->pr_opd[ct] = opr;
  
  return(TRUE);
} /* put_pred */

/* ----------------------------------------------------------
   |         join functions implementing B-tree conceptes   |
   ---------------------------------------------------------- */

/* with a join statement involving indexed prediacte(s) */
join_ix(relptr, slptr, cntr, join_keys, ix_cntr, 
	             duplc_attr, ref_attr, d_indx, tbuf)
     struct relation *relptr;
     struct sel *slptr;
     int    *cntr, ix_cntr, d_indx;
     char    *join_keys, **duplc_attr, **ref_attr, *tbuf;
{
  struct ix_header *ixd;
  struct entry e1, e2;
  struct relation *rptr;
  struct srel *srptr, *n_srptr;
  char    *name;
  char    fname[RNSIZE+6];
  BOOL first, proceed = TRUE;
  long    data_level;
  struct pred *tmp_prptr;
  *cntr = 0;
  
  if (!(ixd = (struct ix_header *) calloc(1, sizeof (struct ix_header))))
    return(FALSE);

  /* preserve name of first relation */
  name = SEL_ATTRS->sa_rname;
  
  if (ix_cntr > 1)  /* both join keys indexed on some attribute */
     return(join_two(relptr, slptr, cntr, join_keys, 
		     duplc_attr, ref_attr, d_indx, tbuf));
  
  
  /* check if first relation contains index */
  if (prptr[0] && strcmp(name, prptr[0]->pr_rlname) == EQUAL) {
    first = TRUE;
    tmp_prptr = prptr[0];
  } else {
    first = FALSE;
    tmp_prptr = prptr[1];
  }
  
  /* relation pointer points to first relation structure */
  rptr = SEL_RELATION;
  
  /* else rel pointer points to second relation structure */
  if (first == FALSE)
    rptr = rptr->rl_next;
  
  /* open the associated index file */
  if (!form_ix(fname, ixd, rptr)) {
    finish_ix(&ixd);
    return(FALSE);
  }
  
  /* copy index type, and locate attribute in index-file */
  strcpy(e1.key, *(tmp_prptr->pr_ixtype));
  
  if (!locate_ix(tmp_prptr, ixd, &e1, &e2, &data_level)) {
    finish_ix(&ixd);
    return(FALSE);
  }
  
  if (first) {  /* scan second relation data file */
    srptr = SEL_RELS->sr_next;  /* non-indexed data file */
    n_srptr = SEL_RELS;         /* indexed data file */
  } else {
    srptr = SEL_RELS;
    n_srptr = SEL_RELS->sr_next;
  }
  
  while (proceed) { /* loop through appropriate data file */
    
    assign_pred(tmp_prptr->pr_opd[0], &e2, &data_level);

    proceed = process(srptr, TRUE);
    
    while (proceed) { /* loop through indexed tuples */
      if (!(read_tuple(rptr, data_level, 
		       n_srptr->sr_scan->sc_tuple)))  {
	finish_ix(&ixd);
	return(FALSE);
      }
      
      if (xinterpret()) {
        
        if (!(join_write(slptr, relptr, tbuf, 
			 duplc_attr, ref_attr, d_indx, cntr))) {
          finish_ix(&ixd);
          return(FALSE);
        }
      }
      
      /* get subsequent entries in index file */
      if (get_next(&e2, ixd) == EOF_IX)
        break;
      rm_trailblks(e2.key);  /* remove any trailing blanks */
      
      if (strncmp(e2.key, tmp_prptr->pr_ixtype[0], 3) != EQUAL)
        break;
      
      /* assign new values to predicate structure */
      assign_pred(tmp_prptr->pr_opd[0], &e2, &data_level);
      
    }
    
    /* copy index type, and locate attribute in index file */
    strcpy(e1.key, tmp_prptr->pr_ixtype[0]);
    
    if (!locate_ix(tmp_prptr, ixd, &e1, &e2, &data_level)) {
      finish_ix(&ixd);
      return(FALSE);
    }
  }
  
  finish_ix(&ixd);
  return(TRUE);
  
} /* join_ix */

/* ---------------------------------------------------------- */

/* form join format and write resulting tuple */
join_write(slptr, relptr, tbuf, duplc_attr, ref_attr, d_indx, cntr)
     struct sel *slptr;
     struct relation *relptr;
     char    *tbuf, **duplc_attr, **ref_attr;
     int    d_indx, *cntr;
{
  int rc;
  /*  form join format, remove any duplicate attribute names */
  if ((rc = do_join(slptr, duplc_attr, ref_attr, d_indx, tbuf)) == FALSE)
    return(FALSE);
  else if (rc == FAIL) /* dupl attr consistency test failed */
    return(FAIL);
  /* write tuple in new relation */
  if (!write_tuple(relptr, tbuf, TRUE)) {
    rc_done(relptr, TRUE);
    return(error(INSBLK));
  }
  
  /* increment resulting relation tuple counter */
  (*cntr)++;
  
  return(TRUE);
  
} /* join_write */

/* ---------------------------------------------------------- */

join_two(relptr, slptr, cntr, join_keys,  
	 duplc_attr, ref_attr, d_indx, tbuf)
     struct relation *relptr;
     struct sel *slptr;
     int    d_indx, *cntr;
     char    **duplc_attr, **ref_attr, *tbuf;
     char    *join_keys;
{
  struct ix_header *ixd1, *ixd2;
  struct entry e1, e2, e3, e4;
  struct relation *rptr;
  struct scan *sc_ptr1, *sc_ptr2;
  struct sattr *saptr;
  char    *name, *ref_joinon = NULL;
  char    fname1[RNSIZE+6], buf1[5];
  char    fname2[RNSIZE+6], buf2[5];
  int     j;
  long    data_level1, data_level2;
  BOOL    bottom;              /* obtains three values */
  BOOL    proceed = TRUE;
  int     f_ix = 0, s_ix = 0;
  int res;
  struct entry temp;

  if (!(ixd1 = (struct ix_header *) calloc(1, sizeof (struct ix_header))))
    return(FALSE);

  if (!(ixd2 = (struct ix_header *) calloc(1, sizeof (struct ix_header))))
    return(FALSE);

  clear_mem(e1.key, 0, MAXKEY);
  clear_mem(e2.key, 0, MAXKEY);
  clear_mem(e3.key, 0, MAXKEY);
  clear_mem(e4.key, 0, MAXKEY);
  clear_mem(ixd1, 0, sizeof(struct ix_header));
  clear_mem(ixd2, 0, sizeof(struct ix_header));
  
  /* loop through semantically indentical attrs in second rel */
   for (j = 0; j < d_indx; j++) 
    if (duplc_attr[j] && strcmp(duplc_attr[j], join_on[0])) {
      ref_joinon = duplc_attr[j];
      break;
    }
 
  /* relation pointer points to first relation structure */
  rptr = SEL_RELATION;
  sc_ptr1 = SEL_SCAN;
  sc_ptr2 = SEL_RELS->sr_next->sr_scan;
  clear_mem(temp.key, 0, MAXKEY);
  
  /* open index files */
  if (!form_ix(fname1, ixd1, rptr) || 
      !form_ix(fname2, ixd2, rptr->rl_next)) {
    finish_ix(&ixd1);
    finish_ix(&ixd2);
    return(FALSE);
  }
  
  /* preserve name of first relation */
  name = SEL_ATTRS->sa_rname;
  
  /* loop thro' the selected attributes construct the index type
   of each joinded attribute, e.g. P0*, S0*, S1*, ... */
  for (saptr = SEL_ATTRS; saptr != NULL; 
                            saptr = saptr->sa_next) {
    
    if (strcmp(saptr->sa_rname, name ) == EQUAL) {
      if (strcmp(saptr->sa_aname, join_on[0]) == EQUAL)
        proceed = FALSE;
      
      /* copy index type of first indexed attribute */
      if (proceed && ATTRIBUTE_KEY == join_keys[0])
        f_ix++; /* relative position index type counter (1st rel.) */
    } else { /* in second relation */
      if ((strcmp(saptr->sa_aname, join_on[1]) == EQUAL) ||
	  ref_joinon &&   /* semantically equiv. atrs. */  
	  strcmp(saptr->sa_aname, ref_joinon) == EQUAL)
        proceed = TRUE;
      
      /* copy index type of second indexed attribute */
      if (!proceed  && ATTRIBUTE_KEY == join_keys[1])
        s_ix++;  /* relative position index type counter (2nd rel.) */
    }
  }

  /* form the keys of both joined attributes by catenating their 
     key types with their respective relative position index 
     type counter */
  key_def(buf1, join_keys[0], f_ix);
  key_def(buf2, join_keys[1], s_ix);
  
  /* locate the joined attributes in both index files,
     start with second file */
  strcpy(e3.key, buf2);
  
  if (!locate_ix(prptr[1], ixd2, &e3, &e4, &data_level2)) {
    finish_ix(&ixd1);
    finish_ix(&ixd2);
    return(FALSE);
  }
  
  do {
    /* locate the join attribute in first index file */
    strcpy(e1.key, buf1);
    

   if (!locate_ix(prptr[0], ixd1, &e1, &e2, &data_level1)) {
      finish_ix(&ixd1);
      finish_ix(&ixd2);
      return(FALSE);
    }
    
    
    /* read first relation data */
    assign_pred(prptr[0]->pr_opd[0], &e2, &data_level1);
    if (!(read_tuple(rptr, data_level1, sc_ptr1->sc_tuple)))  {
      finish_ix(&ixd1);
      finish_ix(&ixd2);
      return(FALSE);
    }
    bottom = FALSE; /* start at top of 1st relation */
    
    /* read second relation data tuple */
    assign_pred(prptr[1]->pr_opd[0], &e4, &data_level2);
    if (!(read_tuple(rptr->rl_next, data_level2, sc_ptr2->sc_tuple))) {
      finish_ix(&ixd1);
      finish_ix(&ixd2);
      return(FALSE);
    }
    
    while (TRUE) {   /* loop thro' 1st and 2nd relation entries */
      
      if (xinterpret()) { 
        /* form join attrs and write result in relptr struct */
        if (join_write(slptr, relptr, tbuf, 
		duplc_attr, ref_attr, d_indx, cntr) == FALSE) {
          finish_ix(&ixd1);
          finish_ix(&ixd2);
          return(FALSE);
        }
        
        /* store value of joined attribute of second relation */
        strcpy(temp.key, e4.key);
        if (bottom)  /* already at bottom of first relation */
          bottom = FAIL;  /* stop scanning, result obtained */
      }
      
      /* get subsequent entries in first index file */
      if (get_next(&e2, ixd1) == EOF_IX) {
        if (bottom != FAIL)
          bottom = TRUE; /* reached bottom of 1st rel index */
        break;
      }
      rm_trailblks(e2.key);  /* remove any trailing blanks */
      
      if (strncmp(e2.key, prptr[0]->pr_ixtype[0], 3) != EQUAL) {
        bottom = TRUE;  /* surpassed 1st relation index */
        break;
      }
      
      /* read first relation data tuple */
      assign_pred(prptr[0]->pr_opd[0], &e2, &data_level1);
      if (!(read_tuple(rptr, data_level1, sc_ptr1->sc_tuple))) {
        finish_ix(&ixd1);
        finish_ix(&ixd2);
        return(FALSE);
      }
      
      /* compare attr values if they do not match exit loop */
      if (temp.key[0] != EOS) {
	res = key_comp(&temp.key[3], &e2.key[3], get_datatype(ixd1, &e2));
        clear_mem(temp.key, 0, MAXKEY); /* reset temporary variable */

	if (res < 0)
	  break;
      }
    } /* end of while-loop */
    
    /* get subsequent entries in second index file */
    if (get_next(&e4, ixd2) == EOF_IX)
      break;
    rm_trailblks(e4.key);  /* remove any trailing blanks */
    
    /* any subsequent entries do not match */
    if (bottom == FAIL && strcmp(e4.key, temp.key) != EQUAL)
      break;
    
    data_level2 = e4.lower_lvl;
    
    /* compare prefixes, determine if a different index 
     * has been reached */
    if (strncmp(e4.key, buf2, 3) != EQUAL)
      break;
    
  } while (TRUE);
  
  finish_ix(&ixd1);
  finish_ix(&ixd2);
  return(TRUE);
  
} /* join_two */

/* ---------------------------------------------------------- */

/* form the appropriate key prefix to be scanned */
key_def(def, key, ix_no)
     char    *def, key;
     int    ix_no;
{
  if (key == 's')
    sprintf(def, "S%d*", ix_no);
  else if (key == 'p')
    sprintf(def, "P%d*", ix_no);
  
  def[3] = EOS;
  
} /* key_def */

/* ------------------- EO access.c file --------------------- */


