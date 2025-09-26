/* A Bison parser, made by GNU Bison 3.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018 Free Software Foundation, Inc.

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         HSQL_STYPE
#define YYLTYPE         HSQL_LTYPE
/* Substitute the variable and function names.  */
#define yyparse         hsql_parse
#define yylex           hsql_lex
#define yyerror         hsql_error
#define yydebug         hsql_debug
#define yynerrs         hsql_nerrs


/* First part of user prologue.  */
#line 1 "bison_parser.y" /* yacc.c:338  */

/**
 * bison_parser.y
 * defines bison_parser.h
 * outputs bison_parser.c
 *
 * Grammar File Spec: http://dinosaur.compilertools.net/bison/bison_6.html
 *
 */
/*********************************
 ** Section 1: C Declarations
 *********************************/

#include "bison_parser.h"
#include "flex_lexer.h"

#include <stdio.h>
#include <string.h>

using namespace hsql;

int yyerror(YYLTYPE* llocp, SQLParserResult* result, yyscan_t scanner, const char *msg) {
	result->setIsValid(false);
	result->setErrorDetails(strdup(msg), llocp->first_line, llocp->first_column);
	return 0;
}


#line 106 "bison_parser.cpp" /* yacc.c:338  */
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "bison_parser.h".  */
#ifndef YY_HSQL_BISON_PARSER_H_INCLUDED
# define YY_HSQL_BISON_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef HSQL_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define HSQL_DEBUG 1
#  else
#   define HSQL_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define HSQL_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined HSQL_DEBUG */
#if HSQL_DEBUG
extern int hsql_debug;
#endif
/* "%code requires" blocks.  */
#line 35 "bison_parser.y" /* yacc.c:353  */

// %code requires block

#include "../../include/sqlparser/statements.h"
#include "../../include/sqlparser/SQLParserResult.h"
#include "parser_typedef.h"

// Auto update column and line number
#define YY_USER_ACTION \
		yylloc->first_line = yylloc->last_line; \
		yylloc->first_column = yylloc->last_column; \
		for(int i = 0; yytext[i] != '\0'; i++) { \
			yylloc->total_column++; \
			yylloc->string_length++; \
				if(yytext[i] == '\n') { \
						yylloc->last_line++; \
						yylloc->last_column = 0; \
				} \
				else { \
						yylloc->last_column++; \
				} \
		}

#line 171 "bison_parser.cpp" /* yacc.c:353  */

/* Token type.  */
#ifndef HSQL_TOKENTYPE
# define HSQL_TOKENTYPE
  enum hsql_tokentype
  {
    SQL_IDENTIFIER = 258,
    SQL_STRING = 259,
    SQL_FLOATVAL = 260,
    SQL_INTVAL = 261,
    SQL_DEALLOCATE = 262,
    SQL_PARAMETERS = 263,
    SQL_INTERSECT = 264,
    SQL_TEMPORARY = 265,
    SQL_TIMESTAMP = 266,
    SQL_DISTINCT = 267,
    SQL_NVARCHAR = 268,
    SQL_RESTRICT = 269,
    SQL_TRUNCATE = 270,
    SQL_ANALYZE = 271,
    SQL_BETWEEN = 272,
    SQL_CASCADE = 273,
    SQL_COLUMNS = 274,
    SQL_CONTROL = 275,
    SQL_DEFAULT = 276,
    SQL_EXECUTE = 277,
    SQL_EXPLAIN = 278,
    SQL_HISTORY = 279,
    SQL_INTEGER = 280,
    SQL_NATURAL = 281,
    SQL_PREPARE = 282,
    SQL_PRIMARY = 283,
    SQL_SCHEMAS = 284,
    SQL_SPATIAL = 285,
    SQL_VARCHAR = 286,
    SQL_VIRTUAL = 287,
    SQL_BEFORE = 288,
    SQL_COLUMN = 289,
    SQL_CREATE = 290,
    SQL_DELETE = 291,
    SQL_DIRECT = 292,
    SQL_DOUBLE = 293,
    SQL_ESCAPE = 294,
    SQL_EXCEPT = 295,
    SQL_EXISTS = 296,
    SQL_EXTRACT = 297,
    SQL_GLOBAL = 298,
    SQL_HAVING = 299,
    SQL_IMPORT = 300,
    SQL_INSERT = 301,
    SQL_ISNULL = 302,
    SQL_OFFSET = 303,
    SQL_RENAME = 304,
    SQL_SCHEMA = 305,
    SQL_SELECT = 306,
    SQL_SORTED = 307,
    SQL_TABLES = 308,
    SQL_UNIQUE = 309,
    SQL_UNLOAD = 310,
    SQL_UPDATE = 311,
    SQL_VALUES = 312,
    SQL_AFTER = 313,
    SQL_ALTER = 314,
    SQL_CROSS = 315,
    SQL_DELTA = 316,
    SQL_FLOAT = 317,
    SQL_GROUP = 318,
    SQL_INDEX = 319,
    SQL_INNER = 320,
    SQL_LIMIT = 321,
    SQL_LOCAL = 322,
    SQL_MERGE = 323,
    SQL_MINUS = 324,
    SQL_ORDER = 325,
    SQL_OUTER = 326,
    SQL_RIGHT = 327,
    SQL_TABLE = 328,
    SQL_UNION = 329,
    SQL_USING = 330,
    SQL_WHERE = 331,
    SQL_CALL = 332,
    SQL_CASE = 333,
    SQL_CHAR = 334,
    SQL_DATE = 335,
    SQL_DESC = 336,
    SQL_DROP = 337,
    SQL_ELSE = 338,
    SQL_FILE = 339,
    SQL_FROM = 340,
    SQL_FULL = 341,
    SQL_HASH = 342,
    SQL_HINT = 343,
    SQL_INTO = 344,
    SQL_JOIN = 345,
    SQL_LEFT = 346,
    SQL_LIKE = 347,
    SQL_LOAD = 348,
    SQL_LONG = 349,
    SQL_NULL = 350,
    SQL_PLAN = 351,
    SQL_SHOW = 352,
    SQL_TEXT = 353,
    SQL_THEN = 354,
    SQL_TIME = 355,
    SQL_VIEW = 356,
    SQL_WHEN = 357,
    SQL_WITH = 358,
    SQL_LOW_PRIORITY = 359,
    SQL_DELAYED = 360,
    SQL_HIGH_PRIORITY = 361,
    SQL_QUICK = 362,
    SQL_IGNORE = 363,
    SQL_DATABASES = 364,
    SQL_DATABASE = 365,
    SQL_CHARACTER = 366,
    SQL_ADD = 367,
    SQL_ALL = 368,
    SQL_AND = 369,
    SQL_ASC = 370,
    SQL_CSV = 371,
    SQL_END = 372,
    SQL_FOR = 373,
    SQL_INT = 374,
    SQL_KEY = 375,
    SQL_NOT = 376,
    SQL_OFF = 377,
    SQL_SET = 378,
    SQL_TBL = 379,
    SQL_TOP = 380,
    SQL_AS = 381,
    SQL_BY = 382,
    SQL_IF = 383,
    SQL_IN = 384,
    SQL_IS = 385,
    SQL_OF = 386,
    SQL_ON = 387,
    SQL_OR = 388,
    SQL_TO = 389,
    SQL_ARRAY = 390,
    SQL_CONCAT = 391,
    SQL_ILIKE = 392,
    SQL_SECOND = 393,
    SQL_MINUTE = 394,
    SQL_HOUR = 395,
    SQL_DAY = 396,
    SQL_MONTH = 397,
    SQL_YEAR = 398,
    SQL_TRUE = 399,
    SQL_FALSE = 400,
    SQL_ESCAPED = 401,
    SQL_DATA = 402,
    SQL_INFILE = 403,
    SQL_CONCURRENT = 404,
    SQL_REPLACE = 405,
    SQL_PARTITION = 406,
    SQL_FIELDS = 407,
    SQL_TERMINATED = 408,
    SQL_OPTIONALLY = 409,
    SQL_ENCLOSED = 410,
    SQL_LINES = 411,
    SQL_ROWS = 412,
    SQL_STARTING = 413,
    SQL_EQUALS = 414,
    SQL_NOTEQUALS = 415,
    SQL_LESS = 416,
    SQL_GREATER = 417,
    SQL_LESSEQ = 418,
    SQL_GREATEREQ = 419,
    SQL_NOTNULL = 420,
    SQL_UMINUS = 421
  };
#endif

/* Value type.  */
#if ! defined HSQL_STYPE && ! defined HSQL_STYPE_IS_DECLARED

union HSQL_STYPE
{
#line 95 "bison_parser.y" /* yacc.c:353  */

	double fval;
	int64_t ival;
	char* sval;
	uintmax_t uval;
	bool bval;

	hsql::SQLStatement* statement;
	hsql::SelectStatement* 	select_stmt;
	hsql::ImportStatement* 	import_stmt;
	hsql::CreateStatement* 	create_stmt;
	hsql::InsertStatement* 	insert_stmt;
	hsql::DeleteStatement* 	delete_stmt;
	hsql::UpdateStatement* 	update_stmt;
	hsql::DropStatement*   	drop_stmt;
	hsql::PrepareStatement* prep_stmt;
	hsql::AlterStatement* alter_stmt;
	hsql::ExecuteStatement* exec_stmt;
	hsql::ShowStatement*    show_stmt;

	hsql::TableName table_name;
	hsql::DatabaseName db_name;
	hsql::TableRef* table;
	hsql::Expr* expr;
	hsql::OrderDescription* order;
	hsql::OrderType order_type;
	hsql::DatetimeField datetime_field;
	hsql::LimitDescription* limit;
	hsql::ColumnDefinition* column_t;
	hsql::ColumnType column_type_t;
	hsql::GroupByDescription* group_t;
	hsql::UpdateClause* update_t;
	hsql::Alias* alias_t;

	std::vector<hsql::SQLStatement*>* stmt_vec;

	std::vector<char*>* str_vec;
	std::vector<hsql::TableRef*>* table_vec;
	std::vector<hsql::ColumnDefinition*>* column_vec;
	std::vector<hsql::UpdateClause*>* update_vec;
	std::vector<hsql::Expr*>* expr_vec;
	std::vector<hsql::OrderDescription*>* order_vec;

#line 394 "bison_parser.cpp" /* yacc.c:353  */
};

typedef union HSQL_STYPE HSQL_STYPE;
# define HSQL_STYPE_IS_TRIVIAL 1
# define HSQL_STYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined HSQL_LTYPE && ! defined HSQL_LTYPE_IS_DECLARED
typedef struct HSQL_LTYPE HSQL_LTYPE;
struct HSQL_LTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define HSQL_LTYPE_IS_DECLARED 1
# define HSQL_LTYPE_IS_TRIVIAL 1
#endif



int hsql_parse (hsql::SQLParserResult* result, yyscan_t scanner);

#endif /* !YY_HSQL_BISON_PARSER_H_INCLUDED  */



#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined HSQL_LTYPE_IS_TRIVIAL && HSQL_LTYPE_IS_TRIVIAL \
             && defined HSQL_STYPE_IS_TRIVIAL && HSQL_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  68
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   658

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  184
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  112
/* YYNRULES -- Number of rules.  */
#define YYNRULES  283
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  519

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   421

#define YYTRANSLATE(YYX)                                                \
  ((unsigned) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,   173,     2,     2,
     178,   179,   171,   169,   182,   170,   180,   172,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,   181,
     162,   159,   163,   183,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   176,     2,   177,   174,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   160,   161,   164,   165,   166,   167,
     168,   175
};

#if HSQL_DEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   263,   263,   284,   290,   299,   303,   307,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   333,
     334,   339,   340,   344,   348,   360,   367,   370,   374,   386,
     395,   399,   408,   417,   418,   419,   423,   424,   425,   429,
     430,   434,   435,   436,   440,   441,   442,   446,   447,   448,
     458,   461,   464,   479,   487,   495,   503,   508,   516,   517,
     521,   522,   526,   527,   531,   537,   538,   539,   540,   541,
     542,   543,   544,   548,   549,   550,   561,   567,   573,   578,
     586,   587,   596,   608,   609,   613,   614,   618,   619,   623,
     636,   645,   658,   659,   660,   661,   665,   666,   676,   687,
     688,   692,   707,   714,   721,   729,   730,   734,   735,   745,
     746,   747,   764,   765,   769,   770,   774,   784,   801,   805,
     806,   807,   811,   812,   816,   828,   829,   833,   837,   838,
     841,   846,   847,   851,   856,   860,   861,   864,   865,   869,
     870,   874,   878,   879,   880,   886,   887,   891,   892,   893,
     894,   895,   896,   897,   898,   905,   906,   910,   911,   915,
     916,   920,   930,   931,   932,   933,   934,   938,   939,   940,
     941,   942,   943,   944,   945,   946,   947,   951,   952,   956,
     957,   958,   959,   960,   964,   965,   966,   967,   968,   969,
     970,   971,   972,   973,   974,   978,   979,   983,   984,   985,
     986,   992,   993,   994,   995,   999,  1000,  1004,  1005,  1009,
    1010,  1011,  1012,  1013,  1014,  1015,  1019,  1020,  1024,  1028,
    1029,  1030,  1031,  1032,  1033,  1036,  1040,  1044,  1048,  1049,
    1050,  1051,  1055,  1056,  1057,  1058,  1059,  1063,  1067,  1068,
    1072,  1073,  1077,  1081,  1085,  1097,  1098,  1108,  1109,  1113,
    1114,  1123,  1124,  1129,  1140,  1149,  1150,  1155,  1160,  1161,
    1166,  1167,  1171,  1172,  1177,  1178,  1186,  1194,  1204,  1223,
    1224,  1225,  1226,  1227,  1228,  1229,  1230,  1231,  1232,  1237,
    1246,  1247,  1252,  1253
};
#endif

#if HSQL_DEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "IDENTIFIER", "STRING", "FLOATVAL",
  "INTVAL", "DEALLOCATE", "PARAMETERS", "INTERSECT", "TEMPORARY",
  "TIMESTAMP", "DISTINCT", "NVARCHAR", "RESTRICT", "TRUNCATE", "ANALYZE",
  "BETWEEN", "CASCADE", "COLUMNS", "CONTROL", "DEFAULT", "EXECUTE",
  "EXPLAIN", "HISTORY", "INTEGER", "NATURAL", "PREPARE", "PRIMARY",
  "SCHEMAS", "SPATIAL", "VARCHAR", "VIRTUAL", "BEFORE", "COLUMN", "CREATE",
  "DELETE", "DIRECT", "DOUBLE", "ESCAPE", "EXCEPT", "EXISTS", "EXTRACT",
  "GLOBAL", "HAVING", "IMPORT", "INSERT", "ISNULL", "OFFSET", "RENAME",
  "SCHEMA", "SELECT", "SORTED", "TABLES", "UNIQUE", "UNLOAD", "UPDATE",
  "VALUES", "AFTER", "ALTER", "CROSS", "DELTA", "FLOAT", "GROUP", "INDEX",
  "INNER", "LIMIT", "LOCAL", "MERGE", "MINUS", "ORDER", "OUTER", "RIGHT",
  "TABLE", "UNION", "USING", "WHERE", "CALL", "CASE", "CHAR", "DATE",
  "DESC", "DROP", "ELSE", "FILE", "FROM", "FULL", "HASH", "HINT", "INTO",
  "JOIN", "LEFT", "LIKE", "LOAD", "LONG", "NULL", "PLAN", "SHOW", "TEXT",
  "THEN", "TIME", "VIEW", "WHEN", "WITH", "LOW_PRIORITY", "DELAYED",
  "HIGH_PRIORITY", "QUICK", "IGNORE", "DATABASES", "DATABASE", "CHARACTER",
  "ADD", "ALL", "AND", "ASC", "CSV", "END", "FOR", "INT", "KEY", "NOT",
  "OFF", "SET", "TBL", "TOP", "AS", "BY", "IF", "IN", "IS", "OF", "ON",
  "OR", "TO", "ARRAY", "CONCAT", "ILIKE", "SECOND", "MINUTE", "HOUR",
  "DAY", "MONTH", "YEAR", "TRUE", "FALSE", "ESCAPED", "DATA", "INFILE",
  "CONCURRENT", "REPLACE", "PARTITION", "FIELDS", "TERMINATED",
  "OPTIONALLY", "ENCLOSED", "LINES", "ROWS", "STARTING", "'='", "EQUALS",
  "NOTEQUALS", "'<'", "'>'", "LESS", "GREATER", "LESSEQ", "GREATEREQ",
  "NOTNULL", "'+'", "'-'", "'*'", "'/'", "'%'", "'^'", "UMINUS", "'['",
  "']'", "'('", "')'", "'.'", "';'", "','", "'?'", "$accept", "input",
  "statement_list", "statement", "preparable_statement", "opt_hints",
  "hint_list", "hint", "prepare_statement", "prepare_target_query",
  "execute_statement", "import_statement", "import_file_type", "file_path",
  "load_statement", "opt_low_priority_or_concurrent",
  "opt_replace_or_ignore", "opt_local", "opt_fields_or_columns",
  "opt_lines", "opt_ignore_lines_rows", "show_statement",
  "create_statement", "opt_temporary", "opt_not_exists",
  "column_def_commalist", "column_def", "column_type",
  "opt_column_nullable", "drop_statement", "opt_exists",
  "delete_statement", "opt_low_priority", "opt_quick", "opt_ignore",
  "truncate_statement", "insert_statement", "opt_priority",
  "opt_column_list", "update_statement", "update_clause_commalist",
  "update_clause", "alter_statement", "opt_default", "opt_equal",
  "select_statement", "select_with_paren", "select_paren_or_clause",
  "select_no_paren", "set_operator", "set_type", "opt_all",
  "select_clause", "opt_distinct", "select_list", "opt_from_clause",
  "from_clause", "opt_where", "opt_group", "opt_having", "opt_order",
  "order_list", "order_desc", "opt_order_type", "opt_top", "opt_limit",
  "expr_list", "opt_literal_list", "literal_list", "expr_alias", "expr",
  "operand", "scalar_expr", "unary_expr", "binary_expr", "logic_expr",
  "in_expr", "case_expr", "case_list", "exists_expr", "comp_expr",
  "function_expr", "extract_expr", "datetime_field", "array_expr",
  "array_index", "between_expr", "column_name", "literal",
  "string_literal", "bool_literal", "num_literal", "int_literal",
  "null_literal", "param_expr", "table_ref", "table_ref_atomic",
  "nonjoin_table_ref_atomic", "table_ref_commalist", "table_ref_name",
  "table_ref_name_no_alias", "table_name", "db_name", "table_alias",
  "opt_table_alias", "alias", "opt_alias", "join_clause", "opt_join_type",
  "join_condition", "opt_semicolon", "ident_commalist", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,    61,
     414,   415,    60,    62,   416,   417,   418,   419,   420,    43,
      45,    42,    47,    37,    94,   421,    91,    93,    40,    41,
      46,    59,    44,    63
};
# endif

#define YYPACT_NINF -353

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-353)))

#define YYTABLE_NINF -279

#define yytable_value_is_error(Yytable_value) \
  (!!((Yytable_value) == (-279)))

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      14,    15,    28,    64,   127,   152,    59,   109,   -53,    94,
      59,    16,    96,   123,    11,   -31,   260,   105,  -353,   206,
     206,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,    18,  -353,    23,   308,   135,  -353,   138,
     243,  -353,   189,   189,   189,   268,  -353,   226,   238,  -353,
    -353,  -353,   261,   368,   369,   261,   381,    28,   381,   257,
     257,   257,   -67,    28,  -353,  -353,   209,   213,  -353,    14,
    -353,   305,  -353,  -353,  -353,  -353,  -353,   -31,   281,   269,
     -31,    50,  -353,   392,    20,   394,   278,   381,    28,   381,
     189,  -353,   261,  -353,   316,  -353,   312,  -353,  -353,  -353,
     185,    28,  -353,   384,   299,   384,   371,    28,    28,   381,
    -353,  -353,   346,  -353,  -353,  -353,  -353,   236,  -353,   349,
    -353,  -353,  -353,   185,   349,   368,     9,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,   237,   239,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,   379,  -353,   244,  -353,
      28,   338,   422,    28,   143,   249,   250,    72,   262,   253,
     232,  -353,   154,   345,   255,  -353,    40,   288,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,   315,  -353,  -353,   320,   401,   328,  -353,
    -353,  -353,  -353,  -353,   293,   439,    50,   264,  -353,   131,
      50,  -353,   395,   396,   404,  -353,    20,  -353,   468,   347,
     -66,    28,   385,  -353,   244,     2,     6,   424,   327,   185,
     108,    69,   306,   232,   389,   185,   171,   298,   -79,     1,
     409,  -353,   185,  -353,   185,   477,   185,  -353,  -353,   232,
    -353,   232,    -8,   309,   103,   232,   232,   232,   232,   232,
     232,   232,   232,   232,   232,   232,   232,   232,   232,   232,
     368,   483,   365,   486,   367,   422,   314,    90,  -353,  -353,
     185,  -353,  -353,  -353,  -353,   368,   368,   368,  -353,  -353,
      99,   -31,   370,   486,   409,    28,    44,  -353,   185,  -353,
    -353,   317,  -353,  -353,  -353,  -353,  -353,  -353,   408,   126,
     137,   185,   185,  -353,   424,   403,   -74,  -353,  -353,   -31,
    -353,   222,  -353,   318,  -353,    30,  -353,   185,   443,  -353,
    -353,  -353,   393,   342,   361,   232,   321,   154,  -353,   434,
     341,   361,   361,   361,   361,   436,   436,   436,   436,   171,
     171,    39,    39,    39,  -102,   359,   380,   -58,  -353,   382,
     539,  -353,   382,   -69,    20,  -353,   439,  -353,  -353,  -353,
    -353,  -353,   535,  -353,   456,   157,  -353,  -353,  -353,   364,
    -353,   169,  -353,   185,   185,   185,  -353,   205,   162,   366,
    -353,   374,   453,  -353,  -353,  -353,   473,   475,   476,   464,
       1,   554,  -353,  -353,  -353,   163,   440,  -353,   232,   361,
     154,   390,   170,  -353,  -353,   185,   483,  -353,  -353,   185,
    -353,   397,  -353,  -353,   398,  -353,  -353,  -353,   113,   185,
    -353,  -353,   479,   179,  -353,  -353,   422,  -353,   486,    20,
    -353,    54,   163,   217,  -353,   185,  -353,    30,     1,  -353,
    -353,  -353,     1,   300,   400,   185,   389,   402,   180,  -353,
    -353,   163,  -353,   163,   565,   567,  -353,   484,  -353,   163,
     501,  -353,  -353,  -353,   191,  -353,  -353,   163,  -353,  -353,
      -3,   468,   -27,  -353,  -353,   405,   406,  -353,    28,  -353,
     410,   185,   196,   185,  -353,  -353,  -353,    -6,     7,   163,
    -353,  -353,   163,   427,   429,   430,   407,   411,   462,   465,
      65,   485,  -353,   579,   587,   467,   469,   589,   244,  -353,
    -353,   593,   596,    95,  -353,  -353,  -353,  -353,  -353
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,     0,     0,     0,     0,    59,    84,     0,    95,   146,
      84,     0,     0,     0,     0,     0,     0,   281,     3,    20,
      20,    18,     9,    10,     7,    11,    17,    13,    15,    12,
      16,    14,     8,   109,   110,   138,     0,   255,    89,    27,
       0,    58,    61,    61,    61,     0,    83,    86,     0,    92,
      93,    94,    88,     0,   126,    88,     0,     0,     0,    81,
      81,    81,    35,     0,    50,    51,     0,     0,     1,   280,
       2,     0,     6,     5,   120,   121,   119,     0,   123,     0,
       0,   154,    79,     0,   158,     0,     0,     0,     0,     0,
      61,    85,    88,    30,     0,    87,     0,   242,   145,   125,
       0,     0,   257,   106,     0,   106,     0,     0,     0,     0,
      33,    34,    40,    52,   113,   112,     4,     0,   114,   138,
     115,   122,   118,     0,   138,     0,     0,   116,   256,   237,
     240,   243,   238,   239,   244,     0,   157,   159,   232,   233,
     234,   241,   235,   236,    26,    25,     0,    57,    97,    56,
       0,     0,     0,     0,   228,     0,     0,     0,     0,     0,
       0,   230,     0,   129,   127,   155,   265,   162,   169,   170,
     171,   164,   166,   172,   165,   184,   173,   174,   175,   168,
     163,   177,   178,     0,   254,   105,     0,     0,     0,    80,
      76,    77,    78,    39,     0,     0,   154,   137,   139,   144,
     154,   149,   151,   150,   147,    28,     0,    60,     0,     0,
       0,     0,     0,    31,    97,   126,     0,     0,     0,     0,
       0,     0,     0,     0,   180,     0,   179,     0,     0,     0,
     132,   128,     0,   263,     0,     0,     0,   264,   161,     0,
     181,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    23,     0,    21,   111,
       0,   143,   142,   141,   117,     0,     0,     0,   160,   282,
       0,     0,     0,     0,   132,     0,     0,   216,     0,   229,
     231,     0,   219,   220,   221,   222,   223,   224,     0,     0,
       0,     0,     0,   203,     0,     0,     0,   176,   167,     0,
     130,   245,   247,     0,   249,   261,   248,     0,   134,   156,
     195,   262,   196,     0,   191,     0,     0,     0,   182,     0,
     194,   193,   209,   210,   211,   212,   213,   214,   215,   186,
     185,   188,   187,   189,   190,     0,     0,   132,    99,   108,
       0,   104,   108,    38,     0,    19,     0,   140,   153,   152,
     148,    96,     0,    55,     0,     0,    62,    82,    29,     0,
      91,     0,   207,     0,     0,     0,   201,     0,     0,     0,
     225,     0,     0,   277,   269,   275,   273,   276,   271,     0,
       0,     0,   260,   253,   258,   131,     0,   124,     0,   192,
       0,     0,     0,   183,   226,     0,     0,    98,   107,     0,
      66,     0,    69,    68,     0,    67,    72,    65,    75,     0,
      37,    36,     0,     0,    22,   283,     0,    54,     0,     0,
     217,     0,   205,     0,   204,     0,   208,   261,     0,   272,
     274,   270,     0,   246,   262,     0,   227,     0,     0,   199,
     197,   101,   100,   103,     0,     0,    73,     0,    64,   102,
       0,    24,    53,    63,     0,   218,   202,   206,   250,   266,
     278,     0,   136,   200,   198,     0,     0,    74,     0,    90,
       0,     0,     0,     0,   133,    70,    71,    43,     0,   279,
     267,   259,   135,     0,     0,    46,   228,     0,     0,     0,
       0,    49,   268,     0,     0,     0,     0,     0,    97,    42,
      41,     0,     0,     0,    32,    45,    44,    47,    48
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -353,  -353,  -353,   542,  -353,   584,  -353,   258,  -353,  -353,
    -353,  -353,  -353,  -254,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,     4,  -353,  -267,  -353,  -353,  -353,
     240,  -353,   603,  -353,    36,  -353,  -353,  -353,  -211,  -353,
    -353,   210,  -353,   510,   265,  -253,   167,   540,   -13,   586,
    -353,  -353,   302,   412,  -353,  -353,  -353,  -262,  -353,  -353,
     166,  -353,   352,  -353,  -353,   -20,  -217,  -353,  -342,   387,
    -117,  -114,  -353,  -353,  -353,  -353,  -353,  -353,   414,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,   136,   -83,  -147,
    -353,  -353,   -46,  -353,  -353,  -353,  -352,   187,  -353,  -353,
    -353,    -2,   112,  -353,   186,   460,  -353,  -353,  -353,  -353,
    -353,   158
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    16,    17,    18,    19,    72,   267,   268,    20,   145,
      21,    22,    94,   212,    23,   112,   422,   194,   495,   501,
     508,    24,    25,    45,    87,   365,   351,   418,   458,    26,
     107,    27,    47,    92,    96,    28,    29,    52,   209,    30,
     347,   348,    31,   186,   409,    32,    33,   119,    34,    77,
      78,   122,    35,   100,   163,   230,   231,   318,   397,   484,
      81,   197,   198,   273,    54,   127,   164,   135,   136,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   221,   174,
     175,   176,   177,   298,   178,   179,   180,   181,   182,   138,
     139,   140,   141,   142,   143,   310,   311,   312,   313,   314,
     183,   315,   103,   392,   393,   394,   238,   316,   389,   490,
      70,   280
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      38,   137,    67,   286,    37,   213,   199,    98,   306,   289,
     496,   353,   423,   493,    99,    97,   366,   483,   317,   282,
       9,     1,   367,   382,   129,   130,    97,    74,   363,     2,
      63,    37,    74,   233,   245,   234,     3,   110,   443,   420,
     220,     4,    36,   233,   224,   228,   226,    88,    89,     5,
       6,    49,    50,    51,   236,   104,   381,   383,    75,     7,
       8,   113,   384,    75,    64,     9,    56,    39,   385,   386,
      10,   371,   480,    11,   260,   154,   129,   130,    97,   201,
     204,   421,   111,   387,   325,   407,   148,   464,   388,    57,
     470,   101,    76,    79,   150,     9,    12,    76,   125,   184,
     308,   369,   299,   380,   202,   190,   191,    13,   232,   224,
     402,    14,   283,   155,   156,   131,   126,   320,   213,   322,
      65,   326,   203,   278,   406,   323,    58,   324,   151,   481,
      40,   330,   331,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   341,   342,   343,   344,   494,    15,   210,   227,
     157,   214,   301,   199,   234,   232,   391,   154,   129,   130,
      97,   463,    41,    46,   132,   133,   235,   131,   234,    59,
     105,   302,   462,   236,   219,   245,   269,   290,   161,   309,
     274,   287,    66,   448,   377,   378,   303,   236,   154,   129,
     130,    97,    15,   158,    48,   155,   156,    60,   328,   147,
     395,   149,    42,   134,   291,     9,    61,   159,   456,   284,
     219,   399,   271,   259,   345,   260,   132,   133,   505,    53,
     375,   192,   234,   506,   329,   374,   155,   156,   472,   358,
     359,   360,   157,   465,   457,   154,   129,   130,    97,   302,
     234,   236,   160,   161,   118,   234,   272,   118,   382,   131,
     162,   517,   518,    43,   376,   134,   431,   432,   433,   236,
      68,   435,    44,   157,   236,   154,   129,   130,    97,   355,
      62,   137,   356,   370,   156,   158,   234,   234,   361,   213,
     131,   362,   383,   368,   446,   196,    69,   384,   451,   159,
     200,   379,   453,   385,   386,   236,   236,   514,   132,   133,
     108,   109,   459,   222,   156,   239,   158,   245,   387,    71,
     157,    82,  -278,   388,   401,    83,    84,    86,   467,   234,
     159,   215,   434,   216,   160,   161,   382,   131,    85,   132,
     133,   234,   162,    91,   466,   240,   427,   134,   236,   428,
     157,    90,   256,   257,   258,   259,   137,   260,   430,   450,
     236,   232,   232,   223,    93,   160,   161,   131,   461,   474,
     383,   206,   232,   162,   489,   384,   492,   159,   134,    95,
     479,   385,   386,   206,    97,   491,   132,   133,   362,   120,
     241,    99,   120,   223,   102,   106,   387,   447,   114,   240,
    -278,   388,   115,   117,   121,   128,   123,   159,   144,   146,
     152,   153,   160,   161,  -251,   185,   132,   133,   240,   242,
     162,   187,   189,   193,   195,   134,   205,   243,   244,    79,
     207,   206,   208,   211,   245,   246,   129,   217,   218,   225,
     229,   262,   160,   161,   241,   263,   240,   232,   261,   264,
     162,   265,   266,   275,   276,   134,   270,   247,   248,   249,
     250,   251,   277,  -279,   252,   253,   398,   254,   255,   256,
     257,   258,   259,   305,   260,   292,   293,   294,   295,   296,
     297,   279,   244,   281,   285,     9,   487,   307,   245,   246,
     321,   241,  -252,   240,   304,   317,   346,   327,   349,   350,
     352,   244,   354,   373,   364,   325,   372,   245,  -279,   400,
     390,   247,   248,   249,   250,   251,   396,   234,   252,   253,
     305,   254,   255,   256,   257,   258,   259,   260,   260,   244,
    -279,  -279,  -279,   250,   251,   245,   246,   252,   253,   403,
     254,   255,   256,   257,   258,   259,   404,   260,   425,   405,
     426,   408,   429,   438,   439,   436,   440,   441,   247,   248,
     249,   250,   251,   437,   442,   252,   253,   444,   254,   255,
     256,   257,   258,   259,   410,   260,   244,   445,   460,   449,
     411,   475,   245,   476,   478,   454,   455,   412,   471,   477,
     498,   473,   499,   509,   485,   486,   500,   216,   488,   503,
     502,   510,   504,   507,   511,   513,   512,   515,  -279,  -279,
     516,   413,  -279,  -279,    73,   254,   255,   256,   257,   258,
     259,   116,   260,    55,   424,   188,   452,   419,   414,   319,
     124,    80,   357,   468,   497,   469,   237,   288,     0,   482,
       0,     0,     0,   415,   300,     0,     0,   416,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   417
};

static const yytype_int16 yycheck[] =
{
       2,    84,    15,   214,     3,   152,   123,    53,   225,     3,
       3,   265,   354,    19,    12,     6,   283,    44,    76,    85,
      51,     7,   284,    26,     4,     5,     6,     9,   281,    15,
      19,     3,     9,     3,   136,   114,    22,   104,   390,   108,
     157,    27,    27,     3,   158,   162,   160,    43,    44,    35,
      36,   104,   105,   106,   133,    57,   309,    60,    40,    45,
      46,    63,    65,    40,    53,    51,    50,     3,    71,    72,
      56,   288,    75,    59,   176,     3,     4,     5,     6,   125,
     126,   150,   149,    86,    92,   347,    88,   429,    91,    73,
     442,    55,    74,    70,    90,    51,    82,    74,    48,   101,
     179,    57,   219,   177,    95,   107,   108,    93,   182,   223,
     327,    97,   178,    41,    42,    95,    66,   234,   265,   236,
     109,   129,   113,   206,   182,   239,   110,   241,    92,   132,
       3,   245,   246,   247,   248,   249,   250,   251,   252,   253,
     254,   255,   256,   257,   258,   259,   152,   178,   150,   162,
      78,   153,    83,   270,   114,   182,   126,     3,     4,     5,
       6,   428,    10,   104,   144,   145,   126,    95,   114,    73,
      58,   102,   426,   133,   102,   136,   196,   171,   171,   178,
     200,   179,    15,   400,   301,   302,   117,   133,     3,     4,
       5,     6,   178,   121,    85,    41,    42,   101,    95,    87,
     317,    89,    50,   183,   217,    51,   110,   135,    95,   211,
     102,   325,    81,   174,   260,   176,   144,   145,   153,   125,
      83,   109,   114,   158,   121,    99,    41,    42,   445,   275,
     276,   277,    78,   179,   121,     3,     4,     5,     6,   102,
     114,   133,   170,   171,    77,   114,   115,    80,    26,    95,
     178,   156,   157,   101,   117,   183,   373,   374,   375,   133,
       0,    99,   110,    78,   133,     3,     4,     5,     6,   179,
     147,   354,   182,   286,    42,   121,   114,   114,   179,   426,
      95,   182,    60,   285,   398,   119,   181,    65,   405,   135,
     124,   304,   409,    71,    72,   133,   133,   508,   144,   145,
      60,    61,   419,    41,    42,    17,   121,   136,    86,   103,
      78,     3,    90,    91,   327,   180,   178,   128,   435,   114,
     135,   178,   117,   180,   170,   171,    26,    95,    85,   144,
     145,   114,   178,   107,   117,    47,   179,   183,   133,   182,
      78,    73,   171,   172,   173,   174,   429,   176,   179,   179,
     133,   182,   182,   121,   116,   170,   171,    95,   179,   179,
      60,   182,   182,   178,   481,    65,   483,   135,   183,   108,
     179,    71,    72,   182,     6,   179,   144,   145,   182,    77,
      92,    12,    80,   121,     3,   128,    86,   400,   179,    47,
      90,    91,   179,    88,   113,     3,   127,   135,     4,   121,
      84,    89,   170,   171,   182,    21,   144,   145,    47,   121,
     178,   112,    41,    67,   178,   183,   179,   129,   130,    70,
      41,   182,   178,    85,   136,   137,     4,   178,   178,   176,
      85,   111,   170,   171,    92,    34,    47,   182,   123,   111,
     178,   148,     3,    48,    48,   183,   182,   159,   160,   161,
     162,   163,    48,    92,   166,   167,   114,   169,   170,   171,
     172,   173,   174,   121,   176,   138,   139,   140,   141,   142,
     143,     3,   130,   126,    89,    51,   478,   179,   136,   137,
       3,    92,   182,    47,   178,    76,     3,   178,   123,     3,
     123,   130,   178,    85,   124,    92,   179,   136,   137,   178,
     182,   159,   160,   161,   162,   163,    63,   114,   166,   167,
     121,   169,   170,   171,   172,   173,   174,   176,   176,   130,
     159,   160,   161,   162,   163,   136,   137,   166,   167,    95,
     169,   170,   171,   172,   173,   174,   177,   176,     3,   159,
      84,   159,   178,    90,    71,   179,    71,    71,   159,   160,
     161,   162,   163,   179,    90,   166,   167,     3,   169,   170,
     171,   172,   173,   174,    25,   176,   130,   127,    89,   179,
      31,     6,   136,     6,    73,   178,   178,    38,   178,    95,
     153,   179,   153,     4,   179,   179,   156,   180,   178,   127,
     179,     4,   127,   108,   127,     6,   127,     4,   162,   163,
       4,    62,   166,   167,    20,   169,   170,   171,   172,   173,
     174,    69,   176,    10,   356,   105,   406,   352,    79,   232,
      80,    35,   270,   437,   488,   438,   166,   215,    -1,   471,
      -1,    -1,    -1,    94,   220,    -1,    -1,    98,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   119
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     7,    15,    22,    27,    35,    36,    45,    46,    51,
      56,    59,    82,    93,    97,   178,   185,   186,   187,   188,
     192,   194,   195,   198,   205,   206,   213,   215,   219,   220,
     223,   226,   229,   230,   232,   236,    27,     3,   285,     3,
       3,    10,    50,   101,   110,   207,   104,   216,    85,   104,
     105,   106,   221,   125,   248,   216,    50,    73,   110,    73,
     101,   110,   147,    19,    53,   109,   230,   232,     0,   181,
     294,   103,   189,   189,     9,    40,    74,   233,   234,    70,
     233,   244,     3,   180,   178,    85,   128,   208,   208,   208,
      73,   107,   217,   116,   196,   108,   218,     6,   276,    12,
     237,   218,     3,   286,   285,   286,   128,   214,   214,   214,
     104,   149,   199,   285,   179,   179,   187,    88,   230,   231,
     236,   113,   235,   127,   231,    48,    66,   249,     3,     4,
       5,    95,   144,   145,   183,   251,   252,   272,   273,   274,
     275,   276,   277,   278,     4,   193,   121,   286,   285,   286,
     208,   218,    84,    89,     3,    41,    42,    78,   121,   135,
     170,   171,   178,   238,   250,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   263,   264,   265,   266,   268,   269,
     270,   271,   272,   284,   285,    21,   227,   112,   227,    41,
     285,   285,   286,    67,   201,   178,   244,   245,   246,   254,
     244,   276,    95,   113,   276,   179,   182,    41,   178,   222,
     285,    85,   197,   273,   285,   178,   180,   178,   178,   102,
     254,   262,    41,   121,   255,   176,   255,   232,   254,    85,
     239,   240,   182,     3,   114,   126,   133,   289,   290,    17,
      47,    92,   121,   129,   130,   136,   137,   159,   160,   161,
     162,   163,   166,   167,   169,   170,   171,   172,   173,   174,
     176,   123,   111,    34,   111,   148,     3,   190,   191,   249,
     182,    81,   115,   247,   249,    48,    48,    48,   272,     3,
     295,   126,    85,   178,   285,    89,   222,   179,   237,     3,
     171,   232,   138,   139,   140,   141,   142,   143,   267,   254,
     262,    83,   102,   117,   178,   121,   250,   179,   179,   178,
     279,   280,   281,   282,   283,   285,   291,    76,   241,   253,
     254,     3,   254,   255,   255,    92,   129,   178,    95,   121,
     255,   255,   255,   255,   255,   255,   255,   255,   255,   255,
     255,   255,   255,   255,   255,   276,     3,   224,   225,   123,
       3,   210,   123,   197,   178,   179,   182,   246,   276,   276,
     276,   179,   182,   229,   124,   209,   210,   241,   285,    57,
     232,   250,   179,    85,    99,    83,   117,   254,   254,   232,
     177,   229,    26,    60,    65,    71,    72,    86,    91,   292,
     182,   126,   287,   288,   289,   254,    63,   242,   114,   255,
     178,   232,   250,    95,   177,   159,   182,   241,   159,   228,
      25,    31,    38,    62,    79,    94,    98,   119,   211,   228,
     108,   150,   200,   252,   191,     3,    84,   179,   182,   178,
     179,   254,   254,   254,   117,    99,   179,   179,    90,    71,
      71,    71,    90,   280,     3,   127,   255,   232,   250,   179,
     179,   254,   225,   254,   178,   178,    95,   121,   212,   254,
      89,   179,   197,   210,   252,   179,   117,   254,   288,   281,
     280,   178,   250,   179,   179,     6,     6,    95,    73,   179,
      75,   132,   295,    44,   243,   179,   179,   285,   178,   254,
     293,   179,   254,    19,   152,   202,     3,   271,   153,   153,
     156,   203,   179,   127,   127,   153,   158,   108,   204,     4,
       4,   127,   127,     6,   222,     4,     4,   156,   157
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   184,   185,   186,   186,   187,   187,   187,   188,   188,
     188,   188,   188,   188,   188,   188,   188,   188,   188,   189,
     189,   190,   190,   191,   191,   192,   193,   194,   194,   195,
     196,   197,   198,   199,   199,   199,   200,   200,   200,   201,
     201,   202,   202,   202,   203,   203,   203,   204,   204,   204,
     205,   205,   205,   206,   206,   206,   206,   206,   207,   207,
     208,   208,   209,   209,   210,   211,   211,   211,   211,   211,
     211,   211,   211,   212,   212,   212,   213,   213,   213,   213,
     214,   214,   215,   216,   216,   217,   217,   218,   218,   219,
     220,   220,   221,   221,   221,   221,   222,   222,   223,   224,
     224,   225,   226,   226,   226,   227,   227,   228,   228,   229,
     229,   229,   230,   230,   231,   231,   232,   232,   233,   234,
     234,   234,   235,   235,   236,   237,   237,   238,   239,   239,
     240,   241,   241,   242,   242,   243,   243,   244,   244,   245,
     245,   246,   247,   247,   247,   248,   248,   249,   249,   249,
     249,   249,   249,   249,   249,   250,   250,   251,   251,   252,
     252,   253,   254,   254,   254,   254,   254,   255,   255,   255,
     255,   255,   255,   255,   255,   255,   255,   256,   256,   257,
     257,   257,   257,   257,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   259,   259,   260,   260,   260,
     260,   261,   261,   261,   261,   262,   262,   263,   263,   264,
     264,   264,   264,   264,   264,   264,   265,   265,   266,   267,
     267,   267,   267,   267,   267,   268,   269,   270,   271,   271,
     271,   271,   272,   272,   272,   272,   272,   273,   274,   274,
     275,   275,   276,   277,   278,   279,   279,   280,   280,   281,
     281,   282,   282,   283,   284,   285,   285,   286,   287,   287,
     288,   288,   289,   289,   290,   290,   291,   291,   291,   292,
     292,   292,   292,   292,   292,   292,   292,   292,   292,   293,
     294,   294,   295,   295
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     3,     2,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     5,
       0,     1,     3,     1,     4,     4,     1,     2,     5,     7,
       1,     1,    14,     1,     1,     0,     1,     1,     0,     1,
       0,     4,     4,     0,     4,     4,     0,     3,     3,     0,
       2,     2,     3,     9,     8,     7,     4,     4,     1,     0,
       3,     0,     1,     3,     3,     1,     1,     1,     1,     1,
       4,     4,     1,     1,     2,     0,     4,     4,     4,     3,
       2,     0,     7,     1,     0,     1,     0,     1,     0,     2,
      10,     7,     1,     1,     1,     0,     3,     0,     7,     1,
       3,     3,     8,     8,     6,     1,     0,     1,     0,     1,
       1,     5,     3,     3,     1,     1,     3,     5,     2,     1,
       1,     1,     1,     0,     7,     1,     0,     1,     1,     0,
       2,     2,     0,     4,     0,     2,     0,     3,     0,     1,
       3,     2,     1,     1,     0,     2,     0,     2,     4,     2,
       2,     2,     4,     4,     0,     1,     3,     1,     0,     1,
       3,     2,     1,     1,     1,     1,     1,     3,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     1,     2,
       2,     2,     3,     4,     1,     3,     3,     3,     3,     3,
       3,     3,     4,     3,     3,     3,     3,     5,     6,     5,
       6,     4,     6,     3,     5,     4,     5,     4,     5,     3,
       3,     3,     3,     3,     3,     3,     3,     5,     6,     1,
       1,     1,     1,     1,     1,     4,     4,     5,     1,     3,
       1,     3,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     1,     1,
       4,     1,     3,     2,     1,     1,     3,     1,     1,     5,
       1,     0,     2,     1,     1,     0,     4,     6,     8,     1,
       2,     1,     2,     1,     2,     1,     1,     1,     0,     1,
       1,     0,     1,     3
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (&yylloc, result, scanner, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if HSQL_DEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined HSQL_LTYPE_IS_TRIVIAL && HSQL_LTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, Location, result, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, hsql::SQLParserResult* result, yyscan_t scanner)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  YYUSE (yylocationp);
  YYUSE (result);
  YYUSE (scanner);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, hsql::SQLParserResult* result, yyscan_t scanner)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyo, *yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yytype, yyvaluep, yylocationp, result, scanner);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, hsql::SQLParserResult* result, yyscan_t scanner)
{
  unsigned long yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       , result, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, result, scanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !HSQL_DEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !HSQL_DEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return (YYSIZE_T) (yystpcpy (yyres, yystr) - yyres);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, hsql::SQLParserResult* result, yyscan_t scanner)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (result);
  YYUSE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yytype)
    {
          case 3: /* IDENTIFIER  */
#line 146 "bison_parser.y" /* yacc.c:1254  */
      { free( (((*yyvaluep).sval)) ); }
#line 1777 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 4: /* STRING  */
#line 146 "bison_parser.y" /* yacc.c:1254  */
      { free( (((*yyvaluep).sval)) ); }
#line 1783 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 5: /* FLOATVAL  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1789 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 6: /* INTVAL  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1795 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 186: /* statement_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).stmt_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).stmt_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).stmt_vec));
}
#line 1808 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 187: /* statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).statement)); }
#line 1814 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 188: /* preparable_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).statement)); }
#line 1820 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 189: /* opt_hints  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).expr_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).expr_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).expr_vec));
}
#line 1833 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 190: /* hint_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).expr_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).expr_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).expr_vec));
}
#line 1846 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 191: /* hint  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 1852 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 192: /* prepare_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).prep_stmt)); }
#line 1858 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 193: /* prepare_target_query  */
#line 146 "bison_parser.y" /* yacc.c:1254  */
      { free( (((*yyvaluep).sval)) ); }
#line 1864 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 194: /* execute_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).exec_stmt)); }
#line 1870 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 195: /* import_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).import_stmt)); }
#line 1876 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 196: /* import_file_type  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1882 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 197: /* file_path  */
#line 146 "bison_parser.y" /* yacc.c:1254  */
      { free( (((*yyvaluep).sval)) ); }
#line 1888 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 198: /* load_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).import_stmt)); }
#line 1894 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 205: /* show_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).show_stmt)); }
#line 1900 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 206: /* create_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).create_stmt)); }
#line 1906 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 207: /* opt_temporary  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1912 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 208: /* opt_not_exists  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1918 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 209: /* column_def_commalist  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).column_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).column_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).column_vec));
}
#line 1931 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 210: /* column_def  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).column_t)); }
#line 1937 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 211: /* column_type  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1943 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 212: /* opt_column_nullable  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1949 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 213: /* drop_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).drop_stmt)); }
#line 1955 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 214: /* opt_exists  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1961 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 215: /* delete_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).delete_stmt)); }
#line 1967 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 216: /* opt_low_priority  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1973 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 217: /* opt_quick  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1979 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 218: /* opt_ignore  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 1985 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 219: /* truncate_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).delete_stmt)); }
#line 1991 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 220: /* insert_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).insert_stmt)); }
#line 1997 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 221: /* opt_priority  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 2003 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 222: /* opt_column_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).str_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).str_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).str_vec));
}
#line 2016 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 223: /* update_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).update_stmt)); }
#line 2022 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 224: /* update_clause_commalist  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).update_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).update_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).update_vec));
}
#line 2035 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 225: /* update_clause  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).update_t)); }
#line 2041 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 226: /* alter_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).alter_stmt)); }
#line 2047 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 227: /* opt_default  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 2053 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 228: /* opt_equal  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 2059 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 229: /* select_statement  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).select_stmt)); }
#line 2065 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 230: /* select_with_paren  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).select_stmt)); }
#line 2071 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 231: /* select_paren_or_clause  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).select_stmt)); }
#line 2077 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 232: /* select_no_paren  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).select_stmt)); }
#line 2083 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 236: /* select_clause  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).select_stmt)); }
#line 2089 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 237: /* opt_distinct  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 2095 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 238: /* select_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).expr_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).expr_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).expr_vec));
}
#line 2108 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 239: /* opt_from_clause  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2114 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 240: /* from_clause  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2120 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 241: /* opt_where  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2126 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 242: /* opt_group  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).group_t)); }
#line 2132 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 243: /* opt_having  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2138 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 244: /* opt_order  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).order_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).order_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).order_vec));
}
#line 2151 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 245: /* order_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).order_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).order_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).order_vec));
}
#line 2164 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 246: /* order_desc  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).order)); }
#line 2170 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 247: /* opt_order_type  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 2176 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 248: /* opt_top  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).limit)); }
#line 2182 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 249: /* opt_limit  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).limit)); }
#line 2188 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 250: /* expr_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).expr_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).expr_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).expr_vec));
}
#line 2201 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 251: /* opt_literal_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).expr_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).expr_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).expr_vec));
}
#line 2214 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 252: /* literal_list  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).expr_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).expr_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).expr_vec));
}
#line 2227 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 253: /* expr_alias  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2233 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 254: /* expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2239 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 255: /* operand  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2245 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 256: /* scalar_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2251 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 257: /* unary_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2257 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 258: /* binary_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2263 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 259: /* logic_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2269 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 260: /* in_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2275 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 261: /* case_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2281 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 262: /* case_list  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2287 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 263: /* exists_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2293 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 264: /* comp_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2299 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 265: /* function_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2305 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 266: /* extract_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2311 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 267: /* datetime_field  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 2317 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 268: /* array_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2323 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 269: /* array_index  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2329 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 270: /* between_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2335 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 271: /* column_name  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2341 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 272: /* literal  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2347 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 273: /* string_literal  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2353 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 274: /* bool_literal  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2359 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 275: /* num_literal  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2365 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 276: /* int_literal  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2371 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 277: /* null_literal  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2377 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 278: /* param_expr  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2383 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 279: /* table_ref  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2389 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 280: /* table_ref_atomic  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2395 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 281: /* nonjoin_table_ref_atomic  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2401 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 282: /* table_ref_commalist  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).table_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).table_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).table_vec));
}
#line 2414 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 283: /* table_ref_name  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2420 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 284: /* table_ref_name_no_alias  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2426 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 285: /* table_name  */
#line 144 "bison_parser.y" /* yacc.c:1254  */
      { free( (((*yyvaluep).table_name).name) ); free( (((*yyvaluep).table_name).schema) ); }
#line 2432 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 286: /* db_name  */
#line 145 "bison_parser.y" /* yacc.c:1254  */
      { free( (((*yyvaluep).db_name).name) ); }
#line 2438 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 287: /* table_alias  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).alias_t)); }
#line 2444 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 288: /* opt_table_alias  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).alias_t)); }
#line 2450 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 289: /* alias  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).alias_t)); }
#line 2456 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 290: /* opt_alias  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).alias_t)); }
#line 2462 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 291: /* join_clause  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).table)); }
#line 2468 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 292: /* opt_join_type  */
#line 143 "bison_parser.y" /* yacc.c:1254  */
      { }
#line 2474 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 293: /* join_condition  */
#line 155 "bison_parser.y" /* yacc.c:1254  */
      { delete (((*yyvaluep).expr)); }
#line 2480 "bison_parser.cpp" /* yacc.c:1254  */
        break;

    case 295: /* ident_commalist  */
#line 147 "bison_parser.y" /* yacc.c:1254  */
      {
	if ((((*yyvaluep).str_vec)) != nullptr) {
		for (auto ptr : *(((*yyvaluep).str_vec))) {
			delete ptr;
		}
	}
	delete (((*yyvaluep).str_vec));
}
#line 2493 "bison_parser.cpp" /* yacc.c:1254  */
        break;


      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (hsql::SQLParserResult* result, yyscan_t scanner)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined HSQL_LTYPE_IS_TRIVIAL && HSQL_LTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.
       'yyls': related to locations.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yylsp = yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

/* User initialization code.  */
#line 73 "bison_parser.y" /* yacc.c:1429  */
{
	// Initialize
	yylloc.first_column = 0;
	yylloc.last_column = 0;
	yylloc.first_line = 0;
	yylloc.last_line = 0;
	yylloc.total_column = 0;
	yylloc.string_length = 0;
}

#line 2612 "bison_parser.cpp" /* yacc.c:1429  */
  yylsp[0] = yylloc;
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = (yytype_int16) yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = (YYSIZE_T) (yyssp - yyss + 1);

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yyls1, yysize * sizeof (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, &yylloc, scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 263 "bison_parser.y" /* yacc.c:1645  */
    {
			for (SQLStatement* stmt : *(yyvsp[-1].stmt_vec)) {
				// Transfers ownership of the statement.
				result->addStatement(stmt);
			}

			unsigned param_id = 0;
			for (void* param : yyloc.param_list) {
				if (param != nullptr) {
					Expr* expr = (Expr*) param;
					expr->ival = param_id;
					result->addParameter(expr);
					++param_id;
				}
			}
			delete (yyvsp[-1].stmt_vec);
		}
#line 2817 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 3:
#line 284 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyvsp[0].statement)->stringLength = yylloc.string_length;
			yylloc.string_length = 0;
			(yyval.stmt_vec) = new std::vector<SQLStatement*>();
			(yyval.stmt_vec)->push_back((yyvsp[0].statement));
		}
#line 2828 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 4:
#line 290 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyvsp[0].statement)->stringLength = yylloc.string_length;
			yylloc.string_length = 0;
			(yyvsp[-2].stmt_vec)->push_back((yyvsp[0].statement));
			(yyval.stmt_vec) = (yyvsp[-2].stmt_vec);
		}
#line 2839 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 5:
#line 299 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.statement) = (yyvsp[-1].prep_stmt);
			(yyval.statement)->hints = (yyvsp[0].expr_vec);
		}
#line 2848 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 6:
#line 303 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.statement) = (yyvsp[-1].statement);
			(yyval.statement)->hints = (yyvsp[0].expr_vec);
		}
#line 2857 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 7:
#line 307 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.statement) = (yyvsp[0].show_stmt);
		}
#line 2865 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 8:
#line 314 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].select_stmt); }
#line 2871 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 9:
#line 315 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].import_stmt); }
#line 2877 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 10:
#line 316 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].import_stmt); }
#line 2883 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 11:
#line 317 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].create_stmt); }
#line 2889 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 12:
#line 318 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].insert_stmt); }
#line 2895 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 13:
#line 319 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].delete_stmt); }
#line 2901 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 14:
#line 320 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].alter_stmt); }
#line 2907 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 15:
#line 321 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].delete_stmt); }
#line 2913 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 16:
#line 322 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].update_stmt); }
#line 2919 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 17:
#line 323 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].drop_stmt); }
#line 2925 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 18:
#line 324 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.statement) = (yyvsp[0].exec_stmt); }
#line 2931 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 19:
#line 333 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr_vec) = (yyvsp[-1].expr_vec); }
#line 2937 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 20:
#line 334 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr_vec) = nullptr; }
#line 2943 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 21:
#line 339 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr_vec) = new std::vector<Expr*>(); (yyval.expr_vec)->push_back((yyvsp[0].expr)); }
#line 2949 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 22:
#line 340 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].expr_vec)->push_back((yyvsp[0].expr)); (yyval.expr_vec) = (yyvsp[-2].expr_vec); }
#line 2955 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 23:
#line 344 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.expr) = Expr::make(kExprHint);
			(yyval.expr)->name = (yyvsp[0].sval);
		}
#line 2964 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 24:
#line 348 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.expr) = Expr::make(kExprHint);
			(yyval.expr)->name = (yyvsp[-3].sval);
			(yyval.expr)->exprList = (yyvsp[-1].expr_vec);
		}
#line 2974 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 25:
#line 360 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.prep_stmt) = new PrepareStatement();
			(yyval.prep_stmt)->name = (yyvsp[-2].sval);
			(yyval.prep_stmt)->query = (yyvsp[0].sval);
		}
#line 2984 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 27:
#line 370 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.exec_stmt) = new ExecuteStatement();
			(yyval.exec_stmt)->name = (yyvsp[0].sval);
		}
#line 2993 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 28:
#line 374 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.exec_stmt) = new ExecuteStatement();
			(yyval.exec_stmt)->name = (yyvsp[-3].sval);
			(yyval.exec_stmt)->parameters = (yyvsp[-1].expr_vec);
		}
#line 3003 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 29:
#line 386 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.import_stmt) = new ImportStatement((ImportType) (yyvsp[-4].uval));
			(yyval.import_stmt)->filePath = (yyvsp[-2].sval);
			(yyval.import_stmt)->schema = (yyvsp[0].table_name).schema;
			(yyval.import_stmt)->tableName = (yyvsp[0].table_name).name;
		}
#line 3014 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 30:
#line 395 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kImportCSV; }
#line 3020 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 31:
#line 399 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.sval) = strdup((yyvsp[0].expr)->name); delete (yyvsp[0].expr); }
#line 3026 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 32:
#line 408 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.import_stmt) = new ImportStatement(kImportCSV);
			(yyval.import_stmt)->filePath = (yyvsp[-8].sval);
			(yyval.import_stmt)->schema = (yyvsp[-4].table_name).schema;
			(yyval.import_stmt)->tableName = (yyvsp[-4].table_name).name;
		}
#line 3037 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 50:
#line 458 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.show_stmt) = new ShowStatement(kShowTables);
		}
#line 3045 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 51:
#line 461 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.show_stmt) = new ShowStatement(kShowDatabases);
		}
#line 3053 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 52:
#line 464 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.show_stmt) = new ShowStatement(kShowColumns);
			(yyval.show_stmt)->schema = (yyvsp[0].table_name).schema;
			(yyval.show_stmt)->name = (yyvsp[0].table_name).name;
		}
#line 3063 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 53:
#line 479 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.create_stmt) = new CreateStatement(kCreateTableFromTbl);
			(yyval.create_stmt)->temporary = (yyvsp[-7].bval);
			(yyval.create_stmt)->ifNotExists = (yyvsp[-5].bval);
			(yyval.create_stmt)->schema = (yyvsp[-4].table_name).schema;
			(yyval.create_stmt)->tableName = (yyvsp[-4].table_name).name;
			(yyval.create_stmt)->filePath = (yyvsp[0].sval);
		}
#line 3076 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 54:
#line 487 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.create_stmt) = new CreateStatement(kCreateTable);
			(yyval.create_stmt)->temporary = (yyvsp[-6].bval);
			(yyval.create_stmt)->ifNotExists = (yyvsp[-4].bval);
			(yyval.create_stmt)->schema = (yyvsp[-3].table_name).schema;
			(yyval.create_stmt)->tableName = (yyvsp[-3].table_name).name;
			(yyval.create_stmt)->columns = (yyvsp[-1].column_vec);
		}
#line 3089 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 55:
#line 495 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.create_stmt) = new CreateStatement(kCreateView);
			(yyval.create_stmt)->ifNotExists = (yyvsp[-4].bval);
			(yyval.create_stmt)->schema = (yyvsp[-3].table_name).schema;
			(yyval.create_stmt)->tableName = (yyvsp[-3].table_name).name;
			(yyval.create_stmt)->viewColumns = (yyvsp[-2].str_vec);
			(yyval.create_stmt)->select = (yyvsp[0].select_stmt);
		}
#line 3102 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 56:
#line 503 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.create_stmt) = new CreateStatement(kCreateDatabase);
			(yyval.create_stmt)->ifNotExists = (yyvsp[-1].bval);
			(yyval.create_stmt)->schema = (yyvsp[0].db_name).name;
		}
#line 3112 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 57:
#line 508 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.create_stmt) = new CreateStatement(kCreateDatabase);
			(yyval.create_stmt)->ifNotExists = (yyvsp[-1].bval);
			(yyval.create_stmt)->schema = (yyvsp[0].db_name).name;
		}
#line 3122 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 58:
#line 516 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3128 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 59:
#line 517 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3134 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 60:
#line 521 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3140 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 61:
#line 522 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3146 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 62:
#line 526 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_vec) = new std::vector<ColumnDefinition*>(); (yyval.column_vec)->push_back((yyvsp[0].column_t)); }
#line 3152 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 63:
#line 527 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].column_vec)->push_back((yyvsp[0].column_t)); (yyval.column_vec) = (yyvsp[-2].column_vec); }
#line 3158 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 64:
#line 531 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.column_t) = new ColumnDefinition((yyvsp[-2].sval), (yyvsp[-1].column_type_t), (yyvsp[0].bval));
		}
#line 3166 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 65:
#line 537 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::INT}; }
#line 3172 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 66:
#line 538 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::INT}; }
#line 3178 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 67:
#line 539 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::LONG}; }
#line 3184 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 68:
#line 540 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::FLOAT}; }
#line 3190 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 69:
#line 541 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::DOUBLE}; }
#line 3196 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 70:
#line 542 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::VARCHAR, (yyvsp[-1].ival)}; }
#line 3202 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 71:
#line 543 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::CHAR, (yyvsp[-1].ival)}; }
#line 3208 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 72:
#line 544 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.column_type_t) = ColumnType{DataType::TEXT}; }
#line 3214 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 73:
#line 548 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3220 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 74:
#line 549 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3226 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 75:
#line 550 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3232 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 76:
#line 561 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.drop_stmt) = new DropStatement(kDropTable);
			(yyval.drop_stmt)->ifExists = (yyvsp[-1].bval);
			(yyval.drop_stmt)->schema = (yyvsp[0].table_name).schema;
			(yyval.drop_stmt)->name = (yyvsp[0].table_name).name;
		}
#line 3243 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 77:
#line 567 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.drop_stmt) = new DropStatement(kDropView);
			(yyval.drop_stmt)->ifExists = (yyvsp[-1].bval);
			(yyval.drop_stmt)->schema = (yyvsp[0].table_name).schema;
			(yyval.drop_stmt)->name = (yyvsp[0].table_name).name;
		}
#line 3254 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 78:
#line 573 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.drop_stmt) = new DropStatement(kDropDatabase);
			(yyval.drop_stmt)->ifExists = (yyvsp[-1].bval);
			(yyval.drop_stmt)->name = (yyvsp[0].db_name).name;
		}
#line 3264 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 79:
#line 578 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.drop_stmt) = new DropStatement(kDropPreparedStatement);
			(yyval.drop_stmt)->ifExists = false;
			(yyval.drop_stmt)->name = (yyvsp[0].sval);
		}
#line 3274 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 80:
#line 586 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3280 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 81:
#line 587 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3286 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 82:
#line 596 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.delete_stmt) = new DeleteStatement();
			(yyval.delete_stmt)->low_priority = (yyvsp[-5].bval);
			(yyval.delete_stmt)->quick = (yyvsp[-4].bval);
			(yyval.delete_stmt)->ignore = (yyvsp[-3].bval);
			(yyval.delete_stmt)->schema = (yyvsp[-1].table_name).schema;
			(yyval.delete_stmt)->tableName = (yyvsp[-1].table_name).name;
			(yyval.delete_stmt)->expr = (yyvsp[0].expr);
		}
#line 3300 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 83:
#line 608 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3306 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 84:
#line 609 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3312 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 85:
#line 613 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3318 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 86:
#line 614 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3324 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 87:
#line 618 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3330 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 88:
#line 619 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3336 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 89:
#line 623 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.delete_stmt) = new DeleteStatement();
			(yyval.delete_stmt)->schema = (yyvsp[0].table_name).schema;
			(yyval.delete_stmt)->tableName = (yyvsp[0].table_name).name;
		}
#line 3346 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 90:
#line 636 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.insert_stmt) = new InsertStatement(kInsertValues);
			(yyval.insert_stmt)->priority = (yyvsp[-8].bval);
			(yyval.insert_stmt)->ignore = (yyvsp[-7].bval);
			(yyval.insert_stmt)->schema = (yyvsp[-5].table_name).schema;
			(yyval.insert_stmt)->tableName = (yyvsp[-5].table_name).name;
			(yyval.insert_stmt)->columns = (yyvsp[-4].str_vec);
			(yyval.insert_stmt)->values = (yyvsp[-1].expr_vec);
		}
#line 3360 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 91:
#line 645 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.insert_stmt) = new InsertStatement(kInsertSelect);
			(yyval.insert_stmt)->priority = (yyvsp[-5].bval);
			(yyval.insert_stmt)->ignore = (yyvsp[-4].bval);
			(yyval.insert_stmt)->schema = (yyvsp[-2].table_name).schema;
			(yyval.insert_stmt)->tableName = (yyvsp[-2].table_name).name;
			(yyval.insert_stmt)->columns = (yyvsp[-1].str_vec);
			(yyval.insert_stmt)->select = (yyvsp[0].select_stmt);
		}
#line 3374 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 92:
#line 658 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3380 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 93:
#line 659 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3386 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 94:
#line 660 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3392 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 95:
#line 661 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3398 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 96:
#line 665 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.str_vec) = (yyvsp[-1].str_vec); }
#line 3404 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 97:
#line 666 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.str_vec) = nullptr; }
#line 3410 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 98:
#line 676 "bison_parser.y" /* yacc.c:1645  */
    {
		(yyval.update_stmt) = new UpdateStatement();
		(yyval.update_stmt)->low_priority = (yyvsp[-5].bval);
		(yyval.update_stmt)->ignore = (yyvsp[-4].bval);
		(yyval.update_stmt)->table = (yyvsp[-3].table);
		(yyval.update_stmt)->updates = (yyvsp[-1].update_vec);
		(yyval.update_stmt)->where = (yyvsp[0].expr);
	}
#line 3423 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 99:
#line 687 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.update_vec) = new std::vector<UpdateClause*>(); (yyval.update_vec)->push_back((yyvsp[0].update_t)); }
#line 3429 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 100:
#line 688 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].update_vec)->push_back((yyvsp[0].update_t)); (yyval.update_vec) = (yyvsp[-2].update_vec); }
#line 3435 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 101:
#line 692 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.update_t) = new UpdateClause();
			(yyval.update_t)->column = (yyvsp[-2].sval);
			(yyval.update_t)->value = (yyvsp[0].expr);
		}
#line 3445 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 102:
#line 707 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.alter_stmt) = new AlterStatement(kAlterDatabase);
			(yyval.alter_stmt)->schema = (yyvsp[-5].db_name).name;
			(yyval.alter_stmt)->dflt = (yyvsp[-4].bval);
			(yyval.alter_stmt)->equal = (yyvsp[-1].bval);
			(yyval.alter_stmt)->charsetName = (yyvsp[0].expr);
		}
#line 3457 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 103:
#line 714 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.alter_stmt) = new AlterStatement(kAlterSchema);
			(yyval.alter_stmt)->schema = (yyvsp[-5].db_name).name;
			(yyval.alter_stmt)->dflt = (yyvsp[-4].bval);
			(yyval.alter_stmt)->equal = (yyvsp[-1].bval);
			(yyval.alter_stmt)->charsetName = (yyvsp[0].expr);
		}
#line 3469 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 104:
#line 721 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.alter_stmt) = new AlterStatement(kAlterTable);
			(yyval.alter_stmt)->tableName = (yyvsp[-3].table_name).name;
			(yyval.alter_stmt)->columns = (yyvsp[0].column_t);
		}
#line 3479 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 105:
#line 729 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3485 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 106:
#line 730 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3491 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 107:
#line 734 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3497 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 108:
#line 735 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3503 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 111:
#line 747 "bison_parser.y" /* yacc.c:1645  */
    {
			// TODO: allow multiple unions (through linked list)
			// TODO: capture type of set_operator
			// TODO: might overwrite order and limit of first select here
			(yyval.select_stmt) = (yyvsp[-4].select_stmt);
			(yyval.select_stmt)->unionSelect = (yyvsp[-2].select_stmt);
			(yyval.select_stmt)->order = (yyvsp[-1].order_vec);

			// Limit could have been set by TOP.
			if ((yyvsp[0].limit) != nullptr) {
				delete (yyval.select_stmt)->limit;
				(yyval.select_stmt)->limit = (yyvsp[0].limit);
			}
		}
#line 3522 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 112:
#line 764 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.select_stmt) = (yyvsp[-1].select_stmt); }
#line 3528 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 113:
#line 765 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.select_stmt) = (yyvsp[-1].select_stmt); }
#line 3534 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 116:
#line 774 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.select_stmt) = (yyvsp[-2].select_stmt);
			(yyval.select_stmt)->order = (yyvsp[-1].order_vec);

			// Limit could have been set by TOP.
			if ((yyvsp[0].limit) != nullptr) {
				delete (yyval.select_stmt)->limit;
				(yyval.select_stmt)->limit = (yyvsp[0].limit);
			}
		}
#line 3549 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 117:
#line 784 "bison_parser.y" /* yacc.c:1645  */
    {
			// TODO: allow multiple unions (through linked list)
			// TODO: capture type of set_operator
			// TODO: might overwrite order and limit of first select here
			(yyval.select_stmt) = (yyvsp[-4].select_stmt);
			(yyval.select_stmt)->unionSelect = (yyvsp[-2].select_stmt);
			(yyval.select_stmt)->order = (yyvsp[-1].order_vec);

			// Limit could have been set by TOP.
			if ((yyvsp[0].limit) != nullptr) {
				delete (yyval.select_stmt)->limit;
				(yyval.select_stmt)->limit = (yyvsp[0].limit);
			}
		}
#line 3568 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 124:
#line 816 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.select_stmt) = new SelectStatement();
			(yyval.select_stmt)->limit = (yyvsp[-5].limit);
			(yyval.select_stmt)->selectDistinct = (yyvsp[-4].bval);
			(yyval.select_stmt)->selectList = (yyvsp[-3].expr_vec);
			(yyval.select_stmt)->fromTable = (yyvsp[-2].table);
			(yyval.select_stmt)->whereClause = (yyvsp[-1].expr);
			(yyval.select_stmt)->groupBy = (yyvsp[0].group_t);
		}
#line 3582 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 125:
#line 828 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = true; }
#line 3588 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 126:
#line 829 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.bval) = false; }
#line 3594 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 128:
#line 837 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.table) = (yyvsp[0].table); }
#line 3600 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 129:
#line 838 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.table) = nullptr; }
#line 3606 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 130:
#line 841 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.table) = (yyvsp[0].table); }
#line 3612 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 131:
#line 846 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = (yyvsp[0].expr); }
#line 3618 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 132:
#line 847 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = nullptr; }
#line 3624 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 133:
#line 851 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.group_t) = new GroupByDescription();
			(yyval.group_t)->columns = (yyvsp[-1].expr_vec);
			(yyval.group_t)->having = (yyvsp[0].expr);
		}
#line 3634 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 134:
#line 856 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.group_t) = nullptr; }
#line 3640 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 135:
#line 860 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = (yyvsp[0].expr); }
#line 3646 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 136:
#line 861 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = nullptr; }
#line 3652 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 137:
#line 864 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.order_vec) = (yyvsp[0].order_vec); }
#line 3658 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 138:
#line 865 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.order_vec) = nullptr; }
#line 3664 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 139:
#line 869 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.order_vec) = new std::vector<OrderDescription*>(); (yyval.order_vec)->push_back((yyvsp[0].order)); }
#line 3670 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 140:
#line 870 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].order_vec)->push_back((yyvsp[0].order)); (yyval.order_vec) = (yyvsp[-2].order_vec); }
#line 3676 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 141:
#line 874 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.order) = new OrderDescription((yyvsp[0].order_type), (yyvsp[-1].expr)); }
#line 3682 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 142:
#line 878 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.order_type) = kOrderAsc; }
#line 3688 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 143:
#line 879 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.order_type) = kOrderDesc; }
#line 3694 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 144:
#line 880 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.order_type) = kOrderAsc; }
#line 3700 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 145:
#line 886 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = new LimitDescription((yyvsp[0].expr)->ival, kNoOffset); delete (yyvsp[0].expr); }
#line 3706 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 146:
#line 887 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = nullptr; }
#line 3712 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 147:
#line 891 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = new LimitDescription((yyvsp[0].expr)->ival, kNoOffset); delete (yyvsp[0].expr); }
#line 3718 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 148:
#line 892 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = new LimitDescription((yyvsp[-2].expr)->ival, (yyvsp[0].expr)->ival); delete (yyvsp[-2].expr); delete (yyvsp[0].expr); }
#line 3724 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 149:
#line 893 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = new LimitDescription(kNoLimit, (yyvsp[0].expr)->ival); delete (yyvsp[0].expr); }
#line 3730 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 150:
#line 894 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = nullptr; }
#line 3736 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 151:
#line 895 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = nullptr;  }
#line 3742 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 152:
#line 896 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = new LimitDescription(kNoLimit, (yyvsp[0].expr)->ival); delete (yyvsp[0].expr); }
#line 3748 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 153:
#line 897 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = new LimitDescription(kNoLimit, (yyvsp[0].expr)->ival); delete (yyvsp[0].expr); }
#line 3754 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 154:
#line 898 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.limit) = nullptr; }
#line 3760 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 155:
#line 905 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr_vec) = new std::vector<Expr*>(); (yyval.expr_vec)->push_back((yyvsp[0].expr)); }
#line 3766 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 156:
#line 906 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].expr_vec)->push_back((yyvsp[0].expr)); (yyval.expr_vec) = (yyvsp[-2].expr_vec); }
#line 3772 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 157:
#line 910 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr_vec) = (yyvsp[0].expr_vec); }
#line 3778 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 158:
#line 911 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr_vec) = nullptr; }
#line 3784 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 159:
#line 915 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr_vec) = new std::vector<Expr*>(); (yyval.expr_vec)->push_back((yyvsp[0].expr)); }
#line 3790 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 160:
#line 916 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].expr_vec)->push_back((yyvsp[0].expr)); (yyval.expr_vec) = (yyvsp[-2].expr_vec); }
#line 3796 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 161:
#line 920 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.expr) = (yyvsp[-1].expr);
			if ((yyvsp[0].alias_t)) {
				(yyval.expr)->alias = strdup((yyvsp[0].alias_t)->name);
				delete (yyvsp[0].alias_t);
			}
		}
#line 3808 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 167:
#line 938 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = (yyvsp[-1].expr); }
#line 3814 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 176:
#line 947 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeSelect((yyvsp[-1].select_stmt)); }
#line 3820 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 179:
#line 956 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpUnaryMinus, (yyvsp[0].expr)); }
#line 3826 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 180:
#line 957 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpNot, (yyvsp[0].expr)); }
#line 3832 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 181:
#line 958 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpIsNull, (yyvsp[-1].expr)); }
#line 3838 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 182:
#line 959 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpIsNull, (yyvsp[-2].expr)); }
#line 3844 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 183:
#line 960 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpNot, Expr::makeOpUnary(kOpIsNull, (yyvsp[-3].expr))); }
#line 3850 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 185:
#line 965 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpMinus, (yyvsp[0].expr)); }
#line 3856 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 186:
#line 966 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpPlus, (yyvsp[0].expr)); }
#line 3862 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 187:
#line 967 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpSlash, (yyvsp[0].expr)); }
#line 3868 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 188:
#line 968 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpAsterisk, (yyvsp[0].expr)); }
#line 3874 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 189:
#line 969 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpPercentage, (yyvsp[0].expr)); }
#line 3880 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 190:
#line 970 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpCaret, (yyvsp[0].expr)); }
#line 3886 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 191:
#line 971 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpLike, (yyvsp[0].expr)); }
#line 3892 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 192:
#line 972 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-3].expr), kOpNotLike, (yyvsp[0].expr)); }
#line 3898 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 193:
#line 973 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpILike, (yyvsp[0].expr)); }
#line 3904 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 194:
#line 974 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpConcat, (yyvsp[0].expr)); }
#line 3910 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 195:
#line 978 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpAnd, (yyvsp[0].expr)); }
#line 3916 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 196:
#line 979 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpOr, (yyvsp[0].expr)); }
#line 3922 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 197:
#line 983 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeInOperator((yyvsp[-4].expr), (yyvsp[-1].expr_vec)); }
#line 3928 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 198:
#line 984 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpNot, Expr::makeInOperator((yyvsp[-5].expr), (yyvsp[-1].expr_vec))); }
#line 3934 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 199:
#line 985 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeInOperator((yyvsp[-4].expr), (yyvsp[-1].select_stmt)); }
#line 3940 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 200:
#line 986 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpNot, Expr::makeInOperator((yyvsp[-5].expr), (yyvsp[-1].select_stmt))); }
#line 3946 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 201:
#line 992 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeCase((yyvsp[-2].expr), (yyvsp[-1].expr), nullptr); }
#line 3952 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 202:
#line 993 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeCase((yyvsp[-4].expr), (yyvsp[-3].expr), (yyvsp[-1].expr)); }
#line 3958 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 203:
#line 994 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeCase(nullptr, (yyvsp[-1].expr), nullptr); }
#line 3964 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 204:
#line 995 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeCase(nullptr, (yyvsp[-3].expr), (yyvsp[-1].expr)); }
#line 3970 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 205:
#line 999 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeCaseList(Expr::makeCaseListElement((yyvsp[-2].expr), (yyvsp[0].expr))); }
#line 3976 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 206:
#line 1000 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::caseListAppend((yyvsp[-4].expr), Expr::makeCaseListElement((yyvsp[-2].expr), (yyvsp[0].expr))); }
#line 3982 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 207:
#line 1004 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeExists((yyvsp[-1].select_stmt)); }
#line 3988 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 208:
#line 1005 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpUnary(kOpNot, Expr::makeExists((yyvsp[-1].select_stmt))); }
#line 3994 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 209:
#line 1009 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpEquals, (yyvsp[0].expr)); }
#line 4000 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 210:
#line 1010 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpEquals, (yyvsp[0].expr)); }
#line 4006 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 211:
#line 1011 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpNotEquals, (yyvsp[0].expr)); }
#line 4012 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 212:
#line 1012 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpLess, (yyvsp[0].expr)); }
#line 4018 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 213:
#line 1013 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpGreater, (yyvsp[0].expr)); }
#line 4024 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 214:
#line 1014 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpLessEq, (yyvsp[0].expr)); }
#line 4030 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 215:
#line 1015 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeOpBinary((yyvsp[-2].expr), kOpGreaterEq, (yyvsp[0].expr)); }
#line 4036 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 216:
#line 1019 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeFunctionRef((yyvsp[-2].sval), new std::vector<Expr*>(), false); }
#line 4042 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 217:
#line 1020 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeFunctionRef((yyvsp[-4].sval), (yyvsp[-1].expr_vec), (yyvsp[-2].bval)); }
#line 4048 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 218:
#line 1024 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeExtract((yyvsp[-3].datetime_field), (yyvsp[-1].expr)); }
#line 4054 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 219:
#line 1028 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.datetime_field) = kDatetimeSecond; }
#line 4060 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 220:
#line 1029 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.datetime_field) = kDatetimeMinute; }
#line 4066 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 221:
#line 1030 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.datetime_field) = kDatetimeHour; }
#line 4072 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 222:
#line 1031 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.datetime_field) = kDatetimeDay; }
#line 4078 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 223:
#line 1032 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.datetime_field) = kDatetimeMonth; }
#line 4084 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 224:
#line 1033 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.datetime_field) = kDatetimeYear; }
#line 4090 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 225:
#line 1036 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeArray((yyvsp[-1].expr_vec)); }
#line 4096 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 226:
#line 1040 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeArrayIndex((yyvsp[-3].expr), (yyvsp[-1].expr)->ival); }
#line 4102 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 227:
#line 1044 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeBetween((yyvsp[-4].expr), (yyvsp[-2].expr), (yyvsp[0].expr)); }
#line 4108 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 228:
#line 1048 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeColumnRef((yyvsp[0].sval)); }
#line 4114 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 229:
#line 1049 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeColumnRef((yyvsp[-2].sval), (yyvsp[0].sval)); }
#line 4120 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 230:
#line 1050 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeStar(); }
#line 4126 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 231:
#line 1051 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeStar((yyvsp[-2].sval)); }
#line 4132 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 237:
#line 1063 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeLiteral((yyvsp[0].sval)); }
#line 4138 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 238:
#line 1067 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeLiteral(true); }
#line 4144 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 239:
#line 1068 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeLiteral(false); }
#line 4150 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 240:
#line 1072 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeLiteral((yyvsp[0].fval)); }
#line 4156 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 242:
#line 1077 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeLiteral((yyvsp[0].ival)); }
#line 4162 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 243:
#line 1081 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.expr) = Expr::makeNullLiteral(); }
#line 4168 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 244:
#line 1085 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.expr) = Expr::makeParameter(yylloc.total_column);
			(yyval.expr)->ival2 = yyloc.param_list.size();
			yyloc.param_list.push_back((yyval.expr));
		}
#line 4178 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 246:
#line 1098 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyvsp[-2].table_vec)->push_back((yyvsp[0].table));
			auto tbl = new TableRef(kTableCrossProduct);
			tbl->list = (yyvsp[-2].table_vec);
			(yyval.table) = tbl;
		}
#line 4189 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 250:
#line 1114 "bison_parser.y" /* yacc.c:1645  */
    {
			auto tbl = new TableRef(kTableSelect);
			tbl->select = (yyvsp[-2].select_stmt);
			tbl->alias = (yyvsp[0].alias_t);
			(yyval.table) = tbl;
		}
#line 4200 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 251:
#line 1123 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.table_vec) = new std::vector<TableRef*>(); (yyval.table_vec)->push_back((yyvsp[0].table)); }
#line 4206 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 252:
#line 1124 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].table_vec)->push_back((yyvsp[0].table)); (yyval.table_vec) = (yyvsp[-2].table_vec); }
#line 4212 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 253:
#line 1129 "bison_parser.y" /* yacc.c:1645  */
    {
			auto tbl = new TableRef(kTableName);
			tbl->schema = (yyvsp[-1].table_name).schema;
			tbl->name = (yyvsp[-1].table_name).name;
			tbl->alias = (yyvsp[0].alias_t);
			(yyval.table) = tbl;
		}
#line 4224 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 254:
#line 1140 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.table) = new TableRef(kTableName);
			(yyval.table)->schema = (yyvsp[0].table_name).schema;
			(yyval.table)->name = (yyvsp[0].table_name).name;
		}
#line 4234 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 255:
#line 1149 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.table_name).schema = nullptr; (yyval.table_name).name = (yyvsp[0].sval);}
#line 4240 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 256:
#line 1150 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.table_name).schema = (yyvsp[-2].sval); (yyval.table_name).name = (yyvsp[0].sval); }
#line 4246 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 257:
#line 1155 "bison_parser.y" /* yacc.c:1645  */
    {(yyval.db_name).name = (yyvsp[0].sval);}
#line 4252 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 259:
#line 1161 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.alias_t) = new Alias((yyvsp[-3].sval), (yyvsp[-1].str_vec)); }
#line 4258 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 261:
#line 1167 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.alias_t) = nullptr; }
#line 4264 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 262:
#line 1171 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.alias_t) = new Alias((yyvsp[0].sval)); }
#line 4270 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 263:
#line 1172 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.alias_t) = new Alias((yyvsp[0].sval)); }
#line 4276 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 265:
#line 1178 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.alias_t) = nullptr; }
#line 4282 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 266:
#line 1187 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.table) = new TableRef(kTableJoin);
			(yyval.table)->join = new JoinDefinition();
			(yyval.table)->join->type = kJoinNatural;
			(yyval.table)->join->left = (yyvsp[-3].table);
			(yyval.table)->join->right = (yyvsp[0].table);
		}
#line 4294 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 267:
#line 1195 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.table) = new TableRef(kTableJoin);
			(yyval.table)->join = new JoinDefinition();
			(yyval.table)->join->type = (JoinType) (yyvsp[-4].uval);
			(yyval.table)->join->left = (yyvsp[-5].table);
			(yyval.table)->join->right = (yyvsp[-2].table);
			(yyval.table)->join->condition = (yyvsp[0].expr);
		}
#line 4307 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 268:
#line 1205 "bison_parser.y" /* yacc.c:1645  */
    {
			(yyval.table) = new TableRef(kTableJoin);
			(yyval.table)->join = new JoinDefinition();
			(yyval.table)->join->type = (JoinType) (yyvsp[-6].uval);
			(yyval.table)->join->left = (yyvsp[-7].table);
			(yyval.table)->join->right = (yyvsp[-4].table);
			auto left_col = Expr::makeColumnRef(strdup((yyvsp[-1].expr)->name));
			if ((yyvsp[-1].expr)->alias != nullptr) left_col->alias = strdup((yyvsp[-1].expr)->alias);
			if ((yyvsp[-7].table)->getName() != nullptr) left_col->table = strdup((yyvsp[-7].table)->getName());
			auto right_col = Expr::makeColumnRef(strdup((yyvsp[-1].expr)->name));
			if ((yyvsp[-1].expr)->alias != nullptr) right_col->alias = strdup((yyvsp[-1].expr)->alias);
			if ((yyvsp[-4].table)->getName() != nullptr) right_col->table = strdup((yyvsp[-4].table)->getName());
			(yyval.table)->join->condition = Expr::makeOpBinary(left_col, kOpEquals, right_col);
			delete (yyvsp[-1].expr);
		}
#line 4327 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 269:
#line 1223 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinInner; }
#line 4333 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 270:
#line 1224 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinLeft; }
#line 4339 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 271:
#line 1225 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinLeft; }
#line 4345 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 272:
#line 1226 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinRight; }
#line 4351 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 273:
#line 1227 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinRight; }
#line 4357 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 274:
#line 1228 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinFull; }
#line 4363 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 275:
#line 1229 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinFull; }
#line 4369 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 276:
#line 1230 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinFull; }
#line 4375 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 277:
#line 1231 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinCross; }
#line 4381 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 278:
#line 1232 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.uval) = kJoinInner; }
#line 4387 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 282:
#line 1252 "bison_parser.y" /* yacc.c:1645  */
    { (yyval.str_vec) = new std::vector<char*>(); (yyval.str_vec)->push_back((yyvsp[0].sval)); }
#line 4393 "bison_parser.cpp" /* yacc.c:1645  */
    break;

  case 283:
#line 1253 "bison_parser.y" /* yacc.c:1645  */
    { (yyvsp[-2].str_vec)->push_back((yyvsp[0].sval)); (yyval.str_vec) = (yyvsp[-2].str_vec); }
#line 4399 "bison_parser.cpp" /* yacc.c:1645  */
    break;


#line 4403 "bison_parser.cpp" /* yacc.c:1645  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, result, scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (&yylloc, result, scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, result, scanner);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, yylsp, result, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, result, scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, result, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, yylsp, result, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1256 "bison_parser.y" /* yacc.c:1903  */

/*********************************
 ** Section 4: Additional C code
 *********************************/

/* empty */
