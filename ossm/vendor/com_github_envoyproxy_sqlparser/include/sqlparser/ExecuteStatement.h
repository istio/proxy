#pragma once

#include "SQLStatement.h"

namespace hsql {

  // Represents SQL Execute statements.
  // Example: "EXECUTE ins_prep(100, "test", 2.3);"
  struct ExecuteStatement : SQLStatement {
    ExecuteStatement();
    virtual ~ExecuteStatement();
      void tablesAccessed(TableAccessMap& accessMap) const override {
        if (parameters != nullptr) {
          for (auto it = parameters->begin(); it != parameters->end(); ++it) {
            (*it)->tablesAccessed(accessMap);
          }
        }
      };

    char* name;
    std::vector<Expr*>* parameters;
  };

} // namsepace hsql

