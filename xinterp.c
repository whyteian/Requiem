

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


/* REQUIEM - boolean expression evaluator, instruction code 
 * is transformed into postfix notation and is placed on stack */

#include "requ.h"

extern union code_cell code[];  /* instruction code array */

extern int wh_signal;           /* selection predicate signal */
struct operand *setup_res();
double atof();
static struct operand *stack[STACKMAX], /* stack */
                      **stack_ptr;      /* stack pointer */
static union code_cell *pc;  /* program cntr during execution */
struct symbol *symb = NULL;  /* rudimentary symbol table */
BOOL xint_error = FALSE;     /* interpretation error variable */
char *s_copy();

/*----------------------------------------------------------- */

/* interpret a boolean expression */
int xinterpret()
{
  struct operand *result;
  BOOL res;
  
  /* check for empty code clause */
  if (wh_signal == FALSE)
    return (TRUE);
  
  /* set up stack with stack pointer stack_ptr */
  stack_ptr = stack;
  /* start executing */
  execute();
  /* get the result from the top of stack */
  if (!xint_error) { /* if no errors, temporary args exist */
    result = *--stack_ptr;
    res = result->o_value.ov_boolean;
    
    if ((result->o_type == TEMPBOOL) || 
	                   (result->o_type == TEMPNUM))
      free_opr(result);
  }
  else res = FALSE;
  
  /* make sure the stack is empty */
  while (stack_ptr != stack) {
    if (((*stack_ptr)->o_type == TEMPBOOL) || 
	               ((*stack_ptr)->o_type == TEMPNUM))
      free((char *) *stack_ptr);
    stack_ptr--;
  }
  
  /* return result */
  return (res);
}

static int execute()  /* execute the code operators */
{
  for (pc = code; (*(*pc++).c_operator)(); )
    ;
}

/*----------------------------------------------------------- */

int xstop()
{
  return (FALSE);
}

static push(oprnd)
     struct operand *oprnd;
{ 
  if (stack_ptr+1 >= &stack[STACKMAX]) {
    xint_error = TRUE;
    return(error(STACKOVRFL));
  }
  *stack_ptr++ = oprnd;
  return(TRUE);
}

int xpush()
{
  return(push(((*pc++).c_operand)));
}

static struct operand *pop()
{  
  if (stack_ptr-1 < stack) {
    xint_error = TRUE;
    return(NULL);
  } else 
    return(*--stack_ptr);
}

int xand()
{
  return (boolean('&'));
}

int xor()
{
  return (boolean('|'));
}

/*----------------------------------------------------------- */

static int boolean(opr)
{
  struct operand *lval,*rval,*result;
  int lv,rv,r;
  
  if ((rval = pop()) == NULL)
    return(error(STACKUNDFL));
  if ((lval = pop()) == NULL)
    return(error(STACKUNDFL));
  lv = lval->o_value.ov_boolean;
  rv = rval->o_value.ov_boolean;
  
  if (!(result = setup_res(BOOLEAN))){
    xint_error = TRUE;
    return(error(INSMEM));
  }
  
  switch (opr) {
  case '&':   
    r = (lv && rv);
    break;
  case '|':   
    r = (lv || rv);
    break;
  }
  result->o_value.ov_boolean = r;
  push(result); /* push result onto stack */
  if ((lval->o_type == TEMPBOOL) || (lval->o_type == TEMPNUM))
    free_opr(lval);
  if ((rval->o_type == TEMPBOOL) || (rval->o_type == TEMPNUM))
    free_opr(rval);
  return (TRUE);
}

/*----------------------------------------------------------- */

int xnot()
{
  struct operand *val,*result;
  
  if ((val = pop()) == NULL)
    return(error(STACKUNDFL));
  
  if (!(result = setup_res(BOOLEAN))) {
    xint_error = TRUE;
    return(error(INSMEM));
  }
  result->o_value.ov_boolean = !val->o_value.ov_boolean;
  push(result); /* push result onto stack */
  
  /* if temporary result free operand structures */
  if ((val->o_type == TEMPBOOL) || (val->o_type == TEMPNUM))
    free_opr(val);
  return (TRUE);
}

/*----------------------------------------------------------- */

int xlss()
{
  return (compare(LSS));
}

int xleq()
{
  return (compare(LEQ));
}

int xpart()
{
  return (compare(PART));
}

int xeql()
{
  return (compare(EQL));
}

int xgeq()
{
  return (compare(GEQ));
}

int xgtr()
{
  return (compare(GTR));
}

int xneq()
{
  return (compare(NEQ));
}

/*----------------------------------------------------------- */

static int compare(cmp)
     int cmp;
{
  struct operand *lval,*rval,*result;
  int comp_res;  /* result of comparison */
  
  if (((rval = pop()) == NULL) || ((lval = pop()) == NULL)) {
    xint_error = TRUE;
    return(error(STACKUNDFL));
  }
  
  if (!(result = setup_res(BOOLEAN))){
    xint_error = TRUE;
    return(error(INSMEM));
  }
  
  if (cmp == PART) /* left oprnd must be same length as right */
    lval->o_value.ov_char.ovc_length =
                 rval->o_value.ov_char.ovc_length;           
  
  if (lval->o_value.ov_char.ovc_type == TCHAR) {
    if (rval->o_value.ov_char.ovc_type != TCHAR) {
      xint_error = TRUE;
      return(error(WRONGID));
    }
    comp_res = comp(lval,rval);
  }
  else {
    if (rval->o_value.ov_char.ovc_type == TCHAR) {
      xint_error = TRUE;
      return(error(WRONGID));
    }
    
    comp_res = ncomp(lval,rval);
  }
  
  switch (cmp) {
  case LSS:   
    comp_res = (comp_res < 0);
    break;
  case LEQ:   
    comp_res = (comp_res <= 0);
    break;
  case EQL:   
    comp_res = (comp_res == 0);
    break;
  case PART:   
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
  
  result->o_value.ov_boolean = comp_res;
  push(result); /* push result onto stack */
  
  /* if temporary results free operand structures */
  if ((lval->o_type == TEMPBOOL) || (lval->o_type == TEMPNUM))
    free_opr(lval);
  if ((rval->o_type == TEMPBOOL) || (rval->o_type == TEMPNUM))
    free_opr(rval);
  
  return (TRUE);
}

/*----------------------------------------------------------------- */

static int comp(lval,rval)    /* compare two string operands */
     struct operand *lval,*rval;
{
  char *l_string,*r_string;   /* char strings to be compared */
  int lcn, rcn;               /* length counters */
  int min_len;                /* minimum length */
  
  l_string = lval->o_value.ov_char.ovc_string;
  r_string = rval->o_value.ov_char.ovc_string;
  
  lcn = lval->o_value.ov_char.ovc_length;
  rcn = rval->o_value.ov_char.ovc_length;
  
  /* strip off blanks */
  while (lcn > 0 && (l_string[lcn-1] == EOS || 
		                    l_string[lcn-1] == 0))
    lcn--;
  while (rcn > 0 && (r_string[rcn-1] == EOS || 
		                    r_string[rcn-1] == 0))
    rcn--;
  
  /*  estimate minimum string length */
  min_len = MIN(lcn, rcn);
  
  while ((min_len--) > 0) {
    if (*l_string != *r_string)
      return((*l_string < *r_string) ? FIRSTLSS : FIRSTGTR);
    l_string++;  
    r_string++;
  }
  
  return((lcn == rcn) ? EQUAL : 
	           ((lcn < rcn) ? FIRSTLSS : FIRSTGTR));
}

/*----------------------------------------------------------- */

static int ncomp(lval,rval)
     struct operand *lval,*rval;
{
  char lstr[NUMBERMAX+1],rstr[NUMBERMAX+1];
  int len;
  double a1, a2;
  
  strncpy(lstr,lval->o_value.ov_char.ovc_string,
	  (len = lval->o_value.ov_char.ovc_length)); 
  lstr[len] = EOS;
  strncpy(rstr,rval->o_value.ov_char.ovc_string,
	  (len = rval->o_value.ov_char.ovc_length)); 
  rstr[len] = EOS;
  
  a1 = atof(lstr);  
  a2 = atof(rstr);
  
  return((a1 == a2) ? EQUAL : ((a1 < a2) ? FIRSTLSS : FIRSTGTR));
}

/*----------------------------------------------------------- */

int xadd()
{
  xdb_add(TRUE);
}

int xsub()
{
  xdb_add(FALSE);
}

/*----------------------------------------------------------- */

static xdb_add(op_add)
     BOOL op_add;
{
  struct operand *lval, *rval;
  double op1, op2, res;
  struct operand *result;
  char res_string[NUMBERMAX+1];
  
  /* get opearands, check for type mismatch */
  if (((rval = pop()) == NULL) || ((lval = pop()) == NULL)) {
    xint_error = TRUE;
    return(error(STACKUNDFL));
  }
  
  if ((rval->o_value.ov_char.ovc_type == TCHAR)
      ||   (lval->o_value.ov_char.ovc_type == TCHAR)
      ||   (rval->o_type == TEMPBOOL)
      ||   (lval->o_type == TEMPBOOL)) {
    xint_error = TRUE;
    return(error(WRONGID));
  }
  
  if (!(result = setup_res(NUMBER))){
    xint_error = TRUE;
    return(error(INSMEM));
  }
  
  op1 = atof(lval->o_value.ov_char.ovc_string);
  op2 = atof(rval->o_value.ov_char.ovc_string);
  if (op_add)  /* overflow is detected in main() - loop */
    res = op1 + op2;
  else
    res = op1 - op2;
  
  sprintf(res_string, "%.4lf", res);
  result->o_value.ov_char.ovc_string = s_copy(res_string);
  result->o_value.ov_char.ovc_type = TREAL;
  result->o_value.ov_char.ovc_length = strlen(res_string);
  
  push(result); /* push result onto stack */
  
  /* free space in case of intermediate results */
  if ((lval->o_type == TEMPBOOL) || (lval->o_type == TEMPNUM))
    free_opr(lval);
  if ((rval->o_type == TEMPBOOL) || (rval->o_type == TEMPNUM))
    free_opr(rval);
  
  return (TRUE);
} /* xdb_add */

/*----------------------------------------------------------- */

int xmul()
{
  printf("multiply\n");
}

int xdiv()
{
  printf("divide\n");
}

int xmod()
{
  printf("modulus\n");
}

/*----------------------------------------------------------- */

int xassign()
{
  
  struct operand *lval, *rval;
  
  /* get operands, check for type mismatch */
  if ((rval = pop()) == NULL)
    return(error(STACKUNDFL));
  if ((lval = pop()) == NULL)
    return(error(STACKUNDFL));
  if ((rval->o_value.ov_char.ovc_type == TCHAR)
      ||   (lval->o_value.ov_char.ovc_type == TCHAR)
      ||   (lval->o_type == TEMPBOOL)) {
    xint_error = TRUE;
    return(error(WRONGID));
  }
  
  if (symb == NULL) { /* if a var has not been encountered */
    if (!(symb = MALLOC(struct symbol)))
      return(error(INSMEM));
  }
  
  /* name of variable was contained in ovc_string */
  symb->varopd->o_value.ov_char.ovc_string = 
    s_copy(lval->o_value.ov_char.ovc_string);

  /* type of variable is always real */
  lval->o_value.ov_char.ovc_type = TREAL; 
  lval->o_value.ov_char.ovc_string = 
    rval->o_value.ov_char.ovc_string;
  lval->o_value.ov_char.ovc_length = 
    rval->o_value.ov_char.ovc_length;
  structcpy((char *) symb->varopd, 
	    (char *) lval, 
	    sizeof(struct operand));
  push(lval);
  return(TRUE);
}

/*----------------------------------------------------------- */

struct operand *setup_res(num) /* set up a result operand struct. */
     int num;
{
  register struct operand *result;
  
  if (!(result = MALLOC(struct operand)))
    return(NULL);
  
  if (num == NUMBER)
    result->o_type = TEMPNUM;
  else
    result->o_type = TEMPBOOL;
  
  return(result);
}

/*----------------------------------------------------------- */

