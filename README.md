# Requiem
Revisiting legacy codebase for Requiem relational database - See LICENSE file for further license details

Following is the README file that accompanied the Requiem source code.



/************************************************************************
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


This distribution of the REQUIEM database prototype comes with a
makefile. In order to install it on a BSD system, simply type 'make'
in the directory where you have stored the source files.  After make
has finsihed, the REQUIEM library 'requ.a' and the REQUIEM program
'requiem' are produced in the current directory. The REQUIEM library
'requ.a' may be used to link programs that participate of the REQUIEM
program interface (For more information about using REQUIEM functions
in application programs see the book: Relational Database Management:
A Systems Programming Approach, published by Prentice Hall, ISBN:
13-771866-7).

Besides the source files, this distribution contains additional files,
whose name start with the prefix 'cre_'. This files contain sample
schema definitions for REQUIEM. They may be used with the 'indirect
command file' feature of REQUIEM to generate several schemata. In
detail:

cre_cat: this file contains the schema definitions for the REQUIEM
system catalogs and must not be changed. REQUIEM expects to find this
file in the directory where the REQUIEM program is located. This file
is consulted at the system initialization phase and used to create the
system catalogs which are automatically created with no user
intervention. REQUIEM informs you which catalogs have been created
sofar and if anything went wrong during the initialization phase. By
executing this defintion statements during start-up, REQUIEM creates
the following files, that hold the system catalog data: systab.db,
systab.indx, sysatrs.db, sysatrs.indx, syskey.db, syskey.indx,
sysview.db, sysview.indx.

cre_schema: this file holds the defintion statements for the
supplier-and-products database, which is used as an example throughout
the above mentioned book.

cre_reminder: this file generates the schemata used in the reminder
example of the book (it is used as a sample program to illustrate the
REQUIEM program interface features, which are at the moment not fully
operational, see chapter 14 of the book for further details).

The last two files are intended to serve as running examples. They may
be changed and adapted at will. In no case you should mess around with
files that have a suffix .db or .indx otherwise the system will
collapse.

Setting Up the System
--------------------
To start the system you must simply type requiem, make sure that the
file cre_cat is in the same directory. This will create all the system
relations required for requiem to operate.

To create your own schema you should open a file, say my_schema, and
define relations according to what is mentioned on the book or the
instructions manual. You would probably want to use the schema
cre_schema that exists in this tape.  This will create an empty
instance of the suppliers and products database as mentioned in the
book. To create this scheam you simply type @cre_schema after the
REQUIEM prompt. Alternatively if for some reason you have exited the
system and you wish to re-enter & create a new schema you simply type

requiem cre_schema

or the name you have chosen for your own schema file.

Subsequently you could try inserting tuples and posing queries to the
system.  Enjoy!

To run the demos provided with this distribution you must simply
type 

@imports
@demos

from within Requiem.

The command "@imports" imports sample data for the supplier and
products database. The command "@demos" runs some sample queries
contained in the corresponding indirect command file. 

I wish to emphasize that this is only the second-test version of a
prototype which offers some  nice features. We are currently working
to improve the system and can report progress in three fronts:
transaction management, embedding Requiem into the C programming
language and upgrading the current program interace, as well as
developing user interface facilities.

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+  As a token of appreciation I would be grateful to you if you   +
+  could share with us any upgrades, modifications and so forth.  +
+  In return I promise to make public all software that we        +
+  implement locally.                                             +
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Please, also refer to the licensing agreement (license file) regarding
distribution and upgrades in general.

If you find (and preferably, fix!) bugs, please let us know.
If you have made any changes, complements or have any ideas that you
would like to share with us, please let us know.
If you want to make complaints, please let us know.
If you require support I am afraid that I can't offer too much,
but I'll try to come back to you if possible.

Address:

Mike P. Papazoglou,
Australian National University,
Dept. of Computer Science,
GPO Box 4, Canberra ACT 2601,
Australia

tel. +61-6-2494725, fax. +61-6-2490010
e-mail: mike@anucsd.anu.edu.au




