#include "SQLiteDB.hpp"
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Statement.h"

namespace rvdb {
class SnippyDataBase {
public:
  // TODO: Fix large amount of quieres
  std::vector<std::unique_ptr<const uint64_t[]>> Storage;
  std::vector<uint64_t> Operands;

  virtual ~SnippyDataBase() = default;
  virtual bool open() = 0;
  virtual void close() = 0;
  virtual void getRows(uint64_t rows, Order order, RVDBOpcodes opType) = 0;
};

class SQLiteDataBase : public SnippyDataBase {
  std::string path;
  std::unique_ptr<SQLite::Database> db;
  // FIXME: Add to DB
  std::unordered_map<RVDBOpcodes, unsigned> map;

public:
  SQLiteDataBase(std::string path);
  ~SQLiteDataBase();

  bool open();

  void close();

  void getRows(uint64_t rows, Order order, RVDBOpcodes opType);
};
} // namespace rvdb

namespace rvdb {
SQLiteDataBase::SQLiteDataBase(std::string path)
    : path(std::move(path)), db(nullptr) {
  map.emplace(RVDB_ADD, 2);
  map.emplace(RVDB_XOR, 2);
  map.emplace(RVDB_MUL, 2);
}
SQLiteDataBase::~SQLiteDataBase() { this->close(); }

bool SQLiteDataBase::open() {
  try {
    db = std::make_unique<SQLite::Database>(path, SQLite::OPEN_READONLY);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "Failed to open database: " << e.what() << std::endl;
    return false;
  }
}

void SQLiteDataBase::close() { db.reset(); }

void SQLiteDataBase::getRows(uint64_t rows, Order order, RVDBOpcodes opType) {
  if (!db) {
    std::cerr << "Database is not open." << std::endl;
    return;
  }

  try {
    std::string queryStr;
    auto Num = map[opType];
    if (order == Order::Rand) {
      queryStr = "SELECT * FROM data_" + std::to_string(Num) +
                 " WHERE op_type=? ORDER BY RANDOM() LIMIT ?;";
    } else {
      queryStr = "SELECT * FROM data_" + std::to_string(Num) +
                 " WHERE op_type=? LIMIT ?;";
    }

    SQLite::Statement query(*db, queryStr);
    query.bind(1, static_cast<int64_t>(opType));
    query.bind(2, static_cast<int64_t>(rows));

    while (query.executeStep()) {
      for (auto i = 0; i < Num; i++) {
        uint64_t value = query.getColumn(i + 2).getInt64();
        Operands.push_back(value);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}
} // namespace rvdb

struct RVDBState {
  RVDBConfig Config;
  std::unique_ptr<rvdb::SQLiteDataBase> SQLiteDB;
};

RVDBState *rvdb_initDatabase(const RVDBConfig *Config) {
  auto State = new RVDBState();
  State->Config = *Config;
  State->SQLiteDB =
      std::make_unique<rvdb::SQLiteDataBase>(Config->dataBasePath);
  if (!State->SQLiteDB->open()) {
    delete State; // Clean up memory if opening fails
    return nullptr;
  }
  return State;
}

struct Operands rvdb_getOperandsByRows(RVDBState *State, uint64_t rows,
                                       Order order, RVDBOpcodes opType) {
  if (State && State->SQLiteDB) {
    auto &SQLiteDB = *State->SQLiteDB;

    SQLiteDB.getRows(rows, order, opType);
    auto Size = SQLiteDB.Operands.size();
    struct Operands Ops {
      new uint64_t[Size], Size, sizeof(uint64_t)
    };

    std::copy(SQLiteDB.Operands.begin(), SQLiteDB.Operands.end(), Ops.Data);
    SQLiteDB.Storage.emplace_back(Ops.Data);

    return Ops;
  }
  std::cerr << "Invalid state or database." << std::endl;
  return {};
}

void rvdb_closeDatabase(RVDBState *State) {
  if (State) {
    State->SQLiteDB.reset();
    delete State;
  }
}
