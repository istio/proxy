#include "../../include/sqlparser/statements.h"

namespace hsql {

  // ColumnDefinition
  ColumnDefinition::ColumnDefinition(char* name, ColumnType type, bool nullable) :
    name(name),
    type(type),
    nullable(nullable) {};

  ColumnDefinition::~ColumnDefinition() {
    free(name);
  }

  ColumnType::ColumnType(DataType data_type, int64_t length) :
    data_type(data_type),
    length(length) {};

  bool operator==(const ColumnType& lhs, const ColumnType& rhs) {
    if (lhs.data_type != rhs.data_type) return false;
    if (lhs.data_type == DataType::VARCHAR || lhs.data_type == DataType::CHAR) {
      return lhs.length == rhs.length;
    }
    return true;
  }

  bool operator!=(const ColumnType& lhs, const ColumnType& rhs) {
    return !(lhs == rhs);
  }

  std::ostream& operator<<(std::ostream& stream, const ColumnType& column_type) {
    switch (column_type.data_type) {
      case DataType::UNKNOWN:
        stream << "UNKNOWN";
        break;
      case DataType::INT:
        stream << "INT";
        break;
      case DataType::LONG:
        stream << "LONG";
        break;
      case DataType::FLOAT:
        stream << "FLOAT";
        break;
      case DataType::DOUBLE:
        stream << "DOUBLE";
        break;
      case DataType::CHAR:
        stream << "CHAR(" << column_type.length << ")";
        break;
      case DataType::VARCHAR:
        stream << "VARCHAR(" << column_type.length << ")";
        break;
      case DataType::TEXT:
        stream << "TEXT";
        break;
    }
    return stream;
  }


  // CreateStatemnet
  CreateStatement::CreateStatement(CreateType type) :
    SQLStatement(kStmtCreate),
    type(type),
    ifNotExists(false),
    filePath(nullptr),
    schema(nullptr),
    tableName(nullptr),
    //databaseName(nullptr),
    columns(nullptr),
    viewColumns(nullptr),
    select(nullptr) {};

  CreateStatement::~CreateStatement() {
    free(filePath);
    free(schema);
    free(tableName);
    //free(databaseName);
    delete select;

    if (columns != nullptr) {
      for (ColumnDefinition* def : *columns) {
        delete def;
      }
      delete columns;
    }

    if (viewColumns != nullptr) {
      for (char* column : *viewColumns) {
        free(column);
      }
      delete viewColumns;
    }
  }

  // AlterStatement
  AlterStatement::AlterStatement(AlterType type) :
    SQLStatement(kStmtAlter),
    type(type),
    schema(nullptr),
    tableName(nullptr),
    //databaseName(nullptr),
    charsetName(nullptr),
    columns(nullptr) {};

  AlterStatement::~AlterStatement() {
    free(schema);
    free(tableName);
    //free(databaseName);
    delete charsetName;
    delete columns;
  }

  // DeleteStatement
  DeleteStatement::DeleteStatement() :
    SQLStatement(kStmtDelete),
    schema(nullptr),
    tableName(nullptr),
    expr(nullptr) {};

  DeleteStatement::~DeleteStatement() {
    free(schema);
    free(tableName);
    delete expr;
  }

  // DropStatament
  DropStatement::DropStatement(DropType type) :
    SQLStatement(kStmtDrop),
    type(type),
    schema(nullptr),
    name(nullptr) {}

  DropStatement::~DropStatement() {
    free(schema);
    free(name);
  }

  // ExecuteStatement
  ExecuteStatement::ExecuteStatement() :
    SQLStatement(kStmtExecute),
    name(nullptr),
    parameters(nullptr) {}

  ExecuteStatement::~ExecuteStatement() {
    free(name);

    if (parameters != nullptr) {
      for (Expr* param : *parameters) {
        delete param;
      }
      delete parameters;
    }
  }

  // ImportStatement
  ImportStatement::ImportStatement(ImportType type) :
    SQLStatement(kStmtImport),
    type(type),
    filePath(nullptr),
    schema(nullptr),
    tableName(nullptr) {};

  ImportStatement::~ImportStatement() {
    free(filePath);
    free(schema);
    free(tableName);
  }

  // InsertStatement
  InsertStatement::InsertStatement(InsertType type) :
    SQLStatement(kStmtInsert),
    type(type),
    schema(nullptr),
    tableName(nullptr),
    columns(nullptr),
    values(nullptr),
    select(nullptr) {}

  InsertStatement::~InsertStatement() {
    free(schema);
    free(tableName);
    delete select;

    if (columns != nullptr) {
      for (char* column : *columns) {
        free(column);
      }
      delete columns;
    }

    if (values != nullptr) {
      for (Expr* expr : *values) {
        delete expr;
      }
      delete values;
    }
  }

  // ShowStatament
  ShowStatement::ShowStatement(ShowType type) :
    SQLStatement(kStmtShow),
    type(type),
    schema(nullptr),
    name(nullptr) {}

  ShowStatement::~ShowStatement() {
    free(schema);
    free(name);
  }

  // SelectStatement.h

  // OrderDescription
  OrderDescription::OrderDescription(OrderType type, Expr* expr) :
    type(type),
    expr(expr) {}

  OrderDescription::~OrderDescription() {
    delete expr;
  }

  // LimitDescription
  LimitDescription::LimitDescription(int64_t limit, int64_t offset) :
    limit(limit >= 0 ? limit : kNoLimit),
    offset(offset > 0 ? offset : kNoOffset) {}

  // GroypByDescription
  GroupByDescription::GroupByDescription() :
    columns(nullptr),
    having(nullptr) {}

  GroupByDescription::~GroupByDescription() {
    delete having;

    if (columns != nullptr) {
      for (Expr* expr : *columns) {
        delete expr;
      }
      delete columns;
    }
  }

  // SelectStatement
  SelectStatement::SelectStatement() :
    SQLStatement(kStmtSelect),
    fromTable(nullptr),
    selectDistinct(false),
    selectList(nullptr),
    whereClause(nullptr),
    groupBy(nullptr),
    unionSelect(nullptr),
    order(nullptr),
    limit(nullptr) {};

  SelectStatement::~SelectStatement() {
    delete fromTable;
    delete whereClause;
    delete groupBy;
    delete unionSelect;
    delete limit;

    // Delete each element in the select list.
    if (selectList != nullptr) {
      for (Expr* expr : *selectList) {
        delete expr;
      }
      delete selectList;
    }

    if (order != nullptr) {
      for (OrderDescription* desc : *order) {
        delete desc;
      }
      delete order;
    }
  }

  // UpdateStatement
  UpdateStatement::UpdateStatement() :
    SQLStatement(kStmtUpdate),
    table(nullptr),
    updates(nullptr),
    where(nullptr) {}

  UpdateStatement::~UpdateStatement() {
    delete table;
    delete where;

    if (updates != nullptr) {
      for (UpdateClause* update : *updates) {
        free(update->column);
        delete update->value;
        delete update;
      }
      delete updates;
    }
  }

  // Alias
  Alias::Alias(char* name, std::vector<char*>* columns) :
    name(name),
    columns(columns) {}

  Alias::~Alias() {
    free(name);
    if (columns) {
      for (char* column : *columns) {
        free(column);
      }
      delete columns;
    }
  }

  // TableRef
  TableRef::TableRef(TableRefType type) :
    type(type),
    schema(nullptr),
    name(nullptr),
    alias(nullptr),
    select(nullptr),
    list(nullptr),
    join(nullptr) {}

  TableRef::~TableRef() {
    free(schema);
    free(name);

    delete select;
    delete join;
    delete alias;

    if (list != nullptr) {
      for (TableRef* table : *list) {
        delete table;
      }
      delete list;
    }
  }

  bool TableRef::hasSchema() const {
    return schema != nullptr;
  }

  const char* TableRef::getName() const {
    if (alias) return alias->name;
    else return name;
  }

  void TableRef::tablesAccessed(TableAccessMap& accessMap, const std::string& op) const {
      switch (type) {
        case kTableName:
          if (name != nullptr) {
            TableAccess::addOperation(accessMap, name, schema, op);
          }
              break;
        case kTableSelect:
          if (select != nullptr) {
            select->tablesAccessed(accessMap);
          }
              break;
        case kTableCrossProduct:
          if (list != nullptr) {
            for (auto it = list->begin(); it != list->end(); ++it) {
              (*it)->tablesAccessed(accessMap, op);
            }
          }
              break;
        case kTableJoin:
          if (join != nullptr) {
            join->tablesAccessed(accessMap, op);
          }

      }
    }

  // JoinDefinition
  JoinDefinition::JoinDefinition() :
    left(nullptr),
    right(nullptr),
    condition(nullptr),
    type(kJoinInner) {}

  JoinDefinition::~JoinDefinition() {
    delete left;
    delete right;
    delete condition;
  }
  void JoinDefinition::tablesAccessed(TableAccessMap& accessMap, const std::string& op) const  {
      if (left != nullptr) {
        left->tablesAccessed(accessMap, op);
      }
      if (right != nullptr) {
        right->tablesAccessed(accessMap, op);
      }
      if (condition != nullptr) {
        condition->tablesAccessed(accessMap);
      }
    };
} // namespace hsql
