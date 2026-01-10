#ifndef six_sql_h
#define six_sql_h

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <sqlite3.h>

using namespace std;

const char* database_path = "app.db";

void init_database() {
    sqlite3* db;
    int rc = sqlite3_open(database_path, &db);
    if (rc == 0) {
        sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA busy_timeout=1000;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
    }
}

class SQLRow : public map<string, string> {
private:
    string table_name;
    string where_column;
    string where_value;
    
public:
    operator bool() const {
        return !this->empty();
    }
    
    string& operator[](const char* key) {
        return map<string, string>::operator[](string(key));
    }
    
    string& operator[](const string& key) {
        return map<string, string>::operator[](key);
    }
    
    const string& operator[](const char* key) const {
        return map<string, string>::at(string(key));
    }
    
    const string& operator[](const string& key) const {
        return map<string, string>::at(key);
    }
    
    void set_metadata(const char* table, const char* col, const char* val) {
        table_name = table;
        where_column = col;
        where_value = val;
    }
    
    string get_table() const { return table_name; }
    string get_where_column() const { return where_column; }
    string get_where_value() const { return where_value; }
};

map<string, SQLRow> pending_updates;

class SQLRowRef {
private:
    SQLRow* ptr;
    
public:
    SQLRowRef(SQLRow* p) : ptr(p) {}
    
    SQLRowRef() : ptr(nullptr) {}
    
    operator bool() const {
        return ptr != nullptr && !ptr->empty();
    }
    
    string& operator[](const char* key) {
        if (ptr) return (*ptr)[key];
        static string empty;
        return empty;
    }
    
    string& operator[](const string& key) {
        if (ptr) return (*ptr)[key];
        static string empty;
        return empty;
    }
    
    const string& operator[](const char* key) const {
        if (ptr) return (*ptr)[key];
        static string empty;
        return empty;
    }
    
    const string& operator[](const string& key) const {
        if (ptr) return (*ptr)[key];
        static string empty;
        return empty;
    }
    
    operator map<string, string>() const {
        if (ptr) {
            return map<string, string>(*ptr);
        }
        return map<string, string>();
    }
    
    auto begin() const {
        if (ptr) return ptr->begin();
        static map<string, string> empty;
        return empty.begin();
    }
    
    auto end() const {
        if (ptr) return ptr->end();
        static map<string, string> empty;
        return empty.end();
    }
};

vector<string> six_sql_get_columns(const char *table) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    vector<string> columns;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return columns;
    }

    string query = "PRAGMA table_info(" + string(table) + ")";
    
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return columns;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* col_name = (const char*)sqlite3_column_text(stmt, 1);
        if (col_name) {
            columns.push_back(col_name);
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return columns;
}

void six_sql_exec(const char *sql) {
    sqlite3* db;
    char *errMsg = NULL;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return;
    }
    
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    } else {
        cout << "Table created successfully." << endl;
    }

    sqlite3_close(db);
}

void six_sql_insert(const char *table, const map<string, string> &data) {
    sqlite3* db;
    sqlite3_stmt* stmt;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_busy_timeout(db, 5000);

    string columns;
    string placeholders;
    int count = 0;
    
    for (const auto &pair : data) {
        if (count > 0) {
            columns += ", ";
            placeholders += ", ";
        }
        columns += pair.first;
        placeholders += "?";
        count++;
    }

    string query = "INSERT INTO " + string(table) + " (" + columns + ") VALUES (" + placeholders + ")";
    
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    int index = 1;
    for (const auto &pair : data) {
        sqlite3_bind_text(stmt, index, pair.second.c_str(), -1, SQLITE_TRANSIENT);
        index++;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
    } else {
        cout << "Row inserted successfully." << endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "PRAGMA optimize;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
}

SQLRowRef six_sql_find_by(const char *table, const char *column, const char *value) {
    sqlite3* db;
    sqlite3_stmt* stmt;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return SQLRowRef();
    }
    
    sqlite3_busy_timeout(db, 5000);

    string query = "SELECT * FROM " + string(table) + " WHERE " + string(column) + "=?";
    
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return SQLRowRef();
    }
    
    sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
    
    string key = string(table) + ":" + string(column) + ":" + string(value);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        SQLRow result;
        int col_count = sqlite3_column_count(stmt);
        for (int i = 0; i < col_count; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            const char* col_value = (const char*)sqlite3_column_text(stmt, i);
            result[col_name] = col_value ? col_value : "";
        }
        result.set_metadata(table, column, value);
        
        pending_updates[key] = result;
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    if (pending_updates.find(key) != pending_updates.end()) {
        return SQLRowRef(&pending_updates[key]);
    }
    return SQLRowRef();
}

SQLRowRef six_sql_find_by(const char *table, const char *column, const string &value) {
    return six_sql_find_by(table, column, value.c_str());
}

SQLRow six_sql_find_by_readonly(const char *table, const char *column, const char *value) {
    sqlite3* db;
    sqlite3_stmt* stmt;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return SQLRow();
    }
    
    sqlite3_busy_timeout(db, 5000);

    string query = "SELECT * FROM " + string(table) + " WHERE " + string(column) + "=?";
    
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return SQLRow();
    }
    
    sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
    
    SQLRow result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int col_count = sqlite3_column_count(stmt);
        for (int i = 0; i < col_count; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            const char* col_value = (const char*)sqlite3_column_text(stmt, i);
            result[col_name] = col_value ? col_value : "";
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return result;
}

SQLRow six_sql_find_by_readonly(const char *table, const char *column, const string &value) {
    return six_sql_find_by_readonly(table, column, value.c_str());
}

bool six_sql_find_by_and_delete(const char *table, const char *column, const char *value) {
    sqlite3* db;
    sqlite3_stmt* stmt;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    string select_query = "SELECT * FROM " + string(table) + " WHERE " + string(column) + "=?";
    
    rc = sqlite3_prepare_v2(db, select_query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
    
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    if (!found) {
        cout << "No row found to delete." << endl;
        sqlite3_close(db);
        return false;
    }

    string delete_query = "DELETE FROM " + string(table) + " WHERE " + string(column) + "=?";
    
    rc = sqlite3_prepare_v2(db, delete_query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }

    cout << "Row deleted successfully." << endl;
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}

bool six_sql_find_by_and_delete(const char *table, const char *column, const string &value) {
    return six_sql_find_by_and_delete(table, column, value.c_str());
}

vector<SQLRow> six_sql_query_all(const char *table) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    vector<SQLRow> results;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return results;
    }

    sqlite3_busy_timeout(db, 5000);

    string query = "SELECT * FROM " + string(table);
    
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SQLRow row;
        int col_count = sqlite3_column_count(stmt);
        for (int i = 0; i < col_count; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            const char* col_value = (const char*)sqlite3_column_text(stmt, i);
            row[col_name] = col_value ? col_value : "";
        }
        results.push_back(row);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return results;
}

void six_sql_commit() {
    sqlite3* db;

    int rc = sqlite3_open(database_path, &db);
    if (rc) {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    for (auto& [key, row] : pending_updates) {
        string query = "SELECT * FROM " + row.get_table() + " WHERE " + row.get_where_column() + "=?";
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
            continue;
        }

        sqlite3_bind_text(stmt, 1, row.get_where_value().c_str(), -1, SQLITE_TRANSIENT);
        
        SQLRow db_row;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int col_count = sqlite3_column_count(stmt);
            for (int i = 0; i < col_count; i++) {
                const char* col_name = sqlite3_column_name(stmt, i);
                const char* col_value = (const char*)sqlite3_column_text(stmt, i);
                db_row[col_name] = col_value ? col_value : "";
            }
        }
        sqlite3_finalize(stmt);

        string set_clause;
        int count = 0;
        for (const auto &pair : row) {
            if (db_row[pair.first] != pair.second) {
                if (count > 0) set_clause += ", ";
                set_clause += pair.first + "=?";
                count++;
            }
        }

        if (count > 0) {
            string update_query = "UPDATE " + row.get_table() + " SET " + set_clause + 
                          " WHERE " + row.get_where_column() + "=?";
            
            rc = sqlite3_prepare_v2(db, update_query.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
                continue;
            }

            int index = 1;
            for (const auto &pair : row) {
                if (db_row[pair.first] != pair.second) {
                    sqlite3_bind_text(stmt, index, pair.second.c_str(), -1, SQLITE_TRANSIENT);
                    index++;
                }
            }
            sqlite3_bind_text(stmt, index, row.get_where_value().c_str(), -1, SQLITE_TRANSIENT);
            
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE) {
                cout << "Row updated successfully." << endl;
            } else {
                cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
            }
            sqlite3_finalize(stmt);
        }
    }
    
    pending_updates.clear();
    sqlite3_exec(db, "PRAGMA optimize;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
}

void six_sql_clear_pending() {
    pending_updates.clear();
}

#endif