

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

writeln(string)
     char *string;
{ /* writes the string to stdout ensuring that only one new line 
   * is displayed, resolves the problem of existent or non-existent 
   * newline chars from files*/
  
  while (*string != EOL && *string != EOS)
    putchar(*string++);
  
} /* writeln */

/* ----------------------------------------------------------------- */

int_convert(ascii_val)
     char *ascii_val;
{
  /* read past blanks */
  while(is_blank(*ascii_val))
    ascii_val++;       /* increment ascii_val to skip blanks 
			* and get the next letter */

  /* check for sign  */
  if (*ascii_val == '-' || *ascii_val == '+')
    ascii_val++;

  while (*ascii_val)
    if (is_digit(*ascii_val))
      ascii_val++;
    else
      return(IO_ERROR);
  
  return(NO_ERROR); 
} /* int_convert */

/* ----------------------------------------------------------------- */

read_string(string, max, fp)
     char *string;
     int max;
     register FILE *fp;
{
  /* reads string entered by user, replaces carriage 
   * return with EOS, if EOF occurs then it returns it, 
   * else it returns the value NO_ERROR 
   */
  int i = 0;
  char letter;
  
  while ((letter = getc(fp)) != EOL && letter != EOF){
    if ( letter >= ' ' && letter <= '~'){
      if (i < max-1){
	string[i++] = letter;
      }
      else{
	printf("***** You're allowed to type in a string");
	printf(" of max. %d chars ! *****\n", max-1);
	return(IO_ERROR);
      }
    } else 
      return(IO_ERROR);
  }
  
  string[i] = EOS; /* replace CR or max. len with string terminator */
  if (i == 0 && letter == EOL) 
    return(EOL); /* return EOL, if only a single line was entered */
  return((letter == EOF) ? EOF : NO_ERROR);
} /* read-string */

/* ----------------------------------------------------------------- */

non_fatal_error(error_message)           
     char *error_message;
{
  int count;
  
  for (count = 0; count<3; count++)
    BELL;
  printf("%s\n", error_message); 
  console_write("Hit <return> to continue:");
  
  /* wait for return */
  read_char();
} /* non_fatal_error */

/* ----------------------------------------------------------------- */

read_char()
{
  /* reads a char entered from keyboard and discards contents of 
   * i/o buffer until newline char, if it meets EOF it returns it
   * else it returns the first character. Used to discard 
   * wrong data entered by user */
  
  int letter; /* letter returned to calling routine */
  int buffer; /* data remaining in the i/p buffer is read into 
                 buffer, buffer for garbage data */
  letter = getchar(); /* get response */
  buffer = letter;
  
  /* get characters until the i/p buffer is empty */
  while(buffer != EOL && buffer != EOF)
    buffer = getchar();
  return((buffer != EOF) ? letter : EOF);
} /* read_char */

/* ----------------------------------------------------------------- */

console_write(string)
     char *string;
{
  /* writes the string to the console ensuring that a newline
   * char is not written */
  
  while(*string != EOL && *string != EOS)
    putchar(*string++);
} /* console_write */

/* ----------------------------------------------------------------- */

char *getnum(prompt, ascii_val, max, fp)
     char *prompt;
     char *ascii_val;
     int max;
     register FILE *fp;
{
  BOOL not_valid = TRUE;  /* FALSE when a valid integer is entered */
  int c;
  
  while(not_valid){
    writeln(prompt);
    if ((c = read_string(ascii_val, max, fp)) == EOF)
      return(NULL);
    if (c == EOL)
      /* get the i/p */
      return(ascii_val);  /* num is unaffected */
    if (isspace(*ascii_val)) {
      non_fatal_error(
	   "SPURIOUS-ERROR: Invalid data. You should not start with space!");

      if (fp != stdin)
          return(NULL);
    } 
    else if (c == IO_ERROR){
      read_char();
      BELL;
      printf("Invalid data entered. Enter an integer value!\n");
      continue;
    }
    /* convert the string to the corresponding integer value */
    else if (int_convert(ascii_val) == NO_ERROR)
      not_valid = FALSE;
    else
      non_fatal_error(
		      "SPURIOUS-ERROR: Invalid data entered. Enter a proper integer !");
  }
  return(ascii_val);
} /* getnum */

/* ----------------------------------------------------------------- */

char *getfld(prompt, individ, max, fp)
     char *individ, *prompt;
     int max;
     register FILE *fp;
{
  /* Prompts the user to enter a field containing text
   * of not more than 20 chars. */
  int c;
  BOOL valid = FALSE; /* TRUE when valid strings are entered */
  
  while (!valid){
    writeln(prompt);
    if ((c = read_string(individ, max, fp)) == NEWLINE)
      return(individ);
    else if (c == EOF)
      return(NULL);
    else if (c == IO_ERROR){
      read_char();
      BELL;
      printf("Invalid data entered. Enter a character string!\n");
      continue;
    } else if (c == NO_ERROR)
      valid = TRUE;
  }
  return(individ);
} /* getfld */

/* ----------------------------------------------------------------- */

float_convert(ascii_val)
     char *ascii_val;
{
  /* read passed blanks */
  while (is_blank(*ascii_val))
    ascii_val++;
  /* first convert the numbers on the left of the decimal point
   */
  while (*ascii_val)
    if  (is_digit(*ascii_val))
      ascii_val++;
    else if (*ascii_val == '.') /*start the fractional part */
      break;
    else 
      return(IO_ERROR);
  
  if (*ascii_val != NULL){
    ascii_val++; /* we're now past decimal point */
    while ( *ascii_val != NULL && strlen(ascii_val) < LINEMAX)
      if (is_digit(*ascii_val))
	ascii_val++;
      else 
	return(IO_ERROR);
  }
  return(NO_ERROR); /* just like in int_convert */
}; /* float_convert */

/* ----------------------------------------------------------------- */

char *getfloat(prompt, ascii_val, max, fp)
     char *prompt;
     char ascii_val[];
     int max;
     register FILE *fp;
{
  BOOL not_valid = TRUE;  /* FALSE if a valid integer is entered */
  int c;
  
  while(not_valid){
    writeln(prompt);
    if ((c = read_string(ascii_val, max, fp)) == EOF ||  c == EOL) /* get the i/p */
      return(ascii_val);  /* num is unaffected */
    
    if (c == IO_ERROR){
      read_char();
      BELL;
      printf(" Invalid data entered. Enter a real number!\n");
      continue;
    }
    /* convert the string to the corresponding integer value */
    if (float_convert(ascii_val) == NO_ERROR)
      not_valid = FALSE;
    else
      non_fatal_error("SPURIOUS-ERROR: Invalid data ! Enter a real no. !");
  }
  return (ascii_val);
} /* get_float */

/* ----------------------------------------------------------------- */

char *fget_string(aname, avalue, type, max_len, fp)
char *aname, *avalue;
int max_len, type;
register FILE *fp;
{   char *prompt_ninput();

  return(prompt_ninput(fp, TRUE, type, max_len, aname, avalue));

} /* fget_string */

/* ----------------------------------------------------------------- */
