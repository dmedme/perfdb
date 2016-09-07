/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     E2STRING = 258,
     NUMBER = 259,
     IDENT = 260,
     IS_DATE = 261,
     TO_DATE = 262,
     TO_CHAR = 263,
     NUM_CHAR = 264,
     SYSDATE = 265,
     TRUNC = 266,
     SYSTEM = 267,
     LENGTH = 268,
     CHR = 269,
     UPPER = 270,
     LOWER = 271,
     INSTR = 272,
     GETENV = 273,
     GETPID = 274,
     PUTENV = 275,
     SUBSTR = 276,
     HEXTORAW = 277,
     GET_MATCH = 278,
     SET_MATCH = 279,
     EVAL = 280,
     BYTEVAL = 281,
     RECOGNISE_SCAN_SPEC = 282,
     URL_ESCAPE = 283,
     URL_UNESCAPE = 284,
     END = 285,
     OR = 286,
     AND = 287,
     NE = 288,
     EQ = 289,
     GE = 290,
     LE = 291,
     RS = 292,
     LS = 293,
     UMINUS = 294
   };
#endif
/* Tokens.  */
#define E2STRING 258
#define NUMBER 259
#define IDENT 260
#define IS_DATE 261
#define TO_DATE 262
#define TO_CHAR 263
#define NUM_CHAR 264
#define SYSDATE 265
#define TRUNC 266
#define SYSTEM 267
#define LENGTH 268
#define CHR 269
#define UPPER 270
#define LOWER 271
#define INSTR 272
#define GETENV 273
#define GETPID 274
#define PUTENV 275
#define SUBSTR 276
#define HEXTORAW 277
#define GET_MATCH 278
#define SET_MATCH 279
#define EVAL 280
#define BYTEVAL 281
#define RECOGNISE_SCAN_SPEC 282
#define URL_ESCAPE 283
#define URL_UNESCAPE 284
#define END 285
#define OR 286
#define AND 287
#define NE 288
#define EQ 289
#define GE 290
#define LE 291
#define RS 292
#define LS 293
#define UMINUS 294




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 78 "cspars.y"

int ival;
struct s_expr * tval;



/* Line 2068 of yacc.c  */
#line 135 "y.tab.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




