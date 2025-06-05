#pragma once

#include "SQLStatement.h"

namespace hsql {

  // Represents "column = value" expressions.
  struct UpdateClause {
    char* column;
    Expr* value;
  };

  // Represents SQL Update statements.
  struct UpdateStatement : SQLStatement {
    UpdateStatement();
    virtual ~UpdateStatement();
      void tablesAccessed(TableAccessMap& accessMap) const override {
        if (updates != nullptr) {
          for (auto it = updates->begin(); it != updates->end(); ++it) {
            (*it)->value->tablesAccessed(accessMap);
          }
        }
        if (where != nullptr) {
          where->tablesAccessed(accessMap);
        }
        if (table != nullptr) {
          table->tablesAccessed(accessMap, TableAccess::OpUpdate);
        }
      };

    // TODO: switch to char* instead of TableRef
    bool low_priority;  // default: false
    bool ignore;        // default: false
    TableRef* table;
    std::vector<UpdateClause*>* updates;
    Expr* where;
  };

} // namsepace hsql

