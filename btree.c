

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
#include "btree.h"


#ifdef MAC
#include <io.h>
#endif

#ifdef DOS
#include <io.h>
#endif
/* global variable declarations */

struct block spare_block ; /* scratch block for splits
			    * and compressing */

int split_size = IXBLK_SPACE;   /* split block when it contains 
				 * more than this many bytes */
int comb_size = (IXBLK_SIZE/2); /* combine block when it 
				 * contains fewer than this 
				 *  many bytes */
BOOL err_sig = FALSE;
static BOOL insert = FALSE;
long get_free();
long write_level();
long next_ix();
long last_ix();
struct block *neighbor();

struct entry dummy_entry = { /* max length of key is 30 chars */
  NULLREC, { 0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0xff,
	       0xff, 0xff, 0xff, 0xff, 0x0,}, 
};

int prev_entry(pixd, pblk, offset)  /* back up one entry before  */
     struct ix_header *pixd;
     struct block *pblk;      /*  entry at specified offset */
     int offset;
{
  if (offset <= 0)            /* are we at start of block ? */
    return(-1);               /* can't back up */
  offset = scan_blk(pixd, pblk, offset);  /* find previous entry */
  return(offset);
} /* prev_entry */

int next_entry(pixd, pblk, offset)       /* go forward one entry */
     struct ix_header *pixd;
     struct block *pblk;       /* past entry at specified offset */
     int offset;
{
  if (offset >= pblk->blk_offset) /* at end of block ? */      
    return(-1);                   /* cannot move forward */
  
  /* move past entry */
  offset += (*(pixd->psize)) (ENT_ADDR(pblk, offset));
  if (offset >= pblk->blk_offset)  /* at end of block ? */     
    return(-1);                    /* no entry allowed here */
  
  return(offset);
} /* next_entry */

int copy_entry(pixd, to, from)           /* copy an entry */
     struct ix_header *pixd;
     struct entry *to;             /* to this address */
     struct entry *from;           /* from this address */
{
  int nsize;
  
  nsize =  (*(pixd->psize)) (from); /* get the entry's size */
  move_struct((char *) to, (char *) from, nsize); 
} /* copy_entry */

int scan_blk(pixd, pblk, n)    /* find the offset of last entry */
     struct ix_header *pixd;
     struct block *pblk; /* in this block */
     int n;              /* starting before this position */
{
  int i, last;
  
  i = 0; 
  last = 0;
  while (i < n) {         /* repeat until position is reached */
    last = i;             /* save current position */
    i += (*(pixd->psize))(ENT_ADDR(pblk, i));
  }
  return(last);           /* return last offset < n */
} /* scan_blk */

int last_entry(pixd, pblk)      /* find last entry in block */
     struct ix_header *pixd;
     struct block *pblk;  /* present block */
{
  /* scan for offset of last entry */
  return(scan_blk(pixd, pblk, pblk->blk_offset)); /* return offset */
} /* last-entry */

int create_ixf(fn)                /* create an index file */
     char fn[];
{
  int fd;

  fd = open(fn, O_CREAT | O_RDWR | O_BINARY, 0600);
  return(fd);
} /* create_ixf */

int read_ixf(pixd, start, buf, nrd)
     struct ix_header *pixd;
     RECPOS start;
     char *buf;
     int nrd;
{
  long lseek();
  lseek(pixd->ixfile, start, 0);
  return(read(pixd->ixfile, buf, nrd));
} /* read_ixf */

int write_ixf(pixd, start, buf, nwrt)
     struct ix_header *pixd;
     RECPOS start;
     char *buf;
     int nwrt;
{
  long lseek();

  lseek(pixd->ixfile, start, 0);
  return(write(pixd->ixfile, buf, nwrt));
} /* write_ixf */

int close_ixf(pixd)      /* close index file */
     struct ix_header *pixd;
{ 
  return(close(pixd->ixfile));
} /* close_ixf */

int open_ixf(fn)        /* open an index file */   
     char fn[];
{
  int fd;

  fd = open(fn, O_RDWR | O_BINARY);
  return(fd);
} /* open_ixf */

int make_free(pixd)   /* set up the available space list */
     struct ix_header *pixd;
{
  struct block *pblk;
  RECPOS ptr, fstart;
  
  pblk = &spare_block;         /* point to scratch block */
  if (pixd->dx.first_fbk == 0) {
    fstart =  ((RECPOS) (MAX_LEVELS +1)) * IXBLK_SIZE;
    pixd->dx.first_fbk = NULLREC; /* no blocks free to start */
    ptr = fstart;
  }
  else ptr = pixd->dx.first_fbk + IXBLK_SIZE;
  
  write_free(pixd, ptr, pblk);
} /* make_free */

int write_free(pixd, rfree, pblk)  /* write out a free block */
     struct ix_header *pixd;
     RECPOS rfree;           /* record no. of block to free */ 
     struct block *pblk;     /* use this as scratch space */
{
  pblk->lvl = FREE_LEVEL;    /* mark block as being free */
  clear_mem(pblk->entries, 0, IXBLK_SPACE); /* zero block */
  /* point to current 1st block, NULLREC at very first block */
  pblk->blk_pos = pixd->dx.first_fbk; 
  write_block(pixd, rfree, pblk);  /* write the free block */
  pixd->dx.first_fbk = rfree; /* make this block first free */
} /* write_free */

int init_bio(pixd)        /* initialize block input-output */
     struct ix_header *pixd;
{
  init_cache(pixd);       /* initialize cache buffers */
} /* init_bio */

int retrieve_block(pixd, ix_lvl, rloc, pblk, current)
     struct ix_header *pixd;
     /* retrieve an index block from cache or from index file */
     int ix_lvl;            /* index level for this block */
     RECPOS rloc;           /* block's location in index file */
     struct block *pblk;    /* put it in this block */
     
     int current;           /* if TRUE put it in cache */
{
  if (chk_cache(pixd, ix_lvl, rloc))   /* look in cache first */
    get_cache(pixd, ix_lvl, rloc, pblk); /* and get it */
  else {
    read_block(pixd, rloc, pblk);   /*  not in cache, read it */
    pblk->blk_pos = rloc;                     /* from disk */
    pblk->lvl = ix_lvl;
    if (current)              /* current block for level 1? */
      put_cache(pixd, ix_lvl, pblk); /* yes - place it in the */
  }                                   /* cache in header */
  return(IX_OK);
} /* retrieve_block */

int update_block(pixd, pblk)      /* update an index block in file */
     struct ix_header *pixd;
     struct block *pblk;        /* address of index block */
{
  if (chk_cache(pixd, pblk->lvl, pblk->blk_pos))
    put_cache(pixd, pblk->lvl, pblk);
  write_block(pixd, pblk->blk_pos, pblk);
} /* update_block */

int get_block(pixd, lvl, pblk)
     struct ix_header *pixd;
     /* allocate and set up a block, retrieve an index block */
     int lvl;                  /* index level for this block */
     struct block *pblk;       /* put it here */
{
  RECPOS rfree;
  
  rfree = pixd->dx.first_fbk; /* get loc. of 1st free block */
  make_free(pixd);            /* produce a free block */
  if (rfree == NULLREC)
    return(IX_FAIL);
  pblk->blk_pos = rfree;     /* record block's file position */
  pblk->lvl = lvl;           /* and its level */
  return(IX_OK);
} /* get_block */

int put_free(pixd, pblk)       /* return a block to the free list */
     struct ix_header *pixd;
     struct block *pblk;
{
  /* when a block is freed ensures that the cache does not
   * contain out of data information about block */
  
  scrub_cache(pixd, pblk->blk_pos);      /* remove from cache */
  pblk->lvl = FREE_LEVEL;          /* mark block as free */
  write_free(pixd, pblk->blk_pos, pblk);
} /* put_free */

int read_block(pixd, rloc, pblk)  /* read an index block */
     struct ix_header *pixd;
     RECPOS rloc;           /* block's location in index file */
     struct block *pblk;    /* store the block here */
{
  return(read_ixf(pixd, rloc, (char *) pblk, sizeof(struct block)));
} /* read_block */

int write_block(pixd, rloc, pblk) /* write an index block */
     struct ix_header *pixd;
     RECPOS rloc;           /* block's location in index file */
     struct block *pblk;    /* the block to write */
{
  return(write_ixf(pixd, rloc, (char *) pblk, sizeof(struct block)));
} /* write_block */

int init_cache(pixd)         /* initialize block i/o cache */ 
     struct ix_header *pixd;
{
  int i;
  
  for (i = 0; i < MAX_LEVELS; i++)   /* denote that there is */
    pixd->cache[i].blk_pos = NULLREC; /* nothing in memory yet */
  /* NEW ADDED */
  pixd->ix_recpos = 0;     /* nothing has been changed */
} /* init_cache */

int chk_cache(pixd, ix_lvl, rloc) /* check cache for an index block */
     struct ix_header *pixd;
     int ix_lvl;            /* index level for this block */
     RECPOS rloc;           /* block's location in index file */
{
  
  if (pixd->cache[ix_lvl].blk_pos != rloc)
    return(FALSE);
  return(TRUE);
  
} /* chk_cache */

int get_cache(pixd, ix_lvl, rloc, to) /* get a block from cache */
     struct ix_header *pixd;
     int ix_lvl;                /* index level for this block */
     RECPOS rloc;               /* block's loc in index file */
     struct block *to;          /* if found, copy it there */
{
  struct block *pblk;
  
  if (pixd->cache[ix_lvl].blk_pos != rloc)
    return(FALSE);
  pblk = &pixd->cache[ix_lvl];
  move_struct((char *) to, (char *) pblk, sizeof(struct block));
  pblk->blk_pos = rloc;
  return(TRUE);
} /* get_cache */

int put_cache(pixd, ix_lvl, pblk)   /* write index block to file */
     struct ix_header *pixd;
     int ix_lvl;
     struct block *pblk;      /* address of index block */
{
  struct block *to;
  
  to = &pixd->cache[ix_lvl];
  move_struct((char *) to, (char *) pblk, 
	      sizeof(struct block)); /* copy the whole block */
} /* put_cache */

int scrub_cache(pixd, rloc)     /* remove a block from the cache */
     struct ix_header *pixd;
     RECPOS rloc;         /* the block's file position */
{                         /* = RECPOS scrubs all levels */
  int i;
  /* search the cache */
  for (i = 0; i < pixd->dx.nl; i++)
    if ((rloc == NULLREC) || (rloc == pixd->cache[i].blk_pos))
      pixd->cache[i].blk_pos = NULLREC;
} /* scrub_cache */

int open_ix(f_name, pixd, cfun, sfun)
     char f_name[];         /* file name */
     struct ix_header *pixd; /* control block for index */
     int (*cfun) ();        /* pointer to compare function */
     int (*sfun) ();        /* pointer to entry size function */
{
  int ret;
  
  if (pixd->ixfile <= 0) {
    ret = open_ixf(f_name);
    if (ret < 0)              /* check for failure */
      return (FALSE);
    pixd->ixfile = ret;
  }
  
  /* read header descriptor from disk and place it into pix 
     descriptor structure */
  read_ixf(pixd, 0L, (char *) &pixd->dx, sizeof(struct ix_disk));
  pixd->pcomp = cfun;  /* record address of compare  */
  pixd->psize = sfun;  /* and of entry size functions */
  init_bio(pixd);         /* initialize block i/o and clear cache */
  go_first(pixd);      /* pos at file start(valid current pos) */
  return(TRUE);
} /* open_ix */

long write_level(pixd, pdum, ndum)
     struct ix_header *pixd;
     struct entry *pdum;          /* dummy entry */
     int ndum;                    /* size of dummy entry */
{
  struct block blk;
  int i;
  RECPOS rloc;
  
  pdum->lower_lvl = NULLREC;
  rloc = 0;
  for (i = 0; i < MAX_LEVELS; i++) {
    rloc += IXBLK_SIZE;
    /* make a block of zeroes */
    clear_mem((char *) &blk, 0, IXBLK_SIZE); 
    move_struct((char *) blk.entries, 
		(char *) pdum, ndum); /* place dummy in block */
    blk.lvl = i;
    blk.blk_offset = ndum;   /* block contains a single entry */
    write_block(pixd, rloc, &blk); /* write the block */
    pdum->lower_lvl = rloc;
  }
  
  return(rloc);
} /* write_level */

int go_first(pixd)            /* go to first entry in index */
     struct ix_header *pixd;  /* points to an index descriptor */
{
  struct block blk;
  
  /* start at root level and */
  first_ix(pixd, pixd->dx.nl-1, pixd->dx.root_bk, &blk);
  return(IX_OK);     
} /* go_first */

int first_ix(pixd, lvl, rloc, pblk) /* set current pos to 1st entry */
     struct ix_header *pixd;
     int lvl;                 /* at this and lower levels */
     RECPOS rloc;             /* cur. block for level 1 */
     struct block *pblk;
{
  
  pixd->pos[lvl].cblock = rloc; /* set pos of current block */
   pixd->pos[lvl].coffset = 0;   /* and its offset */
  retrieve_block(pixd, lvl, rloc,  pblk, CUR); /* get the block */
  
  /* work down to leaf level */
  if (lvl > 0)
    first_ix(pixd, lvl-1, ENT_ADDR(pblk, 0)->lower_lvl, pblk);
  
} /* first_ix */

int go_last(pixd)      /* go to last index entry (dummy entry) */
     struct ix_header *pixd;  /* points to an index descriptor */
{
  struct block blk;
  
  /* start at root */
  final_ix(pixd, pixd->dx.nl-1, pixd->dx.root_bk, &blk); 
  /* position at last entry */
  return(IX_OK);
} /* go_last */

int final_ix(pixd, lvl, rloc, pblk) /* set current pos to 1st entry */
     struct ix_header *pixd;
     int lvl;                 /* at this and lower levels */
     RECPOS rloc;             /* cur. block for level 1 */
     struct block *pblk;
{
  int off;
  
  pixd->pos[lvl].cblock = rloc; /* set current block */
  
  retrieve_block(pixd, lvl, rloc, pblk, CUR); /* get the block */
  
  off = last_entry(pixd, pblk);      /* current offset = last entry */
  pixd->pos[lvl].coffset = off;
  if (lvl > 0)              /* set lower levels */
    final_ix(pixd, lvl-1, ENT_ADDR(pblk, off)->lower_lvl, pblk);
} /* final_ix */

int get_next(pe, pixd)        /* get next index entry */
     struct entry *pe;       /* put the entry here */
     struct ix_header *pixd;  /* points to an index descriptor */
{
  struct block blk;
  
  /* check for dummy entry at end of file */
  copy_current(pixd, 0, pe);
  if (FUNCALL(pixd->pcomp) (pe, &pixd->dx.dum_entr, get_datatype(pixd, pe)) == 0)
    return(EOF_IX);
  
  if (next_ix(pixd, 0, &blk) != NULLREC) { /* got next leaf entry? */
    copy_current(pixd, 0, pe);            /* then copy it */
    return(IX_OK);                  /* return successfully */
  }
  else
    return(EOF_IX);
} /* get_next */

long next_ix(pixd, lvl, pblk)      /* get the next entry on a level */
     struct ix_header *pixd;
     int lvl;                /* level no. */
     struct block *pblk;     /* curent block */
{
  int off;
  RECPOS newblk;
  
  if (lvl >= pixd->dx.nl)       /* above top level ? */
    return(NULLREC);           /* yes - failure */
  /* get current block */
  retrieve_block(pixd, lvl, pixd->pos[lvl].cblock, pblk, CUR);
  /* move to next entry in block */
  off = next_entry(pixd, pblk, pixd->pos[lvl].coffset);
  
  if (off >= 0)       /* past the end of block? */
    pixd->pos[lvl].coffset = off; /* no - record new position */
  else {                         /* next block on this level */
    newblk = next_ix(pixd, lvl + 1, pblk);
    
    if (newblk != NULLREC) {     /* check for begin of index */
                                 /* make this current block */
      pixd->pos[lvl].cblock = newblk; 
      retrieve_block(pixd, lvl,newblk, pblk, CUR); /* put into mem. */
      pixd->pos[lvl].coffset = 0; /* at first entry */
    }
    else return(NULLREC);        /* at start of index - can't */
  }  
  /* return block no. at lower level */
  return( ENT_ADDR(pblk, pixd->pos[lvl].coffset)->lower_lvl);
  
} /* next_ix */

long  last_ix(pixd, lvl, pblk)  /* get previous entry for a level */
     struct ix_header *pixd;
     int lvl;             /* level no. */
     struct block *pblk;  /* space for a block */
{
  int off;
  RECPOS newblk;
  
  if (lvl >= pixd->dx.nl)   /* above top level ? */
    return(NULLREC);       /* yes - failure */
  /* get current block */
  retrieve_block(pixd, lvl, pixd->pos[lvl].cblock, pblk, CUR);
  /* back up one entry in block */
  off = prev_entry(pixd, pblk, pixd->pos[lvl].coffset);
  if (off >= 0)       /* past the beginning of block */
    pixd->pos[lvl].coffset = off;  /* no - record new offset */
  else {                    /* yes - get previous block */
    newblk = last_ix(pixd, lvl + 1, pblk); 
    if (newblk != NULLREC) {/* check for begin of index */
                            /* make this current block */
      pixd->pos[lvl].cblock = newblk;
      retrieve_block(pixd, lvl,newblk, pblk, CUR); /* put into mem. */
      /* offset is last entry in block */
      pixd->pos[lvl].coffset = last_entry(pixd, pblk);
    }
    else return(NULLREC); /* at start of index: can't back up */
  }                       /* return ptr in curr. entry */
  return( ENT_ADDR(pblk, pixd->pos[lvl].coffset)->lower_lvl);
} /* last_ix */

int get_current(pe, pixd)    /* get current index entry */
     struct entry *pe;       /* put entry here */
     struct ix_header *pixd; /* points to an index descriptor */
{
  copy_current(pixd, 0, pe);

} /* get_current */

int find_key(pixd, pe, pb, poff, comp_fun)
     struct ix_header *pixd;
     /* look for a key in a block */
     struct entry *pe;    /* contains the target key */
     struct block *pb;    /* look in this specific block */
     int *poff;           /* store offset where we stop here */
     int (*comp_fun) ();  /* pointer to compare function */
{
  int i;                  /* offset */
  int ret;                /* result of last comparison  */
  struct entry *p;
  
  i = 0;
  /* repeat until the end of block */
  while (i < pb->blk_offset) {
    p = ENT_ADDR(pb, i);  /* get the entry address and */
    /* compare to target key */
    ret = FUNCALL(comp_fun) (pe, ENT_ADDR(pb, i), get_datatype(pixd, pe));
    
    /* if two primary keys match insertion allowed only in sysatrs */
    if (ret == 0 && p->key[0] == 'P' && insert
	&&  strcmp(pixd->dx.ixatts.ix_rel,"sysatrs") != EQUAL)
      if (pixd->dx.ixatts.ix_foreign == 'f' || 
	  strncmp(p->key, "P*2", 3) == EQUAL) 
	err_sig = TRUE;
    
    if (ret <= 0)  /* quit if the target is leq current entry */
      break;
    i = next_entry(pixd, pb, i);    /* move to next entry */
  }
  *poff = i;             /* store the offset where we stopped */
  return(ret);           /* result of last compare */
} /* find_key */

int ins_entry(pixd, pb, pe, off)  /* add an entry to a block */
     struct ix_header *pixd;
     struct block *pb;      /* the block */
     struct entry *pe;      /* the entry to insert */
     int off;               /* the offset where we insert it */
{
  int n_size;
  
  n_size = (*(pixd->psize))(pe); /* how big is the new insert? */
  
  /* move everything to the end of block */
  move_up(pb, off, n_size);  /* make room for the new entry */
  copy_entry(pixd, ENT_ADDR(pb, off), pe);  /* move it in */
  pb->blk_offset += n_size;           /* adjust block size */
  
} /* ins_entry */

int del_entry(pixd, pb, off)         /* remove entry from block */
     struct ix_header *pixd;
     struct block *pb;         /* the block to work on */
     int off;                  /* new location of entry */
{
  int n_size;
  /* get entry size */ 
  n_size = (*(pixd->psize)) (ENT_ADDR(pb, off));
  
  /* move entries above curr. one down */
  move_down(pb, off, n_size);  
  pb->blk_offset -=  n_size;  /* adjust the no. of bytes used */
  
} /* del_entry */

int move_up(pb, off, n)    /* move part of a block upward */
     struct block *pb;     /* the block to work on */
     int off;              /* place to start moving */
     int n;                /* how far up to move things */
{                          /* move entries */
  move_struct((char *)  ENT_ADDR(pb, off + n),  /* to here */
	      (char *) ENT_ADDR(pb, off),     /* from there */
	       pb->blk_offset - off);         /* rest of block */
  
} /* move_up */

int move_down(pb, off, n)   /* move part of a block upward */
     struct block *pb;      /* the block to work on */
     int off;               /* place to start moving */
     int n;                 /* how far up to move things */
{                                 /* move entries */
  move_struct((char *) ENT_ADDR(pb, off),   /* to here */
	      (char *) ENT_ADDR(pb, off + n), /* from there */
	      pb->blk_offset - (off + n));    /* rest of block */
  
} /* move_down */

int combine(pl, pr)          /* combine two blocks */
     struct block *pl;       /* add the left block */
     struct block *pr;       /* to the right block */
{
  move_up(pr, 0, pl->blk_offset); /* make room for left block */
  /* move it in left block contents */
  move_struct((char *) ENT_ADDR(pr, 0), 
	      (char *) ENT_ADDR(pl, 0), 
	      pl->blk_offset);
  /* adjust block size */
  pr->blk_offset = pr->blk_offset + pl->blk_offset;  
} /* combine */

int find_ix(pe, pixd)               /* find the first entry with a key */   
     struct entry *pe;                 /* points to key to be matched */
     struct ix_header *pixd;            /* points to index header */
{
  int ret;
  struct entry temp_entr;
  
  /* make sure that target is < dummy */
  if (FUNCALL(pixd->pcomp) (pe, &pixd->dx.dum_entr, get_datatype(pixd, pe)) >= 0) {
    go_last(pixd);             /* no position at end */
    return(NOTFOUND);          /* return not equal */
  }
  
  ret = find_level(pixd, pixd->dx.nl-1, pe, pixd->dx.root_bk);
  if (ret == 0) {              /* if an entry was found */
    copy_current(pixd, 0, &temp_entr); /* store its record ptr */
    pe->lower_lvl = temp_entr.lower_lvl;
  }
  return(ret);
} /* find_ix */

int find_level(pixd, lvl, pe, r)    /* find key within a level*/
     struct ix_header *pixd;
     int lvl;                 /* the level */
     struct entry *pe;        /* the target entry */
     RECPOS r;                /* block to look in */
{
  struct block blk;           /* scratch blk, with separate 
                               * copy for each blk */
  int ret, off;
  
  retrieve_block(pixd, lvl, r, &blk, CUR); /* retrieve current blk */
  /* look for the key there */
  ret = find_key(pixd, pe, &blk, &off, pixd->pcomp);
  pixd->pos[lvl].cblock = r;   /* make this the current block */
  pixd->pos[lvl].coffset = off;/* and offset in the block */
  /* search lower levels */
  if (lvl > 0)
    ret = find_level(pixd, lvl-1, pe, ENT_ADDR(&blk, off)->lower_lvl);
  return(ret);
  
} /* find_level */

int insert_ix(pe, pixd)      /* find first entry with a key */
     struct entry *pe;      /* points to key to be matched */
     struct ix_header *pixd; /* points to index descriptor */
{
  int ret;
  struct block blk;
  
  ret = ins_level(pixd, 0, pe, &blk); /* insert entry at leaf level */
  
  if (ret == IX_OK)         /* if insertion was successful */
    next_ix(pixd, 0, &blk);       /* move past entry inserted */
  insert = FALSE;           /* reset insert flag */
  return(ret);              /* return success or failure  */
} /* insert_ix */

int ins_level(pixd, lvl, pe, pb)  /* insert an entry */
     struct ix_header *pixd;
     int lvl;               /* at this level */
     
     struct entry *pe;      /* contains the target entry */
     struct block *pb;
{
  int ret;
  
  if (lvl >= pixd->dx.nl)     /* do we need a new level ? */
    return(IX_FAIL);         /* yes - overflow */
  
  retrieve_block(pixd, lvl, pixd->pos[lvl].cblock, pb, CUR);
  /* does it fit into the block ? */
  if ((pb->blk_offset + (*(pixd->psize)) (pe)) <= split_size) {
    ins_entry(pixd, pb, pe, pixd->pos[lvl].coffset); /* put in block */
    update_block(pixd, pb);      /* update the index blk in file */
    ret = IX_OK;
  }
  else ret = split(pixd, lvl, pe, pb); /* split the block */
  
  return(ret);
} /* ins_level */

int split(pixd, lvl, pe, pb)            /* split a block into two */
     struct ix_header *pixd;
     int lvl;
     struct entry *pe;
     struct block *pb;
{
  int half, ins_pos, last , ret;
  struct block *pbb;
  struct entry entr;
  
  ins_pos = pixd->pos[lvl].coffset;/* rem. where insert was */
  
  if ((lvl+1) >= pixd->dx.nl)     /* check for top level */
    return(IX_FAIL);             /* can't split top level blk */
  
  /* make a block long enough to store both old and new entries 
   * this block should temporarily serve as a buffer */
  pbb = (struct block *) calloc(sizeof(struct block) + 
				sizeof(struct entry), 1);
  if (pbb == NULL)               /* did allocation fail ? */
    return(IX_FAIL);             /* yes - exit */
  
  /* do insert in big block */
  pbb->blk_offset = 0;
  combine(pb, pbb);  /* copy contents of old buffer */
  ins_entry(pixd, pbb, pe, pixd->pos[lvl].coffset); /* ins new entry */
  
  /* find where to split, no more than half entries left blk */
  /* start of last entry in left half of big block */
  last = scan_blk(pixd, pbb, pbb->blk_offset/2);
	                            
  half = next_entry(pixd, pbb, last);         /* end of left half */
  /* allocate disk space for left block */
  if (get_block(pixd, lvl, pbb) == IX_FAIL) { /* check for failure */
    nfree((char *) pbb);
    return(IX_FAIL);
  }
  
  /* make an entry for the new block on the upper level */
  copy_entry(pixd, &entr, ENT_ADDR(pbb, last));
  entr.lower_lvl = pbb->blk_pos;  /* point entry to left blk */
  
  ret = ins_level(pixd, lvl+1, &entr, pb); /* ins new index entry */
  /* left block at higher level */
  /* this makes the lvl+1 position */
  /* point to the left block */
  if (ret != IX_OK)  /* higher level failure (overflow) */
    { 
      nfree((char *) pbb);     /* yes - free big block area */
      put_free(pixd, pbb);           /*       free new index block */
      return(ret);             /*       return failure code */
    }
  /* use pb for right block */
  move_struct((char *) ENT_ADDR(pb, 0), 
	      (char *) ENT_ADDR(pbb, half), 
	       pbb->blk_offset - half);
  pb->blk_offset = pbb->blk_offset - half;
  pb->blk_pos =  pixd->pos[lvl].cblock; /* restore blk's loc */
  pb->lvl = lvl;                       /* and its level */
  update_block(pixd, pb);                    /* and update it */
  
  /* fix up left block */
  pbb->blk_offset = half;              /* size = left half */
  /* entries already in place */
  update_block(pixd, pbb);                   /* update left block */
  
  if (ins_pos >= half) {    /* curr. entry in left or right ? */
    /* right - adjust offset */
    pixd->pos[lvl].coffset =  pixd->pos[lvl].coffset-half;
    next_ix(pixd, lvl+1, pb);    /* upper level position */
  }   /* left - make left block current */
  else pixd->pos[lvl].cblock = entr.lower_lvl;
  
  nfree((char *) pbb);     /* free big scratch block */
  return(ret);
} /* split */

delete_ix(pixd)              /* delete the current entry */
     struct ix_header *pixd; /* points to index descriptor */
{
  struct entry temp_entr;
  struct block b;
  int ret;
  
  /* check for dummy entry at end-of-index */
  copy_current(pixd, 0, &temp_entr);
  if (FUNCALL(pixd->pcomp) (&temp_entr, &pixd->dx.dum_entr, get_datatype(pixd, &temp_entr)) == 0)
    return(IX_FAIL);
  /* not at end - delete it */
  ret = del_level(pixd, 0, &b);
  
  return(ret);
} /* delete_ix */

int del_level(pixd, lvl, pb)           /* delete entry within level */
     struct ix_header *pixd;
     int lvl;
     struct block *pb;
{
  int ret;
  struct entry temp_entr;
  
  ret = IX_OK;
  /* retrieve current block */
  retrieve_block(pixd, lvl, pixd->pos[lvl].cblock, pb, CUR);
  del_entry(pixd, pb, pixd->pos[lvl].coffset); /* del entry in blk */
  
  if (pb->blk_offset == 0) {      /* block now empty ? */
    put_free(pixd, pb);                 /* yes - free block */
    ret = del_level(pixd, lvl+1, pb);   /* del entry for empty blk */
    copy_current(pixd, lvl+1, &temp_entr); /* get new cur. blk ptr */
    /* reset pos. for lower levels */
    first_ix(pixd, lvl,temp_entr.lower_lvl, pb); 
    return(ret);
  }
  /* last entry in block deleted ? */
  if ( pixd->pos[lvl].coffset >= pb->blk_offset) {
    /* yes - correct upper index */
    fix_last(pixd, lvl, pb->blk_pos, ENT_ADDR(pb, last_entry(pixd, pb)));
  }
  
  if (pb->blk_offset < comb_size)  /* less than half full ? */
    ret = compress(pixd, lvl, pb);   /* yes -  combine with neighb. */
  else update_block(pixd, pb);
  /* retrieve the  block again */
  retrieve_block(pixd, lvl, pixd->pos[lvl].cblock, pb, CUR);
  /* is position past end of block ? */
  if ( pixd->pos[lvl].coffset >= pb->blk_offset)
    first_ix(pixd, lvl, next_ix(pixd, lvl+1, pb),pb); /* move to next blk */
  
  return(ret);
} /* del_level */

int compress(pixd, lvl, pblk) /* combine a block with the neighbor */
     struct ix_header *pixd;
     int lvl;
     struct block *pblk;   /* block to be combined */
{
  struct block *pt = NULL;
  
  if ((lvl+1) == pixd->dx.nl) { /* is this the root level ? */
    update_block(pixd, pblk);        /* yes - update the block */
    return(IX_OK);             /* and return */
  }
  
  pt = neighbor(pixd, lvl, LEFTN);   /* get left neighbor block */
  if ((pt != NULL) && (pt->blk_offset + pblk->blk_offset 
		                          <= IXBLK_SPACE)) {
    combine(pt, pblk);       /* combine blocks */
    update_block(pixd, pblk);      /* update the right block */
    put_free(pixd, pt);            /* free the left index block */
    /* cblock(lvl) is OK as is, adjust cur. position */
    pixd->pos[lvl].coffset += pt->blk_offset;
    last_ix(pixd, lvl+1, pblk);    /* point higher level to left blk*/
    del_level(pixd, lvl+1, pblk);  /* delete ptr. to left blk. */
    return(IX_OK);
  }
  
  /* get right neighbor block */
  pt = neighbor(pixd, lvl, RIGHTN);
  if ((pt != NULL) && (pt->blk_offset + pblk->blk_offset 
		                            <= IXBLK_SPACE)) {
    combine(pblk, pt);       /* combine blocks */
    update_block(pixd, pt);        /* update the right block */
                             /* right blk is cur. one now */
    pixd->pos[lvl].cblock = pt->blk_pos;
                             /* coffset(lvl) is ok as is */
    put_free(pixd, pblk);          /* free the left index block */
    del_level(pixd, lvl+1, pblk);  /* delete ptr. to left blk. */
    return(IX_OK);
  }
  
  /* can't combine - just update block */
  update_block(pixd, pblk);
  return(IX_OK);
} /* compress */

int fix_last(pixd, lvl, r, pe)  /* fix higher level index */
     struct ix_header *pixd;
     int lvl;             /* level we're now on */
     RECPOS r;            /* lower_lvl for higher level entry */
     struct entry *pe;                  /* entry with new key */
{
  struct entry temp_entr; /* last entry in a blk deleted/replaced */
  
  /* update the higher level index */
  copy_entry(pixd, &temp_entr, pe); /* copy key */
  temp_entr.lower_lvl = r;    /* put in the record pointer */
  return(replace_entry(pixd, lvl+1, &temp_entr));  /* repl. entry */
} /* fix_last */

int replace_entry(pixd, lvl, pe)    /* replace current index entry */
     struct ix_header *pixd;
     int lvl;                 /* at this index level */
     struct entry *pe;        /* new entry */
{
  struct block blk;
  int ret;
  /* retrieve the index block */
  retrieve_block(pixd, lvl, pixd->pos[lvl].cblock, &blk, CUR);
  /* is this the last entry ? */
  if ( pixd->pos[lvl].coffset == last_entry(pixd, &blk))
    fix_last(pixd, lvl, pixd->pos[lvl].cblock, pe); /* fix higher level */
  
  del_entry(pixd, &blk,  pixd->pos[lvl].coffset); /* rem cur. entry */
  /* make room to insert new entry */
  if ((blk.blk_offset + (*(pixd->psize)) (pe)) <= split_size) {
    ins_entry(pixd, &blk, pe, pixd->pos[lvl].coffset); /* ins in blk */
    update_block(pixd, &blk);            /* and update the block */
    ret = IX_OK;
  }
  else ret = split(pixd, lvl, pe, &blk); /* split the block */
  
  return(ret);
  
} /* replace_entry */

struct block *neighbor(pixd, lvl, direction)
     struct ix_header *pixd;
     int lvl;                 /* level to fetch neighbor on */
     int direction;           /* left or right neighbor */
{
  RECPOS rnext;
  int off;
  struct block *pb;
  
  pb = &spare_block;
  lvl++;             /* look in higher level index */
  retrieve_block(pixd, lvl,  pixd->pos[lvl].cblock, pb, CUR);
  /* get the offset on next/prev. entry */
  if (direction == RIGHTN)         /* offset of next entr. */
    off = next_entry(pixd, pb, pixd->pos[lvl].coffset);
  else /* get offset of previous entry */
    off = prev_entry(pixd, pb, pixd->pos[lvl].coffset);
  
  /* at end or beginning ? */
  if (off < 0)
    return(NULL);
  rnext = ENT_ADDR(pb, off)->lower_lvl; /* neighbor's blk no. */
  /* read it into memory */
  retrieve_block(pixd, lvl-1, rnext, pb, NOT_CUR);
  return(pb);             /* return its address */
} /* neighbor */

int copy_current(pixd, lvl, pe)     /* copy current index entry */
     struct ix_header *pixd;
     int lvl;                 /* at this level */
     struct entry *pe;        /* to this address */
{
  struct block *pb;
  struct entry *e;

  pb = &spare_block;          /* get current block */
  retrieve_block(pixd, lvl,  pixd->pos[lvl].cblock, pb, CUR);
  /* copy current entry */

  e = ENT_ADDR(pb, pixd->pos[lvl].coffset);

  copy_entry(pixd, pe, e);
  return(IX_OK);
} /* copy_current */

int find_exact(pe, pixd)
     struct entry *pe;      /* put the entry here */
     struct ix_header *pixd; /* points to an index descriptor */
{
  int ret;
  struct entry e;
  
  copy_entry(pixd, &e, pe);        /* make copy for find call */
  ret = find_ix(&e, pixd);    /* get 1st entry matching key */
  
  /* in case that composite keys match add no new entry */
  if (ret == 0 && strncmp(e.key, "P2*", 3) == EQUAL)
    return(FOUND);

  while (ret == 0) {         /* until keys don't match */
    if (e.lower_lvl == pe->lower_lvl || err_sig) {
      err_sig = FALSE;       /* reset flag */
      return(FOUND);         /* return it if they match */
    }
    
    if (get_next(&e, pixd) == EOF_IX) /* get next entry */
      return(NOTFOUND);      /* at end of index - not found */
    
    ret = FUNCALL(pixd->pcomp) (&e, pe, get_datatype(pixd, pe)); /* compare keys */
  }
  
  return(ret);               /* not FOUND - non-zero code */
} /* find_exact */

int find_ins(pe, pix)           /* find pos., insert an entry */
     struct entry *pe;              /* the entry */
     struct ix_header *pix;         /* points to an index descriptor */
{                   /* returns IX_FAIL if entry present */
  insert = TRUE;          
  if (find_exact(pe, pix) == FOUND)   /* look for the entry */
    return(IX_FAIL);             /* already there - failure */
  
  return(insert_ix(pe, pix));  /* do the insertion */
} /* find_ins */

int find_del(pe, pix)        /* find pos., insert an entry */
     struct entry *pe;       /* the entry */
     struct ix_header *pix;  /* points to an index descriptor */
{                   /* returns IX_FAIL if entry not present */
  
  if (find_exact(pe, pix) != FOUND) /* look for the entry */
    return(IX_FAIL);                /* not there - failure */
  
  /* find_exact has set the cur. position */
  return(delete_ix(pix));           /* del the current entry */
  
} /* find_del */

int compf(p1, p2, data_type)
     struct entry *p1, *p2;
     int data_type;
{
  switch (data_type) {
  case TCHAR:
    return(strcomp(p1->key, p2->key));
  case TNUM:
    if (strncmp(p1->key, p2->key, 3) == EQUAL)
      return(numcomp(p1->key + 3, p2->key + 3));
    else
      return(strcomp(p1->key, p2->key));
  case TREAL:
    if (strncmp(p1->key, p2->key, 3) == EQUAL)
      return(realcomp(p1->key + 3, p2->key + 3));
    else
      return(strcomp(p1->key, p2->key));
  }
  return(-1);
}

int sizef(p1)
     struct entry *p1;
{
  int s;

  s = strlen(p1->key) + sizeof(RECPOS) + 1;
  while (s % sizeof(RECPOS))			/* align to word boundary */
    s++;
  return(s);
}

strcomp(s, t)
     char *s, *t;
{
  *t =(*t)&127;
  for (; *s == *t; s++, t++)
    if (*s == '\0')
      return(0);
  return(*s-*t);
}

int get_datatype(pixd, e)
     struct ix_header *pixd;
     struct entry *e;
{
  switch (e->key[0]) {
  case 'P':
    switch (e->key[1]) {
    case '0':
      return(pixd->dx.ixatts.ix_datatype[0]);
    case '1':
      return(pixd->dx.ixatts.ix_datatype[1]);
    case '2':
      return(TCHAR);
    }
  case 'S':
    case '0':
      return(pixd->dx.ixatts.ix_datatype[2]);
    case '1':
      return(pixd->dx.ixatts.ix_datatype[3]);
  }
  return(-1);
}

int key_comp(k1, k2, data_type)
     char *k1, *k2;
     int data_type;
{
  switch (data_type) {
  case TCHAR:
    return(strcomp(k1, k2));
  case TNUM:
    return(numcomp(k1, k2));
  case TREAL:
    return(realcomp(k1, k2));
  }
  return(-1);
}

