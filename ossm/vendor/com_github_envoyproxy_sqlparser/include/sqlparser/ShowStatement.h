#pragma once

#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

  enum ShowType {
    kShowColumns,
    kShowTables,
    kShowDatabases
  };

  // Represents SQL SHOW statements.
  // Example "SHOW TABLES;"
  struct ShowStatement : SQLStatement {

    ShowStatement(ShowType type);
    virtual ~ShowStatement();
      void tablesAccessed(TableAccessMap& accessMap) const override {
          TableAccess::addOperation(accessMap, name, schema, TableAccess::OpShow);
      };

    ShowType type;
    char* schema;
    char* name;
  };

} // namespace hsql
