

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


/* REQUIEM - definition file */
#include <stdio.h>
#include <ctype.h> 

#ifndef MAC
#include <fcntl.h>
#endif

#define SNAPSHOT "snap"

/*-----------------------------------------------------------
  |                       definitions                       |
  ----------------------------------------------------------- */

#define BOOL       int
#define DQUOTE     '"'
#define DELETION   -2
#define EOL       '\n' /* define end of line */
#define EQUAL       0 
#define FALSE       0
#define FAIL       -1
#define ILLEGAL    -2
#define FIRSTGTR    1
#define FIRSTLSS   -1
#define HTSIZE     1009 /* must be prime to hash */
#define INDEX       0
#define IO_ERROR   -2   /* returned if user enters wrong data */
#define NEWLINE   '\n'  /* define carriage return line feed */
#define NO_ERROR    0   /* error free computation */
#define NOINDEX     1
#define TERMINATOR ';'  /* RQL sentence terminator */
#define SUCCESS     2
#define SLASH      '/'
#define TRUE        1
#define VOID       int

/* ----------------------------------------------------------
   |              external function  definitions            |
   ---------------------------------------------------------- */

#ifndef MAC
extern char *rmalloc();
extern char *malloc();
extern char *calloc();
extern char *calloc();
extern char *sprintf();
extern char *strcat();
extern char *strcpy();
#endif
extern char **dcalloc();

/* ----------------------------------------------------------
   |                   macro definitions                    |
   ---------------------------------------------------------- */

#define is_alpha(ch) ((((ch) >= 'A' && (ch) <= 'Z') ||\
		       ((ch) >= 'a' && (ch) <= 'z')) ? 1 : 0)
#define is_digit(x) ((x>='0' && x<='9') ? 1 : 0)
#define is_blank(x) ((x==' ') ? 1 : 0)
#define to_decimal(x) (x-'0')
#define to_upper(ch) (((ch) >= 'a' && (ch) <= 'z')?\
		      (ch) - 'a' + 'A' : (ch))
#define put_back(ch)   (put_back_char = (ch))
#define to_lower(ch) (((ch) >= 'A' && (ch) <= 'Z') ?\
		      (ch) - 'A' + 'a' : (ch))
#define BELL           putchar(7)
#ifndef MAX
#define MAX(x, y)      (((x) >= (y)) ? (x) : (y)) 
#endif
#ifndef MIN
#define MIN(x, y)      (((x) <= (y)) ? (x) : (y)) 
#endif
#define is_sign(x)      ((x == '-' || x == '+') ? 1 : 0)
#define MALLOC(x)       ((x *)malloc(sizeof(x)))
#define CALLOC(n, x)    ((x *)calloc((unsigned) n, sizeof(x)))
  
  /* --------------------------------------------------------
     | The following macros simplify the inline code,       |
     | they prevent having to type complex constructs like: |
     | slptr->sl_rels->sr_scan                              |
     | this would be written as:  sel_scan                  |
     -------------------------------------------------------- */
  
#define ATTRIBUTE_KEY   saptr->sa_attr->at_key
#define ATTRIBUTE_SIZE  saptr->sa_attr->at_size
#define ATTRIBUTE_TYPE  saptr->sa_attr->at_type
#define OPERAND_LENGTH  c_operand->o_value.ov_char.ovc_length
#define OPERAND_STRING  c_operand->o_value.ov_char.ovc_string
#define OPERAND_TYPE    c_operand->o_value.ov_char.ovc_type
#define RELATION_HEADER rptr->rl_header
#define SCAN_HEADER     sptr->sc_relation->rl_header
#define SCAN_RELATION   sptr->sc_relation
#define SCAN_TUPLE      sptr->sc_tuple
#define SEL_ATTRS       slptr->sl_attrs
#define SEL_HEADER      slptr->sl_rels->sr_scan->sc_relation->rl_header
#define SEL_NATTRS      slptr->sl_rels->sr_scan->sc_nattrs
#define SEL_RELATION    slptr->sl_rels->sr_scan->sc_relation
#define SEL_RELS        slptr->sl_rels
#define SEL_SCAN        slptr->sl_rels->sr_scan
#define SEL_TUPLE       slptr->sl_rels->sr_scan->sc_tuple
#define SREL_HEADER     srptr->sr_scan->sc_relation->rl_header
#define SREL_NEXT       srptr->sr_next
#define SREL_RELATION   srptr->sr_scan->sc_relation
  
  /* --------------------------------------------------------
     |                   program limits                     |
     -------------------------------------------------------- */
  
#define CODEMAX    100     /* maximum length of code array */
#define KEYWORDMAX 12      /* maximum keyword length */
#define LINEMAX    132     /* maximum input line length */
#define NUMBERMAX  30      /* maximum number length */
#define STACKMAX   20      /* maximum interpreter stack size */
#define STRINGMAX  132     /* maximum string length */
#define TABLEMAX   132     /* maximum table output line */
  
  /* --------------------------------------------------------
     |                  token definitions                   |
     -------------------------------------------------------- */
  
#define EOS            '\0'
#define LSS             -1
#define LEQ             -2
#define EQL             -3
#define NEQ             -4
#define GEQ             -5
#define PART            -6
#define GTR             -7
#define ADD             -8
#define SUB             -9
#define MUL             -10
#define DIV             -11
#define MOD             -12
#define INSERT          -13
#define CHAR            -14
#define NUM             -15
#define ID              -16
#define STRING          -17
#define NUMBER          -18
#define UPDATE          -19
#define PRINT           -20
#define IMPORT          -21
#define EXPORT          -22
#define INTO            -23
#define VIEW            -24
#define FOREIGN         -25
#define EXTRACT         -26
#define DEFINE          -27
#define SHOW            -28
#define USING           -29
#define SORT            -30
#define BY              -31
#define KEY             -32
#define REFERENCES      -33
#define SET             -34
#define REAL            -35
#define REALNO          -36
#define ALL             -37
#define OVER            -38
#define PROJECT         -39
#define MODIFY          -40
#define DROP            -41
#define JOIN            -42
#define ON              -43
#define WITH            -44
#define SIGNEDNO        -45
#define UNION           -46
#define INTERSECT       -47
#define DIFFERENCE      -48
#define DISPLAY         -49
#define FOCUS           -50
#define AS              -51
#define HELP            -52
#define SELECT          -53
#define FROM            -54
#define WHERE           -55
#define CREATE          -56
#define DELETE          -57
#define ASGN            -58
#define IN              -59
#define UNIQUE          -60
#define SECONDARY       -61
#define BOOLEAN         -62
#define COUNT           -63
#define GROUP           -64
#define AVG             -65
#define MAXM            -66
#define MINM            -67
#define SUM             -68
#define ASSIGN          -69
#define TO              -70
#define PURGE           -71
#define EXIT            -72
#define QUIT            -73
#define IS              -74
#define WHEN            -75
#define EXPOSE          -76
  
  /* --------------------------------------------------------
     |            operand type definitions                  |
     -------------------------------------------------------- */
  
#define LITERAL     1
#define ATTR        2
#define TEMPBOOL    3
#define TEMPNUM     4
#define VAR         5
  
  /* --------------------------------------------------------
     |            attribute type  definitions               |
     -------------------------------------------------------- */
  
#define TCHAR   1
#define TNUM    2
#define TREAL   3
  
  /* --------------------------------------------------------
     |             tuple status code definitions            |
     -------------------------------------------------------- */
  
#define ACTIVE  1
#define DELETED 0
  
  /* --------------------------------------------------------
     |     relation header & page format definitions        |
     -------------------------------------------------------- */
  
#define ASIZE       16      /* size of an attribute entry */
#define ANSIZE      10      /* size of an attribute name */
#define HEADER_SIZE 512     /* size of relation header */
#define HDSIZE      16      /* size of a relation entry */
#define NATTRS      31      /* number of attributes in header block */
#define QUATTRSIZE  21      /* qualified attribute size */
#define RNSIZE      15      /* size of a relation name */
  
  /* --------------------------------------------------------
     |              error code definitions                  |
     -------------------------------------------------------- */
  
#define END        0  /* end of retrieval set */
#define INSMEM     1  /* insufficient memory */
#define RELFNF     2  /* relation file not found */
#define BADHDR     3  /* bad relation file header */
#define TUPINP     4  /* tuple input error */
#define TUPOUT     5  /* tuple output error */
#define RELFUL     6  /* relation file full */
#define RELCRE     7  /* error creating relation file */
#define DUPATT     8  /* duplicate attribute on relation create */
#define MAXATT     9  /* too many attributes on relation create */
#define INSBLK     10 /* insufficient disk blocks */
#define SYNTAX     11 /* command syntax error */
#define ATUNDF     12 /* attribute name undefined */
#define ATAMBG     13 /* attribute name ambiguous */
#define RLUNDF     14 /* relation name undefined */
#define CDSIZE     15 /* boolean expression code too big */
#define INPFNF     16 /* input file not found */
#define OUTCRE     17 /* output file creation error */
#define INDFNF     18 /* indirect command file not found */
#define BADSET     19 /* bad set parameter */
#define HASHSIZ    20 /* hash table size exceeded */
#define UNOP       21 /* union operand not allowed */
#define UNLINK     22 /* error in removing files */
#define LINK       23 /* error in renaming files */
#define WRLEN      24 /* wrong length of identifier */
#define BADCURS    25 /* cursor mispositioned */
#define WRONGID    26 /* wrong identifier specified */
#define UNDEFVAR   27 /* undefined variable */
#define STACKOVRFL 28 /* stack overflow */
#define STACKUNDFL 29 /* stack underflow */
#define INCNST     30 /* inconsistent info. */
#define INDXCRE    31 /* index creation error */
#define NOKEY      32 /* key attribute not specified */
#define IXFLNF     33 /* index file not found */
#define IXFAIL     34 /* indexing failed */
#define DUPTUP     35 /* duplicate tuple */
#define NOEXSTUP   36 /* non-existent tuple */
#define IXATRNF    37 /* indexed attribute not found */
#define IXDELFAIL  38 /* index delete failed */
#define KEYEXCD    39 /* key length exceeded */ 
#define WRIXCOM    40 /* write index error */
#define WRPREDID   41 /* wrong predicate id */
#define AGGRPARAM  42 /* wrong aggregate function parameter */
#define FILEMV     43 /* error in file transfer */
#define WRNRELREM  44 /* non-existent relation file */
#define UNDEFTYPE  45 /* undefined attribute type */
#define LINELONG   46 /* command line too long */
#define UPDTERR    47 /* error in updating */
#define NOALIAS    48 /* no aliases allowed */
#define NJOIN      49 /* join operation failed */
#define QUATTR     50 /* define qualified attribute is needed */
#define UNATTR     51 /* inaplicable attribute name */
#define MAX5       52 /* maximum no. of attributes exceeded */
#define NESTED     53 /* error in nested command */
#define VIEWDEF    54 /* wrong view definition */
#define VIEWDEL    55 /* wrong view deletion */
#define UNIQEXCD   56 /* no. of unique keys exceeded */
#define TOOMANY    57 /* too many indexed attributes */
#define WRNGFGNKEY 58 /* wrong foreign key */
#define WRNGCOMB   59 /* wrong foreign key combination */
#define WRNGINS    60 /* wrong insertion operation */
#define ILLREL     61 /* illegal relation name */ 
#define ILLJOIN    62 /* unmatched attribute names in join */
#define INPERR     63 /* erroneous input data */

  /* --------------------------------------------------------
     |                structure definitions                 |
     -------------------------------------------------------- */
  
  /* ------------- command & query structures --------------- */
  
struct attribute { /* static boundary: cater for alignment */
  short at_size;             /* attribute size in bytes */
  char  at_name[ANSIZE+1];   /* attribute name */
  char  at_type;             /* attribute type */
  char  at_key;              /* attribute key */
  char  at_semid;	/* semantically identical attribute */
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
  char rl_name[RNSIZE+1]; /* relation name */
  int rl_store;           /* flag indicating a store happened */
  int rl_fd;              /* file descriptor for relation file */
  int rl_scnref;          /* number of scans for this relation */
  struct header rl_header;  /* relation file header block */
  struct relation *rl_next; /* pointer to next relation */
};

struct scan {
  struct relation *sc_relation; /* ptr to relation definition */
  long sc_recpos;               /* tuple-position in datafile */
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

/* --------------code evaluation structures ---------------- */

union code_cell {
  int (*c_operator)();
  struct operand *c_operand;
};

struct operand {             
  int o_type;           /* type ,i.e., ATTR, VAR, etc. */
  union  {
    struct {
      int ovc_type;     /* type of attribute, i.e., char, int */
      char *ovc_string; /* attribute value */
      int ovc_length;   /* length of attribute */
    } ov_char;
    int ov_boolean;     /* result of a logical operation */
  } o_value;
};

struct symbol {
  char varname[ANSIZE+1];
  struct operand *varopd;
};

/* -------------- pgm interface structures ------------------ */

struct binding {
  struct attribute *bd_attr;  /* bound attribute */
  char *bd_vtuple;            /* pointer to value in tuple */
  char *bd_vuser;             /* pointer to user buffer */
  struct binding *bd_next;    /* next binding */
};

/* ---------------- view & icf structures ------------------- */

struct cmd_file {
  FILE *cf_fp;
  struct text *cf_text;
  int cf_savech;
  char *cf_lptr;
  struct cmd_file *cf_next;
};

struct text {
  char *txt_text;
  struct text *txt_next;
};


struct view {
  char *view_name;
  struct text *view_text;
  struct view *view_next;
};

/* -------------- transient file structures ----------------- */

struct trans_file {
  char t_file[RNSIZE+5];
  struct trans_file *t_next;
};

/* --------------- hash & indexing structures --------------- */

struct hash {
  char      *value;       /* token value to be hashed */
  char      *relation;    /*  relation token  */
  int       dup_tcnt;     /* no of duplicate entries */
  double    aggr_param;   /* aggregate function parameter */
};

struct pred {
  char *pr_rlname;         /* name of rel containing index */
  char **pr_ixtype;        /* type of index */
  char **pr_atname;        /* attr name of indexed attr */
  struct operand **pr_opd; /* operand struct of indexed attr */
};

struct skey {
  int sk_type;
  struct attribute *sk_aptr;
  int sk_start;
  struct skey *sk_next;
};

/* ---------------------------------------------------------- */



