

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


/* REQUIEM - view manipulation file */

#include "requ.h"

extern char lexeme[];
extern char cformat[];
extern struct view *views;
extern int saved_ch;
extern BOOL wh_signal;

extern char  *strip_blank(), *strip_semicol();
BOOL  view_operation = FALSE;
extern char *cprompt;
extern struct  cmd_file *lex_cfp,*temp_cfp;   /* command file context ptrs */

/* --------------------- define a view ---------------------- */
int     view_define()
{
  struct view *view_ptr = NULL;
  struct text *texts, *txt_ptr, *txt_last, *tex_def;
  char     textline[LINEMAX+1], creator[20], perms[10];
  char base[RNSIZE+1], *tex_buf, **args, **dcalloc();
  char *path(), *status();
  int arg_index, loc, length = 0, tbase = 0, toffset = 0;
  char *get_line();
  BOOL temp_wh_signal = wh_signal;

  if (nxtoken_type() != VIEW)
    return (error(SYNTAX));
  
  /* get view name */
  if (nxtoken_type() != ID)
    return (error(SYNTAX));
  
  /* if view name exists as relation exit */
  if (fexists(lexeme))
    return (error(VIEWDEF));
  
  /* find the view in the view table */
  for (view_ptr = views; view_ptr != NULL;
                 view_ptr = view_ptr->view_next)
    if (strcmp(view_ptr->view_name, lexeme) == EQUAL) {
      printf("\t Drop view to redefine it !\n");
      cmd_clear();
      return(TRUE);
    }
  
  /* allocate and initialize a view structure */
  if ((view_ptr = 
       (struct view *) malloc(sizeof(struct view ))) == NULL)
    return (error(INSMEM));
  if ((view_ptr->view_name = 
                 rmalloc(strlen(lexeme) + 1)) == NULL) {
    nfree((char *) view_ptr);
    return (error(INSMEM));
  }
  strcpy(view_ptr->view_name, lexeme);
  view_ptr->view_text = NULL;
  
  if (nxtoken_type() != AS)
    return (error(SYNTAX));
  
  /* make sure that the rest of the line is blank */
  if (!db_flush())
    return (FALSE);
  
  /* setup  prompt strings */
  db_prompt((char *) 0, "VIEW-DEFINITION: ");
  
  /* get definition text */
  for (txt_last = NULL; ; txt_last = txt_ptr) {
    
    if (txt_last != NULL)
     printf("If view definition complete press <return> to exit\n");
    /* get a line */
    get_line(textline);
    
    if (textline[0] == EOS || textline[0] == '\n')
      break;
    
    /* allocate a view text structure */
    if ((txt_ptr = 
	 (struct text *)malloc(sizeof(struct text ))) == NULL) {
      view_free(view_ptr);
      return (error(INSMEM));
    }
    
    if ((txt_ptr->txt_text = 
	 rmalloc(strlen(textline) + 1)) == NULL) {
      view_free(view_ptr);
      return (error(INSMEM));
    }
    
    if (txt_last == NULL)
      strcpy(txt_ptr->txt_text, textline);
    else
      sprintf(txt_ptr->txt_text, " %s", textline);
    
    txt_ptr->txt_next = NULL;
    
    /* link it into the view list */
    if (txt_last == NULL)
      view_ptr->view_text = txt_ptr;
    else /* catenate continuation of view text */
      txt_last->txt_next = txt_ptr;
  }
  
  /* check syntactic correctness of view */
  for (tex_def = view_ptr->view_text; tex_def != NULL;
       tex_def = tex_def->txt_next)
    length += strlen(tex_def->txt_text)+1;
  
  if ((tex_buf = 
       (char *) calloc(1, (unsigned) length + 2)) == NULL) {
    view_free(view_ptr);
    return (error(INSMEM));
  }
  
  if (strlen(tex_buf) > LINEMAX) {
    view_free(view_ptr);
    cmd_clear();
    return (error(LINELONG));
  }
  
  for (tex_def = view_ptr->view_text; tex_def != NULL;
       tex_def = tex_def->txt_next) {
    strcat(tex_buf, " ");
    strcat(tex_buf, tex_def->txt_text);
  }
  
  clear_prompts();

  if (!parse(tex_buf, TRUE, FALSE)) {
    printf("syntactically wrong view definition\n");
    nfree(tex_buf);
    view_free(view_ptr);
    saved_ch = NEWLINE;   /* force a newline */
    return(FALSE);
  }
  else {
    printf("View definition syntactically O.K\n");
    saved_ch = NEWLINE;   /* force a newline */
  }
  
  /* link the new view into the view list */
  if (txt_last == NULL) {
    view_free(view_ptr);
    return(TRUE);
  }
  else {
    view_ptr->view_next = views;
    views = view_ptr;
  }
  
  /* insert catalog arguments in argument array */
  arg_index = 5;
  args = dcalloc(arg_index);
  args[0] = view_ptr->view_name;
  args[1] = base;
  args[2] = rmalloc(LINEMAX+1);
  
  /* if long text expected */
  if (view_ptr->view_text->txt_next != NULL) {
    for (texts = view_ptr->view_text; texts != NULL; 
	                      texts = texts->txt_next) {
      toffset += tbase;
      strip_blank(args[2] + toffset, 
		  texts->txt_text, 
		  strlen(texts->txt_text), &tbase);
    }
    *(args[2]+strlen(args[2])) = EOS;
  }
  else
    strcpy(args[2],  view_ptr->view_text->txt_text);
  
  /* find base name of a view */
  if (strncmp("project", args[2], 7) == EQUAL) {
    loc = 7;
    get_base(base, &loc, args[2]);
  }
  else if (strncmp("select", args[2], 6) == EQUAL) {
    loc = right_index(args[2], "from");  /* find "from" */
    get_base(base, &loc, args[2]);
  }
  else /* more than one base names are involved */
    strcpy(base, "none");
  
  /* insert sysview catalog entry, three args */
  if (!cat_insert("sysview", args, 3)) {
    nfree(args[2]); 
    nfree((char *) args);
    return(FALSE);
  }
  
  status(base, perms);
  path(creator);
  
  args[1] = creator;
  args[2] = perms;
  args[3] = base;
  args[4] =  (char *) calloc(1, 2);
  *(args[4]) = '0';
  *(args[4]+1) = EOS;
  
  /* insert sysview catalog entry, five args */
  if (!cat_insert("systab", args, arg_index)) {
    nfree(args);
    return(FALSE);
  }
  
  nfree(tex_buf);
  nfree(args);
  cmd_clear(); /*  clear command line */
  
  wh_signal = temp_wh_signal;

  /* return successfully */
  return (TRUE);
  
} /* view_define */

/* ---------------------------------------------------------- */

/* show a view */
int     view_show()
{
  struct view *view_ptr;
  struct text *txt_ptr;
  
  if (nxtoken_type() != VIEW)
    return (error(SYNTAX));
  
  /* get view name */
  if (nxtoken_type() != ID)
    return (error(SYNTAX));
  
  /* find the view in the view table */
  for (view_ptr = views; view_ptr != NULL; 
                         view_ptr = view_ptr->view_next)
    if (strcmp(view_ptr->view_name, lexeme) == EQUAL) {
      printf("VIEW-EXPANSION: ");
      for (txt_ptr = view_ptr->view_text; txt_ptr != NULL; 
	   txt_ptr = txt_ptr->txt_next)
	printf("\t%s\n", txt_ptr->txt_text);
      break;
    }
  
  /* check for successful search */
  if (view_ptr == NULL)
    printf("*** no view named: %s ***\n", lexeme);
  
  /* return successfully */
  return (TRUE);
  
} /* view_show */

/* ---------------------------------------------------------- */

/* free a view definition */
view_free(view_ptr)
     struct view *view_ptr;
{
  struct text *txt_ptr;
  
  while ((txt_ptr = view_ptr->view_text) != NULL) {
    view_ptr->view_text = txt_ptr->txt_next;
    nfree(txt_ptr->txt_text);
    nfree((char *) txt_ptr);
  }
  nfree(view_ptr->view_name);
  nfree((char *) view_ptr);
} /* view_free */

/* ---------------------------------------------------------- */

int load_views()
{
  struct sel *retrieve();
  struct sel *slptr;
  struct sattr *saptr;
  struct view *view_ptr = NULL;
  struct text *txt_ptr = NULL;
  char     texbuf[LINEMAX+1], nbuf[15];
  int     tcnt, i;
  BOOL temp_wh_signal = wh_signal;
  
  if ((slptr = retrieve("sysview over viewname, text")) == NULL)
    return(FALSE);
  else
    cmd_kill(); /* to clear command line */
  
  for (tcnt = 0; fetch(slptr, TRUE); tcnt++) {
    /* create the tuple from the selected attributes */
    for (saptr = SEL_ATTRS; saptr != NULL; saptr = saptr->sa_next) {
      for (i = 0; i < ATTRIBUTE_SIZE; i++) {
	if (strcmp(saptr->sa_aname, "viewname") == EQUAL)
	  nbuf[i] = saptr->sa_aptr[i];
	else
	  texbuf[i] =  saptr->sa_aptr[i];
      }
    }
    
    /* allocate and initialize a view structure */
    if ((view_ptr = 
	(struct view *) malloc(sizeof(struct view ))) == NULL) {
      done(slptr);
      return (error(INSMEM));
    }

    if ((view_ptr->view_name = 
	 calloc(1, (unsigned) strlen(nbuf) + 1)) == NULL) {
      nfree((char *) view_ptr);
      done(slptr);
      return (error(INSMEM));
    }
    
    strcpy(view_ptr->view_name, nbuf);
    view_ptr->view_text = NULL;
    
    /* get definition text, allocate a view text structure */
    if ((txt_ptr = 
	 (struct text *)malloc(sizeof(struct text ))) == NULL) {
      view_free(view_ptr);
      done(slptr);
      return (error(INSMEM));
    }
    
    if ((txt_ptr->txt_text = 
	 rmalloc(strlen(texbuf) + 1)) == NULL) {
      view_free(view_ptr);
      done(slptr);
      return (error(INSMEM));
    }
    strcpy(txt_ptr->txt_text, texbuf);
    txt_ptr->txt_next = NULL;
    
    /* link it into the view list */
    view_ptr->view_text = txt_ptr;
    
    /* link the new view into the view list */
    view_ptr->view_next = views;
    views = view_ptr;
  }
  
  db_rclose(SEL_SCAN);  /* close file */
  
  if (tcnt)
    printf("LOADING: %d views loaded\n", tcnt);

  wh_signal = temp_wh_signal;
  
  return(TRUE);
} /* load_view */

/* ----------------- drop a defined view -------------------- */
int drop()
{  
  char buf[65], prompt[65], response[5];
  struct view *view_ptr, *view_last;
  char *getfld();
  BOOL temp_wh_signal = wh_signal;

  if (nxtoken_type() != VIEW)
    return(error(SYNTAX));
  
  if (nxtoken_type() != ID)
    return(error(SYNTAX));
  
  sprintf(buf, "delete sysview where viewname = \"%s\"",lexeme);
  sprintf(prompt, "Commit changes ? (<yes> or <no>)\t: ");
  getfld(prompt, response, 4, stdin);
  
  if (strcmp(response, "yes") == EQUAL) {
    
    /* find the view in the view table and free it */
    for (view_ptr = views, view_last = NULL; view_ptr != NULL; 
	 view_last = view_ptr, view_ptr = view_ptr->view_next)
      if (strcmp(view_ptr->view_name, lexeme) == EQUAL) {
	if (view_last == NULL)
	  views = view_ptr->view_next;
	else
	  view_last->view_next = view_ptr->view_next;
	
	view_free(view_ptr);
	break;
      }
    
    /* delete the sysview entry first, then the systab entry */
    if (!parse(buf, FALSE, FALSE))
      return(error(VIEWDEL));
    
    sprintf(buf, "delete systab where relname = \"%s\"",lexeme);
    
    if (!parse(buf, FALSE, FALSE))
      return(error(VIEWDEL));
    
    sprintf(cformat, "#  view %s definition deleted  #", lexeme);
  }
  wh_signal = temp_wh_signal;

  return(TRUE);
} /* drop */

/* ---------------------------------------------------------- */
struct sel *delete_view()
{
  char *pt_error();
  struct view *view_ptr;
  struct sel *slptr, *retrieve();
  char base[15], predicate[LINEMAX+1], fmt[2*LINEMAX+1];
  int loc;        /* location where predicate lies */
  BOOL  found = FALSE;
  BOOL temp_wh_signal = wh_signal;
  
  /* get the  view name */
  if (nxtoken_type() != ID)
    return((struct sel *)pt_error(SYNTAX));
  
  for (view_ptr = views; view_ptr != (struct view *)NULL; 
                              view_ptr = view_ptr->view_next)
    if (strcmp(view_ptr->view_name, lexeme) == EQUAL) {
      if (!view_specs(view_ptr, base, predicate, &loc))
	return((struct sel *)NULL);
      found = TRUE;
      break;
    }
  
  if (found == FALSE)
    return((struct sel *)pt_error(SYNTAX));
  
  if (next_token() != WHERE)
    return((struct sel *)pt_error(SYNTAX));
  
  /* signify a view operation */
  view_operation = TRUE;
  
  if (loc != -1) /* there exists a "where" clause in view */
    sprintf(fmt, " %s where %s & ", base, predicate + loc);
  else
    sprintf(fmt, "%s where", base);
  
  if ((slptr = retrieve(fmt)) == (struct sel *) NULL)
    return((struct sel *)pt_error(VIEWDEL));

  wh_signal = temp_wh_signal;

  if (temp_cfp) {
    lex_cfp = temp_cfp;           /* restore cf pointer */
    temp_cfp = NULL;
  }
  

  return(slptr);
} /* delete_view */

/* ----------- find view and base relation name ------------- */
which_view(vname)
     char *vname;
{
  char fmt[STRINGMAX];
  struct sel *slptr, *retrieve();
  struct sattr *saptr;
  BOOL temp_wh_signal = wh_signal;
  
  if (find_view(vname)) {
    sprintf(fmt, "systab over relname, deriv");
    
    if ((slptr = retrieve(fmt)) == NULL)
      return(FAIL);
    else
      cmd_kill();   /* clear command line */
    
    while (fetch(slptr, TRUE)) {
      saptr = SEL_ATTRS;
      if (strcmp(saptr->sa_aptr, vname) == EQUAL)
	break;
    }

    /* copy the base relation name */
    strcpy(vname, saptr->sa_next->sa_aptr);

    done(slptr);
    
    if (strcmp(vname, "base") != EQUAL) {
      wh_signal = temp_wh_signal;
      if (temp_cfp) {
	lex_cfp = temp_cfp;           /* restore cf pointer */
	temp_cfp = NULL;
      }
  
      return(TRUE);
    }
  }
  
  return(FALSE);
  
} /* which_view */
/* ---------------------------------------------------------- */

BOOL find_view(view_name)
     char *view_name;
{    struct view *view_ptr;
     BOOL view_found = FALSE;
     
     for (view_ptr = views; view_ptr != (struct view *)NULL; 
	                        view_ptr = view_ptr->view_next)
       if (strcmp(view_ptr->view_name, view_name) == EQUAL) {
	 view_found = TRUE;
	 break;
       }
     return(view_found);
   } /* find_view */
/* --------------- get view specifications ------------------ */
static int  view_specs(view_ptr, base, predicate, loc)
     struct view *view_ptr;
     char *base, *predicate;
     int *loc;
{      
  int  toffset = 0, tbase = 0;
  
  struct text *texts;
  
  /* if long text expected */
  if (view_ptr->view_text->txt_next != NULL) {
    
    for (texts = view_ptr->view_text; texts != NULL; 
	                        texts = texts->txt_next) {
      toffset += tbase;
      strip_semicol(predicate + toffset, texts->txt_text, 
		              strlen(texts->txt_text), &tbase);
    }
    *(predicate+strlen(predicate)) = EOS;
  }
  else
    cpy_nosemi(predicate,  view_ptr->view_text->txt_text);
  
  /* find base name of a view */
  if (strncmp("select", predicate, 6) == EQUAL) {
    
    *loc = right_index(predicate, "from");  /* find "from" */
    
    /* get the base name */
    get_base(base, loc, predicate);
    *loc = right_index(predicate, "where");  /* find "where" */
    
    /* skip blanks after "where" */
    if (*loc != -1)
      while (is_blank(*(predicate + *loc)))
	(*loc)++;
    
  }
  else { /* more than one base names are involved */
    printf("Unaccepted operation on view specified\n");
    return(FALSE);
  }
  
  return(TRUE);
  
}  /* view_specifics */

/* ---------------------------------------------------------- */
int view_stat()
{
  struct view *view_ptr;
  char  predicate[LINEMAX+1], fmt[2*LINEMAX+1], base[ANSIZE+1];
  int tkn, loc;        /* location where predicate lies */
  BOOL  found = FALSE;
  BOOL where_flag = FALSE;
  
  /* get the  view name */
  if (token_type() != ID)
    return(FALSE);
  
  for (view_ptr = views; view_ptr != (struct view *)NULL; 
       view_ptr = view_ptr->view_next)
    if (strcmp(view_ptr->view_name, lexeme) == EQUAL) {
      if (!view_specs(view_ptr, base, predicate, &loc))
	return(FAIL);
      found = TRUE;
      break;
    }
  
  if (found == FALSE)
    return(FALSE);
  else
    nxtoken_type();        /* clear view name */
  
  /* get next standard input token */
  tkn = this_token();
  
 if (tkn == WHERE)
    where_flag = TRUE;
  else if (tkn != ';')
    return(FAIL);

  if (tkn == ';')   /* pushback delimiter */
    push_back(tkn);

  /* signify a view operation */
  view_operation = TRUE;
  
  if (loc != -1 && where_flag) /* there is a "where" clause */
    sprintf(fmt, " %s where %s & ", base, predicate + loc);
  else if (loc != -1 && where_flag == FALSE)
    sprintf(fmt, "%s where %s", base, predicate + loc);
  else
    sprintf(fmt, "%s", base);
  
  q_scan(fmt);
  
  return(TRUE);
} /* view_stat */

/* ---------------------------------------------------------- */

get_base(base, loc, predicate)
     char *base, *predicate;
     int *loc;
{   
  int i;
  
  /* skip blanks after "from" */
  while (is_blank(*(predicate + *loc)))
    (*loc)++;
  
  /* find base name */
  for (i = 0; isalnum(*(predicate + *loc))  && i < ANSIZE; (*loc)++, i++)
    base[i] = *(predicate + *loc);   /* copy base name */
  base[i] = EOS;
  
} /* get_base */

/* ---------------------------------------------------------- */
