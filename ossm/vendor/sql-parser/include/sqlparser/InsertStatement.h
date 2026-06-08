#pragma once

#include "SQLStatement.h"
#include "SelectStatement.h"

namespace hsql {
  enum InsertType {
    kInsertValues,
    kInsertSelect
  };

  // Represents SQL Insert statements.
  // Example: "INSERT INTO students VALUES ('Max', 1112233, 'Musterhausen', 2.3)"
  struct InsertStatement : SQLStatement {
    InsertStatement(InsertType type);
    virtual ~InsertStatement();
      void tablesAccessed(TableAccessMap& accessMap) const override {
        if (tableName != nullptr) {
            TableAccess::addOperation(accessMap, tableName, schema, TableAccess::OpInsert);
        }
        if (values != nullptr) {
          for (auto it = values->begin(); it != values->end(); ++it) {
            (*it)->tablesAccessed(accessMap);
          }
        }
        if (select != nullptr) {
          select->tablesAccessed(accessMap);
        }
      };

    InsertType type;
    bool priority;      // default: false
    bool ignore;        // default: false
    char* schema;
    char* tableName;
    std::vector<char*>* columns;
    std::vector<Expr*>* values;
    SelectStatement* select;
  };

} // namsepace hsql

