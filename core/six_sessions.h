#ifndef six_sessions_h
#define six_sessions_h

#include <iostream>
#include <unordered_map>
#include <map>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <memory>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <argon2.h>
#include <mutex>
#include <queue>
#include <ctime>
#include "six_sql.h"

using namespace std;

struct Session;
struct RefreshToken;
struct SessionAuditLog;

class SaltGenerator;
class SessionEncryption;
class SecureSessionHash;
class RateLimiter;
class SessionAuditLogger;
class SessionCookieManager;

class SaltGenerator {
public:
    static string get_unique_salt() {
        return generate_random_salt(16);
    }

private:
    static string generate_random_salt(size_t length) {
        vector<unsigned char> buffer(length);
        if (!RAND_bytes(buffer.data(), length)) {
            throw runtime_error("Failed to generate salt");
        }
        
        stringstream ss;
        for (size_t i = 0; i < length; i++) {
            ss << hex << setw(2) << setfill('0') << (int)buffer[i];
        }
        return ss.str();
    }
};

struct Session {
    string session_id;
    string session_id_hash;
    int user_id = 0;
    string data = "{}";
    
    string created_at;
    string updated_at;
    string expires_at;
    string last_activity;
    
    string ip_address;
    string user_agent;
    string session_salt;
    
    string refresh_token;
    string refresh_token_hash;
    string refresh_expires_at;

    bool exists = false;
    bool is_valid = true;

    string created_ip;
    string last_activity_ip;
};

struct RefreshToken {
    string refresh_token;
    string refresh_token_hash;
    int user_id;
    string session_id;
    string expires_at;
    string ip_address;
    string created_at;
    bool revoked = false;
};

struct SessionAuditLog {
    int id;
    string session_id;
    int user_id;
    string action;
    string ip_address;
    string user_agent;
    string timestamp;
    string details;
};

struct SessionMetrics {
    int64_t total_sessions = 0;
    int64_t active_sessions = 0;
    int64_t hijack_attempts = 0;
    int64_t login_attempts_failed = 0;
    int64_t rate_limit_triggers = 0;
    double avg_session_age_hours = 0.0;
};

class SessionEncryption {
private:
    static const vector<unsigned char> MASTER_KEY;
    static const vector<unsigned char> MASTER_IV;
    
public:
    static string encrypt_session_data(const string& plaintext);
    static string decrypt_session_data(const string& ciphertext_hex);

private:
    static void initialize_keys();
};

const vector<unsigned char> SessionEncryption::MASTER_KEY = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

const vector<unsigned char> SessionEncryption::MASTER_IV = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

inline string SessionEncryption::encrypt_session_data(const string& plaintext) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    vector<unsigned char> ciphertext(plaintext.length() + 16);
    int len = 0;
    int ciphertext_len = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, MASTER_KEY.data(), MASTER_IV.data());
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, (unsigned char*)plaintext.c_str(), plaintext.length());
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    stringstream ss;
    for (int i = 0; i < ciphertext_len; i++) {
        ss << hex << setw(2) << setfill('0') << (int)ciphertext[i];
    }
    return ss.str();
}

inline string SessionEncryption::decrypt_session_data(const string& ciphertext_hex) {
    vector<unsigned char> ciphertext(ciphertext_hex.length() / 2);
    for (size_t i = 0; i < ciphertext_hex.length(); i += 2) {
        string byte = ciphertext_hex.substr(i, 2);
        ciphertext[i / 2] = (unsigned char)strtol(byte.c_str(), nullptr, 16);
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    vector<unsigned char> plaintext(ciphertext_hex.length());
    int len = 0;
    int plaintext_len = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, MASTER_KEY.data(), MASTER_IV.data());
    EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size());
    plaintext_len = len;
    EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    return string((char*)plaintext.data(), plaintext_len);
}

class SecureSessionHash {
private:
    static constexpr uint32_t t_cost = 1;
    static constexpr uint32_t m_cost = 32768;
    static constexpr uint32_t parallelism = 1;

public:
    static string hash_with_salt(const string& data, const string& salt) {
        vector<unsigned char> hash(32);
        
        vector<unsigned char> salt_bytes(salt.length() / 2);
        for (size_t i = 0; i < salt.length(); i += 2) {
            string byte = salt.substr(i, 2);
            salt_bytes[i / 2] = (unsigned char)strtol(byte.c_str(), nullptr, 16);
        }
        
        int result = argon2id_hash_raw(
            t_cost, m_cost, parallelism,
            data.c_str(), data.length(),
            salt_bytes.data(), salt_bytes.size(),
            hash.data(), hash.size()
        );
        
        if (result != ARGON2_OK) {
            throw runtime_error("Argon2 hashing failed: " + string(argon2_error_message(result)));
        }
        
        stringstream ss;
        for (size_t i = 0; i < hash.size(); i++) {
            ss << hex << setw(2) << setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
};

class RateLimiter {
private:
    struct RateLimitEntry {
        int attempts = 0;
        time_t reset_time = 0;
    };
    
    static unordered_map<string, RateLimitEntry> ip_attempts;
    static mutex rate_limit_mutex;
    
    static constexpr int MAX_ATTEMPTS = 5;
    static constexpr int LOCKOUT_DURATION = 15 * 60;
    static constexpr int RESET_WINDOW = 60;

public:
    static bool is_rate_limited(const string& ip_address) {
        lock_guard<mutex> lock(rate_limit_mutex);
        
        auto& entry = ip_attempts[ip_address];
        time_t now = time(nullptr);
        
        if (now > entry.reset_time) {
            entry.attempts = 0;
            entry.reset_time = now + RESET_WINDOW;
        }
        
        if (entry.attempts >= MAX_ATTEMPTS) {
            return true;
        }
        
        return false;
    }
    
    static void record_failed_attempt(const string& ip_address) {
        lock_guard<mutex> lock(rate_limit_mutex);
        
        auto& entry = ip_attempts[ip_address];
        time_t now = time(nullptr);
        
        if (now > entry.reset_time) {
            entry.attempts = 0;
            entry.reset_time = now + RESET_WINDOW;
        }
        
        entry.attempts++;
    }
    
    static void clear_failed_attempts(const string& ip_address) {
        lock_guard<mutex> lock(rate_limit_mutex);
        ip_attempts[ip_address].attempts = 0;
    }
};

unordered_map<string, RateLimiter::RateLimitEntry> RateLimiter::ip_attempts;
mutex RateLimiter::rate_limit_mutex;

class SessionAuditLogger {
private:
    static queue<SessionAuditLog> log_queue;
    static mutex log_mutex;

public:
    static void log_session_event(
        const string& session_id,
        int user_id,
        const string& action,
        const string& ip_address,
        const string& user_agent,
        const string& details = ""
    ) {
        time_t t = time(nullptr);
        char buf[40];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmtime(&t));
        
        SessionAuditLog log;
        log.session_id = session_id;
        log.user_id = user_id;
        log.action = action;
        log.ip_address = ip_address;
        log.user_agent = user_agent;
        log.timestamp = buf;
        log.details = details;
        
        {
            lock_guard<mutex> lock(log_mutex);
            log_queue.push(log);
        }
        
        write_audit_log_to_db(log);
    }
    
    static void detect_hijack_attempt(const string& session_id, const string& new_ip) {
        log_session_event(session_id, 0, "hijack_attempt", new_ip, "", 
                         "Session accessed from different IP");
    }

private:
    static void write_audit_log_to_db(const SessionAuditLog& log) {
        map<string, string> row = {
            {"session_id", log.session_id},
            {"user_id", to_string(log.user_id)},
            {"action", log.action},
            {"ip_address", log.ip_address},
            {"user_agent", log.user_agent},
            {"timestamp", log.timestamp},
            {"details", log.details}
        };
        
        try {
            six_sql_insert("session_audit_log", row);
        } catch (...) {
            cerr << "Failed to write audit log" << endl;
        }
    }
};

queue<SessionAuditLog> SessionAuditLogger::log_queue;
mutex SessionAuditLogger::log_mutex;

static unordered_map<string, Session> session_cache;
static unordered_map<string, RefreshToken> refresh_token_cache;
static mutex session_mutex;

string generate_secure_random_bytes(size_t length) {
    vector<unsigned char> buffer(length);
    if (!RAND_bytes(buffer.data(), length)) {
        throw runtime_error("Failed to generate random bytes");
    }
    
    stringstream ss;
    for (size_t i = 0; i < length; i++) {
        ss << hex << setw(2) << setfill('0') << (int)buffer[i];
    }
    return ss.str();
}

string generate_session_id() {
    return generate_secure_random_bytes(32);
}

string timestamp_now() {
    time_t t = time(nullptr);
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmtime(&t));
    return buf;
}

string timestamp_plus_hours(int hours) {
    using namespace chrono;
    auto now = system_clock::now();
    auto exp = now + chrono::hours(hours);
    time_t t = system_clock::to_time_t(exp);

    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmtime(&t));
    return buf;
}

string timestamp_plus_days(int days) {
    return timestamp_plus_hours(days * 24);
}

Session create_session(int user_id, const string& ip_address, const string& user_agent) {
    if (RateLimiter::is_rate_limited(ip_address)) {
        throw runtime_error("Too many login attempts. Try again later.");
    }
    
    Session s;
    s.session_id = generate_session_id();
    s.session_salt = SaltGenerator::get_unique_salt();
    s.session_id_hash = SecureSessionHash::hash_with_salt(s.session_id, s.session_salt);
    
    s.user_id = user_id;
    s.data = "{}";
    s.created_at = timestamp_now();
    s.updated_at = s.created_at;
    s.last_activity = s.created_at;
    
    s.expires_at = timestamp_plus_hours(1);
    
    s.ip_address = ip_address;
    s.user_agent = user_agent;
    s.created_ip = ip_address;
    s.last_activity_ip = ip_address;
    
    s.refresh_token = generate_secure_random_bytes(32);
    string refresh_salt = SaltGenerator::get_unique_salt();
    s.refresh_token_hash = SecureSessionHash::hash_with_salt(s.refresh_token, refresh_salt);
    s.refresh_expires_at = timestamp_plus_days(30);
    
    s.exists = true;
    s.is_valid = true;

    {
        lock_guard<mutex> lock(session_mutex);
        session_cache[s.session_id] = s;
    }

    string encrypted_data = SessionEncryption::encrypt_session_data(s.data);
    
    map<string, string> row = {
        {"session_id_hash", s.session_id_hash},
        {"session_salt", s.session_salt},
        {"user_id", to_string(user_id)},
        {"data_encrypted", encrypted_data},
        {"created_at", s.created_at},
        {"updated_at", s.updated_at},
        {"expires_at", s.expires_at},
        {"ip_address", ip_address},
        {"user_agent", user_agent},
        {"created_ip", ip_address},
        {"last_activity", s.last_activity},
        {"last_activity_ip", ip_address},
        {"refresh_token_hash", s.refresh_token_hash},
        {"refresh_expires_at", s.refresh_expires_at},
        {"is_valid", "1"}
    };

    try {
        six_sql_insert("sessions", row);
    } catch (...) {
        cerr << "Failed to insert session into database" << endl;
    }
    
    SessionAuditLogger::log_session_event(
        s.session_id, user_id, "create", ip_address, user_agent,
        "New session created"
    );
    
    RateLimiter::clear_failed_attempts(ip_address);

    return s;
}

Session load_session(const string& id, const string& current_ip, const string& current_user_agent) {
    {
        lock_guard<mutex> lock(session_mutex);
        if (session_cache.count(id)) {
            Session s = session_cache[id];
            
            if (s.ip_address != current_ip) {
                SessionAuditLogger::detect_hijack_attempt(id, current_ip);
                return {};
            }
            
            if (s.user_agent != current_user_agent) {
                SessionAuditLogger::detect_hijack_attempt(id, current_ip);
                return {};
            }
            
            string now = timestamp_now();
            if (now > s.expires_at) {
                session_cache.erase(id);
                return {};
            }
            
            return s;
        }
    }

    string session_salt = SaltGenerator::get_unique_salt();
    string session_hash = SecureSessionHash::hash_with_salt(id, session_salt);
    auto row = six_sql_find_by("sessions", "session_id_hash", session_hash.c_str());
    if (!row) {
        return {};
    }

    Session s;
    s.session_id = id;
    s.session_id_hash = row["session_id_hash"];
    s.session_salt = row["session_salt"];
    s.user_id = stoi(row["user_id"]);
    
    try {
        s.data = SessionEncryption::decrypt_session_data(row["data_encrypted"]);
    } catch (...) {
        s.data = "{}";
    }
    
    s.created_at = row["created_at"];
    s.updated_at = row["updated_at"];
    s.expires_at = row["expires_at"];
    s.ip_address = row["ip_address"];
    s.user_agent = row["user_agent"];
    s.created_ip = row["created_ip"];
    s.last_activity = row["last_activity"];
    s.last_activity_ip = row["last_activity_ip"];
    s.refresh_token_hash = row["refresh_token_hash"];
    s.refresh_expires_at = row["refresh_expires_at"];
    s.is_valid = row["is_valid"] == "1";
    s.exists = true;

    if (s.ip_address != current_ip || s.user_agent != current_user_agent) {
        SessionAuditLogger::detect_hijack_attempt(id, current_ip);
        return {};
    }

    string now = timestamp_now();
    if (now > s.expires_at) {
        return {};
    }

    s.last_activity = now;
    s.last_activity_ip = current_ip;

    {
        lock_guard<mutex> lock(session_mutex);
        session_cache[id] = s;
    }

    SessionAuditLogger::log_session_event(
        id, s.user_id, "load", current_ip, current_user_agent
    );

    return s;
}

Session refresh_session(const string& session_id, const string& refresh_token, 
                       const string& current_ip, const string& current_user_agent) {
    Session s = load_session(session_id, current_ip, current_user_agent);
    if (!s.exists) {
        RateLimiter::record_failed_attempt(current_ip);
        throw runtime_error("Invalid session");
    }
    
    string refresh_salt = SaltGenerator::get_unique_salt();
    string refresh_hash = SecureSessionHash::hash_with_salt(refresh_token, refresh_salt);
    if (refresh_hash != s.refresh_token_hash) {
        RateLimiter::record_failed_attempt(current_ip);
        throw runtime_error("Invalid refresh token");
    }
    
    string now = timestamp_now();
    if (now > s.refresh_expires_at) {
        throw runtime_error("Refresh token expired");
    }
    
    Session new_session = create_session(s.user_id, current_ip, current_user_agent);
    
    SessionAuditLogger::log_session_event(
        session_id, s.user_id, "refresh", current_ip, current_user_agent,
        "Session refreshed with new ID"
    );
    
    return new_session;
}

void save_session(Session& s) {
    {
        lock_guard<mutex> lock(session_mutex);
        session_cache[s.session_id] = s;
    }

    string encrypted_data = SessionEncryption::encrypt_session_data(s.data);
    
    auto row = six_sql_find_by("sessions", "session_id_hash", s.session_id_hash.c_str());
    if (!row) return;

    row["user_id"] = to_string(s.user_id);
    row["data_encrypted"] = encrypted_data;
    row["updated_at"] = timestamp_now();
    row["last_activity"] = timestamp_now();
    row["is_valid"] = s.is_valid ? "1" : "0";

    six_sql_commit();
}

void destroy_session(const string& id) {
    Session s;
    {
        lock_guard<mutex> lock(session_mutex);
        if (session_cache.count(id)) {
            s = session_cache[id];
        }
        session_cache.erase(id);
    }
    
    try {
        auto row = six_sql_find_by("sessions", "session_id_hash", s.session_id_hash.c_str());
        if (row) {
            row["is_valid"] = "0";
            six_sql_commit();
            
            SessionAuditLogger::log_session_event(
                id, s.user_id, "logout", "", "",
                "Session destroyed - user logged out"
            );
        }
    } catch (...) {
        cerr << "Error destroying session" << endl;
    }
}

void revoke_all_user_sessions(int user_id, const string& reason = "") {
    {
        lock_guard<mutex> lock(session_mutex);
        vector<string> to_delete;
        
        for (auto& [id, session] : session_cache) {
            if (session.user_id == user_id) {
                to_delete.push_back(id);
            }
        }
        
        for (const auto& id : to_delete) {
            session_cache.erase(id);
        }
    }
    
    try {
        string sql = "UPDATE sessions SET is_valid = 0 WHERE user_id = " + to_string(user_id);
        six_sql_exec(sql.c_str());
    } catch (...) {
        cerr << "Error revoking all sessions for user " << user_id << endl;
    }
    
    SessionAuditLogger::log_session_event(
        "", user_id, "revoke_all", "", "",
        "All sessions revoked: " + reason
    );
}

void setup_sessions_in_database() {
    try {
        six_sql_exec(
            "CREATE TABLE IF NOT EXISTS sessions ("
            "session_id_hash TEXT PRIMARY KEY, "
            "session_salt TEXT, "
            "user_id INTEGER, "
            "data_encrypted TEXT, "
            "created_at DATETIME, "
            "updated_at DATETIME, "
            "expires_at DATETIME, "
            "ip_address TEXT, "
            "user_agent TEXT, "
            "created_ip TEXT, "
            "last_activity DATETIME, "
            "last_activity_ip TEXT, "
            "refresh_token_hash TEXT, "
            "refresh_expires_at DATETIME, "
            "is_valid INTEGER, "
            "created_at_ts BIGINT"
            ");"
        );
        
        six_sql_exec(
            "CREATE TABLE IF NOT EXISTS session_audit_log ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "session_id TEXT, "
            "user_id INTEGER, "
            "action TEXT, "
            "ip_address TEXT, "
            "user_agent TEXT, "
            "timestamp DATETIME, "
            "details TEXT"
            ");"
        );
        
        six_sql_exec("CREATE INDEX IF NOT EXISTS idx_expires_at ON sessions(expires_at);");
        six_sql_exec("CREATE INDEX IF NOT EXISTS idx_user_id ON sessions(user_id);");
        six_sql_exec("CREATE INDEX IF NOT EXISTS idx_is_valid ON sessions(is_valid);");
        six_sql_exec("CREATE INDEX IF NOT EXISTS idx_audit_user ON session_audit_log(user_id);");
        six_sql_exec("CREATE INDEX IF NOT EXISTS idx_audit_action ON session_audit_log(action);");
    } catch (const exception& e) {
        cerr << "Error setting up sessions database: " << e.what() << endl;
    }
}

static bool _sessions_init = (setup_sessions_in_database(), true);

#endif