
#include "sqlhelper.h"
#include <iostream>
#include <string>

namespace hsql {

  void printOperatorExpression(Expr* expr, uintmax_t numIndent);
  void printAlias(Alias* alias, uintmax_t numIndent);

  std::string indent(uintmax_t numIndent) {
    return std::string(numIndent, '\t');
  }
  void inprint(int64_t val, uintmax_t numIndent) {
    std::cout << indent(numIndent).c_str() << val << "  " << std::endl;
  }
  void inprint(double val, uintmax_t numIndent) {
    std::cout << indent(numIndent).c_str() << val << std::endl;
  }
  void inprint(const char* val, uintmax_t numIndent) {
    std::cout << indent(numIndent).c_str() << val << std::endl;
  }
  void inprint(const char* val, const char* val2, uintmax_t numIndent) {
    std::cout << indent(numIndent).c_str() << val << "->" << val2 << std::endl;
  }
  void inprintC(char val, uintmax_t numIndent) {
    std::cout << indent(numIndent).c_str() << val << std::endl;
  }
  void inprintU(uint64_t val, uintmax_t numIndent) {
    std::cout << indent(numIndent).c_str() << val << std::endl;
  }

  void printTableRefInfo(TableRef* table, uintmax_t numIndent) {
    switch (table->type) {
    case kTableName:
      inprint(table->name, numIndent);
      if(table->schema)  {
        inprint("Schema", numIndent + 1);
        inprint(table->schema, numIndent + 2);
      }
      break;
    case kTableSelect:
      printSelectStatementInfo(table->select, numIndent);
      break;
    case kTableJoin:
      inprint("Join Table", numIndent);
      inprint("Left", numIndent + 1);
      printTableRefInfo(table->join->left, numIndent + 2);
      inprint("Right", numIndent + 1);
      printTableRefInfo(table->join->right, numIndent + 2);
      inprint("Join Condition", numIndent + 1);
      printExpression(table->join->condition, numIndent + 2);
      break;
    case kTableCrossProduct:
      for (TableRef* tbl : *table->list) printTableRefInfo(tbl, numIndent);
      break;
    }

    if (table->alias) {
      printAlias(table->alias, numIndent);
    }
  }

  void printAlias(Alias* alias, uintmax_t numIndent) {
    inprint("Alias", numIndent + 1);
    inprint(alias->name, numIndent + 2);

    if (alias->columns) {
      for (char* column : *(alias->columns)) {
        inprint(column, numIndent + 3);
      }
    }
  }

  void printOperatorExpression(Expr* expr, uintmax_t numIndent) {
    if (expr == nullptr) {
      inprint("null", numIndent);
      return;
    }

    switch (expr->opType) {
    case kOpAnd:
      inprint("AND", numIndent);
      break;
    case kOpOr:
      inprint("OR", numIndent);
      break;
    case kOpNot:
      inprint("NOT", numIndent);
      break;
    default:
      inprintU(expr->opType, numIndent);
      break;
    }
    printExpression(expr->expr, numIndent + 1);
    if (expr->expr2 != nullptr) {
        printExpression(expr->expr2, numIndent + 1);
    } else if (expr->exprList != nullptr) {
        for (Expr* e : *expr->exprList) printExpression(e, numIndent + 1);
    }
  }

  void printExpression(Expr* expr, uintmax_t numIndent) {
    switch (expr->type) {
    case kExprStar:
      inprint("*", numIndent);
      break;
    case kExprColumnRef:
      inprint(expr->name, numIndent);
      if(expr->table) {
        inprint("Table:", numIndent+1);
        inprint(expr->table, numIndent+2);
      }
      break;
    // case kExprTableColumnRef: inprint(expr->table, expr->name, numIndent); break;
    case kExprLiteralFloat:
      inprint(expr->fval, numIndent);
      break;
    case kExprLiteralInt:
      inprint(expr->ival, numIndent);
      break;
    case kExprLiteralString:
      inprint(expr->name, numIndent);
      break;
    case kExprFunctionRef:
      inprint(expr->name, numIndent);
      for (Expr* e : *expr->exprList) printExpression(e, numIndent + 1);
      break;
    case kExprOperator:
      printOperatorExpression(expr, numIndent);
      break;
    case kExprSelect:
      printSelectStatementInfo(expr->select, numIndent);
      break;
    case kExprParameter:
      inprint(expr->ival, numIndent);
      break;
    case kExprArray:
      for (Expr* e : *expr->exprList) printExpression(e, numIndent + 1);
      break;
    case kExprArrayIndex:
      printExpression(expr->expr, numIndent + 1);
      inprint(expr->ival, numIndent);
      break;
    default:
      std::cerr << "Unrecognized expression type " << expr->type << std::endl;
      return;
    }
    if (expr->alias != nullptr) {
      inprint("Alias", numIndent + 1);
      inprint(expr->alias, numIndent + 2);
    }
  }

  void printSelectStatementInfo(const SelectStatement* stmt, uintmax_t numIndent) {
    inprint("SelectStatement", numIndent);
    inprint("Fields:", numIndent + 1);
    for (Expr* expr : *stmt->selectList) printExpression(expr, numIndent + 2);

    if (stmt->fromTable != nullptr) {
      inprint("Sources:", numIndent + 1);
      printTableRefInfo(stmt->fromTable, numIndent + 2);
    }

    if (stmt->whereClause != nullptr) {
      inprint("Search Conditions:", numIndent + 1);
      printExpression(stmt->whereClause, numIndent + 2);
    }

    if (stmt->groupBy != nullptr) {
      inprint("GroupBy:", numIndent + 1);
      for (Expr* expr : *stmt->groupBy->columns) printExpression(expr, numIndent + 2);
      if (stmt->groupBy->having != nullptr) {
        inprint("Having:", numIndent + 1);
        printExpression(stmt->groupBy->having, numIndent + 2);
      }
    }

    if (stmt->unionSelect != nullptr) {
      inprint("Union:", numIndent + 1);
      printSelectStatementInfo(stmt->unionSelect, numIndent + 2);
    }

    if (stmt->order != nullptr) {
      inprint("OrderBy:", numIndent + 1);
      printExpression(stmt->order->at(0)->expr, numIndent + 2);
      if (stmt->order->at(0)->type == kOrderAsc) inprint("ascending", numIndent + 2);
      else inprint("descending", numIndent + 2);
    }

    if (stmt->limit != nullptr) {
      inprint("Limit:", numIndent + 1);
      inprint(stmt->limit->limit, numIndent + 2);
    }
  }



  void printImportStatementInfo(const ImportStatement* stmt, uintmax_t numIndent) {
    inprint("ImportStatment", numIndent);
    inprint(stmt->filePath, numIndent + 1);
    inprint(stmt->tableName, numIndent + 1);
  }

  void printCreateStatementInfo(const CreateStatement* stmt, uintmax_t numIndent) {
    inprint("CreateStatment", numIndent);
    inprint(stmt->tableName, numIndent + 1);
    inprint(stmt->filePath, numIndent + 1);
  }

  void printInsertStatementInfo(const InsertStatement* stmt, uintmax_t numIndent) {
    inprint("InsertStatment", numIndent);
    inprint(stmt->tableName, numIndent + 1);
    if (stmt->columns != nullptr) {
      inprint("Columns", numIndent + 1);
      for (char* col_name : *stmt->columns) {
        inprint(col_name, numIndent + 2);
      }
    }
    switch (stmt->type) {
    case kInsertValues:
      inprint("Values", numIndent + 1);
      for (Expr* expr : *stmt->values) {
        printExpression(expr, numIndent + 2);
      }
      break;
    case kInsertSelect:
      printSelectStatementInfo(stmt->select, numIndent + 1);
      break;
    }
  }

  void printStatementInfo(const SQLStatement* stmt) {
    switch (stmt->type()) {
//    case kStmtSelect:
//      printSelectStatementInfo((const SelectStatement*) stmt, 0);
//      break;
//    case kStmtInsert:
//      printInsertStatementInfo((const InsertStatement*) stmt, 0);
//      break;
//    case kStmtCreate:
//      printCreateStatementInfo((const CreateStatement*) stmt, 0);
//      break;
//    case kStmtImport:
//      printImportStatementInfo((const ImportStatement*) stmt, 0);
//      break;
      case kStmtSelect:
        std::cout << "SELECT\n";
            break;
      case kStmtDelete:
        std::cout << "DELETE\n";
            break;
      case kStmtInsert:
        std::cout << "INSERT\n";
            break;
      case kStmtUpdate:
        std::cout << "UPDATE\n";
            break;
      case kStmtCreate:
        std::cout << "CREATE\n";
            break;
      case kStmtDrop:
        std::cout << "DROP\n";
            break;
      case kStmtAlter:
        std::cout << "ALTER\n";
            break;
      case kStmtExecute:
        std::cout << "EXECUTE\n";
            break;
      case kStmtPrepare:
        std::cout << "PREPARE\n";
            break;
      case kStmtShow:
        std::cout << "SHOW\n";
            break;
      default:
        break;
    }
    TableAccessMap t;
    stmt->tablesAccessed(t);
    for (auto it = t.begin(); it != t.end(); ++it) {
      std::cout << it->first << ":";
      for (auto i = it->second.begin(); i != it->second.end(); ++i)
        std::cout << *i << ' ';
    }
  }

} // namespace hsql
