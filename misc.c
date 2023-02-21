

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


/*  Misc. procedures */

#include "requ.h"

#ifdef UNIX
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef MAC
#include <io.h>
#endif

#ifdef DOS
#include <sys/types.h>
#endif

static double avg_cnt, avg_sum;

extern BOOL   sys_flag;	/* true for system defined relations */
extern int sref_cntr;	
extern struct pred *prptr[];

/* free only if pointer is not NULL */
nfree(p)
     char *p;
{
  if (p)
    free(p);

} /* nfree */

/* ---------------------------------------------------------- */

char *strip_catenate(to, from, n)
     char *to, *from;
     int n;
{
  int i, j;
  
  for (i = 0, j = 0;  i < n; i++)
    if (from[i])
      to[j++] = from[i];
  return(to);
} /* strip_catenate */

/* ---------------------------------------------------------- */

char *rem_blanks(to, from, n)
     char *to, *from;
     int n;
{
  int i, j;
  
  i = j= 0;
  for (j = 0 ;  j < n && n> 0 ; j++)
    if (from[j] != ' ')
      to[i++] = from[j];
  to[i] = EOS;
  return(to);
} /* rem_blanks */

/* ---------------------------------------------------------- */

rm_trailblks(s)
     char *s;
{
  char *ch_ptr;
  
  for (ch_ptr = s + strlen(s); *ch_ptr == ' ' && ch_ptr > s; ch_ptr--)
    ;
  *(ch_ptr) = EOS;
} /* rm_trailblks */

/* ---------------------------------------------------------- */

rm_lead_blks(in_string, out_string)
     char *in_string, *out_string;
{    int i, j;

     i = j = 0;  

     while (in_string[i] == ' ' && i < strlen(in_string))
            i++;

     for (; i < strlen(in_string); )
        out_string[j++] = in_string[i++];

     out_string[j] = EOS;
} /* rm_lead_blks */

/* ---------------------------------------------------------- */

char *strip_blank(to, from, len, j)
     char *to, *from;
     int len, *j;
{
  int i;
  
  i = *j = 0;
  len++;   /* increment to get terminator character */
  for ( ;  len>0 ; len--)
    if (from[i])
      to[i++] = from[(*j)++];
    else { 
      if (from[i] == EOS)
	to[i++] = ' '; /* leave one blank between words */
      
      while (from[(*j)++] == ' ')
	;
      to[i++] = ' '; /* leave one blank between words */
    }
  
  return(to);
} /* strip_blank */

/* ---------------------------------------------------------- */

char *strip_semicol(to, from, len, j)
     char *to, *from;
     int len, *j;
{
  int i;
  
  i = *j = 0;
  len++;   /* increment to get terminator character */
  for ( ;  len>0 ; len--)
    if (from[i] && from[i] != ';')
      to[i++] = from[(*j)++];
    else { 
      if (from[i] == EOS || from[i] == ';')
	to[i++] = ' '; /* leave one blank between words */
      
      while (from[(*j)++] == ' ')
	;
      to[i++] = ' '; /* leave one blank between words */
    }
  
  return(to);
} /* strip_semicol */

/* ---------------------------------------------------------- */

/* ---------------------------------------------------------- */

/* copy structures by bytes */
structcpy(to, from, length)
     char *to, *from;
     int length;
{
  if (length <= 0)
    return(FALSE);
  
  bcopy(from, to, length);

  return(TRUE);
} /* structcpy */

/* ---------------------------------------------------------- */

char **dcalloc(n)
     int n;
{
  char **p, *calloc();
  
  p = CALLOC(n, char *);
  if (p)
    return(p);
  else 
    return(NULL);
} /* dcalloc */

/* ---------------------------------------------------------- */

char *fold(str1)
     char *str1;
{
  char *str;
  int i;
  
  str = malloc((unsigned) strlen(str1)+1);
  
  for (i = 0; str1[i] != EOS; i++) 
    if (is_alpha(str1[i]))
      str[i] = to_lower(str1[i]);
  
  str[i] = EOS;
  
  return(str);
} /* fold */

/* ---------------------------------------------------------- */

char *unfold(str1)
     char *str1;
{
  char *str;
  int i;
  
  str = calloc(1, (unsigned) strlen(str1)+1);
  
  for (i = 0; str1[i] != EOS; i++) 
    str[i] = is_alpha(str1[i]) ? toupper(str1[i]) : str1[i];
  
  str[i] = EOS;
  
  return(str);
} /* unfold */

/* ---------------------------------------------------------- */

/* moves bytes, allows for overlap */ 
int move_struct(dest, origin, nbytes)
     char *dest;          /* move data to destination */
     char *origin;        /* move data from origin */
     int nbytes;          /* no. of bytes to be transferred */
{
  if ( origin > dest)
    return(structcpy(dest, origin, nbytes));
  else {  /* origin and destination overlap */
    dest += nbytes-1;      /* point to end of areas */
    origin += nbytes-1;
    if (nbytes <= 0)
      return(FALSE);
    
    while (nbytes--)        /* stop when all bytes are moved */
      *dest-- = *origin--;
  }
  return(TRUE);  /* return successfully */
} /* move_struct */

/* ---------------------------------------------------------- */

cpy_nosemi(s, t)            /* copy t to s; without semicols */
     char *s, *t;
{  
  while (*t != ';')
    *s++ = *t++;
  
  *s = EOS;
} /* cpy_nosemi */

/* ---------------------------------------------------------- */

char *s_copy(str)
     char *str;
{
  register char *s;
  
  if ((s = calloc(1, (unsigned) strlen(str)+1)) == NULL)
    return(NULL);
  
  strcpy(s, str);
  return(s);
} /* s_copy */

/* ---------------------------------------------------------- */

int cat_strs (result, s1, s2)
     char result[];
     char s1[];
     char s2[];
{
  strcpy(result, s1);
  strcat(result, s2);
}

char	*stck(rel, arg)
     char	rel[];
     int	arg;
{
  sprintf(rel, "%s%d", SNAPSHOT, arg);
  return(rel);
} /* stck */

/* ---------------------------------------------------------- */

make_quattr(rel_name, att_name)
     char	*rel_name, *att_name;
{
  strcat(rel_name, ".");
  strcat(rel_name, att_name);
  
} /* make_quattr */

/* ---------------------------------------------------------- */

l_substring(string, result)
     char *string;
     register char *result;
{
  register char *bufptr = string;
  
  while((*result++ = *bufptr++) != '.')
    ;
  *--result = EOS;
} /* l_substring */

/* ---------------------------------------------------------- */

BOOL r_substring(string, result)
     char *string;
     register char *result;
{
  register char *bufptr = string;
  
  while (*bufptr != '.' && *bufptr)
    bufptr++;

  if (! *bufptr)
    return(FALSE);

  bufptr++;		/* clear dot */

  while(*result++ = *bufptr++)
    ;
  return(TRUE);
} /* r_substring */

/* ---------------------------------------------------------- */

lsubstring_index(string, substring)
     char *string, *substring;
{ 
  int res;

  res = strncmp(string, substring, strlen(substring));

  if (res == 0)
    return(0);
  else
    return(FAIL);
} /* lsubstring_index */

/* ---------------------------------------------------------- */

/* free two-dimensional array and all of its components which must be 
   allocated by a call to malloc or calloc */
buf_free(buf, len)
     char *buf[];
     int len;
{
  int i;
  
  if (buf){
    for (i = 0; i < len; i++)
      if (buf[i])
	nfree(buf[i]);
      else
	break;
    
    nfree((char *) buf);
  }
} /* buf_free */

/* ---------------------------------------------------------- */

free_pred(prptr)
     struct pred *prptr[];
{
  int i;

  for (i = 0; i < 2; i++) {
    if (prptr[i]) {
      nfree(prptr[i]->pr_rlname);
      buf_free(prptr[i]->pr_ixtype, 10);
      buf_free(prptr[i]->pr_atname, 10);
      nfree((char *) prptr[i]->pr_opd); 
      nfree((char *) prptr[i]); 
      prptr[i] = NULL;
    }
  }
} /* free_pred */

/* ---------------------------------------------------------- */

int set_pred(prptr)
     struct pred **prptr;
{
  if ((*prptr = MALLOC(struct pred )) == NULL)
    return(FALSE);
  (*prptr)->pr_rlname = NULL;
  if (!((*prptr)->pr_ixtype = dcalloc(10)))
    return (FALSE);
  if (!((*prptr)->pr_atname = dcalloc(10)))
    return (FALSE);
  
  if (((*prptr)->pr_opd = 
       ((struct operand**)calloc(10, sizeof(struct operand)))) == NULL)
    return(error(INSMEM));
  
  return(TRUE);
} /* set_pred */

/* ---------------------------------------------------------- */

save_pred(tmp_prptr)
     struct pred *tmp_prptr[];
{
  int i;

  for (i = 0; i < 2; i++) {
    tmp_prptr[i] = prptr[i];
    prptr[i] = NULL;
  }
} /* save_pred */

/* ---------------------------------------------------------- */

restore_pred(tmp_prptr)
     struct pred *tmp_prptr[];
{
  int i;

  for (i = 0; i < 2; i++)
    prptr[i] = tmp_prptr[i];
} /* restore_pred */

/* ---------------------------------------------------------- */

insert_string(string, substring, loc)
     char *string, *substring;
     int loc;
{ 
  int index_str = 0;       /* index into the string */ 
  int t_index = 0;    
  char temp_buf[100];
  
  /* we can append the substring at the end of current string */
  if (loc > strlen(string))
    return(FAIL);
  
  /* copy the characters in the string prior to the start 
     location to the temporary buffer */
  while(index_str < loc)
    temp_buf[t_index++] = string[index_str++];
  
  temp_buf[t_index] = EOS;
  
  /* catenate the substring */
  strcat(temp_buf, substring);
  
  t_index = strlen(temp_buf);
  while (temp_buf[t_index++] = string[index_str++]);
  
  /* copy temporary buffer back into original string */
  strcpy(string, temp_buf);

  return(TRUE);
} /* insert_string */

/* ---------------------------------------------------------- */

remove_substring(strng, loc, no_of_chars)
     char *strng;
     int loc;            /* location of first char to remove */
     int no_of_chars;  	 /* number of chars to remove */
{
  int len;   /* length of string */
  int loc2;  /* location of 1st char remaining after removal */
  
  len = strlen(strng);
  
  if (loc >= len || loc < 0)
    return(FAIL);
  
  /* see whether no. of chars to be deleted GTE string length */
  if (loc + no_of_chars <= len) /* no */
    loc2 = loc + no_of_chars;
  else                          /* yes */
    loc2 = len;                 /* delete up to EOS */
  
  /* remove characters now */
  while (strng[loc++] = strng[loc2++]);
  
  strng[loc] = strng[loc2];

  return(TRUE);
} /* remove_string */

/* ---------------------------------------------------------- */

double maximum(arg1, arg2)
     double arg1;
     double arg2;
{
  return(MAX(arg1, arg2));
} /* maximum */

/* ---------------------------------------------------------- */

double minimum(arg1, arg2)
     double arg1;
     double arg2;
{
  return(MIN(arg1, arg2));
} /* minimum */

/* ---------------------------------------------------------- */

double sum(arg1, arg2)
     double arg1;
     double arg2;
{
  return(arg1+arg2);
} /* sum */

/* ---------------------------------------------------------- */

double average(arg1, arg2)
     double arg1;
     double arg2;
{
  if (avg_cnt == 1) 
    avg_sum = arg1 + arg2;
  else
    avg_sum += arg1;

  avg_cnt++;
  return(avg_sum / avg_cnt);
} /* average */

/* ---------------------------------------------------------- */
/*ARGSUSED*/
double count(arg1, arg2)
     double arg1;
     double arg2;
{
  return(arg2 + 1);
} /* count */

/* ---------------------------------------------------------- */

init_group()
{
  avg_cnt = 1;
  avg_sum = 0;
} /* init_group */

/* ---------------------------------------------------------- */

f_move(from, to)
     char *from, *to;
{
  unlink(to);  /* purge file in case it already exists */
  
  if (!f_mv(from, to))
    return(FALSE);
  
  return(TRUE);
} /* f_move */

/* ---------------------------------------------------------- */

f_mv(from, to)
     char *from, *to;
{
  
  if (rename(from, to) == FAIL) /* give aux. file the rel. name */
    return (FALSE);
  
  return(TRUE);
} /* f_mv */

/* ---------------------------------------------------------- */


/*ARGSUSED*/
char  *status(fname, perm)  /* "s" command */
     char fname[];
     char perm[];
{
#ifdef UNIX
  struct stat sb;
  char f_name[20];
  int fd;
  
  sprintf(f_name, "%s.db", fname); 
  fd = open(f_name, O_RDONLY);
  fstat(fd, &sb);
  
  sprintf(perm, "%o", sb.st_mode & 0777);

  close(fd);
#else
  sprintf(perm, "%o", 800);
#endif
  return(perm);
} /* status */

/* ---------------------------------------------------------- */

char *path(pathname)
     char pathname[];
{
  char *getlogin(), *cuserid();
  
  if (getlogin() != NULL)
    strcpy(pathname, getlogin());
  else
    strcpy(pathname, cuserid((char *) 0));
  
  pathname[strlen(pathname)+1] = EOS;
  return(pathname);
} /* path */

/* ---------------------------------------------------------- */

int fexists(fname)
     char fname[];
{
  char f_name[20];
  
  sprintf(f_name,"%s.db", fname);
  f_name[strlen(f_name)] = EOS;
  
  return(file_exists(f_name));
} /* fexists */

/* ---------------------------------------------------------- */

int file_exists(fname)
     char fname[];
{ int fd;

  if ((fd = open(fname, O_RDONLY)) == FAIL)
    return(FALSE);
  else
    close(fd);
  return(TRUE);
} /* file_exists */

/* ---------------------------------------------------------- */

int rsubstring_index(string, substring)
     char *string, *substring;
{  
  if (strncmp(string + strlen(string) - strlen(substring), 
		                    substring, strlen(substring)) == EQUAL)
    return(strlen(string) - strlen(substring));
  else 
    return(FAIL);
}
/* ---------------------------------------------------------- */

int right_index(string, substring)
     char *string, *substring;
{
  register i, j, k;
  int loc = -1;  /* rightmost occurrence of substring */
  
  for (i = 0; *(string+i) != EOS; i++)
    for (j = i, k = 0; (*(substring+k) != EOS)  &&
	 *(substring+k) == *(string+j); k++, j++) 
      if (*(substring+k+1) == ' ' || *(substring+k+1) == EOS){
	/* substring found, point past it */
	loc = i + strlen(substring);
	break;
      }
  
  /* return location , -1 if not found */
  return(loc);
} /* right_index */

/* ---------------------------------------------------------- */

int clear_mem(p, c, tcnbyt)
     char *p;            /* start of memory area */
     int c;              /* value to write there */
     int tcnbyt;         /* total no. of bytes to be cleared */
{
  while (tcnbyt--)
    *p++ = c;            /* clear current location */
} /* clear_mem */

/* ---------------------------------------------------------- */

char *rmalloc(n)       /* check return from malloc */
     int n;
{
  char *ptr;
  char *pt_error();

  ptr = malloc((unsigned) n);
  
  if (ptr == (char *) 0)
    return(pt_error(INSMEM));
  
  return(ptr);
} /* rmalloc */

/* ---------------------------------------------------------- */

stf(s, res)   /* convert string to float */
     char s[];
     float *res;
{
  float val, power;
  int i;
  
  for (i = 0; s[i] == ' ' || s[i] == NEWLINE; i++)
    ;   /* skip white space */
  
  for (val = 0 ; s[i] >= '0' && s[i] <= '9'; i++)
    val = 10 * val + s[i] - '0';
  
  if (s[i] == '.')
    i++;
  
  for (power = 1; s[i] >= '0' && s[i] <= '9'; i++) {
    val = 10 * val + s[i] - '0';
    power *= 10;
  }
  *res = val / power;
} /* stf */

/* ---------------------------------------------------------- */

requ_init(argc, argv)
     int argc;
     char *argv[];
{
  sref_cntr = 5;	/* assume 5 garbage snapshots at most */
  db_remove();		/* delete garbage */
  
  /* create and load system catatalogs */
  if (!fexists("systab")) {
    if (!file_exists("cre_cat")) {
      printf("DANGER: System catalog definition file missing,");
      printf("\n \t \t create the cre_file and define catalogs!\n");
      exit();
    }
    sys_flag = TRUE;
    com_file("cre_cat");     
  }

  /* load views  */

  if (fexists("sysview")) {
    load_views(); 
  } 

  if (argc == 2) {
    if (fexists("systab") && fexists("sysatrs"))
      com_file(argv[1]);
    else {
      printf("Create the system catalogs first,");
      printf("and then call a command file !\n");
    }
  } 
} /* requ_init */

/* ---------------------------------------------------------- */

index_type(index, key)
  char *index;
      char key;
{
  switch (key) {
      case 'p':
	strcpy(index, "prim");
	break;
      case 's':
	strcpy(index, "secnd");
	break;
      case 'f':
	strcpy(index, "fgn");
	break;
      default:
	strcpy(index, "none");
	break;
      }
      index[strlen(index)] = EOS;
} /* index_type */

/* ---------------------------------------------------------- */

numcomp(s1, s2)
     char *s1, *s2;
{
  int i1, i2;
  
  i1 = atoi(s1);
  i2 = atoi(s2);

  return(i1 == i2 ? 0 : (i1 > i2 ? 1 : -1));
} /* numcomp */

/* ---------------------------------------------------------- */

realcomp(s1, s2)
     char *s1, *s2;
{
  float i1, i2;
  
  i1 = atof(s1);
  i2 = atof(s2);

  return(i1 == i2 ? 0 : (i1 > i2 ? 1 : -1));
} /* realcomp */

#ifndef UNIX
static char user_name[ANSIZE + 1];

init_username()
{
	printf("Please enter your user identification: ");
	scanf("%10s", user_name);
	cmd_kill();
}

char *getlogin()
{
	return(user_name);
}

char *cuserid(buf)
	char *buf;
{
	if (buf != (char *) 0) {
		strncpy(buf, user_name, ANSIZE);
		buf[strlen(user_name)] = EOS;
	}
	
	return(user_name);
}

bcopy(from, to, length)
     register char *from, *to;
     int length;
{
  while (length-- > 0)
    *to++ = *from++;
}

#endif
/* ---------------------- EOF misc.c -------------------------- */



