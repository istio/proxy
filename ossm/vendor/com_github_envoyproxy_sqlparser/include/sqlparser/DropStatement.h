#pragma once

#include "SQLStatement.h"

// Note: Implementations of constructors and destructors can be found in statements.cpp.
namespace hsql {

  enum DropType {
    kDropTable,
    kDropSchema,
    kDropIndex,
    kDropView,
    kDropDatabase,
    kDropPreparedStatement
  };

  // Represents SQL Delete statements.
  // Example "DROP TABLE students;"
  struct DropStatement : SQLStatement {

    DropStatement(DropType type);
    virtual ~DropStatement();
      void tablesAccessed(TableAccessMap& accessMap) const override {
        if ((name != nullptr) && (type == kDropTable)) {
            TableAccess::addOperation(accessMap, name, schema, TableAccess::OpDrop);
        }
      };
    DropType type;
    bool ifExists;
    char* schema;
    char* name;
  };

} // namespace hsql
