%e 600
%p 1750
%n 240
%k 800
%a 2400
%o 2600
%{
/* englexdef - %W% %G% - input for lexical analysis generator lex */
/* to be used in the recognition of words in English documents */

#include <ctype.h>
#include <stdio.h>
#include "IRfiles.h"

    struct word_results lex_answer;

    int char_pos=0;
#define DDMMYY(x,y)    {{static char temp_date[9];\
    if (isdigit(*(yytext+1)))\
    {\
     temp_date[0] = *yytext;\
     temp_date[1] = *(yytext+1);\
    }\
    else\
    {\
     temp_date[0] = '0';\
     temp_date[1] = *yytext;\
    }\
    temp_date[2] = '/';\
    temp_date[3] = 48 + x;\
    temp_date[3] = 48 + y;\
    temp_date[5] = '/';\
    temp_date[7] = *(yytext + yyleng -1);\
    temp_date[6] = *(yytext + yyleng -2);\
    lex_answer.word_type = TEXTWORD;\
    lex_answer.word_ptr = temp_date;\
    lex_answer.word_length = 8;\
    lex_answer.start_pos = char_pos;\
    char_pos += yyleng;\
    return &lex_answer;\
    }\
    }
%}
%%
[A-Za-z0-9]+ |
[A-Za-z0-9]+"-"[A-Za-z0-9]+  |
[A-Za-z0-9.-_]+"@"[A-Za-z0-9.-_]+  |
[A-Za-z0-9]+"'s" |
[0-9]+ |
[0-9][0-9.,]+ |
[.,][0-9]+ {lex_answer.word_type = TEXTWORD;
      lex_answer.word_ptr = yytext;
      lex_answer.start_pos = char_pos;
      lex_answer.word_length = 0xff & yyleng;
      char_pos += yyleng;
      return &lex_answer;};
\n[\n]+ {lex_answer.word_type = PARA;
      lex_answer.word_ptr = NULL;
      lex_answer.start_pos = char_pos;
      lex_answer.word_length = 0;    /* Paragraph delimiters assumed to be
                       null */
      char_pos += yyleng;
      return &lex_answer;};
[^A-Za-z0-9\n]+ |
\n         {char_pos += yyleng;};
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Jj][Aa][NnUuAaRrYy]*|1|01)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,1)
    };
([1-9]|[0-2][0-9])[ .,\n/.]+([Ff][EeBbRrUuAaRrYy]*|2|02)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,2)
    };
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Mm][Aa][Rr][CcHh]*|3|03)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,3)
    };
([1-9]|[0-2][0-9]|30)[ .,\n/.]+([Aa][Pp][RrIiLl]*|4|04)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,4)
    };
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Mm][Aa][Yy]|5|05)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,5)
    };
([1-9]|[0-2][0-9]|30)[ .,\n/.]+([Jj][Uu][Nn][Ee]*|6|06)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,6)
    };
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Jj][Uu][Ll][Yy]*|7|07)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,7)
    };
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Aa][Uu][GgUuSsTt]*|8|08)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,8)
    };
([1-9]|[0-2][0-9]|30)[ .,\n/.]+([Ss][EePpTtEeMmBbEeRr]*|9|09)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(0,9)
    };
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Oo][CcTtOoBbEeRr]*|10)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(1,0)
    };
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Nn][OoVvEeMmBbEeRr]*|11)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(1,1)
    };
([1-9]|[0-2][0-9]|3[01])[ .,\n/.]+([Dd][EeCcEeMmBbEeRr]*|12)[ \n/,.]+((19|20)[0-9][0-9]|[0-9][0-9]) {
    DDMMYY(1,2)
    };
%%
struct open_results open_object;
int yywrap() { return 1;}

extern     datum doc_id_key, doc_name_content;

datum  found_name;

extern int fclose();    /* do not know why this is not in stdio.h */
extern int fseek();    /* do not know why this is not in stdio.h */
int eng_seek (channel, offset, flag)
    FILE * channel;
    long offset;
    int flag;
{
    int ret_val;
    if ((ret_val = fseek(channel,offset,flag)) == -1)
     fseek(channel,0,2);
    char_pos = ftell(channel);
#ifdef ANCIENT
    yysptr = yysbuf;
            /* Clear the lex look-ahead */
#endif
    return ret_val;
}
