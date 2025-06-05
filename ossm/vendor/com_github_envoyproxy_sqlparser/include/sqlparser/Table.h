#pragma once

#include "Expr.h"
#include <stdio.h>
#include <vector>

namespace hsql {

  struct SelectStatement;
  struct JoinDefinition;
  struct TableRef;

  // Possible table reference types.
  enum TableRefType {
    kTableName,
    kTableSelect,
    kTableJoin,
    kTableCrossProduct
  };

  struct TableName {
    char* schema;
    char* name;
  };

  struct Alias {
    Alias(char* name, std::vector<char*>* columns = nullptr);
    ~Alias();

    char* name;
    std::vector<char*>* columns;
  };

  // Holds reference to tables. Can be either table names or a select statement.
  struct TableRef {
    TableRef(TableRefType type);
    virtual ~TableRef();
      void tablesAccessed(TableAccessMap& accessMap, const std::string& op) const;

    TableRefType type;

    char* schema;
    char* name;
    Alias* alias;

    SelectStatement* select;
    std::vector<TableRef*>* list;
    JoinDefinition* join;

    // Returns true if a schema is set.
    bool hasSchema() const;

    // Returns the alias, if it is set. Otherwise the name.
    const char* getName() const;
  };

  // Possible types of joins.
  enum JoinType {
    kJoinInner,
    kJoinFull,
    kJoinLeft,
    kJoinRight,
    kJoinCross,
    kJoinNatural
  };

  // Definition of a join construct.
  struct JoinDefinition {
    JoinDefinition();
    virtual ~JoinDefinition();
      void tablesAccessed(TableAccessMap& accessMap, const std::string& op) const;

    TableRef* left;
    TableRef* right;
    Expr* condition;

    JoinType type;
  };

} // namespace hsql
