

#include "thirdparty/microtest/microtest.h"
#include "sql_asserts.h"
#include "SQLParser.h"

using namespace hsql;

TEST(SelectTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT * FROM students;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_NULL(stmt->whereClause);
  ASSERT_NULL(stmt->groupBy);
}

TEST(SelectExprTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT a, MAX(b), CUSTOM(c, F(un)) FROM students;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_NULL(stmt->whereClause);
  ASSERT_NULL(stmt->groupBy);

  ASSERT_EQ(stmt->selectList->size(), 3);

  ASSERT(stmt->selectList->at(0)->isType(kExprColumnRef));
  ASSERT_STREQ(stmt->selectList->at(0)->getName(), "a");

  ASSERT(stmt->selectList->at(1)->isType(kExprFunctionRef));
  ASSERT_STREQ(stmt->selectList->at(1)->getName(), "MAX");
  ASSERT_NOTNULL(stmt->selectList->at(1)->exprList);
  ASSERT_EQ(stmt->selectList->at(1)->exprList->size(), 1);
  ASSERT(stmt->selectList->at(1)->exprList->at(0)->isType(kExprColumnRef));
  ASSERT_STREQ(stmt->selectList->at(1)->exprList->at(0)->getName(), "b");

  ASSERT(stmt->selectList->at(2)->isType(kExprFunctionRef));
  ASSERT_STREQ(stmt->selectList->at(2)->getName(), "CUSTOM");
  ASSERT_NOTNULL(stmt->selectList->at(2)->exprList);
  ASSERT_EQ(stmt->selectList->at(2)->exprList->size(), 2);
  ASSERT(stmt->selectList->at(2)->exprList->at(0)->isType(kExprColumnRef));
  ASSERT_STREQ(stmt->selectList->at(2)->exprList->at(0)->getName(), "c");

  ASSERT(stmt->selectList->at(2)->exprList->at(1)->isType(kExprFunctionRef));
  ASSERT_STREQ(stmt->selectList->at(2)->exprList->at(1)->getName(), "F");
  ASSERT_EQ(stmt->selectList->at(2)->exprList->at(1)->exprList->size(), 1);
  ASSERT(stmt->selectList->at(2)->exprList->at(1)->exprList->at(0)->isType(kExprColumnRef));
  ASSERT_STREQ(stmt->selectList->at(2)->exprList->at(1)->exprList->at(0)->getName(), "un");
}

TEST(SelectSubstrTest) {
  TEST_PARSE_SINGLE_SQL(
          "SELECT SUBSTR(a, 3, 5) FROM students;",
          kStmtSelect,
          SelectStatement,
          result,
          stmt);

  ASSERT_NULL(stmt->whereClause);
  ASSERT_NULL(stmt->groupBy);

  ASSERT_EQ(stmt->selectList->size(), 1);

  ASSERT(stmt->selectList->at(0)->isType(kExprFunctionRef));
  ASSERT_STREQ(stmt->selectList->at(0)->getName(), "SUBSTR");

  ASSERT_NOTNULL(stmt->selectList->at(0)->exprList);
  ASSERT_EQ(stmt->selectList->at(0)->exprList->size(), 3);

  ASSERT(stmt->selectList->at(0)->exprList->at(0)->isType(kExprColumnRef));
  ASSERT_STREQ(stmt->selectList->at(0)->exprList->at(0)->getName(), "a");

  ASSERT(stmt->selectList->at(0)->exprList->at(1)->isType(kExprLiteralInt));
  ASSERT_EQ(stmt->selectList->at(0)->exprList->at(1)->ival, 3);

  ASSERT(stmt->selectList->at(0)->exprList->at(2)->isType(kExprLiteralInt));
  ASSERT_EQ(stmt->selectList->at(0)->exprList->at(2)->ival, 5);
}


TEST(SelectHavingTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT city, AVG(grade) AS avg_grade FROM students GROUP BY city HAVING AVG(grade) < -2.0",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_FALSE(stmt->selectDistinct);

  GroupByDescription* group = stmt->groupBy;
  ASSERT_NOTNULL(group);
  ASSERT_EQ(group->columns->size(), 1);
  ASSERT_EQ(group->having->opType, kOpLess);
  ASSERT(group->having->expr->isType(kExprFunctionRef));
  ASSERT(group->having->expr2->isType(kExprLiteralFloat));
  ASSERT_EQ(group->having->expr2->fval, -2.0);
}


TEST(SelectDistinctTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT DISTINCT grade, city FROM students;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT(stmt->selectDistinct);
  ASSERT_NULL(stmt->whereClause);
}

TEST(SelectSchemaTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT grade FROM some_schema.students;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT(stmt->fromTable);
  ASSERT_EQ(std::string(stmt->fromTable->schema), "some_schema");
}

TEST(SelectGroupDistinctTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT city, COUNT(name), COUNT(DISTINCT grade) FROM students GROUP BY city;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_FALSE(stmt->selectDistinct);
  ASSERT_EQ(stmt->selectList->size(), 3);
  ASSERT(!stmt->selectList->at(1)->distinct);
  ASSERT(stmt->selectList->at(2)->distinct);
}

TEST(OrderByTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT grade, city FROM students ORDER BY grade, city DESC;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_NULL(stmt->whereClause);
  ASSERT_NOTNULL(stmt->order);

  ASSERT_EQ(stmt->order->size(), 2);
  ASSERT_EQ(stmt->order->at(0)->type, kOrderAsc);
  ASSERT_STREQ(stmt->order->at(0)->expr->name, "grade");

  ASSERT_EQ(stmt->order->at(1)->type, kOrderDesc);
  ASSERT_STREQ(stmt->order->at(1)->expr->name, "city");
}

TEST(SelectBetweenTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT grade, city FROM students WHERE grade BETWEEN -1 and c;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);


  Expr* where = stmt->whereClause;
  ASSERT_NOTNULL(where);
  ASSERT(where->isType(kExprOperator));
  ASSERT_EQ(where->opType, kOpBetween);

  ASSERT_STREQ(where->expr->getName(), "grade");
  ASSERT(where->expr->isType(kExprColumnRef));

  ASSERT_EQ(where->exprList->size(), 2);
  ASSERT(where->exprList->at(0)->isType(kExprLiteralInt));
  ASSERT_EQ(where->exprList->at(0)->ival, -1);
  ASSERT(where->exprList->at(1)->isType(kExprColumnRef));
  ASSERT_STREQ(where->exprList->at(1)->getName(), "c");
}

TEST(SelectConditionalSelectTest) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT * FROM t WHERE a = (SELECT MIN(v) FROM tt) AND EXISTS (SELECT * FROM test WHERE x < a);",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  Expr* where = stmt->whereClause;
  ASSERT_NOTNULL(where);
  ASSERT(where->isType(kExprOperator));
  ASSERT_EQ(where->opType, kOpAnd);

  // a = (SELECT ...)
  Expr* cond1 = where->expr;
  ASSERT_NOTNULL(cond1);
  ASSERT_NOTNULL(cond1->expr);
  ASSERT_EQ(cond1->opType, kOpEquals);
  ASSERT_STREQ(cond1->expr->getName(), "a");
  ASSERT(cond1->expr->isType(kExprColumnRef));

  ASSERT_NOTNULL(cond1->expr2);
  ASSERT(cond1->expr2->isType(kExprSelect));

  SelectStatement* select2 = cond1->expr2->select;
  ASSERT_NOTNULL(select2);
  ASSERT_STREQ(select2->fromTable->getName(), "tt");


  // EXISTS (SELECT ...)
  Expr* cond2 = where->expr2;
  ASSERT_EQ(cond2->opType, kOpExists);
  ASSERT_NOTNULL(cond2->select);

  SelectStatement* ex_select = cond2->select;
  ASSERT_STREQ(ex_select->fromTable->getName(), "test");
}

TEST(SelectCaseWhen) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT MAX(CASE WHEN a = 'foo' THEN x ELSE 0 END) FROM test;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_EQ(stmt->selectList->size(), 1);
  Expr* func = stmt->selectList->at(0);

  ASSERT_NOTNULL(func);
  ASSERT(func->isType(kExprFunctionRef));
  ASSERT_EQ(func->exprList->size(), 1);

  Expr* caseExpr = func->exprList->at(0);
  ASSERT_NOTNULL(caseExpr);
  ASSERT(caseExpr->isType(kExprOperator));
  ASSERT_EQ(caseExpr->opType, kOpCase);
  ASSERT_NULL(caseExpr->expr);
  ASSERT_NOTNULL(caseExpr->exprList);
  ASSERT_NOTNULL(caseExpr->expr2);
  ASSERT_EQ(caseExpr->exprList->size(), 1);
  ASSERT(caseExpr->expr2->isType(kExprLiteralInt));

  Expr* whenExpr = caseExpr->exprList->at(0);
  ASSERT(whenExpr->expr->isType(kExprOperator));
  ASSERT_EQ(whenExpr->expr->opType, kOpEquals);
  ASSERT(whenExpr->expr->expr->isType(kExprColumnRef));
  ASSERT(whenExpr->expr->expr2->isType(kExprLiteralString));
}

TEST(SelectCaseWhenWhen) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT CASE WHEN x = 1 THEN 1 WHEN 1.25 < x THEN 2 END FROM test;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_EQ(stmt->selectList->size(), 1);
  Expr* caseExpr = stmt->selectList->at(0);
  ASSERT_NOTNULL(caseExpr);
  ASSERT(caseExpr->isType(kExprOperator));
  ASSERT_EQ(caseExpr->opType, kOpCase);
  // CASE [expr] [exprList]                                    [expr2]
  //             [expr]                  [expr]
  //             [expr]     [expr2]      [expr]        [expr2]
  // CASE (null) WHEN X = 1 THEN 1       WHEN 1.25 < x THEN 2  (null)
  ASSERT_NULL(caseExpr->expr);
  ASSERT_NOTNULL(caseExpr->exprList);
  ASSERT_NULL(caseExpr->expr2);
  ASSERT_EQ(caseExpr->exprList->size(), 2);

  Expr* whenExpr = caseExpr->exprList->at(0);
  ASSERT_EQ(whenExpr->expr->opType, kOpEquals);
  ASSERT(whenExpr->expr->expr->isType(kExprColumnRef));
  ASSERT(whenExpr->expr->expr2->isType(kExprLiteralInt));

  Expr* whenExpr2 = caseExpr->exprList->at(1);
  ASSERT_EQ(whenExpr2->expr->opType, kOpLess);
  ASSERT(whenExpr2->expr->expr->isType(kExprLiteralFloat));
  ASSERT(whenExpr2->expr->expr2->isType(kExprColumnRef));
}

TEST(SelectCaseValueWhenWhenElse) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT CASE x WHEN 1 THEN 0 WHEN 2 THEN 3 WHEN 8 THEN 7 ELSE 4 END FROM test;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_EQ(stmt->selectList->size(), 1);
  Expr* caseExpr = stmt->selectList->at(0);
  ASSERT_NOTNULL(caseExpr);
  ASSERT(caseExpr->isType(kExprOperator));
  ASSERT_EQ(caseExpr->opType, kOpCase);
  ASSERT_NOTNULL(caseExpr->expr);
  ASSERT_NOTNULL(caseExpr->exprList);
  ASSERT_NOTNULL(caseExpr->expr2);
  ASSERT_EQ(caseExpr->exprList->size(), 3);
  ASSERT(caseExpr->expr->isType(kExprColumnRef));

  Expr* whenExpr = caseExpr->exprList->at(2);
  ASSERT(whenExpr->expr->isType(kExprLiteralInt));
  ASSERT_EQ(whenExpr->expr2->ival, 7);
}

TEST(SelectJoin) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT City.name, Product.category, SUM(price) FROM fact\
      INNER JOIN City ON fact.city_id = City.id\
      OUTER JOIN Product ON fact.product_id = Product.id\
      GROUP BY City.name, Product.category;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  const TableRef* table = stmt->fromTable;
  const JoinDefinition* outer_join = table->join;
  ASSERT_EQ(table->type, kTableJoin);
  ASSERT_EQ(outer_join->type, kJoinFull);

  ASSERT_EQ(outer_join->right->type, kTableName);
  ASSERT_STREQ(outer_join->right->name, "Product");
  ASSERT_EQ(outer_join->condition->opType, kOpEquals);
  ASSERT_STREQ(outer_join->condition->expr->table, "fact");
  ASSERT_STREQ(outer_join->condition->expr->name, "product_id");
  ASSERT_STREQ(outer_join->condition->expr2->table, "Product");
  ASSERT_STREQ(outer_join->condition->expr2->name, "id");

  // Joins are are left associative.
  // So the second join should be on the left.
  ASSERT_EQ(outer_join->left->type, kTableJoin);

  const JoinDefinition* inner_join = outer_join->left->join;
  ASSERT_EQ(inner_join->type, kJoinInner);
  ASSERT_EQ(inner_join->left->type, kTableName);
  ASSERT_STREQ(inner_join->left->name, "fact");
  ASSERT_EQ(inner_join->right->type, kTableName);
  ASSERT_STREQ(inner_join->right->name, "City");

  ASSERT_EQ(inner_join->condition->opType, kOpEquals);
  ASSERT_STREQ(inner_join->condition->expr->table, "fact");
  ASSERT_STREQ(inner_join->condition->expr->name, "city_id");
  ASSERT_STREQ(inner_join->condition->expr2->table, "City");
  ASSERT_STREQ(inner_join->condition->expr2->name, "id");
}


TEST(SelectColumnOrder) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT *\
    FROM a,\
         (SELECT a AS b FROM a) b,\
         (SELECT a AS c FROM a) c,\
         (SELECT a AS d FROM a) d;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_EQ(stmt->fromTable->list->size(), 4);

  // Make sure the order of the table list is corrects
  ASSERT_STREQ(stmt->fromTable->list->at(0)->name, "a");
  ASSERT_STREQ(stmt->fromTable->list->at(1)->alias->name, "b");
  ASSERT_STREQ(stmt->fromTable->list->at(2)->alias->name, "c");
  ASSERT_STREQ(stmt->fromTable->list->at(3)->alias->name, "d");
}


TEST(SelectAliasAbsent) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT * FROM students;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

    ASSERT_NULL(stmt->fromTable->alias);
}

TEST(SelectAliasSimple) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT * FROM students AS s1;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

    Alias* alias = stmt->fromTable->alias;
    ASSERT_NOTNULL(alias);
    ASSERT_STREQ(alias->name, "s1");
    ASSERT_NULL(alias->columns);
}

TEST(SelectAliasWithColumns) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT * FROM students AS s1(id, city);",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

    Alias* alias = stmt->fromTable->alias;
    ASSERT_NOTNULL(alias);

    ASSERT_NOTNULL(alias->name);
    ASSERT_STREQ(alias->name, "s1");

    ASSERT_NOTNULL(alias->columns);
    ASSERT_EQ(alias->columns->size(), 2);
    ASSERT_STREQ(alias->columns->at(0), "id");
    ASSERT_STREQ(alias->columns->at(1), "city");
}

TEST(Operators) {
  SelectStatement* stmt;
  SQLParserResult result;

  SQLParser::parse("SELECT * FROM foo where a =  1; \
		    SELECT * FROM foo where a == 2; \
		    SELECT * FROM foo where a != 1; \
		    SELECT * FROM foo where a <> 1; \
		    SELECT * FROM foo where a >  1; \
		    SELECT * FROM foo where a <  1; \
		    SELECT * FROM foo where a >= 1; \
		    SELECT * FROM foo where a <= 1; \
        SELECT * FROM foo where a = TRUE; \
        SELECT * FROM foo where a = false;", &result);

  stmt = (SelectStatement*) result.getStatement(0);
  ASSERT_EQ(stmt->whereClause->opType, kOpEquals);
  ASSERT_EQ(stmt->whereClause->expr2->ival, 1);
  ASSERT_EQ(stmt->whereClause->expr2->isBoolLiteral, false);

  stmt = (SelectStatement*) result.getStatement(1);
  ASSERT_EQ(stmt->whereClause->opType, kOpEquals);
  ASSERT_EQ(stmt->whereClause->expr2->ival, 2);

  stmt = (SelectStatement*) result.getStatement(2);
  ASSERT_EQ(stmt->whereClause->opType, kOpNotEquals);

  stmt = (SelectStatement*) result.getStatement(3);
  ASSERT_EQ(stmt->whereClause->opType, kOpNotEquals);

  stmt = (SelectStatement*) result.getStatement(4);
  ASSERT_EQ(stmt->whereClause->opType, kOpGreater);

  stmt = (SelectStatement*) result.getStatement(5);
  ASSERT_EQ(stmt->whereClause->opType, kOpLess);

  stmt = (SelectStatement*) result.getStatement(6);
  ASSERT_EQ(stmt->whereClause->opType, kOpGreaterEq);

  stmt = (SelectStatement*) result.getStatement(7);
  ASSERT_EQ(stmt->whereClause->opType, kOpLessEq);

  stmt = (SelectStatement*) result.getStatement(8);
  ASSERT_EQ(stmt->whereClause->opType, kOpEquals);
  ASSERT_EQ(stmt->whereClause->expr2->ival, 1);
  ASSERT_EQ(stmt->whereClause->expr2->isBoolLiteral, true);

  stmt = (SelectStatement*) result.getStatement(9);
  ASSERT_EQ(stmt->whereClause->opType, kOpEquals);
  ASSERT_EQ(stmt->whereClause->expr2->ival, 0);
  ASSERT_EQ(stmt->whereClause->expr2->isBoolLiteral, true);
}

TEST(JoinTypes) {
  SelectStatement* stmt;
  SQLParserResult result;

  SQLParser::parse("SELECT * FROM x join y on a=b; \
		    SELECT * FROM x inner join y on a=b; \
		    SELECT * FROM x left join y on a=b; \
		    SELECT * FROM x left outer join y on a=b; \
		    SELECT * FROM x right join y on a=b; \
		    SELECT * FROM x right outer join y on a=b; \
		    SELECT * FROM x full join y on a=b; \
		    SELECT * FROM x outer join y on a=b; \
		    SELECT * FROM x full outer join y on a=b; \
		    SELECT * FROM x natural join y; \
		    SELECT * FROM x cross join y on a=b; \
		    SELECT * FROM x, y where a = b;", &result);

  stmt = (SelectStatement*) result.getStatement(0);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinInner);

  stmt = (SelectStatement*) result.getStatement(1);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinInner);

  stmt = (SelectStatement*) result.getStatement(2);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinLeft);

  stmt = (SelectStatement*) result.getStatement(3);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinLeft);

  stmt = (SelectStatement*) result.getStatement(4);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinRight);

  stmt = (SelectStatement*) result.getStatement(5);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinRight);

  stmt = (SelectStatement*) result.getStatement(6);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinFull);

  stmt = (SelectStatement*) result.getStatement(7);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinFull);

  stmt = (SelectStatement*) result.getStatement(8);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinFull);

  stmt = (SelectStatement*) result.getStatement(9);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinNatural);

  stmt = (SelectStatement*) result.getStatement(10);
  ASSERT_EQ(stmt->fromTable->join->type, kJoinCross);

  stmt = (SelectStatement*) result.getStatement(11);
  ASSERT_NULL(stmt->fromTable->join);
}

TEST(SetLimitOffset) {
  SelectStatement* stmt;

  TEST_PARSE_SQL_QUERY("select a from t1 limit 1; \
                    select a from t1 limit 1 offset 1; \
                    select a from t1 limit 0; \
                    select a from t1 limit 0 offset 1; \
                    select a from t1 limit 1 offset 0; \
                    select a from t1 limit ALL offset 1; \
                    select a from t1 limit NULL offset 1; \
                    select a from t1 offset 1; \
                    select top 10 a from t1; \
                    select top 20 a from t1 limit 10;",
        result, 10);

  stmt = (SelectStatement*) result.getStatement(0);
  ASSERT_EQ(stmt->limit->limit, 1);
  ASSERT_EQ(stmt->limit->offset, kNoOffset);

  stmt = (SelectStatement*) result.getStatement(1);
  ASSERT_EQ(stmt->limit->limit, 1);
  ASSERT_EQ(stmt->limit->offset, 1);

  stmt = (SelectStatement*) result.getStatement(2);
  ASSERT_EQ(stmt->limit->limit, 0);
  ASSERT_EQ(stmt->limit->offset, kNoOffset);

  stmt = (SelectStatement*) result.getStatement(3);
  ASSERT_EQ(stmt->limit->limit, 0);
  ASSERT_EQ(stmt->limit->offset, 1);

  stmt = (SelectStatement*) result.getStatement(4);
  ASSERT_EQ(stmt->limit->limit, 1);
  ASSERT_EQ(stmt->limit->offset, kNoOffset);

  stmt = (SelectStatement*) result.getStatement(5);
  ASSERT_EQ(stmt->limit->limit, kNoLimit);
    ASSERT_EQ(stmt->limit->offset, 1);

  stmt = (SelectStatement*) result.getStatement(6);
  ASSERT_EQ(stmt->limit->limit, kNoLimit);
  ASSERT_EQ(stmt->limit->offset, 1);

  stmt = (SelectStatement*) result.getStatement(7);
  ASSERT_EQ(stmt->limit->limit, kNoLimit);
  ASSERT_EQ(stmt->limit->offset, 1);

  stmt = (SelectStatement*) result.getStatement(8);
  ASSERT_EQ(stmt->limit->limit, 10);
  ASSERT_EQ(stmt->limit->offset, kNoOffset);

  stmt = (SelectStatement*) result.getStatement(9);
  ASSERT_EQ(stmt->limit->limit, 10);
  ASSERT_EQ(stmt->limit->offset, kNoOffset);
}

TEST(Extract) {
  SelectStatement* stmt;

  TEST_PARSE_SQL_QUERY("select extract(year from dc) FROM t;"
                       "select x, extract(month from dc) AS t FROM t;"
                       "select x FROM t WHERE extract(minute from dc) > 2011;",
                       result, 3);

  stmt = (SelectStatement*) result.getStatement(0);
  ASSERT_TRUE(stmt->selectList);
  ASSERT_EQ(stmt->selectList->size(), 1u);
  ASSERT_EQ(stmt->selectList->at(0)->type, kExprFunctionRef);
  ASSERT_EQ(stmt->selectList->at(0)->name, std::string("EXTRACT"));
  ASSERT_EQ(stmt->selectList->at(0)->datetimeField, kDatetimeYear);
  ASSERT_TRUE(stmt->selectList->at(0)->expr);
  ASSERT_EQ(stmt->selectList->at(0)->expr->type, kExprColumnRef);

  stmt = (SelectStatement*) result.getStatement(1);
  ASSERT_TRUE(stmt->selectList);
  ASSERT_EQ(stmt->selectList->size(), 2u);
  ASSERT_EQ(stmt->selectList->at(1)->type, kExprFunctionRef);
  ASSERT_EQ(stmt->selectList->at(1)->name, std::string("EXTRACT"));
  ASSERT_EQ(stmt->selectList->at(1)->datetimeField, kDatetimeMonth);
  ASSERT_TRUE(stmt->selectList->at(1)->expr);
  ASSERT_EQ(stmt->selectList->at(1)->expr->type, kExprColumnRef);
  ASSERT_TRUE(stmt->selectList->at(1)->alias);
  ASSERT_EQ(stmt->selectList->at(1)->alias, std::string("t"));

  stmt = (SelectStatement*) result.getStatement(2);
  ASSERT_TRUE(stmt->whereClause);
  ASSERT_TRUE(stmt->whereClause->expr);
  ASSERT_EQ(stmt->whereClause->expr->type, kExprFunctionRef);
  ASSERT_EQ(stmt->whereClause->expr->name, std::string("EXTRACT"));
  ASSERT_EQ(stmt->whereClause->expr->datetimeField, kDatetimeMinute);
}

TEST(NoFromClause) {
  TEST_PARSE_SINGLE_SQL(
    "SELECT 1 + 2;",
    kStmtSelect,
    SelectStatement,
    result,
    stmt);

  ASSERT_TRUE(stmt->selectList);
  ASSERT_FALSE(stmt->fromTable);
  ASSERT_FALSE(stmt->whereClause);
  ASSERT_FALSE(stmt->groupBy);

  ASSERT_EQ(stmt->selectList->size(), 1u);
  ASSERT_EQ(stmt->selectList->at(0)->type, kExprOperator);
  ASSERT_EQ(stmt->selectList->at(0)->expr->type, kExprLiteralInt);
  ASSERT_EQ(stmt->selectList->at(0)->expr2->type, kExprLiteralInt);
}
