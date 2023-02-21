


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

/* ----------------------------------------------------------
   | this file contains the definitions and data structures |
   | used by application programs                           |
   ---------------------------------------------------------- */

#define FALSE	0
#define TRUE	1

#define ASIZE   16  /* size of a attribute entry */
#define ANSIZE  10  /* size of a attribute name */
#define HDSIZE  16  /* size of a relation entry */
#define NATTRS  31  /* number of attributes in header block */
#define RNSIZE  10  /* size of a relation name */


/* -------------- pgm interface structures ------------------ */

struct binding {
  struct attribute *bd_attr;  /* bound attribute */
  char *bd_vtuple;            /* pointer to value in tuple */
  char *bd_vuser;             /* pointer to user buffer */
  struct binding *bd_next;    /* next binding */
};

/* ------------- command and query structures --------------- */

struct attribute { /* static boundary: cater for alignment */
  short at_size;             /* attribute size in bytes */
  char  at_name[ANSIZE];     /* attribute name */
  char  at_type;             /* attribute type */
  char  at_key;              /* attribute key */
  char  at_semid;	/* semantically identical attribute */
  char  at_unused[ASIZE-ANSIZE-5]; /* unused space */
};

/* in disk file descriptor */
struct header {  /* alignement: size must be 512 bytes */
  long  hd_avail;     /* address of first free record */
  short hd_cursor;    /* relative cursor position */
  short hd_data;      /* offset to first data byte */
  short hd_size;      /* size of each tuple in bytes */
  short hd_tcnt;      /* no. of tuples in relation */
  short hd_tmax;      /* relative pos. of last tuple */
  char hd_unique;     /* unique attribute specifier */
  char hd_unused[HDSIZE-15];         /* unused space */
  struct attribute hd_attrs[NATTRS]; /* table of attributes */
};

struct relation {
  char rl_name[RNSIZE]; /* relation name */
  int rl_store;         /* flag indicating a store happened */
  int rl_fd;            /* file descriptor for relation file */
  int rl_scnref;        /* number of scans for this relation */
  struct header rl_header;  /* relation file header block */
  struct relation *rl_next; /* pointer to next relation */
};

struct scan {
  struct relation *sc_relation; /* ptr to relation definition */
  long sc_recpos;               /* tuple position in data file */
  unsigned int sc_nattrs;       /* no. of attrs in relation */
  unsigned int sc_dtnum;        /* desired tuple number */
  unsigned int sc_atnum;        /* actual tuple number */
  int sc_store;                 /* flag indicating a store */
  char *sc_tuple;               /* tuple buffer */
};

struct sattr {
  char *sa_rname;            /* relation name */
  char *sa_aname;            /* attribute name */
  char *sa_name;             /* alternate attribute name */
  char *sa_aptr;             /* ptr to attr in tuple buffer */
  struct srel *sa_srel;      /* ptr to the selected relation */
  struct attribute *sa_attr; /* attribute structure ptr */
  struct sattr *sa_next;     /* next selected attr in list */
};

struct sel {
  struct srel *sl_rels;         /* selected relations */
  struct sattr *sl_attrs;       /* selected attributes */
  struct binding *sl_bindings;  /* user variable bindings */
};

struct srel {
  char *sr_name;         /* alternate relation name */
  struct scan *sr_scan;  /* relation scan structure ptr */
  int sr_ctuple;         /* current fetched tuple flag */
  int sr_update;         /* updated tuple flag */
  struct srel *sr_next;  /* next selected relation in list */
};

struct unattrs {
  char *name;   /* attr name participating in a union  */
  short size;   /* size of attribute in bytes */
};

/* ------------------- forward declarations ----------------- */

extern struct sel *retrieve();
