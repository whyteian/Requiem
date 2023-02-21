

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


#include "requ.h"

extern float lex_value;
struct hash **Hash_Table;
int  HTIndex;

int init_hash()
{
  Hash_Table = (struct hash **) calloc(HTSIZE, sizeof(struct hash *));
} /* init_hash */

BOOL db_hash(word, rel_name, aggr_par)
     char *word, *rel_name;
     double aggr_par;
{
  return (insert_token(word, rel_name, aggr_par));
} /* db_hash */

BOOL insert_token(word, rel_name, aggr_par)
     char   *word, *rel_name;
     double aggr_par;
{  /* insert_token */
  char *s_copy();
  
  HTIndex = hash(word);
  if (HTIndex == -1)
    return (error(HASHSIZ));
  
  if (Hash_Table[HTIndex] == NULL) {
    if ((Hash_Table[HTIndex] = 
	 (struct hash *) CALLOC(1, struct hash)) == NULL)
      return(error(INSMEM));
    
    Hash_Table[HTIndex]->value = s_copy(word);
    Hash_Table[HTIndex]->dup_tcnt = 1;
    Hash_Table[HTIndex]->relation = s_copy(rel_name);
    Hash_Table[HTIndex]->aggr_param = aggr_par;
  } else {
    Hash_Table[HTIndex]->dup_tcnt++;
    return(TRUE);
  }
  return(FALSE);
}  /* insert_token */

int hash(word)
     char  *word;
{  /* hash */
  int  HTIndex;
  int  id_len;
  int  Init_HTIndex;
  int  probe_cntr = 0;
  
  id_len = strlen(word);
  if (id_len == 0)
    fprintf(stderr,"Hash: Word of no length \n");
  HTIndex = Init_HTIndex = transform_id(word);
  
  if (Hash_Table[HTIndex] == (struct hash *) NULL)
    ; /* we've got it */
  else if/* have we found the correct index ? */
    (strncmp(word, Hash_Table[HTIndex]->value, id_len) == EQUAL)
      ; /* done a direct hit */
  else /* a collision generate new indices */
    for (probe_cntr = 0; probe_cntr < (HTSIZE / 2);
	 probe_cntr++) {
      HTIndex = generate_new_indx(Init_HTIndex, 
				  probe_cntr) ;
      if (Hash_Table[HTIndex]->value == NULL)
	break; /* we've got it */
      else if (strncmp(word, Hash_Table[HTIndex]->value, id_len)
		== EQUAL)
	break;   /* we've got it */
    }
  if (probe_cntr >= (HTSIZE/ 2))
    return(-1);
  return(HTIndex);
}  /* Hash */

int generate_new_indx(orig_key, probe_no)
     int orig_key;
     int probe_no;
{  /* generate_new_index */
  return((orig_key + probe_no * probe_no) % HTSIZE);
}  /* generate_new_indx */

int transform_id(word)
     char word[];
{    /* transform_id */
  int   term;
  int   word_index;
  
  term = 0;
  for (word_index = strlen(word) - 1; word_index > -1;
       word_index--)
    term = (257*term) + word[word_index];
  term = ( term < 0) ? -term : term;
  return(term % HTSIZE);
}  /* transform_id */

VOID hash_done()
{
  register int i;
  
  for (i = 0; i < HTSIZE; i++){
    if (Hash_Table[i] != NULL) {
      nfree(Hash_Table[i]->value);
      nfree((char *) Hash_Table[i]->relation);
      nfree((char *) Hash_Table[i]);
    }
  }
  nfree((char *) Hash_Table);
  Hash_Table = NULL;
}

int db_hsplace(buf, name, in_buf, rptr)
     char *buf, *in_buf, *name;
     struct relation *rptr;
{ 
  unsigned cnt = 0;
  
  if (!(db_hash(buf, name, (double) 0))){
    cnt++;
    if (!write_tuple(rptr, in_buf, TRUE)) {
      rc_done(rptr, TRUE);
      free(in_buf);
      hash_done();
      return (error(INSBLK));
    }      
  }
  return (cnt);
} /* db_hsplace */

int db_hplace(buf, name, in_buf, rptr)
     char *buf, *in_buf, *name;
     struct relation *rptr;
{ 
  unsigned cnt = 0;
  
  if (db_hash(buf, name, (double) 0)){
    cnt++;
    if (write(rptr->rl_fd, in_buf, RELATION_HEADER.hd_size) != 
	RELATION_HEADER.hd_size) {
      rc_done(rptr, TRUE);
      free(in_buf);
      hash_done();
      return (error(INSBLK));
    }
    RELATION_HEADER.hd_tcnt++;
  }
  return (cnt);
} /* db_hsplace */

sort_table(sort_no)
     int  sort_no;
{  /* sort_table */
  struct hash  *exchange;
  int        base_tcnt;
  int        cur_tcnt;
  int        cur_gap_tcnt;
  int        gap;
  int        last_in_buff;
  
  last_in_buff = sort_no;
  for (gap = last_in_buff / 2; gap > 0; gap /= 2)
    for (base_tcnt = gap; base_tcnt < last_in_buff; base_tcnt++)
      for (cur_tcnt = base_tcnt - gap ; cur_tcnt >= 0;
	   cur_tcnt -= gap) {
	cur_gap_tcnt = cur_tcnt + gap;
	if (name_cmp(cur_tcnt, cur_gap_tcnt,Hash_Table) < EQUAL)
	  break;
	/* otherwise exchange the elements */
	exchange = Hash_Table[cur_tcnt];
	Hash_Table[cur_tcnt] = Hash_Table[cur_gap_tcnt];
	Hash_Table[cur_gap_tcnt] = exchange;
      } /* end for */
}  /* sort_buffer */

int name_cmp(first, second, Hash_Table)
     int   first;
     int   second;
     struct hash *Hash_Table[];
{    /* name_cmp */
  /* if first entry has no value the first is less */
  if (Hash_Table[first] == (struct hash *) NULL)
    return(FIRSTLSS);
  
  /* if second has no value then first is gtr */
  if (Hash_Table[second] == (struct hash *) NULL)
    return(FIRSTGTR);
  
  /* otherwise return the comparison from strcmp */
  return(strcmp(Hash_Table[first]->value, 
		Hash_Table[second]->value));
}  /* name_cmp */

int write_hash(buffer_len, rptr, tkn, tbuf, auxbuf)
     int buffer_len, tkn;
     struct relation *rptr;
     char *tbuf, *auxbuf;
{
  register int tcntr;
  char format[10];
  int aggr_len, cnt = 0;
  
  aggr_len = RELATION_HEADER.hd_attrs[1].at_size;
  
  /* sort table entries first  */
  sort_table(HTSIZE); 
  
  for (tcntr = 0; tcntr < HTSIZE; tcntr++) {
    if (Hash_Table[tcntr] && Hash_Table[tcntr]->value) {
      if (tkn == FALSE || 
	  eval_tkn(tkn, Hash_Table[tcntr]->aggr_param)) {
	strncpy(tbuf+1, Hash_Table[tcntr]->value, buffer_len);
	
	/* leave two places after decimal point */
	sprintf(format, "%%%d.2lf", aggr_len-3);
	
	/* write the result of the function called in query */
	sprintf(auxbuf, format, Hash_Table[tcntr]->aggr_param);
	
	/* catenate second buffer after first entry */
	sprintf(tbuf+buffer_len+1+(aggr_len-strlen(auxbuf)),
		                               "%s", auxbuf);
	
	if (write(rptr->rl_fd, tbuf, RELATION_HEADER.hd_size) !=
	                RELATION_HEADER.hd_size) {
	  rc_done(rptr, TRUE);
	  hash_done();
	  return (error(INSBLK));
	}
	
	cnt++;
	RELATION_HEADER.hd_tcnt++;
	RELATION_HEADER.hd_tmax++;
      }
    }
  }
  return(cnt);
}

int eval_tkn(tkn, qua)
     int tkn;
     double qua;
{
  int comp_res;
  
  comp_res = dcomp(qua, lex_value);
  
  switch (tkn) {
  case LSS:   
    comp_res = (comp_res < 0);
    break;
  case LEQ:   
    comp_res = (comp_res <= 0);
    break;
  case EQL:   
    comp_res = (comp_res == 0);
    break;
  case GEQ:   
    comp_res = (comp_res >= 0);
    break;
  case GTR:   
    comp_res = (comp_res > 0);
    break;
  case NEQ:   
    comp_res = (comp_res != 0);
    break;
  }
  return(comp_res); 
}

dcomp(arg1, arg2)
     double arg1, arg2;
{
  return((arg1 == arg2) ? EQUAL : 
	 ((arg1 < arg2) ? FIRSTLSS : FIRSTGTR));
}

hash_data(tmprel_ptr, rptr, slptr, buf, cntr)
     struct relation *rptr, 	/* old rel pointer */
       *tmprel_ptr;             /* temp rel pointer */
     struct sel *slptr;
     char    *buf;
     int    *cntr;
{
  int    i, abase;
  struct sattr *saptr;
  char    *strip_catenate(), *aux_buf;
  
  /* allocate and initialize an auxiliary  tuple buffer */
  if ((aux_buf = (char *) 
   calloc(1, (unsigned) RELATION_HEADER.hd_size + 1)) == NULL) {
    rc_done(rptr, TRUE);
    return (error(INSMEM));
  }
  
  /* create the tuple from the selected attributes */
  abase = 1;
  buf[0] = ACTIVE; /* initialize indexes */
  for (saptr = SEL_ATTRS;saptr != NULL;saptr = saptr->sa_next) {
    for (i = 0; i < saptr->sa_attr->at_size; i++)
      buf[abase + i] = saptr->sa_aptr[i];
    
    /* reset */
    abase += i;
  }
  
  strip_catenate(aux_buf, buf, RELATION_HEADER.hd_size);
  
  /* eliminate redundant (duplicate) tuples if necessary
   * and write rest tuples into relation file */
  (*cntr) +=
    db_hsplace(aux_buf, tmprel_ptr->rl_name, buf, tmprel_ptr);
  nfree(aux_buf);
  return(TRUE);
  
} /* hash_data */
