%{
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

%}
/*********************************
 ** Section 2: Bison Parser Declarations
 *********************************/


// Specify code that is included in the generated .h and .c files
%code requires {
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
}

// Define the names of the created files (defined in Makefile)
// %output  "bison_parser.cpp"
// %defines "bison_parser.h"

// Tell bison to create a reentrant parser
%define api.pure full

// Prefix the parser
%define api.prefix {hsql_}
%define api.token.prefix {SQL_}

%define parse.error verbose
%locations

%initial-action {
	// Initialize
	@$.first_column = 0;
	@$.last_column = 0;
	@$.first_line = 0;
	@$.last_line = 0;
	@$.total_column = 0;
	@$.string_length = 0;
};


// Define additional parameters for yylex (http://www.gnu.org/software/bison/manual/html_node/Pure-Calling.html)
%lex-param   { yyscan_t scanner }

// Define additional parameters for yyparse
%parse-param { hsql::SQLParserResult* result }
%parse-param { yyscan_t scanner }


/*********************************
 ** Define all data-types (http://www.gnu.org/software/bison/manual/html_node/Union-Decl.html)
 *********************************/
%union {
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
}


/*********************************
 ** Destructor symbols
 *********************************/
%destructor { } <fval> <ival> <uval> <bval> <order_type> <datetime_field> <column_type_t>
%destructor { free( ($$.name) ); free( ($$.schema) ); } <table_name>
%destructor { free( ($$.name) ); } <db_name>
%destructor { free( ($$) ); } <sval>
%destructor {
	if (($$) != nullptr) {
		for (auto ptr : *($$)) {
			delete ptr;
		}
	}
	delete ($$);
} <str_vec> <table_vec> <column_vec> <update_vec> <expr_vec> <order_vec> <stmt_vec>
%destructor { delete ($$); } <*>


/*********************************
 ** Token Definition
 *********************************/
%token <sval> IDENTIFIER STRING
%token <fval> FLOATVAL
%token <ival> INTVAL

/* SQL Keywords */
%token DEALLOCATE PARAMETERS INTERSECT TEMPORARY TIMESTAMP
%token DISTINCT NVARCHAR RESTRICT TRUNCATE ANALYZE BETWEEN
%token CASCADE COLUMNS CONTROL DEFAULT EXECUTE EXPLAIN
%token HISTORY INTEGER NATURAL PREPARE PRIMARY SCHEMAS
%token SPATIAL VARCHAR VIRTUAL BEFORE COLUMN CREATE DELETE DIRECT
%token DOUBLE ESCAPE EXCEPT EXISTS EXTRACT GLOBAL HAVING IMPORT
%token INSERT ISNULL OFFSET RENAME SCHEMA SELECT SORTED
%token TABLES UNIQUE UNLOAD UPDATE VALUES AFTER ALTER CROSS
%token DELTA FLOAT GROUP INDEX INNER LIMIT LOCAL MERGE MINUS ORDER
%token OUTER RIGHT TABLE UNION USING WHERE CALL CASE CHAR DATE
%token DESC DROP ELSE FILE FROM FULL HASH HINT INTO JOIN
%token LEFT LIKE LOAD LONG NULL PLAN SHOW TEXT THEN TIME
%token VIEW WHEN WITH LOW_PRIORITY DELAYED HIGH_PRIORITY 
%token QUICK IGNORE DATABASES DATABASE CHARACTER ADD ALL AND ASC CSV END FOR INT KEY
%token NOT OFF SET TBL TOP AS BY IF IN IS OF ON OR TO
%token ARRAY CONCAT ILIKE SECOND MINUTE HOUR DAY MONTH YEAR
%token TRUE FALSE
%token ESCAPED DATA INFILE CONCURRENT REPLACE PARTITION FIELDS TERMINATED OPTIONALLY
%token ENCLOSED LINES ROWS STARTING

/*********************************
 ** Non-Terminal types (http://www.gnu.org/software/bison/manual/html_node/Type-Decl.html)
 *********************************/
%type <stmt_vec>	    statement_list
%type <statement> 	    statement preparable_statement
%type <exec_stmt>	    execute_statement
%type <prep_stmt>	    prepare_statement
%type <select_stmt>     select_statement select_with_paren select_no_paren select_clause select_paren_or_clause
%type <import_stmt>     import_statement load_statement
%type <create_stmt>     create_statement
%type <insert_stmt>     insert_statement
%type <delete_stmt>     delete_statement truncate_statement
%type <update_stmt>     update_statement
%type <drop_stmt>	    drop_statement
%type <show_stmt>	    show_statement
%type <alter_stmt>	    alter_statement
%type <table_name>      table_name
%type <db_name>		    db_name
%type <sval> 		    file_path prepare_target_query
%type <bval> 		    opt_not_exists opt_exists opt_distinct opt_column_nullable opt_temporary
%type <bval>		    opt_low_priority opt_priority opt_quick opt_ignore opt_default opt_equal
%type <uval>		    import_file_type opt_join_type
%type <table> 		    opt_from_clause from_clause table_ref table_ref_atomic table_ref_name nonjoin_table_ref_atomic
%type <table>		    join_clause table_ref_name_no_alias
%type <expr> 		    expr operand scalar_expr unary_expr binary_expr logic_expr exists_expr extract_expr
%type <expr>		    function_expr between_expr expr_alias param_expr
%type <expr> 		    column_name literal int_literal num_literal string_literal bool_literal
%type <expr> 		    comp_expr opt_where join_condition opt_having case_expr case_list in_expr hint
%type <expr> 		    array_expr array_index null_literal
%type <limit>		    opt_limit opt_top
%type <order>		    order_desc
%type <order_type>	    opt_order_type
%type <datetime_field>	datetime_field
%type <column_t>	    column_def
%type <column_type_t>   column_type
%type <update_t>	    update_clause
%type <group_t>		    opt_group
%type <alias_t>		    opt_table_alias table_alias opt_alias alias

%type <str_vec>		ident_commalist opt_column_list
%type <expr_vec> 	expr_list select_list opt_literal_list literal_list hint_list opt_hints
%type <table_vec> 	table_ref_commalist
%type <order_vec>	opt_order order_list
%type <update_vec>	update_clause_commalist
%type <column_vec>	column_def_commalist

/******************************
 ** Token Precedence and Associativity
 ** Precedence: lowest to highest
 ******************************/
%left		OR
%left		AND
%right		NOT
%nonassoc	'=' EQUALS NOTEQUALS LIKE ILIKE
%nonassoc	'<' '>' LESS GREATER LESSEQ GREATEREQ

%nonassoc	NOTNULL
%nonassoc	ISNULL
%nonassoc	IS				/* sets precedence for IS NULL, etc */
%left		'+' '-'
%left		'*' '/' '%'
%left		'^'
%left		CONCAT

/* Unary Operators */
%right  UMINUS
%left		'[' ']'
%left		'(' ')'
%left		'.'
%left   JOIN
%%
/*********************************
 ** Section 3: Grammar Definition
 *********************************/

// Defines our general input.
input:
		statement_list opt_semicolon {
			for (SQLStatement* stmt : *$1) {
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
			delete $1;
		}
	;


statement_list:
		statement {
			$1->stringLength = yylloc.string_length;
			yylloc.string_length = 0;
			$$ = new std::vector<SQLStatement*>();
			$$->push_back($1);
		}
	|	statement_list ';' statement {
			$3->stringLength = yylloc.string_length;
			yylloc.string_length = 0;
			$1->push_back($3);
			$$ = $1;
		}
	;

statement:
		prepare_statement opt_hints {
			$$ = $1;
			$$->hints = $2;
		}
	|	preparable_statement opt_hints {
			$$ = $1;
			$$->hints = $2;
		}
	|	show_statement {
			$$ = $1;
		}
	;


preparable_statement:
		select_statement { $$ = $1; }
	|	import_statement { $$ = $1; }
    |   load_statement { $$ = $1; }
    |   create_statement { $$ = $1; }
	|	insert_statement { $$ = $1; }
	|	delete_statement { $$ = $1; }
	|	alter_statement { $$ = $1; }
	|	truncate_statement { $$ = $1; }
	|	update_statement { $$ = $1; }
	|	drop_statement { $$ = $1; }
	|	execute_statement { $$ = $1; }
	;


/******************************
 * Hints
 ******************************/

opt_hints:
    WITH HINT '(' hint_list ')' { $$ = $4; }
  | /* empty */ { $$ = nullptr; }
  ;


hint_list:
	  hint { $$ = new std::vector<Expr*>(); $$->push_back($1); }
	| hint_list ',' hint { $1->push_back($3); $$ = $1; }
	;

hint:
		IDENTIFIER {
			$$ = Expr::make(kExprHint);
			$$->name = $1;
		}
	| IDENTIFIER '(' literal_list ')' {
			$$ = Expr::make(kExprHint);
			$$->name = $1;
			$$->exprList = $3;
		}
	;


/******************************
 * Prepared Statement
 ******************************/
prepare_statement:
		PREPARE IDENTIFIER FROM prepare_target_query {
			$$ = new PrepareStatement();
			$$->name = $2;
			$$->query = $4;
		}
	;

prepare_target_query: STRING

execute_statement:
		EXECUTE IDENTIFIER {
			$$ = new ExecuteStatement();
			$$->name = $2;
		}
	|	EXECUTE IDENTIFIER '(' opt_literal_list ')' {
			$$ = new ExecuteStatement();
			$$->name = $2;
			$$->parameters = $4;
		}
	;


/******************************
 * Import Statement
 ******************************/
import_statement:
		IMPORT FROM import_file_type FILE file_path INTO table_name {
			$$ = new ImportStatement((ImportType) $3);
			$$->filePath = $5;
			$$->schema = $7.schema;
			$$->tableName = $7.name;
		}
	;

import_file_type:
		CSV { $$ = kImportCSV; }
	;

file_path:
		string_literal { $$ = strdup($1->name); delete $1; }
	;


/******************************
 * LOAD DATA statement
 * LOAD DATA INFILE 'data.txt' INTO TABLE db2.my_table;
 ******************************/
load_statement:
  LOAD DATA opt_low_priority_or_concurrent opt_local INFILE file_path opt_replace_or_ignore INTO TABLE table_name opt_fields_or_columns opt_lines opt_ignore_lines_rows opt_column_list {
			$$ = new ImportStatement(kImportCSV);
			$$->filePath = $6;
			$$->schema = $10.schema;
			$$->tableName = $10.name;
		}
	;

opt_low_priority_or_concurrent:
		LOW_PRIORITY 
	|	CONCURRENT
    |       /* empty */
	;

opt_replace_or_ignore:
		REPLACE
	|	IGNORE
    |       /* empty */
	;

opt_local:
		LOCAL    
	|	/* empty */    
	;

opt_fields_or_columns:
                FIELDS TERMINATED BY STRING
                | COLUMNS TERMINATED BY STRING
                |        /* empty */
                ;

opt_lines:
        LINES STARTING BY STRING
        | LINES TERMINATED BY STRING
        |        /* empty */
        ;

opt_ignore_lines_rows:
                IGNORE INTVAL LINES
        |        IGNORE INTVAL ROWS
        |       /* empty */
;

/******************************
 * Show Statement
 * SHOW TABLES;
 * SHOW DATABASES;
 ******************************/

show_statement:
		SHOW TABLES {
			$$ = new ShowStatement(kShowTables);
		}
	|	SHOW DATABASES {
			$$ = new ShowStatement(kShowDatabases);
		}
	|	SHOW COLUMNS table_name {
			$$ = new ShowStatement(kShowColumns);
			$$->schema = $3.schema;
			$$->name = $3.name;
		}
	;


/******************************
 * Create Statement
 * CREATE TABLE students (name TEXT, student_number INTEGER, city TEXT, grade DOUBLE)
 * CREATE TABLE students FROM TBL FILE 'test/students.tbl'
 * CREATE { DATABASE | SCHEMA } [IF NOT EXISTS] name
 ******************************/
create_statement:
		CREATE opt_temporary TABLE opt_not_exists table_name FROM TBL FILE file_path {
			$$ = new CreateStatement(kCreateTableFromTbl);
			$$->temporary = $2;
			$$->ifNotExists = $4;
			$$->schema = $5.schema;
			$$->tableName = $5.name;
			$$->filePath = $9;
		}
	|	CREATE opt_temporary TABLE opt_not_exists table_name '(' column_def_commalist ')' {
			$$ = new CreateStatement(kCreateTable);
			$$->temporary = $2;
			$$->ifNotExists = $4;
			$$->schema = $5.schema;
			$$->tableName = $5.name;
			$$->columns = $7;
		}
	|	CREATE VIEW opt_not_exists table_name opt_column_list AS select_statement {
			$$ = new CreateStatement(kCreateView);
			$$->ifNotExists = $3;
			$$->schema = $4.schema;
			$$->tableName = $4.name;
			$$->viewColumns = $5;
			$$->select = $7;
		}
	|	CREATE DATABASE opt_not_exists db_name {
			$$ = new CreateStatement(kCreateDatabase);
			$$->ifNotExists = $3;
			$$->schema = $4.name;
		}
	|	CREATE SCHEMA opt_not_exists db_name {
			$$ = new CreateStatement(kCreateDatabase);
			$$->ifNotExists = $3;
			$$->schema = $4.name;
		}
	;

opt_temporary:
		TEMPORARY { $$ = true; }
	|	/* empty */ { $$ = false; }
	;

opt_not_exists:
		IF NOT EXISTS { $$ = true; }
	|	/* empty */ { $$ = false; }
	;

column_def_commalist:
		column_def { $$ = new std::vector<ColumnDefinition*>(); $$->push_back($1); }
	|	column_def_commalist ',' column_def { $1->push_back($3); $$ = $1; }
	;

column_def:
		IDENTIFIER column_type opt_column_nullable {
			$$ = new ColumnDefinition($1, $2, $3);
		}
	;

column_type:
		INT { $$ = ColumnType{DataType::INT}; }
	|	INTEGER { $$ = ColumnType{DataType::INT}; }
	|	LONG { $$ = ColumnType{DataType::LONG}; }
	|	FLOAT { $$ = ColumnType{DataType::FLOAT}; }
	|	DOUBLE { $$ = ColumnType{DataType::DOUBLE}; }
	|	VARCHAR '(' INTVAL ')' { $$ = ColumnType{DataType::VARCHAR, $3}; }
	|	CHAR '(' INTVAL ')' { $$ = ColumnType{DataType::CHAR, $3}; }
	|	TEXT { $$ = ColumnType{DataType::TEXT}; }
	;

opt_column_nullable:
		NULL { $$ = true; }
	|	NOT NULL { $$ = false; }
	|	/* empty */ { $$ = false; }
	;

/******************************
 * Drop Statement
 * DROP TABLE students;
 * DROP DATABASE IF EXISTS db_name;
 * DEALLOCATE PREPARE stmt;
 ******************************/

drop_statement:
		DROP TABLE opt_exists table_name {
			$$ = new DropStatement(kDropTable);
			$$->ifExists = $3;
			$$->schema = $4.schema;
			$$->name = $4.name;
		}
	|	DROP VIEW opt_exists table_name {
			$$ = new DropStatement(kDropView);
			$$->ifExists = $3;
			$$->schema = $4.schema;
			$$->name = $4.name;
		}
	|	DROP DATABASE opt_exists db_name {
			$$ = new DropStatement(kDropDatabase);
			$$->ifExists = $3;
			$$->name = $4.name;
		}
	|	DEALLOCATE PREPARE IDENTIFIER {
			$$ = new DropStatement(kDropPreparedStatement);
			$$->ifExists = false;
			$$->name = $3;
		}
	;

opt_exists:
		IF EXISTS   { $$ = true; }
	|	/* empty */ { $$ = false; }
	;

/******************************
 * Delete Statement / Truncate statement
 * DELETE FROM students WHERE grade > 3.0
 * DELETE FROM students <=> TRUNCATE students
 ******************************/
delete_statement:
		DELETE opt_low_priority opt_quick opt_ignore FROM table_name opt_where {
			$$ = new DeleteStatement();
			$$->low_priority = $2;
			$$->quick = $3;
			$$->ignore = $4;
			$$->schema = $6.schema;
			$$->tableName = $6.name;
			$$->expr = $7;
		}
	;

opt_low_priority:
		LOW_PRIORITY   { $$ = true; }
	|	/* empty */    { $$ = false; }
	;

opt_quick:
		QUICK          { $$ = true; }
	|	/* empty */    { $$ = false; }
	;

opt_ignore:
		IGNORE         { $$ = true; }
	|	/* empty */    { $$ = false; }
	;

truncate_statement:
		TRUNCATE table_name {
			$$ = new DeleteStatement();
			$$->schema = $2.schema;
			$$->tableName = $2.name;
		}
	;

/******************************
 * Insert Statement
 * INSERT INTO students VALUES ('Max', 1112233, 'Musterhausen', 2.3)
 * INSERT INTO employees SELECT * FROM stundents
 ******************************/
insert_statement:
		INSERT opt_priority opt_ignore INTO table_name opt_column_list VALUES '(' literal_list ')' {
			$$ = new InsertStatement(kInsertValues);
			$$->priority = $2;
			$$->ignore = $3;
			$$->schema = $5.schema;
			$$->tableName = $5.name;
			$$->columns = $6;
			$$->values = $9;
		}
	|	INSERT opt_priority opt_ignore INTO table_name opt_column_list select_no_paren {
			$$ = new InsertStatement(kInsertSelect);
			$$->priority = $2;
			$$->ignore = $3;
			$$->schema = $5.schema;
			$$->tableName = $5.name;
			$$->columns = $6;
			$$->select = $7;
		}
	;


opt_priority:
		LOW_PRIORITY    { $$ = true; }
	|	DELAYED         { $$ = true; }
	|	HIGH_PRIORITY   { $$ = true; }
	|	/* empty */     { $$ = false; }
	;

opt_column_list:
		'(' ident_commalist ')' { $$ = $2; }
	|	/* empty */ { $$ = nullptr; }
	;


/******************************
 * Update Statement
 * UPDATE students SET grade = 1.3, name='Felix Fürstenberg' WHERE name = 'Max Mustermann';
 ******************************/

update_statement:
	UPDATE opt_low_priority opt_ignore table_ref_name_no_alias SET update_clause_commalist opt_where {
		$$ = new UpdateStatement();
		$$->low_priority = $2;
		$$->ignore = $3;
		$$->table = $4;
		$$->updates = $6;
		$$->where = $7;
	}
	;

update_clause_commalist:
		update_clause { $$ = new std::vector<UpdateClause*>(); $$->push_back($1); }
	|	update_clause_commalist ',' update_clause { $1->push_back($3); $$ = $1; }
	;

update_clause:
		IDENTIFIER '=' expr {
			$$ = new UpdateClause();
			$$->column = $1;
			$$->value = $3;
		}
	;

/******************************
 * Alter Statement
 * ALTER DATABASE db_name CHARACTER SET charset_name
 * ALTER SCHEMA db_name CHARACTER SET charset_name
 * ALTER TABLE table_name ADD column
 ******************************/

alter_statement:
		ALTER DATABASE db_name opt_default CHARACTER SET opt_equal expr {
			$$ = new AlterStatement(kAlterDatabase);
			$$->schema = $3.name;
			$$->dflt = $4;
			$$->equal = $7;
			$$->charsetName = $8;
		}
	|	ALTER SCHEMA db_name opt_default CHARACTER SET opt_equal expr {
			$$ = new AlterStatement(kAlterSchema);
			$$->schema = $3.name;
			$$->dflt = $4;
			$$->equal = $7;
			$$->charsetName = $8;
		}
	|	ALTER TABLE table_name ADD COLUMN column_def {
			$$ = new AlterStatement(kAlterTable);
			$$->tableName = $3.name;
			$$->columns = $6;
		}
	;

opt_default:
		DEFAULT      { $$ = true; }
	|	/* empty */  { $$ = false; }
	;

opt_equal:
		'='          { $$ = true; }
	|	/* empty */  { $$ = false; }
	;



/******************************
 * Select Statement
 ******************************/

select_statement:
		select_with_paren
	|	select_no_paren
	|	select_with_paren set_operator select_paren_or_clause opt_order opt_limit {
			// TODO: allow multiple unions (through linked list)
			// TODO: capture type of set_operator
			// TODO: might overwrite order and limit of first select here
			$$ = $1;
			$$->unionSelect = $3;
			$$->order = $4;

			// Limit could have been set by TOP.
			if ($5 != nullptr) {
				delete $$->limit;
				$$->limit = $5;
			}
		}
	;

select_with_paren:
		'(' select_no_paren ')' { $$ = $2; }
	|	'(' select_with_paren ')' { $$ = $2; }
	;

select_paren_or_clause:
		select_with_paren
	|	select_clause
	;

select_no_paren:
		select_clause opt_order opt_limit {
			$$ = $1;
			$$->order = $2;

			// Limit could have been set by TOP.
			if ($3 != nullptr) {
				delete $$->limit;
				$$->limit = $3;
			}
		}
	|	select_clause set_operator select_paren_or_clause opt_order opt_limit {
			// TODO: allow multiple unions (through linked list)
			// TODO: capture type of set_operator
			// TODO: might overwrite order and limit of first select here
			$$ = $1;
			$$->unionSelect = $3;
			$$->order = $4;

			// Limit could have been set by TOP.
			if ($5 != nullptr) {
				delete $$->limit;
				$$->limit = $5;
			}
		}
	;

set_operator:
		set_type opt_all
	;

set_type:
		UNION
	|	INTERSECT
	|	EXCEPT
	;

opt_all:
		ALL
	|	/* empty */
	;

select_clause:
		SELECT opt_top opt_distinct select_list opt_from_clause opt_where opt_group {
			$$ = new SelectStatement();
			$$->limit = $2;
			$$->selectDistinct = $3;
			$$->selectList = $4;
			$$->fromTable = $5;
			$$->whereClause = $6;
			$$->groupBy = $7;
		}
	;

opt_distinct:
		DISTINCT { $$ = true; }
	|	/* empty */ { $$ = false; }
	;

select_list:
		expr_list
	;

opt_from_clause:
        from_clause  { $$ = $1; }
    |   /* empty */  { $$ = nullptr; }

from_clause:
		FROM table_ref { $$ = $2; }
	;


opt_where:
		WHERE expr { $$ = $2; }
	|	/* empty */ { $$ = nullptr; }
	;

opt_group:
		GROUP BY expr_list opt_having {
			$$ = new GroupByDescription();
			$$->columns = $3;
			$$->having = $4;
		}
	|	/* empty */ { $$ = nullptr; }
	;

opt_having:
		HAVING expr { $$ = $2; }
	|	/* empty */ { $$ = nullptr; }

opt_order:
		ORDER BY order_list { $$ = $3; }
	|	/* empty */ { $$ = nullptr; }
	;

order_list:
		order_desc { $$ = new std::vector<OrderDescription*>(); $$->push_back($1); }
	|	order_list ',' order_desc { $1->push_back($3); $$ = $1; }
	;

order_desc:
		expr opt_order_type { $$ = new OrderDescription($2, $1); }
	;

opt_order_type:
		ASC { $$ = kOrderAsc; }
	|	DESC { $$ = kOrderDesc; }
	|	/* empty */ { $$ = kOrderAsc; }
	;

// TODO: TOP and LIMIT can take more than just int literals.

opt_top:
		TOP int_literal { $$ = new LimitDescription($2->ival, kNoOffset); delete $2; }
	|	/* empty */ { $$ = nullptr; }
	;

opt_limit:
		LIMIT int_literal { $$ = new LimitDescription($2->ival, kNoOffset); delete $2; }
	|	LIMIT int_literal OFFSET int_literal { $$ = new LimitDescription($2->ival, $4->ival); delete $2; delete $4; }
	|	OFFSET int_literal { $$ = new LimitDescription(kNoLimit, $2->ival); delete $2; }
	|	LIMIT ALL { $$ = nullptr; }
	|	LIMIT NULL { $$ = nullptr;  }
	|	LIMIT ALL OFFSET int_literal { $$ = new LimitDescription(kNoLimit, $4->ival); delete $4; }
	|	LIMIT NULL OFFSET int_literal { $$ = new LimitDescription(kNoLimit, $4->ival); delete $4; }
	|	/* empty */ { $$ = nullptr; }
	;

/******************************
 * Expressions
 ******************************/
expr_list:
		expr_alias { $$ = new std::vector<Expr*>(); $$->push_back($1); }
	|	expr_list ',' expr_alias { $1->push_back($3); $$ = $1; }
	;

opt_literal_list:
		literal_list { $$ = $1; }
	|	/* empty */ { $$ = nullptr; }
	;

literal_list:
		literal { $$ = new std::vector<Expr*>(); $$->push_back($1); }
	|	literal_list ',' literal { $1->push_back($3); $$ = $1; }
	;

expr_alias:
		expr opt_alias {
			$$ = $1;
			if ($2) {
				$$->alias = strdup($2->name);
				delete $2;
			}
		}
	;

expr:
		operand
	|	between_expr
	|	logic_expr
	|	exists_expr
	|	in_expr
	;

operand:
		'(' expr ')' { $$ = $2; }
	|	array_index
	|	scalar_expr
	|	unary_expr
	|	binary_expr
	|	case_expr
	|	function_expr
	|	extract_expr
	|	array_expr
	|	'(' select_no_paren ')' { $$ = Expr::makeSelect($2); }
	;

scalar_expr:
		column_name
	|	literal
	;

unary_expr:
		'-' operand { $$ = Expr::makeOpUnary(kOpUnaryMinus, $2); }
	|	NOT operand { $$ = Expr::makeOpUnary(kOpNot, $2); }
	|	operand ISNULL { $$ = Expr::makeOpUnary(kOpIsNull, $1); }
	|	operand IS NULL { $$ = Expr::makeOpUnary(kOpIsNull, $1); }
	|	operand IS NOT NULL { $$ = Expr::makeOpUnary(kOpNot, Expr::makeOpUnary(kOpIsNull, $1)); }
	;

binary_expr:
		comp_expr
	|	operand '-' operand			{ $$ = Expr::makeOpBinary($1, kOpMinus, $3); }
	|	operand '+' operand			{ $$ = Expr::makeOpBinary($1, kOpPlus, $3); }
	|	operand '/' operand			{ $$ = Expr::makeOpBinary($1, kOpSlash, $3); }
	|	operand '*' operand			{ $$ = Expr::makeOpBinary($1, kOpAsterisk, $3); }
	|	operand '%' operand			{ $$ = Expr::makeOpBinary($1, kOpPercentage, $3); }
	|	operand '^' operand			{ $$ = Expr::makeOpBinary($1, kOpCaret, $3); }
	|	operand LIKE operand		{ $$ = Expr::makeOpBinary($1, kOpLike, $3); }
	|	operand NOT LIKE operand	{ $$ = Expr::makeOpBinary($1, kOpNotLike, $4); }
	|	operand ILIKE operand		{ $$ = Expr::makeOpBinary($1, kOpILike, $3); }
	|	operand CONCAT operand	{ $$ = Expr::makeOpBinary($1, kOpConcat, $3); }
	;

logic_expr:
		expr AND expr	{ $$ = Expr::makeOpBinary($1, kOpAnd, $3); }
	|	expr OR expr	{ $$ = Expr::makeOpBinary($1, kOpOr, $3); }
	;

in_expr:
		operand IN '(' expr_list ')'			{ $$ = Expr::makeInOperator($1, $4); }
	|	operand NOT IN '(' expr_list ')'		{ $$ = Expr::makeOpUnary(kOpNot, Expr::makeInOperator($1, $5)); }
	|	operand IN '(' select_no_paren ')'		{ $$ = Expr::makeInOperator($1, $4); }
	|	operand NOT IN '(' select_no_paren ')'	{ $$ = Expr::makeOpUnary(kOpNot, Expr::makeInOperator($1, $5)); }
	;

// CASE grammar based on: flex & bison by John Levine
// https://www.safaribooksonline.com/library/view/flex-bison/9780596805418/ch04.html#id352665
case_expr:
		CASE expr case_list END         	{ $$ = Expr::makeCase($2, $3, nullptr); }
	|	CASE expr case_list ELSE expr END	{ $$ = Expr::makeCase($2, $3, $5); }
	|	CASE case_list END			        { $$ = Expr::makeCase(nullptr, $2, nullptr); }
	|	CASE case_list ELSE expr END		{ $$ = Expr::makeCase(nullptr, $2, $4); }
	;

case_list:
		WHEN expr THEN expr              { $$ = Expr::makeCaseList(Expr::makeCaseListElement($2, $4)); }
	|	case_list WHEN expr THEN expr    { $$ = Expr::caseListAppend($1, Expr::makeCaseListElement($3, $5)); }
	;

exists_expr:
		EXISTS '(' select_no_paren ')' { $$ = Expr::makeExists($3); }
	|	NOT EXISTS '(' select_no_paren ')' { $$ = Expr::makeOpUnary(kOpNot, Expr::makeExists($4)); }
	;

comp_expr:
		operand '=' operand			{ $$ = Expr::makeOpBinary($1, kOpEquals, $3); }
	|	operand EQUALS operand			{ $$ = Expr::makeOpBinary($1, kOpEquals, $3); }
	|	operand NOTEQUALS operand	{ $$ = Expr::makeOpBinary($1, kOpNotEquals, $3); }
	|	operand '<' operand			{ $$ = Expr::makeOpBinary($1, kOpLess, $3); }
	|	operand '>' operand			{ $$ = Expr::makeOpBinary($1, kOpGreater, $3); }
	|	operand LESSEQ operand		{ $$ = Expr::makeOpBinary($1, kOpLessEq, $3); }
	|	operand GREATEREQ operand	{ $$ = Expr::makeOpBinary($1, kOpGreaterEq, $3); }
	;

function_expr:
               IDENTIFIER '(' ')' { $$ = Expr::makeFunctionRef($1, new std::vector<Expr*>(), false); }
       |       IDENTIFIER '(' opt_distinct expr_list ')' { $$ = Expr::makeFunctionRef($1, $4, $3); }
       ;

extract_expr:
         EXTRACT '(' datetime_field FROM expr ')'    { $$ = Expr::makeExtract($3, $5); }
    ;

datetime_field:
        SECOND { $$ = kDatetimeSecond; }
    |   MINUTE { $$ = kDatetimeMinute; }
    |   HOUR { $$ = kDatetimeHour; }
    |   DAY { $$ = kDatetimeDay; }
    |   MONTH { $$ = kDatetimeMonth; }
    |   YEAR { $$ = kDatetimeYear; }

array_expr:
	  	ARRAY '[' expr_list ']' { $$ = Expr::makeArray($3); }
	;

array_index:
	   	operand '[' int_literal ']' { $$ = Expr::makeArrayIndex($1, $3->ival); }
	;

between_expr:
		operand BETWEEN operand AND operand { $$ = Expr::makeBetween($1, $3, $5); }
	;

column_name:
		IDENTIFIER { $$ = Expr::makeColumnRef($1); }
	|	IDENTIFIER '.' IDENTIFIER { $$ = Expr::makeColumnRef($1, $3); }
	|	'*' { $$ = Expr::makeStar(); }
	|	IDENTIFIER '.' '*' { $$ = Expr::makeStar($1); }
	;

literal:
		string_literal
	|	bool_literal
	|	num_literal
	|	null_literal
	|	param_expr
	;

string_literal:
		STRING { $$ = Expr::makeLiteral($1); }
	;

bool_literal:
		TRUE { $$ = Expr::makeLiteral(true); }
	|	FALSE { $$ = Expr::makeLiteral(false); }
	;

num_literal:
		FLOATVAL { $$ = Expr::makeLiteral($1); }
	|	int_literal
	;

int_literal:
		INTVAL { $$ = Expr::makeLiteral($1); }
	;

null_literal:
	    	NULL { $$ = Expr::makeNullLiteral(); }
	;

param_expr:
		'?' {
			$$ = Expr::makeParameter(yylloc.total_column);
			$$->ival2 = yyloc.param_list.size();
			yyloc.param_list.push_back($$);
		}
	;


/******************************
 * Table
 ******************************/
table_ref:
		table_ref_atomic
	|	table_ref_commalist ',' table_ref_atomic {
			$1->push_back($3);
			auto tbl = new TableRef(kTableCrossProduct);
			tbl->list = $1;
			$$ = tbl;
		}
	;


table_ref_atomic:
		nonjoin_table_ref_atomic
	|	join_clause
	;

nonjoin_table_ref_atomic:
		table_ref_name
	|	'(' select_statement ')' opt_table_alias {
			auto tbl = new TableRef(kTableSelect);
			tbl->select = $2;
			tbl->alias = $4;
			$$ = tbl;
		}
	;

table_ref_commalist:
		table_ref_atomic { $$ = new std::vector<TableRef*>(); $$->push_back($1); }
	|	table_ref_commalist ',' table_ref_atomic { $1->push_back($3); $$ = $1; }
	;


table_ref_name:
		table_name opt_table_alias {
			auto tbl = new TableRef(kTableName);
			tbl->schema = $1.schema;
			tbl->name = $1.name;
			tbl->alias = $2;
			$$ = tbl;
		}
	;


table_ref_name_no_alias:
		table_name {
			$$ = new TableRef(kTableName);
			$$->schema = $1.schema;
			$$->name = $1.name;
		}
	;


table_name:
		IDENTIFIER                { $$.schema = nullptr; $$.name = $1;}
	|	IDENTIFIER '.' IDENTIFIER { $$.schema = $1; $$.name = $3; }
	;


db_name:
		IDENTIFIER                {$$.name = $1;}
	;


table_alias:
		alias
	|	AS IDENTIFIER '(' ident_commalist ')' { $$ = new Alias($2, $4); }
	;


opt_table_alias:
		table_alias
	|	/* empty */ { $$ = nullptr; }


alias:
		AS IDENTIFIER { $$ = new Alias($2); }
	|	IDENTIFIER { $$ = new Alias($1); }
	;


opt_alias:
		alias
	|	/* empty */ { $$ = nullptr; }


/******************************
 * Join Statements
 ******************************/

join_clause:
		table_ref_atomic NATURAL JOIN nonjoin_table_ref_atomic
		{
			$$ = new TableRef(kTableJoin);
			$$->join = new JoinDefinition();
			$$->join->type = kJoinNatural;
			$$->join->left = $1;
			$$->join->right = $4;
		}
	|	table_ref_atomic opt_join_type JOIN table_ref_atomic ON join_condition
		{
			$$ = new TableRef(kTableJoin);
			$$->join = new JoinDefinition();
			$$->join->type = (JoinType) $2;
			$$->join->left = $1;
			$$->join->right = $4;
			$$->join->condition = $6;
		}
	|
		table_ref_atomic opt_join_type JOIN table_ref_atomic USING '(' column_name ')'
		{
			$$ = new TableRef(kTableJoin);
			$$->join = new JoinDefinition();
			$$->join->type = (JoinType) $2;
			$$->join->left = $1;
			$$->join->right = $4;
			auto left_col = Expr::makeColumnRef(strdup($7->name));
			if ($7->alias != nullptr) left_col->alias = strdup($7->alias);
			if ($1->getName() != nullptr) left_col->table = strdup($1->getName());
			auto right_col = Expr::makeColumnRef(strdup($7->name));
			if ($7->alias != nullptr) right_col->alias = strdup($7->alias);
			if ($4->getName() != nullptr) right_col->table = strdup($4->getName());
			$$->join->condition = Expr::makeOpBinary(left_col, kOpEquals, right_col);
			delete $7;
		}
	;

opt_join_type:
		INNER		{ $$ = kJoinInner; }
	|	LEFT OUTER	{ $$ = kJoinLeft; }
	|	LEFT		{ $$ = kJoinLeft; }
	|	RIGHT OUTER	{ $$ = kJoinRight; }
	|	RIGHT		{ $$ = kJoinRight; }
	|	FULL OUTER	{ $$ = kJoinFull; }
	|	OUTER		{ $$ = kJoinFull; }
	|	FULL		{ $$ = kJoinFull; }
	|	CROSS		{ $$ = kJoinCross; }
	|	/* empty, default */	{ $$ = kJoinInner; }
	;


join_condition:
		expr
		;


/******************************
 * Misc
 ******************************/

opt_semicolon:
		';'
	|	/* empty */
	;


ident_commalist:
		IDENTIFIER { $$ = new std::vector<char*>(); $$->push_back($1); }
	|	ident_commalist ',' IDENTIFIER { $1->push_back($3); $$ = $1; }
	;

%%
/*********************************
 ** Section 4: Additional C code
 *********************************/

/* empty */
