/* Context Hunting	Header	SCCS ID: %W% %G%

	This file has to be included by modules that wish to access
	the context hunting software.

*/
#ifndef CONTHUNT_H
#define CONTHUNT_H
long int con_set();
				/* Context Hunter Setter Upper;
					this is not really an int;
					if it is not negative, cast it
					as a struct DOCCON_cont_header *
				 */

struct hunt_results {
		     int type_of_answer;
		     long start_pos;
		     long end_pos;
		    }; /* the result of a search */


#define NOT_FOUND 0
#define FOUND 1


struct hunt_results * cont_hunt();
			/* the context hunter itself */
#endif

