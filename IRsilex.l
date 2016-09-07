REGEXP [^ \n\t{}()"]+
AND [ \n\t]*&&[ \n\t]*
NOT [ \n\t]*&![ \n\t]*
STARTPROX \{
ENDPROX \}
SEQOP \"
LEFTBRACKET \(
RIGHTBRACKET \)
BLANKSPACE [ \n\t]+
%{
/* IRilexdef - %W% %G% - case insensitive lexical analysis generator */
/* to be used in the parsing of the search conditions */


       int seqflag=0;
%}
%%
{BLANKSPACE} ;
{REGEXP} {char * x; for (x = yytext;
			 *x != '\0';
			 x++)
	  if ((*x <= 'Z') && (*x >= 'A'))  *x |= 0x20;
/*	for some reason there are problems with isupper and tolower */
	  return(REGEXP);};
{AND} {return(AND);};
{NOT} {return(NOT);};
{LEFTBRACKET} {return(LEFTBRACKET);};
{RIGHTBRACKET} {return(RIGHTBRACKET);};
{SEQOP} {if (seqflag)
	 {
	  seqflag=0;
	  return(ENDSEQ);
	 }
         else
	 {
	  seqflag=1;
	  return(STARTSEQ);
	 }
        };
{STARTPROX} {return(STARTPROX);};
{ENDPROX}   {return(ENDPROX);};
%%
int yywrap() { return 1;}
