#pragma once

#include <vector>
#include "TableAccess.h"
#include "Expr.h"

namespace hsql {
  enum StatementType {
    kStmtError, // unused
    kStmtSelect,
    kStmtImport,
    kStmtInsert,
    kStmtUpdate,
    kStmtDelete,
    kStmtCreate,
    kStmtDrop,
    kStmtPrepare,
    kStmtExecute,
    kStmtExport,
    kStmtRename,
    kStmtAlter,
    kStmtShow
  };

  // Base struct for every SQL statement
  struct SQLStatement : public TableAccess {

    SQLStatement(StatementType type) :
            hints(nullptr),
            type_(type) {};

    virtual ~SQLStatement() {
        if (hints != nullptr) {
            for (Expr* hint : *hints) {
                delete hint;
            }
        }
        delete hints;
    }

    StatementType type() const {
        return type_;
    }

    bool isType(StatementType type) const {
        return (type_ == type);
    }

    // Shorthand for isType(type).
    bool is(StatementType type) const {
        return isType(type);
    }

    // Length of the string in the SQL query string
    size_t stringLength;

    std::vector<Expr*>* hints;

   private:
    StatementType type_;
  };

} // namespace hsql

