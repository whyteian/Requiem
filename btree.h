

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

/* btree.h - data structures and constants for BTREE modules */

#define CUR        1  /* getting current block */
#define NOT_CUR    0  /* getting non-current block */
#define UPDATES    1  /* update indexed attributes */
#define NO_UPDATE  0  /* no indexed attribute updates */
#define EOF_IX    -2  /* return value for end-of-index */
#define IX_FAIL   -1  /* return value for failed operation */
#define IX_OK      0  /* return value for sucess */
#define NULLREC   -1L /* special return value for file pos of
		       * index block/data structure */
#define MAX_LEVELS  4 /* four index levels permitted */
#define MAXKEY     30 /* space to accept max. length of key */
#define IXBLK_SIZE  1024  /* no. of bytes in a block on disk */
#define IXBLK_SPACE (IXBLK_SIZE - 2 * sizeof(short) - sizeof(RECPOS))
		      /* bytes of entry space in blk */
#define LEFTN      -1 /* request for left neighbor */
#define RIGHTN      1 /* request for right neighbor */
#define FOUND       0 /* entry found */
#define NOTFOUND    1 /* entry not found */
#define FREE_LEVEL -1 /* marks a block as free */
#define MAXKEYS     2 /* max no. of keys per index */
#define KEYSIZE    16 /* size of key */
#define BNSIZE     10 /* equivalent of ANSIZE in requ.h */
typedef long RECPOS;  /* file pos. of index block/data rec. */

/* call a function using a pointer */
#define FUNCALL(pfun)      (* (pfun) )
/* get the address of entry in a block */
#define ENT_ADDR(pb, off) ((struct entry *)\
			   ((char *)((pb)->entries)+(off)))
  
struct entry { /* entry format in index */
  RECPOS lower_lvl;  /* points to lower level */
  char key[MAXKEY];  /* start of key value with ficticious size 
                      * whose actual data type is unknown */
};

struct block { /* index block format */
  RECPOS blk_pos;      /* index file location of block 
			* or location of next free block */
  short blk_offset;    /* first unused location in block */
  short lvl;           /* records level no. -1 for free */
  char entries[IXBLK_SPACE];   /* space for entries */
};

struct indx_attrs { /* indexed attributes */
  char ix_refkey[MAXKEYS][24];    /* referenced attributes */
  char ix_prim[MAXKEYS][BNSIZE];  /* primary attributes */
  char ix_secnd[MAXKEYS][BNSIZE]; /* secondary attributes */
  char ix_rel[12];                /* relation name */
  char ix_foreign;                /* indicates a foreign key */
  int ix_datatype[2 * MAXKEYS];   /* data type of key attributes */
};

struct ix_disk { /* disk file index descriptor */
  short nl;           /* no. of index levels */
  RECPOS root_bk;     /* location of root block in file */
  RECPOS first_fbk;   /* relative address of first free block */
  struct indx_attrs ixatts;    /* indexed attribute entry */
  struct entry dum_entr;       /* dummy entry */
};

struct descriptor { /* index level descriptor entry */
  RECPOS cblock;  /* current block no. */
  int  coffset;   /* cur.offset within block for indx level */
};

struct ix_header { /* in-memory index descriptor */
  int  ixfile;         /* descriptor for open index file */
  int (*pcomp) ();     /* pointer to compare function */
  int (*psize) ();     /* pointer to entry size function */
  struct ix_disk dx;               /* disk resident stuff */
  struct block cache[MAX_LEVELS];  /* cache for curent blocks */
  struct descriptor pos[MAX_LEVELS];    
  RECPOS ix_recpos;    /* indicate which one has been changed */
};
