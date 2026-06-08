#pragma once

#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

  // Represents SQL Delete statements.
  // Example: "DELETE FROM students WHERE grade > 3.0"
  // Note: if (expr == nullptr) => delete all rows (truncate)
  struct DeleteStatement : SQLStatement {
    DeleteStatement();
    virtual ~DeleteStatement();
    void tablesAccessed(TableAccessMap& accessMap) const override {
      if (expr != nullptr) {
        expr->tablesAccessed(accessMap);
      }
      if (tableName != nullptr) {
          TableAccess::addOperation(accessMap, tableName, schema, TableAccess::OpDelete);
      }
    }
    bool low_priority;  // default: false
    bool quick;         // default: false
    bool ignore;        // default: false
    char* schema;
    char* tableName;
    Expr* expr;
  };

} // namespace hsql

