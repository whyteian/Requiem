

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


/* REQUIEM - lexical scanning functions */

#include "requ.h"

static char *pbuf = NULL;        /* pointer to input buffer */

extern BOOL view_operation;
extern BOOL wh_signal;
BOOL view_exists = FALSE;         /* view-existence flag */
char lexeme[2*STRINGMAX+1];       /* string token value */
int lex_token;                    /* current token mnemonic */
char *lptr;                       /* current command line ptr */
float lex_value;                  /* current token value */
int saved_ch;                     /* saved character */
int saved_tkn;                    /* saved token type */
struct cmd_file *lex_cfp;         /* command file context ptr */
struct view *views;               /* view definitions */
static char *iprompt,*cprompt;    /* input prompts */
static char cmdline[2*LINEMAX+1]; /* current command line */
static int line_start;            /* flag indicates line start */
extern struct cmd_file *temp_cfp; /* preserved command file context pointer */
static struct {                   /* keyword table */
  char *keyname;
  int keytoken;
} keywords[] = {         
  "all", ALL, 
  "as", AS,
  "assign", ASSIGN,
  "avg", AVG,
  "by", BY,
  "char", CHAR,
  "count", COUNT,
  "create", CREATE,
  "define", DEFINE,
  "delete", DELETE,
  "difference", DIFFERENCE,
  "display", DISPLAY,
  "drop", DROP,
  "exit", EXIT,
  "export", EXPORT,
  "expose", EXPOSE,
  "extract", EXTRACT,
  "foreign", FOREIGN,
  "focus", FOCUS,
  "from", FROM,
  "group", GROUP,
  "help", HELP,
  "insert", INSERT,
  "intersect", INTERSECT,
  "import", IMPORT,
  "in", IN,
  "into", INTO,
  "is", IS, 
  "join", JOIN,
  "key", KEY,
  "minm", MINM,
  "maxm", MAXM,
  "modify", MODIFY,
  "num", NUM,
  "on", ON,
  "over", OVER,
  "print", PRINT,
  "project", PROJECT,
  "purge", PURGE,
  "quit", QUIT,
  "real", REAL,
  "references", REFERENCES,
  "secondary", SECONDARY,
  "select", SELECT,
  "set", SET,
  "show", SHOW,
  "sum", SUM,
  "to", TO,
  "union", UNION,
  "unique", UNIQUE,
  "update", UPDATE,
  "using", USING,
  "view", VIEW,
  "with", WITH,
  "when", WHEN,
  "where", WHERE,
  NULL, 0,
};

/* ---------------------------------------------------------- */

/* s_init - initialize the scanner */
s_init()
{
  /* at beginning of line */
  line_start = TRUE;
  
  /* make the command line null */
  lptr = NULL;
  
  /* no lookahead yet */
  saved_ch = EOS;
  saved_tkn = 0;
  
  /* no indirect command files */
  lex_cfp = NULL;
  
  /* no views defined */
  views = NULL;
}

/* ---------------------------------------------------------- */

/* db_prompt(ip,cp) - initialize prompt strings */
db_prompt(ip,cp)
     char *ip,*cp;
{
  /* save initial and continuation prompt strings */
  iprompt = ip; /* initial prompt */
  cprompt = cp; /* continuation prompt */
}

/* ---------------------------------------------------------- */

/* q_scan(qfmt) - initiate line scan query parsing */
q_scan(qfmt)
     char *qfmt;
{  
  /* set up the command line */
  if (qfmt)
    if (strlen(qfmt) < LINEMAX)
      strcpy(cmdline, qfmt);
    else
      return(error(LINELONG));
  
  /* start at the beginning of the command line */
  lptr = cmdline;
  iprompt = NULL;

  if (! temp_cfp)
    temp_cfp = lex_cfp;      /* preserve old cf pointer */
  lex_cfp = NULL;
  
  /* no saved characters yet */
  saved_ch = EOS;
  saved_tkn = 0;

  return(TRUE);
}

/* ---------------------------------------------------------- */

/* db_flush - flush the current input line */
int db_flush()
{   
  if (view_operation) {
    cmd_clear();
    return(TRUE);
  }
  
  while (saved_ch != '\n')
    if (saved_ch > ' ') /* normal chars other than space exist */
      return (error(SYNTAX));
    else
      saved_ch = get_char();
  
  saved_ch = EOS;
  line_start = TRUE;
  return (TRUE);
} /* db_flush */

/* ---------------------------------------------------------- */

/* com_file - setup an indirect command file */
int com_file(fname)
     char *fname;
{    struct cmd_file *new_cfp;
     char *malloc();
     
     if ((new_cfp = (struct cmd_file *)
	      malloc(sizeof(struct cmd_file))) == NULL)
       return (error(INSMEM));
     else if ((new_cfp->cf_fp = fopen(fname,"r")) == NULL) {
       nfree((char *) new_cfp);
       return (error(INDFNF));
     }
     new_cfp->cf_text = NULL;
     new_cfp->cf_savech = saved_ch;
     new_cfp->cf_lptr = lptr;
     new_cfp->cf_next = lex_cfp;
     lex_cfp = new_cfp;
     
     /* return successfully */
     return (TRUE);
   }

/* ---------------------------------------------------------- */

/* cmd_kill - kill indirect command file environment */
cmd_kill()
{
  struct cmd_file *old_cfp;
  
  while ((old_cfp = lex_cfp) != NULL) {
    lex_cfp = old_cfp->cf_next;
    if (old_cfp->cf_fp != NULL)
      fclose(old_cfp->cf_fp);
    saved_ch = old_cfp->cf_savech;
    lptr = old_cfp->cf_lptr;
    nfree((char *) old_cfp);
  }
  
  cmd_clear();       /* clear the command line environment */
  
} /* cmd_kill */

/* ---------------------------------------------------------- */

/* clear a command line and set saved characters */
cmd_clear() 
{
  
  if (lptr){
    lptr = NULL;
    lex_token = ';'; 
    return(lex_token);
  } 
  else
    while (saved_ch != '\n')
      saved_ch = get_char();
  
  saved_ch = EOS;
  saved_tkn = 0;
  line_start = TRUE;
  
  return(TRUE);
} /* cmd_clear */

/* ---------------------------------------------------------- */

cmd_clear_nested()
{
  lptr = NULL;
  saved_tkn = 0;
  saved_ch = EOS;
} /* cmd_clear_nested */

/* ---------------------------------------------------------- */

/* next_token - get next token (after skipping the current) */
int next_token()
{
  /* get the current token */
  this_token();
  
  /* make sure another is read on next call */
  saved_tkn = 0;
  
  /* make sure that you exit parser in case of command line 
   * arguments */
  if (lex_token == NULL && lptr && *lptr == EOS) {
    lex_token = ';';
    lptr = NULL;
    
    if (view_operation == TRUE) {
      view_operation = FALSE;
      this_token();     /* clear cmd-line terminator */
      saved_tkn = 0;
    }
  }
  
  /* return the current token */
  return (lex_token);
} /* next_token */

/* ---------------------------------------------------------- */

/* this_token - return the current input token */
int this_token()
{
  struct view *view_ptr;
  struct cmd_file *new_cfp;
  char *malloc();
  
  /* find a token that's not a view call */
  while (token_type() == ID) {
    
    /* check for a view call */
    for (view_ptr = views; view_ptr != NULL; 
	 view_ptr = view_ptr->view_next)
      if (strcmp(lexeme, view_ptr->view_name) == EQUAL) {
	/* view name */
	if ((new_cfp = 
	     (struct cmd_file *)malloc(sizeof(struct cmd_file)))
	    == NULL)
	  printf("*** error expanding view: %s ***\n",lexeme);
	else { /* load view environment to an icf structure */
	  new_cfp->cf_fp = NULL;
	  new_cfp->cf_text = view_ptr->view_text->txt_next;
	  new_cfp->cf_lptr = lptr; 
	  lptr = view_ptr->view_text->txt_text; 
	  /* point to view text */
	  new_cfp->cf_savech = saved_ch; saved_ch = EOS;
	  new_cfp->cf_next = lex_cfp;   /* link icf structure */
	  lex_cfp = new_cfp;
	  view_exists = TRUE;            /* view called */
	}
	saved_tkn = 0;
	break;
      }
    
    if (view_ptr == NULL)
      break;
  }
  
  return (lex_token);
} /* this_token */

/* ---------------------------------------------------------- */

/* token_type - return the current input token */
int token_type()
{
  int ch;
  
  /* check for a saved token */
  if ((lex_token = saved_tkn))
    return (lex_token);
  
  /* get the next non-blank character */
  ch = skip_blanks();
  
  /* check type of character */
  if (isalpha(ch))             /* identifier or keyword */
    get_id();
  else if (is_sign(ch)) {      /* sign */
    if (!wh_signal)
      get_signum();
    else get_predopd();        /* part of a logical expresion */
  }
  else if (isdigit(ch))        /* number */
    get_number();
  else if (ch == '"')          /* string */
    get_string();
  else if (get_predopd())      /* relational operator */
    ;
  else                          /* single character token */
    lex_token = next_char();
  
  /* save the type of the token */
  saved_tkn = lex_token;
  
  /* return the token */
  return (lex_token);
} /* token_type*/

/* ---------------------------------------------------------- */

/* get_id - get a keyword or a user identifier */
static get_id()
{
  int ch,nchars,i;
  char *fold();
  
  /* input letters and digits */
  ch = skip_blanks();
  nchars = 0;
  while (isalpha(ch) || isdigit(ch)) {
    if (nchars < KEYWORDMAX)
      lexeme[nchars++] = ch;
    
    next_char(); ch = look_ahead();
  }
  
  /* terminate the keyword */
  lexeme[nchars] = EOS;
  
  /* assume it's an identifier */
  lex_token = ID;
  if (isupper(*lexeme))
    strcpy(lexeme, fold(lexeme));
  
  /* check for keywords */
  for (i = 0; keywords[i].keyname != NULL; i++)
    if (strcmp(lexeme,keywords[i].keyname) == EQUAL)
      lex_token = keywords[i].keytoken;
  
} /* get_id */

/* ---------------------------------------------------------- */

static get_signum()
{
  int ch,nchars;
  
  /* input letters and digits */
  ch = skip_blanks();
  nchars = 0;
  
  while (is_sign(ch) || isdigit(ch)) {
    if (nchars && is_sign(ch))
      return(error(SYNTAX));
    
    if (nchars < NUMBERMAX)
      lexeme[nchars++] = ch;
    /* clear previous character, and prepare to receive
       the next one */
    next_char(); ch = look_ahead();
  }
  
  /* terminate the keyword */
  lexeme[nchars] = EOS;
  
  /* assume it's an identifier */
  lex_token = SIGNEDNO; /* token is a signed number */
  
  return(TRUE);
} /* get_signum */

/* ---------------------------------------------------------- */

/* get_number - get an integer or a  float */
static get_number()
{
  int ch, ndigits, no_real;
  
  /* read digits and at most one decimal point */
  ch = skip_blanks();
  ndigits = 0; no_real = TRUE;
  while (isdigit(ch) || (no_real && ch == '.')) {
    if (ch == '.')
      no_real = FALSE;
    if (ndigits < NUMBERMAX)
      lexeme[ndigits++] = ch;
    next_char();         /* this combination clears character */
    ch = look_ahead();   /* in savetch and reads next char */
  }
  
  /* terminate the number */
  lexeme[ndigits] = EOS;
  
  /* get the value of the number or real */
  stf(lexeme, &lex_value); 
  
  if (no_real) 
    lex_token = NUMBER; /* token is an integer */
  else 
    lex_token = REALNO;  /* token is real*/
  
  
} /* get_number */ 

/* ---------------------------------------------------------- */

/* get_string - get a string */
static get_string()
{
  int ch,nchars;
  
  /* skip the opening quote */
  next_char();
  
  /* read characters until a closing quote is found */
  ch = look_ahead();
  nchars = 0;
  while (ch && ch != '"') {
    if (nchars < STRINGMAX)
      lexeme[nchars++] = ch;
    next_char(); ch = look_ahead();
  }
  
  /* terminate the string */
  lexeme[nchars] = EOS;
  
  /* skip the closing quote */
  next_char();
  
  /* token is a string */
  lex_token = STRING;
} /* get_string */

/* ---------------------------------------------------------- */

/* get_predopd - get a predicate operator */
static int get_predopd()
{
  int ch;
  
  switch (ch = skip_blanks()) { /* get first non-blank char */
  case '=':
    next_char();  ch = skip_blanks();
    if (ch == '=') { /* definition of partial comparison */
      next_char();
      lex_token = PART;
      return(TRUE);
    }
    else
      lex_token = EQL; /* definition of equal  */
    return (TRUE); 
  case '<':
    next_char(); ch = skip_blanks();
    if (ch == '>') { /* definition of non-equal is <> */
      next_char();
      lex_token = NEQ;
    }
    else if (ch == '=') { /* definition of less equal is <= */ 
      next_char();
      lex_token = LEQ;
    }
    else
      lex_token = LSS;
    return (TRUE);
  case '>':
    next_char(); ch = skip_blanks();
    if (ch == '=') { /* definition of greater equal is >= */
      next_char();
      lex_token = GEQ;
    }
    else
      lex_token = GTR; /* definition of greater */
    return (TRUE);
  case ':':
    next_char(); ch = skip_blanks();
    if (ch == '=') { /* definition of assignment */
      next_char();
      lex_token = ASGN;
      return(TRUE);
    }
    else return(FALSE);
    
  case '+':
    next_char();
    lex_token = ADD; /* definition of add  */
    return (TRUE); 
  case '-':
    next_char();
    lex_token = SUB; /* definition of subtract  */
    return (TRUE); 
  case '*':
    next_char();
    lex_token = MUL; /* definition of multiply  */
    return (TRUE); 
  case '/':
    next_char();
    lex_token = DIV; /* definition of divide  */
    return (TRUE); 
  case '%':
    next_char();
    lex_token = MOD; /* definition of modulus  */
    return (TRUE); 
  default:
    return(FALSE);  /* no other relational ops */
  }
} /* get_predopd */

/* ---------------------------------------------------------- */

/* skip_blanks - get the next non-blank character */
static int skip_blanks()
{
  int ch;
  
  /* skip blank characters */
  while ((ch = look_ahead()) <= ' ' && ch != EOS)
    next_char();
  
  /* return the first non-blank */
  return (ch);
} /* skip_blanks */

/* ---------------------------------------------------------- */

/* look_ahead - get the current lookahead character */
static int look_ahead()
{
  /* get a lookahead character */
  if (saved_ch == EOS)
    saved_ch = next_char();
  
  /* if a chracter is already saved return it else
   * return new lookahead character */
  return (saved_ch);
  
} /* look_ahead */

/* ---------------------------------------------------------- */

/* next_char - get the next character */
static int next_char()
{
  char fname[STRINGMAX+1];
  int ch,i;
  
  /* return the lookahead character if there is one */
  if (saved_ch != EOS) {
    ch = saved_ch;
    saved_ch = EOS;
    return (ch);
  }
  
  /* get a character */
  ch = get_char();
  
  /* skip spaces at the beginning of a command */
  if (line_start && iprompt != NULL) 
    while (ch <= ' ')
      ch = get_char();
  
  /* use continuation prompt next time */
  iprompt = NULL;
  
  /* check for indirect command file */
  while (ch == '@') {
    for (i = 0; (saved_ch = get_char()) > ' '; )
      if (i < STRINGMAX)
	fname[i++] = saved_ch;
    fname[i] = EOS;
    if (com_file(fname) != TRUE)
      printf("*** error opening command file: %s ***\n",fname);
    ch = get_char();
  }
  
  /* return the character */
  return (ch);
} /* next_char */

/* ---------------------------------------------------------- */

/* get_char - get the current character */

static int get_char()
{
  struct cmd_file *old_cfp;
  int ch;
  
  /* check for input from command line */
  if (lptr != NULL) { 
    while (*lptr == EOS && lex_cfp)   /* if there is an icf */
      if (lex_cfp->cf_text == NULL) { /* consumed after lptr */
	old_cfp = lex_cfp;
	ch = lex_cfp->cf_savech; saved_ch = EOS;
	lptr = lex_cfp->cf_lptr;    /* get value of icf lptr */
	
	/* reset icf context pointer to point to next
	   cmd_file entry in list */ 
	lex_cfp = lex_cfp->cf_next;
	nfree((char *) old_cfp);
	if (ch != EOS)
	  return (ch);
	if (lptr == NULL)
	  break;
      }
      else {   /* view text processing */
	lptr = lex_cfp->cf_text->txt_text; /* point to view text */
	/* point to continuation of view text */
	lex_cfp->cf_text = lex_cfp->cf_text->txt_next;
      }
    
    if (lptr != NULL && *lptr)
      return (*lptr++);
    else if (*lptr == EOS)
      return(EOS);
  }
  
  /* print prompt if necessary */
  if (line_start && lex_cfp == NULL && temp_cfp == NULL)  {  
    if (iprompt != NULL)
      printf("%s",iprompt);
    else if (cprompt != NULL)
      printf("%s",cprompt);
  } 
  
  if (lex_cfp == NULL && temp_cfp == NULL) { /* read stdin */
    if ((ch = in_chars(stdin)) == NEWLINE)
      line_start = TRUE; /* signal begining of newline */
    else
      line_start = FALSE;
  } else if (lex_cfp) {  /* read indirect command file */
    if ((ch = in_chars(lex_cfp->cf_fp)) == EOF) {
      old_cfp = lex_cfp;
      ch = lex_cfp->cf_savech; saved_ch = EOS;
      lptr = lex_cfp->cf_lptr;
      lex_cfp = lex_cfp->cf_next;
      fclose(old_cfp->cf_fp);
      nfree((char *) old_cfp);
    }
  } else if (temp_cfp) {  /* return from canned query to icf */
    ch = NEWLINE;
  }
    
  /* return the character */
  return (ch);
} /* get_char */

/* ---------------------------------------------------------- */

/* input characters from file or standard input */
int in_chars(fp)
     FILE *fp;
{
  static char buf[LINEMAX] = {EOS};
  int ch, i;

  if (! pbuf)
    pbuf = buf;
  
  if (fp!=stdin)
    if ((ch = getc(fp)) == NEWLINE) /* place in char from file */
      return getc(fp);   /* if NEWLINE get char in next line */
    else
      return ch;
  
  if (*pbuf > 0)
    return *pbuf++;
  
  pbuf = buf;
  for (i = 0; (ch = getc(fp)) != EOF; )
    if (i < LINEMAX)  {
      buf[i++] = ch;     /* fill buffer with on-line chars */
      if (ch == NEWLINE)    break;
    }
    else {
      printf("*** line too long ***\nRetype> ");
      i = 0;
    }
  buf[i] = EOS;
  return in_chars(fp);
} /* in_chars */

/* ---------------------------------------------------------- */

push_back(c)
     char c;
{
  if (! pbuf)
    return(FALSE);

  if (*pbuf) {
    *--pbuf = c;
    return(TRUE);
  } else 
    return(FALSE);
} /* push_back */

/* ---------------------------------------------------------- */

/* get_line - get a line from the current input;
 *              to be used in conjunction with views */
char *get_line(buf)
     char *buf;
{
  int ch, i;
  
  for (i= 0; (ch = next_char()) != '\n' && ch != EOF; )
    if (i < LINEMAX)
      buf[i++] = ch;
    else {
      printf("******* line too long ********\nRetype> ");
      i = 0;
    }
  buf[i] = EOS;
  
  return(buf);
}

/* ---------------------------------------------------------- */

/* this function is used with views, it simply checks for the 
 * existence of a view exactly as in the case of this_token */
int nxtoken_type()
{
  /* get the current token */
  token_type();
  
  /* make sure another is read on next call */
  saved_tkn = 0;
  
  /* return the current token */
  return (lex_token);
}

/* ---------------------------------------------------------- */

clear_prompts()
{
  cprompt = NULL;
  iprompt = NULL;
}

/* ---------------------------------------------------------- */
