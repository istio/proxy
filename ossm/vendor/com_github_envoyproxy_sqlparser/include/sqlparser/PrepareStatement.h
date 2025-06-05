#pragma once

#include "SQLStatement.h"

namespace hsql {

  // Represents SQL Prepare statements.
  // Example: PREPARE test FROM 'SELECT * FROM test WHERE a = ?;'
  struct PrepareStatement : SQLStatement {
    PrepareStatement(): SQLStatement(kStmtPrepare),
                        name(nullptr),
                        query(nullptr) {}

      virtual ~PrepareStatement() {
          free(name);
          free(query);
      }

      void tablesAccessed(TableAccessMap&) const override {};

    char* name;

    // The query that is supposed to be prepared.
    char* query;
  };

} // namsepace hsql

