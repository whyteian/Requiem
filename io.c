

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


/* REQUIEM -  File Manager low level I/O routines */

#include "requ.h"
#include "btree.h"


#ifdef MAC
#include <io.h>
#endif

#ifdef DOS
#include <io.h>
#endif

extern int sref_cntr;
extern char errvar[];
extern BOOL wh_signal;
extern struct  cmd_file *lex_cfp, *temp_cfp ; /* command file context ptrs */

struct ix_header *pixd = NULL;
static char rel_fl[RNSIZE+5];
long seek();
static struct relation *relations = NULL; /* loaded rel defns */
struct ix_header *db_index();

/* -------------- find the specified relation --------------- */
struct relation *rfind(rname)
     char *rname;
{
  int fd;
  char filename[RNSIZE+5];
  struct relation *rptr;
  char *pt_error();
  
  /* look for relation in list currently loaded */
  for (rptr = relations; rptr != NULL; rptr = rptr->rl_next)
    if (strcmp(rname,rptr->rl_name) == EQUAL)
      return (rptr);
  
  /* create a file name by appending .db */
  make_fname(filename,rname);
  
  /* lookup the relation file */
  if ((fd = open(filename, O_RDONLY | O_BINARY)) == -1) {
    strcpy(errvar, rname);
    errvar[strlen(rname)] = EOS;
    
    return ((struct relation *)pt_error(RELFNF));
  }
  
  /* allocate a new relation structure */
  if ((rptr = CALLOC(1, struct relation)) == NULL) {
    close(fd);
    return ((struct relation *)pt_error(INSMEM));
  }
  
  /* initialize the relation structure */
  rptr->rl_scnref = 0;
  
  /* read header block from file relation.db into rl_header*/
  if (read(fd, (char *) &RELATION_HEADER, HEADER_SIZE) 
                               != HEADER_SIZE) {
    free((char *) rptr);
    close(fd);
    return ((struct relation *)pt_error(BADHDR));
  }
  
  /* close the relation file */
  close(fd);
  
  /* store the relation name */
  strncpy(rptr->rl_name, rname, RNSIZE);
  rptr->rl_name[MIN(RNSIZE, strlen(rname))] = EOS;

  /* link new relation into relation list */
  rptr->rl_next = relations;
  relations = rptr;
  
  /* return the new relation structure pointer */
  return (rptr);
}/* rfind */

/* ---------------------------------------------------------- */

struct scan *db_ropen(rname)     /* open a relation file */
     char *rname;
{
  struct relation *rptr;
  struct scan *sptr;
  char *pt_error();
  char filename[RNSIZE+5];
  int i;
  
  /* find the relation definition */
  if ((rptr = rfind(rname)) == NULL)
    return ((struct scan *)0);
  
  /* allocate a new scan structure */
  if ((sptr = CALLOC(1, struct scan)) == NULL)
    return ((struct scan *)pt_error(INSMEM));
  
  /* allocate a tuple buffer */
  if ((SCAN_TUPLE = (char *) calloc(1, rptr->rl_header.hd_size)) == NULL) {
    free((char *) sptr);
    return ((struct scan *)pt_error(INSMEM));
  }
  
  /* initialize the scan structure */
  SCAN_RELATION = rptr;      /* store relation struct addrs */
  sptr->sc_dtnum = 0;        /* desired tuple (non-existant) */
  sptr->sc_atnum = 0;        /* actual tuple (non-existant) */
  sptr->sc_store = FALSE;    /* no store done since open */
  sptr->sc_nattrs = 0;       /* total no. of attributes */
  
  for (i = 0; i < NATTRS; i++) {
    
    /* check for the last attribute */
    if (SCAN_HEADER.hd_attrs[i].at_name[0] == EOS)
      break;
    
    sptr->sc_nattrs++;
  }
  
  /* open relation file if necessary */
  if (rptr->rl_scnref++ == 0) {
    
    /* create the relation file name */
    make_fname(filename,rname);
    
    /* open the relation file */

    if ((rptr->rl_fd = open(filename, O_RDWR | O_BINARY)) == -1) {
      rptr->rl_scnref--;
      free(SCAN_TUPLE);
      free((char *) sptr);
      strcpy(errvar, rname);
      errvar[strlen(rname)] = EOS;
      
      return ((struct scan *)pt_error(RELFNF));
    }
  }
  
  /* return the new scan structure pointer */
  return (sptr);
}/* db_ropen */

/* ---------------------------------------------------------- */

int db_rclose(sptr)     /* close the relation file */
     struct scan *sptr;
{
  /* close relation file if this is the last reference */
  if (--SCAN_RELATION->rl_scnref == 0) {
    
    /* rewrite header if any stores took place */
    if (sptr->sc_store) {
      
      /* write the header block */
      if (!rc_done(SCAN_RELATION, TRUE)) {
	free(SCAN_TUPLE);
	free((char *) sptr);
	return (error(BADHDR));
      }
    }            /* close the relation and index file */
    else {
      close(SCAN_RELATION->rl_fd);
    }
    dequeue_relations(SCAN_RELATION);
  }
  
  /* free the scan structure */
  free(SCAN_TUPLE);
  free((char *) SCAN_RELATION);
  free((char *) sptr);
  
  /* return successfully */
  return (TRUE);
} /* db_rclose */

/* ---------------------------------------------------------- */

dequeue_relations(rel_ptr)
     struct relation *rel_ptr;
{
  struct relation *rptr, *lastrptr;
  
  
  /* NEW */
  /* in case the index file belongs to the relation, close it */
  if (pixd) { 
    /* check the consistency of data and index file */
    if (strcmp(rel_ptr->rl_name, pixd->dx.ixatts.ix_rel) == EQUAL) {
      close_ix(pixd);
      pixd = NULL;   /* reset index header pointer */
    }
  }

  /* if the last relation in the list should be removed */
  if (rel_ptr == relations && relations->rl_next == NULL) {
    relations = NULL;
    return;
  }
    
  /* remove  the relations from the relation list */
  lastrptr = NULL;
  for (rptr = relations; rptr != NULL; rptr = rptr->rl_next) {
    if (rptr == rel_ptr) {
      if (lastrptr == NULL)
	relations = rptr->rl_next;
      else
	lastrptr->rl_next = rptr->rl_next;
    }
    lastrptr = rptr;
  }
} /* dequeue_relations */

/* ---------------------------------------------------------- */

int fetch_tuple(sptr)  /* fetch next tuple from relation file */
     struct scan *sptr;
{
  /* look for an active tuple */
  while (TRUE) {
    
    /* check for this being the last tuple */
    if (!get_tuple(sptr, (int) sptr->sc_dtnum + 1))
      return (FALSE);
    
    /* increment the tuple number */
    sptr->sc_dtnum += 1;
    
    /* return if the tuple found is active */
    if (SCAN_TUPLE[0] == ACTIVE)
      return (TRUE);
    
  }
} /* fetch_tuple */

/* read data tuple at specified offset; put contents into buf */
read_tuple(rptr, data_offset, buf)
     struct relation *rptr;
     long    data_offset;
     char    *buf;
{
  long lseek();

  /* seek the tuple in the data file */
  lseek(rptr->rl_fd, data_offset, 0);
  
  /* read tuple from data relation file */
  if (read(rptr->rl_fd, buf, RELATION_HEADER.hd_size) != 
      RELATION_HEADER.hd_size)
    return(error(TUPINP));
  
  return(TRUE);
} /* read_tuple */

/* ----- update current tuple data and relation files ------- */
int db_rupdate(ixd, sptr, key_buf, del_buf)
     struct ix_header *ixd;
     struct scan *sptr;
     char **key_buf;
     char **del_buf;
{   
  long lseek();

  /* check for foreign key consistency */
  /* if attr is indexed remove its old value from index file */
  if (key_buf) {
    if (!updt_ixattr(&ixd, sptr, del_buf, key_buf))
      return(FALSE);
  }
 
  /* keep the pos. of record found */
  if (ixd)
     ixd->ix_recpos = sptr->sc_recpos; 
  
  /* make sure the status byte indicates an active tuple */
  SCAN_TUPLE[0] = ACTIVE;
  
  /* find the appropriate position for the tuple */
  lseek(SCAN_RELATION->rl_fd, sptr->sc_recpos, 0);
  
  pixd = ixd;  /* set global index header variable */
  /* write the tuple check for index */
  if (!place_tuple(SCAN_RELATION, sptr->sc_recpos, 
		   key_buf, SCAN_TUPLE, UPDATES))
    return(FALSE);

  /* perform  foreign key modifications if necessary */
  if (key_buf && del_buf) 
    return(change_fgnkey(ixd, sptr, FALSE));
  
  return(TRUE);
  
} /* db_rupdate */

/* ---------------------------------------------------------- */

int store_tuple(sptr, key_buf)      /* store a new tuple */
     struct scan *sptr;
     char **key_buf;
{       
  int tnum;
  char *buf;
  long next_free;
  long lseek();

  buf = (char *)calloc(1, (unsigned) SCAN_HEADER.hd_size+1);
  
  /* make sure the status byte indicates an active tuple */
  SCAN_TUPLE[0] = ACTIVE;
  
  if (SCAN_HEADER.hd_avail != 0)  { 
    	/* available space from previous deletions */         
    sptr->sc_recpos = SCAN_HEADER.hd_avail;
    
    /* read tuple from first available position */
     read_tuple(SCAN_RELATION, sptr->sc_recpos, buf);
    sscanf(buf+1, "%ld", &next_free); /* find next free pos */
    SCAN_HEADER.hd_avail = next_free; /* reset addr at header */
    
    /* return to original tuple position */
    lseek(SCAN_RELATION->rl_fd, sptr->sc_recpos, 0);
    
    /* new current entry in data file */
    tnum = (sptr->sc_recpos - HEADER_SIZE)/SCAN_HEADER.hd_size;
  }
  else {  /* no previous deletions */
    /* estimate no. of current entries in data file */
    tnum = SCAN_HEADER.hd_tcnt + 1;
    
    /* find tuple disk position */
    sptr->sc_recpos =  
      seek(SCAN_RELATION->rl_fd, SCAN_HEADER.hd_size, tnum); 
    
    /* preserve position of last tuple in data file */ 
    SCAN_HEADER.hd_tmax = tnum;
  }
  
  /* write the tuple */ 
  if (!place_tuple(SCAN_RELATION, sptr->sc_recpos, 
 		         key_buf, SCAN_TUPLE, NO_UPDATE))
    return (FALSE);
  
  /* remember which tuple is in the buffer */
  sptr->sc_atnum = tnum;
  
  /* update the tuple count */
  SCAN_HEADER.hd_tcnt += 1;
  
  /* remember that a tuple was stored */
  sptr->sc_store = TRUE;
  
  /* free buffer and return successfully */
  nfree(buf);
  return (TRUE);
} /* store_tuple */

/* ---------------------------------------------------------- */

/* get a tuple from the relation file */
int get_tuple(sptr,tnum)
     struct scan *sptr;
     int tnum;
{
  /* check to see if the tuple is already in the buffer */
  if (tnum == sptr->sc_atnum)
    return (TRUE);
  
  /* check for this being beyond the last tuple in data file */
  if (tnum > SCAN_HEADER.hd_tmax)
    return (error(TUPINP));
  
  /* read the tuple from its  disk position */
  sptr->sc_recpos = 
    seek(SCAN_RELATION->rl_fd, SCAN_HEADER.hd_size, tnum);
  
  if (read(SCAN_RELATION->rl_fd, SCAN_TUPLE,SCAN_HEADER.hd_size)
                                  != SCAN_HEADER.hd_size)
    return (error(TUPINP));
  
  /* remember which tuple is in the buffer */
  sptr->sc_atnum = tnum;
  
  /* return successfully */
  return (TRUE);
} /* get_tuple */

/* -------------- place tuple into data-file ---------------- */
int place_tuple(rptr, recpos,  key_buf, tuple_buf, updt)
     struct relation *rptr;
     long recpos;
     char **key_buf, *tuple_buf;
     BOOL updt;          /* update signal */
{
  /* perform indexing */
  if (key_buf != NULL && updt == FALSE) {
    if ((pixd = db_index(rptr->rl_name, recpos, key_buf)) 
	                                           == NULL)
      return (FALSE);
  }          /* if updating is required */
  else if (key_buf != NULL && updt == TRUE)
    if (!update_ix(recpos, key_buf, pixd))
      return (FALSE);
  
  if (!write_tuple(rptr, tuple_buf, FALSE))
    return (error(TUPOUT));
  
  if (key_buf && updt == FALSE && pixd) {
    /* NEW ADDED */
    /* in case the index file belongs to the relation, close it */
    /* check the consistency of data and index file */
    if (strcmp(rptr->rl_name, pixd->dx.ixatts.ix_rel) == EQUAL) {
      close_ix(pixd);
      free(pixd);
      pixd = NULL;
    }
  }
  
  /* return successfully */
  return (TRUE);
  
} /* place_tuple */

/* ---------------- update modified tuples ------------------ */
int     update_tuple(pixd, srptr, key_buf, del_buf)
     struct ix_header *pixd;
     struct srel *srptr;
     char     **key_buf, **del_buf;
{
  /* check each selected relation for updates */
  if (srptr->sr_update)
    return(db_rupdate(pixd, srptr->sr_scan, key_buf, del_buf));
  
  /* return successfully */
  return(TRUE);
} /* update_tuple */

/* -------------- delete the specified tuple ---------------- */
int     delete_tuple(pixd, sptr, del_buf)
     struct ix_header *pixd;
     struct scan *sptr;
     char     **del_buf;
{  
  long lseek();
  BOOL  incr = FALSE; /* do not increment tuple counter */
  
  if (del_buf) {
    if (!del_ixattr(&pixd, sptr, del_buf))
      return(error(IXDELFAIL));
  }
  
  /* make sure the status byte indicates a deleted tuple  */
  sptr->sc_tuple[0] = DELETED;
  
  /*  get address of 1st free record  */
  sprintf(sptr->sc_tuple+1, "%ld", SCAN_HEADER.hd_avail);
  
  SCAN_HEADER.hd_avail = sptr->sc_recpos; /* 1st free record */
  
  /* find the position of the tuple in the data file */
  lseek(SCAN_RELATION->rl_fd, sptr->sc_recpos, 0);
  
  /* write the discarded tuple tuple back */
  if (!write_tuple(SCAN_RELATION, SCAN_TUPLE, incr))
    return (error(TUPOUT));
  
  /* decrement tuple counter */
  SCAN_HEADER.hd_tcnt--; 
  
  /* perform  foreign key modifications if necessary */
  if (del_buf)
    return(change_fgnkey(pixd, sptr, TRUE));
  
  /* return successfully */
  return (TRUE);
  
} /* delete_tuple */

/* ------------ transfer a file to another ------------------ */
int transfer(old_path, new_attrs)
     char *old_path;                /* existing file */
     struct attribute *new_attrs[]; /* attr val to be stored */
{
  char old_file[RNSIZE + 5], new_file[RNSIZE + 5];
  struct sel *retrieve(), *old_sel;
  struct relation *rc_create(), *new_rel;
  struct sattr *saptr;
  struct attribute *aptr;
  char *tbuf, *avalue, *stck(), *rem_blanks();
  char    **key_buf = NULL;
  /* tuple counter,  primary, and secondary key counters */
  int     tcnt, p_cntr = 0, s_cntr = 2; 
  int  tup_offset, i, displ, size_diff;
  long rec_pos;
  BOOL temp_wh_signal = wh_signal;
  
  /* create a new relation structure */
  sref_cntr++;
  if ((new_rel = rc_create(stck(rel_fl, sref_cntr))) == NULL) {
    rc_done(new_rel, TRUE);
    return (FALSE);
  }
  
  /* retrieve the relation file */
  if ((old_sel = retrieve(old_path)) == NULL)
    return(FALSE);
  
  cmd_clear();   /* clears command line produced by retrieve */
  
  /* create the selected attrs and decide about new rel  size */
  for (saptr = old_sel->sl_attrs, i = 0; 
       saptr != NULL; i++,saptr = saptr->sa_next) {
    /* add the attributes allowing enough space for
       the longest common attribute */
    if (!add_attr(new_rel,new_attrs[i]->at_name, 
		  new_attrs[i]->at_type,
		  new_attrs[i]->at_size,
		  new_attrs[i]->at_key)) {
      nfree((char *) new_rel);
      done(old_sel);
      return (FALSE);
    }
  }
  
  /* create the relation and index file headers */
  if (!rc_header(new_rel, NOINDEX)) {
    done(old_sel);
    return (FALSE);
  }
  
  /* allocate and initialize a tuple buffer */
  if ((tbuf = 
    (char *)calloc(1,(unsigned)(new_rel->rl_header.hd_size+1))) 
      == NULL ||(avalue = 
   (char *)calloc(1,(unsigned)(new_rel->rl_header.hd_size+1))) 
      == NULL) {
    done(old_sel);
    rc_done(new_rel, TRUE);
    return (error(INSMEM));
  }
  *tbuf = ACTIVE;
  
  /* loop thro the tuples of the old rel, copy them to new */
  for (tcnt = 0; fetch(old_sel, FALSE); tcnt++) {
    
    /* create the tuple from the values of selected attrs */
    tup_offset = 1;
    
    for (saptr = old_sel->sl_attrs, i = 0; saptr != NULL; 
	                        saptr = saptr->sa_next, i++) {
      aptr = saptr->sa_attr;
      size_diff = new_attrs[i]->at_size - ATTRIBUTE_SIZE;
      
      if (new_attrs[i]->at_name[0] != EOS)
	if (!(displ = 
	      adjust(saptr, tbuf, size_diff, tup_offset))) {
	  done(old_sel);
	  rc_done(new_rel, TRUE);
	  return(FALSE);
	}

      /* check if indexing is required */
      if (aptr->at_key != 'n') {
	rem_blanks(avalue, saptr->sa_aptr, ATTRIBUTE_SIZE);
	check_ix(&key_buf, aptr, &p_cntr, &s_cntr, avalue);
      }
      tup_offset += displ;
    }
    
    /* remove current index entries */
    if (!del_ixattr(&pixd, old_sel->sl_rels->sr_scan, key_buf)) {
      done(old_sel);
      rc_done(new_rel, TRUE);
      return(error(IXDELFAIL));
    }
    
    /* estimate record position of tuple to be stoted */
    rec_pos = 
      seek(new_rel->rl_fd, new_rel->rl_header.hd_size, tcnt+1); 
    
    /* update the  index file */
    if (!update_ix(rec_pos, key_buf, pixd)) {
      buf_free(key_buf, 4);
      key_buf = NULL;
      done(old_sel);
      rc_done(new_rel, TRUE);
      return (FALSE);
    }
    
    /* write the tuple into the new data file */
    if (!write_tuple(new_rel, tbuf, FALSE)) {
      done(old_sel);
      rc_done(new_rel, TRUE);
      nfree(tbuf);
      return (error(INSBLK));
    }
    
    /* reset counters */
    p_cntr = 0; s_cntr = 2;
  }
  
  /* cater for changes in size and name */
  new_rel->rl_header.hd_tcnt =
    old_sel->sl_rels->sr_scan->sc_relation->rl_header.hd_tcnt;
  /* save auxilliary relation name */
  sprintf(new_file, "%s.db", new_rel->rl_name); 
  strcpy(new_rel->rl_name, 
	 old_sel->sl_rels->sr_scan->sc_relation->rl_name);
  /* save original relation name */
  sprintf(old_file, "%s.db", old_sel->sl_rels->sr_scan->
	  sc_relation->rl_name); 
  
  /* finish the selection */
  done(old_sel);
  
  /* finish relation creation */
  if (!rc_done(new_rel, TRUE))
    return (FALSE);
  
  /* move contents of new-file to old-file */
  if (!f_move(new_file, old_file))
    return(error(FILEMV));
  
  /* do housekeeping  and return successfully */
  nfree(tbuf);
  wh_signal = temp_wh_signal;
  if (temp_cfp) {
    lex_cfp = temp_cfp;           /* restore cf pointer */
    temp_cfp = NULL;
  }
  
  return (TRUE);
}/* transfer */

/* ---------- seek a tuple in a relation file --------------- */
static long seek(file_descr, tup_size, tnum)
     int file_descr,  	/* file descriptor */
         tup_size,    	/* size of tuple in bytes */
         tnum;
{
  long offset;
  long lseek();
  
  offset = 
    (long) HEADER_SIZE + ((long) (tnum - 1) * (long) tup_size);
  return(lseek(file_descr, offset, 0));
}/* seek */

/* ---------------------------------------------------------- */

static make_fname(fname,rname)/* make rel name from file name */
     char *fname,*rname;
{
  strncpy(fname,rname,RNSIZE);
  fname[MIN(RNSIZE, strlen(rname))] = EOS;
  strcat(fname,".db");
}/* make_fname */

/* ------------ write current tuple into data file ---------- */
write_tuple(rptr, buff, incrs)
     struct  relation *rptr;
     char *buff;
     BOOL incrs;
{
  if (write(rptr->rl_fd, buff, RELATION_HEADER.hd_size) 
                                != RELATION_HEADER.hd_size) {
    rc_done(rptr, TRUE);
    nfree(buff);
    return (FALSE);
  }
  
  if (incrs) {  /* increment header counters */
    RELATION_HEADER.hd_tcnt++;
    RELATION_HEADER.hd_tmax++;
  }
  
  return(TRUE);
  
} /* write_tuple */

/* - get a record by its no. where it is known to be stored - */
cseek(sptr, tnum)
     struct scan *sptr;
     int    tnum;
{
  long    offset, pos;
  long lseek();
  
  offset = (long) SCAN_HEADER.hd_data + 
    ((long) tnum * (long) SCAN_HEADER.hd_size);
  
  /* get to current position */
  if ((pos = lseek(SCAN_RELATION->rl_fd, offset, 1)) == -1
      || pos < (long) SCAN_HEADER.hd_data
      || tnum  >= SCAN_HEADER.hd_tcnt)
    return (FALSE);
  else
    return (TRUE);
} /* cseek */

/* ---------------------------------------------------------- */

int adjust(saptr, tbuf, size_diff, offset)
     struct sattr *saptr;
     char *tbuf;
     int size_diff, offset;
{
  int i, attr_size, s_indx = 0;
  char *size_buf;
  char format[10];
  struct relation *rel;
  
  /* estimate size of new attribute */
  attr_size =  ATTRIBUTE_SIZE + size_diff;
  
  /* get a relation pointer */
  rel = saptr->sa_srel->sr_scan->sc_relation;
  
  if ((size_buf = (char *) 
      calloc(1, (unsigned) rel->rl_header.hd_size+1)) == NULL) {
    rc_done(rel, TRUE);
    return (error(INSMEM));
  }
  
  for (i = 0; i < ATTRIBUTE_SIZE; i++) {
    if ((ATTRIBUTE_TYPE == TNUM) || (ATTRIBUTE_TYPE == TREAL)) {
      size_buf[s_indx++] = saptr->sa_aptr[i];
      if ( i == ATTRIBUTE_SIZE-1){
	if (size_diff < 0)    /* displace appropriately */
	  displace_num(size_buf, ATTRIBUTE_SIZE, size_diff);
	
	/* right adjust number to fit length of 
	 * widest common attribute */
	sprintf(format, "%%%ds", attr_size);
	sprintf(tbuf + offset, format, size_buf);
	
	/* increment index to get past last character */
	i++;
	s_indx = 0;
	break;
      }
    }
    else {      /* only if character strings are involved */
      if (i < attr_size)
	tbuf[offset + i] = saptr->sa_aptr[i];
      else if (size_diff < 0)
	return(i);
      
      
      if (i == ATTRIBUTE_SIZE - 1)
	i += size_diff; /* new displacement */
    }
  }
  
  nfree(size_buf);
  return(i);     /* return displacement */
} /* adjust */

/* ----- displace if new field length < than initial -------- */

static displace_num(buf, len, diff) 
     char *buf;
     int len, diff;
{
  int i;
  
  for (i= 0; i < len+diff; i++)
    buf[i] = buf[i-diff];
  buf[i] = EOS;
  
} /* displace_num */
/* ---------------------------------------------------------- */
