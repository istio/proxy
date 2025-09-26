#pragma once

#include "SQLStatement.h"

namespace hsql {
  enum ImportType {
    kImportCSV,
    kImportTbl, // Hyrise file format
  };

  // Represents SQL Import statements.
  struct ImportStatement : SQLStatement {
    ImportStatement(ImportType type);
    virtual ~ImportStatement();
      void tablesAccessed(TableAccessMap& accessMap) const override {
        if (tableName != nullptr) {
            TableAccess::addOperation(accessMap, tableName, schema, TableAccess::OpImport);
        }
      };

    ImportType type;
    char* filePath;
    char* schema;
    char* tableName;
  };

} // namespace hsql

