

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

/* store - store tuples */
int store(slptr, key_buf)
     struct sel *slptr;
     char **key_buf;
{
  struct srel *srptr;
  
  /* check each selected relation for stores */
  for (srptr = slptr->sl_rels; srptr != NULL; srptr = SREL_NEXT)
    if (srptr->sr_update) /* with indexing, if required */
      if (!store_tuple(srptr->sr_scan, key_buf))
	return (FALSE);
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */
/* bind - bind a user buffer to the value of an attribute */
int requiem_bind(slptr,rname,aname,avalue)
     struct sel *slptr; char *rname,*aname,*avalue;
{
  struct binding *newbd;
  struct srel *srptr;
  char *malloc();
  
  /* allocate and initialize a binding structure */
  if ((newbd = 
      (struct binding *)malloc(sizeof(struct binding))) == NULL)
    return (error(INSMEM));
  newbd->bd_vuser = avalue;
  
  /* find the attribute */
  if (!find_attr(slptr,rname,aname,
		&newbd->bd_vtuple,&srptr,&newbd->bd_attr))
    return (FALSE);
  
  /* link the new binding into the binding list */
  newbd->bd_next = slptr->sl_bindings;
  slptr->sl_bindings = newbd;
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */
/* get - get the value of an attribute */
int get(slptr,rname,aname,avalue)
     struct sel *slptr; char *rname,*aname,*avalue;
{
  struct srel *srptr;
  struct attribute *aptr;
  char *vptr;
  
  /* find the attribute */
  if (!find_attr(slptr,rname,aname,&vptr,&srptr,&aptr))
    return (FALSE);
  
  /* get the attribute value */
  get_attr(aptr,vptr,avalue);
  
  /* return successfully */
  return (TRUE);
}

/* ---------------------------------------------------------- */
/* put - put the value of an attribute */
int put(slptr,rname,aname,avalue)
     struct sel *slptr; char *rname,*aname,*avalue;
{
  struct srel *srptr;
  struct attribute *aptr;
  char *vptr;
  
  /* find the attribute */
  if (!find_attr(slptr,rname,aname,&vptr,&srptr,&aptr))
    return (FALSE);
  
  /* put the attribute value */
  store_attr(aptr,vptr,avalue);
  
  /* mark the tuple as updated */
  srptr->sr_update = TRUE;
  
  /* return successfully */
  return (TRUE);
}
/* ---------------------------------------------------------- */
