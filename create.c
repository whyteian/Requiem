/************************************************************************
 *									*
 *	     Copyright (c) 1989 Mike Papazoglou and Willi Valder	*
 *                        All rights reserved				*
 *									*
 *									*
 *    Permission to copy this program or any part of this program is	*
 *    granted in case of personal, non-commercial use only. Any use	*
 *    for profit or other commercial gain without written permissions	*
 *    of the authors is prohibited. 					*
 *									*
 *    You may freely distribute this program to friends, colleagues,	*
 *    etc.  As a courtesy we would appreciate it if you left our names	*
 *    on the code as the authors.					*
 *    We also, of course, disavow any liabilities 			*
 *    for any bizarre things you may choose to do with the system.	*
 *									*
 *    We would appreciate though your letting us know about any 	*
 *    extensions, modifications, project suggestions, etc.		*
 *    Please mail all bugs, comments, complaints and suggestions to:	*
 *    									*
 *									*
 *    GMD (German National Research Center for Computer Science)	*
 *    Institut fuer Systemtechnik (f2G2)				*
 *    P.O. Box 1240							*
 *    Schloss Birlinghoven						*
 *    D-5205 St. Augustin 1, West Germany				*
 *									*
 *    email:    valder@gmdzi.uunetp  					*
 *              mikis@gmdzi.uunet					*
 *									*
 ************************************************************************/

/* REQUIEM - relation creation file */

#include "requ.h"
#include "btree.h"

#ifdef UNIX
#include <sys/file.h>
#else
#include <io.h>
#endif

extern struct entry dummy_entry;
extern struct ix_header *pci;
struct indx_attrs *ix_atrs = NULL;
extern  char  errvar[];
extern  char  lexeme[];
extern  int   lex_token;
extern  float   lex_value;
BOOL sys_flag = FALSE;		/* true for system catalogs */

/* ---------------------------------------------------------- */

/* rc_create(rname) - begin the creation of a new relation */
struct relation *rc_create(rname)
     char *rname;
{
  char *pt_error();
  struct relation *rptr;
  
  if (! sys_flag) {
    if (strncmp(rname, "sys", 3) == EQUAL)
      return((struct relation *) pt_error(ILLREL));
  }

  /* allocate the relation structure, with zeroed contents */
  if ((rptr = (struct relation*)
           calloc(1, sizeof(struct relation))) == NULL)
    return ((struct relation *) pt_error(INSMEM));
  
  /* initialize the relation structure */
  strncpy(rptr->rl_name, rname, RNSIZE);
  rptr->rl_name[MIN(strlen(rname), RNSIZE)] = EOS;
  RELATION_HEADER.hd_tcnt = 0;
  RELATION_HEADER.hd_data = HEADER_SIZE;
  RELATION_HEADER.hd_size = 1;
  RELATION_HEADER.hd_avail = 0;
  RELATION_HEADER.hd_attrs[0].at_name[0] = EOS;
  RELATION_HEADER.hd_cursor = 0;
  RELATION_HEADER.hd_unique = 'f'; /* unique specifier: FALSE */
  
  /* return the new relation structure pointer */
  return (rptr);
} /* rc_create */

/* ---------------------------------------------------------- */

/* rc_header - create the relation header */
int rc_header(rptr, no_ix)
     struct relation *rptr;
     BOOL no_ix;
{
  char rname[RNSIZE+1],filename[RNSIZE+4];
  
  /* create the relation file name */
  strncpy(rname, rptr->rl_name, MIN(strlen(rptr->rl_name), RNSIZE));
  rname[MIN(strlen(rptr->rl_name), RNSIZE)] = EOS;
  sprintf(filename,"%s.db",rname);
  
  /* create the relation file with read-write permissions
     no overwriting is allowed if file exists */
  if ((rptr->rl_fd = rc_exist(filename)) == -1) {
    free((char *) rptr);
    return (error(RELCRE));
  }
  
  /* create index file, if required */
  if (no_ix == FALSE)
    if (!b_index(rname))
      return(error(INDXCRE));
  
  /* write the header to the relation file */
  if (write(rptr->rl_fd, (char *) &RELATION_HEADER, 
	                          HEADER_SIZE) != HEADER_SIZE) {
    close(rptr->rl_fd);
    free((char *) rptr);
    return (error(BADHDR));
  }
  
  /* return successfully */
  return (TRUE);
} /* rc_header */

/* --------  finish the creation of a new relation  --------- */
int rc_done(rptr, r_free)
     struct relation *rptr;
     BOOL r_free;           /* free rptr flag */
{
  long lseek();

  /* write the header to the relation file */
  lseek(rptr->rl_fd,0L,0);
  if ((write(rptr->rl_fd, (char *) &RELATION_HEADER,
	                         HEADER_SIZE)) != HEADER_SIZE) {
    close(rptr->rl_fd);
    free((char *) rptr);
    return (error(BADHDR));
  }
  

  /* close the relation file */
  close(rptr->rl_fd);
  
  /* free the relation structure, if required */
  if (r_free)
    nfree((char *) rptr);
  
  /* return successfully */
  return (TRUE);
} /* rc_done */

/* -------- add an attribute to relation being created ------ */
int add_attr(rptr,aname,type,size,key)
     struct relation *rptr;
     char *aname;
     int type,size;
     char key;
{
  int i;

  /* look for attribute name */
  for (i = 0; i < NATTRS; i++)
    if (RELATION_HEADER.hd_attrs[i].at_name[0] == EOS)
      break;
    else if (strncmp(aname,
	  RELATION_HEADER.hd_attrs[i].at_name,ANSIZE) == EQUAL)
      return (error(DUPATT));
  
  
  /* check for too many attributes */
  if (i == NATTRS)
    return (error(MAXATT));
  
  /* store the new attribute */
  strncpy(RELATION_HEADER.hd_attrs[i].at_name,aname,ANSIZE);
  RELATION_HEADER.hd_attrs[i].
      at_name[MIN(strlen(aname), ANSIZE)] = EOS;
  RELATION_HEADER.hd_attrs[i].at_type = type;
  RELATION_HEADER.hd_attrs[i].at_size = size;
  RELATION_HEADER.hd_attrs[i].at_key = key;
  RELATION_HEADER.hd_attrs[i].at_semid = 'f';

  /* terminate the attribute table */
  if (++i != NATTRS)
    RELATION_HEADER.hd_attrs[i].at_name[0] = EOS;
  
  /* update the tuple size */
  RELATION_HEADER.hd_size += size;
  
  /* return successfully */
  return (TRUE);
} /* add_attr */

/* --------------- check key definitions -------------------- */
BOOL chk_key_defs(aname, key, rptr, pc, sc, atype)
     char *aname, *key;
     struct relation  *rptr;
     int *pc, *sc;       /* primary and secondary key indexes */
     int atype;
{
  int tkn;
  
  switch (tkn = next_token()) {
  case UNIQUE:
    
    *key = 'p';
    /* key length + prefix must be less than max. key length */
    if ((int) (lex_value + 3) >= MAXKEY) {
      strcpy(errvar, aname);
      return(error(KEYEXCD));
    }
    
    /* only one unique key per relation is allowed */
    if (RELATION_HEADER.hd_unique == 't')
      return(error(UNIQEXCD));
    else
      RELATION_HEADER.hd_unique = 't'; /* set to TRUE */
    
    if (!ix_attr(rptr->rl_name, aname, tkn, atype, ++(*pc)))
      return(FALSE);
    
    break;
    
  case SECONDARY:
    
    *key = 's';
    if ((int) (lex_value + 3) >= MAXKEY) {
      strcpy(errvar, aname);
      return(error(KEYEXCD));
    }
    
    if (!ix_attr(rptr->rl_name, aname, tkn, atype, ++(*sc)))
      return(FALSE);
    
    break;
  default:
    return(FALSE);
  }
  
  /* return successfully */
  return(TRUE);
} /* chk_key_defs */

/* ------------check definition of foreign key -------------- */

int chk_fgn_key(rptr, fgn_key, qualif, ref_key, pc, atype)
     struct relation *rptr;
     char *fgn_key, *qualif, *ref_key;
     int *pc;
     int atype;
{
  struct scan *sptr, *db_ropen();
  int i;
  BOOL found = FALSE;
  
  
  /* open the relation, return a scan structure pointer */
  if ((sptr = db_ropen(qualif)) == NULL)
    return (FALSE);
  
  /* look if referenced key exists and is a primary key */
  for (i = 0; i < sptr->sc_nattrs; i++)
    if (strcmp(ref_key,SCAN_HEADER.hd_attrs[i].at_name)==EQUAL) {
      if (SCAN_HEADER.hd_attrs[i].at_key == 'p') {
	
	/* make an index entry for this foreign key */
	if (!ix_attr(rptr->rl_name, fgn_key, FOREIGN, atype, ++(*pc))) {
	  db_rclose(sptr);
	  return(FALSE);
	}
	
	/* insert referenced key info. */
	ix_refkeys(qualif, ref_key, *pc);
	
	found = TRUE;      /* primary key in qualifier found */
      }
      break;
    }
  
  if (found == FALSE)
    return(error(WRNGFGNKEY));
  
  /* close the relation */
  db_rclose(sptr);
  
  /* correct foreign key definition */
  return(TRUE);
} /* chk_fgn_key */

/* change key definition to primary (for foreign keys only) */
change_key(rptr, aname, key)
     struct relation *rptr;
     char *aname;
     char key;
{    
  int i;
  
  /* look for attribute name */
  for (i = 0; i < NATTRS; i++)
    if (strcmp(aname,RELATION_HEADER.hd_attrs[i].at_name)==EQUAL) {
      /* store the new key definition */
      RELATION_HEADER.hd_attrs[i].at_key = key;
      break;
    }
  
} /* change_key */

/* --------- verify existence of a file name  -------------- */
static int rc_exist(filename)
     char *filename;
{
  int fd;
  
  if (!strncmp(filename, SNAPSHOT, strlen(SNAPSHOT))) { /* only for aux files */
    fd = open(filename, O_CREAT | O_RDWR | O_BINARY, 0600);
    if (fd == -1)
      printf("Warning: Auxilliary file is redeclared !!\n");
  }
  else { /* no file overwriting is allowed */
    fd = open(filename, O_CREAT | O_RDWR | O_EXCL | O_BINARY, 0600);
    if (fd == -1)
      printf("Warning: Relation file is redeclared !!\n");
  }
  return (fd);
} /* rc_exist */

/* --------- report syntax error during creation ------------ */

int rc_error(rptr)
     struct relation *rptr;
{
  nfree((char *) rptr);
  return(error(SYNTAX));
} /* rc_error */

/* ------------- finish creation of a relation -------------- */

int cre_finish(rptr)
     struct relation *rptr;
{
  /* finish rel creation, construct index, do not free rptr */
  if (!rc_header(rptr, INDEX))  /* create a relation header */
    return (FALSE);
  
  /* close the relation and index files */
  close(rptr->rl_fd);
  dequeue_relations(rptr);
  
  /* return successfully */
  nfree((char *) ix_atrs);
  ix_atrs = NULL;
  return(TRUE);
  
} /* cre_finish */


/* ---------------------------------------------------------
   |           Index part of creation routines             |
   --------------------------------------------------------- */

int	b_index(fname)
     char	*fname;
{
  char	fn[16];
  
  cat_strs(fn, fname, ".indx");
  
  if (!create_ix(fn, &dummy_entry, sizeof(struct entry)))
    return(FALSE);
  
  /* return successfully */
  return(TRUE);
}

/* ---------------------------------------------------------- */

int create_ix(f_name, pdum, ndum) /* create a new index file */
     char	f_name[];         /* name of the file */
     struct entry *pdum;          /* dummy enry for EOF */
     int	ndum;             /* size of dummy entry */
{
  struct block blk;
  int	ret, i;
  struct ix_header *pixhd;
  long write_level();

  if ((pixhd = (struct ix_header*)
       calloc(1, sizeof(struct ix_header))) == NULL)
    return (error(INSMEM));
  
  ret = create_ixf(f_name);
  if (ret < 0)
    return(FALSE);
  
  pixhd->ixfile = ret;
  pixhd->dx.nl = MAX_LEVELS;  /* all levels present */
  
  strcpy(pixhd->dx.ixatts.ix_rel, ix_atrs->ix_rel);
  
  for (i = 0; ix_atrs->ix_prim[i] && i < MAXKEYS; i++)
    strcpy(pixhd->dx.ixatts.ix_prim[i], ix_atrs->ix_prim[i]);
  
  for (i = 0; ix_atrs->ix_secnd[i] && i < MAXKEYS; i++)
    strcpy(pixhd->dx.ixatts.ix_secnd[i], ix_atrs->ix_secnd[i]);
  
  for (i = 0; ix_atrs->ix_refkey[i] && i < MAXKEYS; i++)
    strcpy(pixhd->dx.ixatts.ix_refkey[i], ix_atrs->ix_refkey[i]);
  
  pixhd->dx.ixatts.ix_foreign = ix_atrs->ix_foreign;
  
  for (i = 0; i < 2 * MAXKEYS; i++)
    pixhd->dx.ixatts.ix_datatype[i] = ix_atrs->ix_datatype[i];

  /* set up initial struct with a single entry using the
   * dummy key as provided by the calling function */
  move_struct((char *) &pixhd->dx.dum_entr, (char *) pdum, ndum);
  clear_mem((char *)&blk,0,IXBLK_SIZE); /* make block of zeroes */
  write_block(pixhd, 0L, &blk);   /* write at beginning of file */
  
  /* set up index block for each level, 
   * record location of root block */
  pixhd->dx.root_bk = write_level(pixhd, pdum, ndum);
  
  /* set up a free block list, start it after dummy blocks */
  make_free(pixhd);
  close_ix(pixhd);         /* close file updating header */
  
  nfree((char *) pixhd);
  pixhd = NULL;
  return(TRUE);            /* successful creation */
} /* create_ix */

/* ---------------------------------------------------------- */

int close_ix(pixd)          /* close an index file */
     struct ix_header *pixd;    /* index header */
{
  write_ixf(pixd, 0L, (char *) &pixd->dx, sizeof(struct ix_disk));
  close_ixf(pixd);           /* close the index file */
} /* close_ix */

/* ---- make an index entry for the specified attribute ----- */

int	ix_attr(rel_name, attr_name, type, data_type, tcnt)
     char	*rel_name, *attr_name;
     int	type;
     int        data_type;
     int	tcnt;
{
  if (tcnt > 2)
    return(error(TOOMANY));
  
  if (ix_atrs == NULL) {
    if ((ix_atrs = CALLOC(1, struct indx_attrs)) == NULL)
      return(error(INSMEM));
    
    ix_atrs->ix_foreign = 'f';  /* set foreign keys to FALSE */
  }
  strcpy(ix_atrs->ix_rel, rel_name);
  
  /* preserve names of primary, secondary and referenced keys */
  if (type == UNIQUE || type == FOREIGN) {
    strcpy(ix_atrs->ix_prim[tcnt-1], attr_name);
    ix_atrs->ix_datatype[tcnt - 1] = data_type;
  } else if (type == SECONDARY) {
    strcpy(ix_atrs->ix_secnd[tcnt-1], attr_name);
    ix_atrs->ix_datatype[tcnt + 1] = data_type;
  }

  /* if two foreign keys exist they must be combined */
  if (type == FOREIGN && tcnt == 2)
    ix_atrs->ix_foreign = 't';
  
  return(TRUE);
} /* ix_attr */

/* ---------------------------------------------------------- */

ix_refkeys(qualif, ref_key, tcnt)
     char *qualif, *ref_key;
     int	tcnt;
{     
  char q_ref[30];         /* qualified reference */
  
  strcpy(q_ref, qualif);   
  
  /* primary keys in qualifying rel referenced by fgn keys */
  make_quattr(q_ref, ref_key);
  strcpy(ix_atrs->ix_refkey[tcnt-1], q_ref);
  
} /* ix_refkeys */

/* ---------------------------------------------------------- */

 
