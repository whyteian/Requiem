

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


/* REQUIEM - expression compiler
   syntax: eliminates left recursion.
   
   <expression> ::= <primary> { <logop> <primary> }
   <primary>    ::= <statement> | <assgnt> <relop> <simple-expr>
   <statement>  ::= <simple-expr> { <relop> <simple-expr> }
   <assignment> ::= '(' <variable> ':=' <simple-expr> ')'
   <simple-expr>::= <term> { <addop> <term> }
   <term>       ::= <factor> { <mulop> <factor> }
   <factor>     ::= <operand>|'('<expression>')'|<nop> <factor> 
   <operand>    ::= <number>|<string>|<attribute>|<variable> 
   <variable>   ::= '$'<chars>
   <attribute>  ::= [<rname>.] <aname>
   <number>     ::= <digit> | <number> <digit>
   <string>     ::= '"' <chars> '"'
   <chars>      ::= nil | <chars> { <character> }
   <nop>        ::= '~'
   <logop>      ::= '&' | '|'
   <relop>      ::= '=' | '<>' | '<' | '>' | '<=' | '>=' | '=='
   <addop>      ::= '+' | '-' 
   <mulop>      ::= '*' | '/' | '%'
   */	

#include "requ.h"

extern int lex_token;            /* current token value */
extern char lexeme[];            /* string token value */
extern BOOL xint_error;          /* variable to report 
			          * interpretation errors */
extern struct symbol *symb;      /* symbol table for vars */
struct operand *get_opdstr();
union code_cell code[CODEMAX+1]; /* code array */
static struct sel *selptr;       /* saves sel-struct ptr */
static int cndx;                 /* code array index */
static char varname[NUMBERMAX+1];/* stores temp var name */
BOOL non_ixattr = FALSE;         /* non-indexed attr in pred */

/* interpretation operators */
extern int xand();
extern int xor();
extern int xnot();
extern int xlss();
extern int xleq();
extern int xeql();
extern int xgeq();
extern int xgtr();
extern int xneq();
extern int xpart();
extern int xadd();
extern int xsub();
extern int xmul();
extern int xdiv();
extern int xmod();
extern int xpush();
extern int xstop();
extern int xassign();

/* ---------------------------------------------------------- */

/* xcompile - compile a boolean expression */
int xcompile(slptr)
     struct sel *slptr;
{
  /* save selection struc. ptr, initialize code array index */
  selptr = slptr;
  cndx = 0;
  
  /* parse the boolean expression */
  if (!expression()) {
    code[cndx++].c_operator = xstop;
    free_lits(code);
    return (FALSE);
  }
  
  /* stop code execution by placing xstop as last instruction */
  code[cndx++].c_operator = xstop;
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* operator - insert an operator into the code array */
static int operator(opr)
     int (*opr)(); /* pointer to opr function */
{
  /* insert the operator */
  if (cndx < CODEMAX)
    code[cndx++].c_operator = opr; /* called thro' code array */
  else
    return (error(CDSIZE));
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* operand - insert an operand into the code array */
static int operand(opr)
     struct operand *opr;
{
  /* insert the push operator */
  if (!operator(xpush))
    return (FALSE);
  
  /* insert the operand */
  if (cndx < CODEMAX)
    code[cndx++].c_operand = opr;
  else
    return (error(CDSIZE));
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* expr - compile an expression */
static int expression()
{       int c;
	
	if (!primary())
	  return (FALSE);
	
        while ((c = this_token()) == '|' || c == '&') {
	  next_token();
	  if (!primary())
            return (FALSE);
	  switch(c) {
	  case '|':        if (!operator(xor))
	    return (FALSE);
	    break;
	  case '&':        if (!operator(xand))
	    return (FALSE);
	    break;
	  default:         return(error(SYNTAX));
	  }
	}
	
	return (TRUE);
      }

/* ---------------------------------------------------------- */

static int primary()
{
  int tkn;
  
  if (this_token() == '(') {
    next_token();
    if (this_token() != '$') {
      if (!expression())
	return(error(SYNTAX));
      else {
	if (next_token() != ')')
	  return (error(SYNTAX));
      }
      return(TRUE);
    }
    
    next_token();  /* strip off the $ */
    
    if (!assign())   /* left operand */
      return(FALSE);
    
    if (next_token() != ')')
      return (error(SYNTAX));
    
    /* check for relational operation */
    if (this_token() <= LSS && lex_token >= GTR) {
      tkn = next_token();
      if (!simple())    /* right operand */
	return (FALSE);
      
      if (!relop(tkn))  /* comparison operator */
	return(FALSE);
      
    } else return(error(SYNTAX));
    
  }
  else {
    if (!statement())  /* left operand */
      return (FALSE);
    
  }
  return (TRUE);
}

/* ---------------------------------------------------------- */

static int statement()
{
  int tkn;  BOOL rel_opr = FALSE;
  
  if (!simple())  /* left operand */
    return (FALSE);
  while (this_token() <= LSS && lex_token >= GTR) {
    tkn = next_token();
    if (!simple())  /* right operand */
      return (FALSE);
    
    if (!relop(tkn)) /* comparison operator */
      return(FALSE);
    
    rel_opr = TRUE;
  }
  
  if (rel_opr == FALSE)
    if (!in_list())
      return(FALSE);
  
  return (TRUE);
}

/* ---------------------------------------------------------- */

static int relop(tkn)
     int tkn;
{
  switch (tkn) {
  case LSS:
    if (!operator(xlss))
      return (FALSE);
    break;
  case LEQ:
    if (!operator(xleq))
      return (FALSE);
    break;
  case EQL:
    if (!operator(xeql))
      return (FALSE);
    break;
  case PART:
    if (!operator(xpart))
      return (FALSE);
    break;
  case NEQ:
    if (!operator(xneq))
      return (FALSE);
    break;
  case GEQ:
    if (!operator(xgeq))
      return (FALSE);
    break;
  case GTR:
    if (!operator(xgtr))
      return (FALSE);
    break;
  }
  
  return(TRUE);
  
} /* relop */		      

/* ---------------------------------------------------------- */

static int factor()
{
  if (this_token() == '('){
    next_token();
    if (!expression())
      return(FALSE);
    if (next_token() != ')')
      return(error(SYNTAX));
  }
  else if (this_token() == '~') {
    next_token(); /* same as this_token, but clears lookahead char */
    if (!factor())
      return (FALSE);
    if (!operator(xnot))
      return (FALSE);
  }
  else if (!get_operand())
    return (FALSE);
  
  return (TRUE);
}

/* ---------------------------------------------------------- */

static int assign()
{
  struct operand *opr;
  char *s_copy();
  
  if (next_token() != ID)
    return(error(SYNTAX));
  
  /* save the variable name */
  strncpy(varname,lexeme,MIN(strlen(lexeme), NUMBERMAX));
  varname[MIN(strlen(lexeme), NUMBERMAX)] = EOS;
  
  /* get a new operand structure */
  if (!(opr = get_opdstr()))
    return(error(INSMEM));
  
  /* initialize the new operand structure */
  opr->o_type = VAR;
  opr->o_value.ov_char.ovc_type = TREAL;
  opr->o_value.ov_char.ovc_string = s_copy(varname);
  
  /* insert the operand into the code array */
  if (!operand(opr)) {
    free_opr(opr);
    return (FALSE);
  }
  
  if (next_token() != ASGN)
    return(FALSE);
  
  if (!simple())
    return(FALSE);
  
  if (!operator(xassign))
    return(FALSE);
  
  return (TRUE);
}

/* ---------------------------------------------------------- */

static int simple()
{
  int tkn;
  
  if (!term())  /* left operand */
    return(FALSE);
  
  while(this_token() <= ADD && lex_token >= SUB) {
    tkn = next_token();
    if (!term())  /* right operand */
      return(FALSE);
    switch(tkn) {
    case ADD:
      if (!operator(xadd))
	return (FALSE);
      break;
    case SUB:
      if (!operator(xsub))
	return (FALSE);
      break;
    }
  }
  
  return(TRUE);
}

/* ---------------------------------------------------------- */

static int term()
{
  int tkn;
  
  if (!factor())  /* left operand */
    return(FALSE);
  
  while(this_token() <= MUL && lex_token >= MOD) {
    tkn = next_token();
    if (!factor())  /* right operand */
      return(FALSE);
    switch(tkn) {
    case MUL:
      if (!operator(xmul))
	return (FALSE);
      break;
    case DIV:
      if (!operator(xdiv))
	return (FALSE);
      break;
    case MOD:
      if (!operator(xmod))
	return (FALSE);
      break;
      
    }
  }
  
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* get_operand - get operand (number, string, or attribute) */
static int get_operand()
{
  /* determine operand type */
  if (next_token() == NUMBER)
    return (get_number(NUMBER)); /* result gets value TNUM */
  else if (lex_token == REALNO)
    return (get_number(REALNO)); /* result gets value TREAL */
  else if (lex_token == ID)
    return (xget_attr());        /* result gets value atype */
  else if (lex_token == STRING)
    return (get_string());       /* result gets value TCHAR */
  else if (lex_token == '$')
    return (get_variable());
  else
    return (error(SYNTAX));
}

/* ---------------------------------------------------------- */

/* xget_attr - get an attribute argument */
static int xget_attr()
{
  struct operand *opr;
  struct relation *rlptr;
  char rname[RNSIZE+1], aname[ANSIZE+1];
  char *val_ptr;
  BOOL secnd_rel = FALSE;    /* second relation flag */
  int atype,alen;
  int i, fail_ix = 0;
  short n_attrs;
  
  /* save the attribute name */
  strncpy(aname, lexeme, MIN(strlen(lexeme), ANSIZE));
  aname[MIN(strlen(lexeme), ANSIZE)] = EOS;
  
  /* get a new operand structure */
  if (!(opr = get_opdstr()))
    return(error(INSMEM));
  
  /* not a var, check for "." indicating a qual. attr name */
  if (this_token() == '.') { /* all join queries take this path */
    next_token();
    
    /* the previous ID was really a relation name */
    strcpy(rname, aname);
    
    /* check for the real attribute name */
    if (next_token() != ID)
      return (error(SYNTAX));
    
    /* save the attribute name */
    strncpy(aname,lexeme,MIN(strlen(lexeme), ANSIZE));
    aname[MIN(strlen(lexeme), ANSIZE)] = EOS;
    
    /* lookup the attribute name */
    if (!select_attr(selptr, rname, aname, &atype, &val_ptr, &alen))
      return (FALSE);
  }
  else  /* not qualified atribute */
    if (!select_attr(selptr, (char *) 0, aname, &atype,
		                             &val_ptr,&alen))
      return (FALSE);
  
  /* initialize the new operand structure */
  opr->o_type = ATTR;
  opr->o_value.ov_char.ovc_type = atype; 
  rlptr = selptr->sl_rels->sr_scan->sc_relation;     
  n_attrs = selptr->sl_rels->sr_scan->sc_nattrs;
  
  /* in case of join we must make sure that the second relation
   *  structure is also accessed and a new indexed environment is laid out */
  if (rlptr->rl_next &&strcmp(rname,rlptr->rl_name) != EQUAL) {
    rlptr = rlptr->rl_next;
    n_attrs =  selptr->sl_rels->sr_next->sr_scan->sc_nattrs;
    secnd_rel = TRUE;     /* second relation in query */
  }
  
  /* loop through the attributes of the sel structure */
  for (i = 0; i < n_attrs ;  i++) {
    
    /* compare the predicate name with the  attribute names */
    if (strcmp(aname, rlptr->rl_header.hd_attrs[i].at_name)
	                                                == EQUAL)
      break;
    
  }
  
  /* set-up environment for indexed attributes */
  if (rlptr->rl_header.hd_attrs[i].at_key != 'n')  
    fail_ix = get_ixattr(secnd_rel, rlptr, &opr, 
			 rlptr->rl_header.hd_attrs[i].at_name,
			 rlptr->rl_header.hd_attrs[i].at_key, 
			 alen);

  /* if indexed environment already set-up, or non-indexed attrs */
  if (fail_ix == FAIL ||
      rlptr->rl_header.hd_attrs[i].at_key == 'n')  {
    opr->o_value.ov_char.ovc_string = val_ptr;
    opr->o_value.ov_char.ovc_length = alen;
    non_ixattr = TRUE;   /* non-indexed atribute in predicate */
  }
  
  /* insert the operand into the code array */
  if (!operand(opr)) {
    free_opr(opr);
    return (FALSE);
  }
  
  /* return successfully */
  return (TRUE);
} /* xget_attr */

/* ---------------------------------------------------------- */
 
/* get_variable - get a variable argument */
static int get_variable()
{
  struct operand *opr;
  char name[ANSIZE+1];
  
  if (next_token() != ID)
    return(error(SYNTAX));
  
  /* save the attribute name */
  strncpy(name,lexeme,MIN(strlen(lexeme), ANSIZE));
  name[MIN(strlen(lexeme), ANSIZE)] = EOS;
  
  if ((strcmp(name, varname)) == EQUAL) {
    if (symb == NULL) {
      if (!(symb = MALLOC(struct symbol)))
	return(error(INSMEM));
      if (!(symb->varopd = MALLOC(struct operand)))
	return(error(INSMEM));
    }
    opr = symb->varopd;
    /* insert the operand into the code array */
    if (!operand(opr)) {
      free_opr(opr);
      return (FALSE);
    }
    
    return(TRUE);
  } else 
    return(error(UNDEFVAR));
}

/* ---------------------------------------------------------- */

/* get_number - get a numeric operand */
static int get_number(num)
     int num; /* descriminates between integers and reals */
{
  struct operand *opr;
  
  /* get a new operand structure */
  if (!(opr = get_opdstr()))
    return(error(INSMEM));
  
  /* initialize the new operand structure */
  opr->o_type = LITERAL;
  if ((opr->o_value.ov_char.ovc_string =
       rmalloc(strlen(lexeme)+1)) == NULL) {
    free_opr(opr);
    return (error(INSMEM));
  }
  
  /* operand type is either number or real */
  if (num == NUMBER)
    opr->o_value.ov_char.ovc_type = TNUM;
  else
    opr->o_value.ov_char.ovc_type = TREAL;
  
  strcpy(opr->o_value.ov_char.ovc_string,lexeme);
  opr->o_value.ov_char.ovc_length = strlen(lexeme);
  
  /* insert the operand into the code array */
  if (!operand(opr)) {
    free_opr(opr);
    return (FALSE);
  }
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

/* get_string - get a string operand */
static int get_string()
{
  struct operand *opr;
  
  /* get a new operand structure */
  if (!(opr = get_opdstr()))
    return(error(INSMEM));
  
  /* initialize the new operand structure */
  opr->o_type = LITERAL;
  if ((opr->o_value.ov_char.ovc_string =
       rmalloc(strlen(lexeme)+1)) == NULL) {
    free_opr(opr);
    return (error(INSMEM));
  }
  
  /* operand type is character */
  opr->o_value.ov_char.ovc_type = TCHAR;
  strcpy(opr->o_value.ov_char.ovc_string,lexeme);
  opr->o_value.ov_char.ovc_length = strlen(lexeme);
  
  /* insert the operand into the code array */
  if (!operand(opr)) {
    free_opr(opr);
    return (FALSE);
  }
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */

static int in_list()
{
  int num, tkn, cntr = 0;
  struct operand *opr;
  
  if (next_token() != IN)
    return(error(SYNTAX));
  
  if (next_token() == '(') {
    while ((num = next_token()) == NUMBER || num == REALNO) {
      
      cntr++;   /* increment loop counter, and get number */
      get_number(num);
      
      if ((tkn = next_token()) == ',' || tkn == ')') {
	
	/* test for equality */
	if (!operator(xeql))
	  return(FALSE);
	
	/* wait until both arguments of xor are present */
	if (cntr > 1) {
	  if (!operator(xor))
	    return(FALSE);
	}
	
	if (tkn == ')')
	  break;
      } else
	return(error(SYNTAX));
      
      /* if more numbers to come insert operand in code array*/
      if  ((num = this_token()) == NUMBER || num == REALNO) {
	
	/* get a new operand structure */
	if (!(opr = get_opdstr()))
	  return(error(INSMEM));
	
	/* new operand is the attribute met before the "in" clause */
	opr =  code[1].c_operand;
	
	/* insert the operand into the code array */
	if (!operand(opr)) {
	  free_opr(opr);
	  return (FALSE);
	}
      } else 
	return(error(SYNTAX));
    }
  } 
  else 
    return(error(SYNTAX));
  
  if (cntr == 1)  /* need more than one argument in in_list */
    return(error(SYNTAX));
  else
    return(TRUE);
  
} /* in_list */

/* ---------------------------------------------------------- */

/* free_lits - free the literals in a code array */
free_lits(cptr)
     union code_cell *cptr;
{
  for (; (*cptr).c_operator != xstop; cptr++)
    if ((*cptr).c_operator == xpush )
      if ((*++cptr).c_operand->o_type == LITERAL)
	free((*cptr).OPERAND_STRING);
}

/* ---------------------------------------------------------- */

/* assign a new operand structure */
struct operand *get_opdstr()
{
  struct operand *opr;
  
  if (!(opr = MALLOC(struct operand)))
    return (NULL);
  else
    return(opr);
}

/* ---------------------------------------------------------- */

/* free an operand structure */
free_opr(opr)
     struct operand *opr;
{
  if (opr->o_type == LITERAL || opr->o_type == VAR)
    free(opr->o_value.ov_char.ovc_string);
  free((char *) opr);
}

/* ---------------------------------------------------------- */

