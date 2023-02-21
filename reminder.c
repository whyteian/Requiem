

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


#include <time.h>
#include "interface.h"

main()
{
   struct sel *invoice;      /* selection pointer */
   char term_of_payment[20]; /* buffer for attribute value*/
   char cust_name[10];       /* holds the customer name */
   char invoice_no[10];      /* the id of the invoice in question */
   char today[20];           /* holds the current date */
   char *getDate();

   requ_init(0, (char **) 0);	/* initialise requiem */

   /* generated a selection structure for the INVOICE relation */
   if ((invoice = retrieve("invoice where amount <> 0")) == NULL) {
     printf("Cannot retrieve invoices\n");
     exit(1);
   }

   /* put the current date into the variable 'today'; the current 
      date is provided by the function 'getDate()' that has to be
      implemented in terms of the operating system being used       
   */

   strcpy(today, getDate());

   /* bind the user variables to attributes */
   requiem_bind(invoice, "invoice", "termofpay", term_of_payment);
   requiem_bind(invoice, "invoice", "name", cust_name);
   requiem_bind(invoice, "invoice", "invoiceno", invoice_no);

   /* loop through all selected tuples and check whether 
      the term of payment has exceeded. If so call the 
      function print_reminder to print a reminder.
   */

   while (fetch(invoice, FALSE)) {
     if (datecmp(today, term_of_payment) >= 0) {
       print_reminder(cust_name, invoice_no, term_of_payment);
     }
   }

   /* finish the selection */
   done(invoice);   
}

print_reminder(name, invoice_no, term_of_payment)
     char *name;             /* the customer name */
     char *invoice_no;       /* the invoice number */
     char *term_of_payment;  /* term of payment for the given invoice */
{
   struct sel *customer;       /* selection pointer */
   char street[30];            /* street */
   char location[30];          /* location */
   char retrieve_sentence[40]; /* holds the retrieve sentence */

   /* generated a selection structure for the CUSTOMER relation */
   sprintf(retrieve_sentence, "customer where name = \"%s\"", name);

   if ((customer = retrieve(retrieve_sentence)) == NULL) {
     printf("Cannot retrieve customer\n");
       exit(1);
   }

   /* bind the user variables to attributes */
   requiem_bind(customer, "customer", "street", street);
   requiem_bind(customer, "customer", "location", location);

   /* loop through all selected tuples and check whether the 
      term of payment has expired. If so, call the function 
      do_print() with appropriate parameters to print a reminder.
   */

   while (fetch(customer, FALSE)) {
     do_print(name, location, street, invoice_no, term_of_payment);      
  }

  /* finish the selection */
  done(customer);
}

do_print(name, location, street, invoice_no, term_of_payment)
     char *name, *location, *street, *invoice_no, *term_of_payment;
{
  printf("Mr./Mrs. %s in %s, %s:\n\tyour invoice %s should have been paid by %s\n",
	 name, location, street, invoice_no, term_of_payment);
}

/* return the current date as a character string in the form mm/dd/yyyy */
char *getDate()
{
  char date[15];
  struct tm *timePtr;
  long tPtr;
  
  time(&tPtr);
  timePtr = localtime(&tPtr);

  sprintf(date,"%.2d/%.2d/19%.2d", 
	  timePtr->tm_mon + 1, timePtr->tm_mday, timePtr->tm_year);

  return(date);
}

/* compare dates, must be improved !! */

datecmp(d1, d2)
     char *d1, *d2;
{
  return(strcmp(d1, d2));
}

  
