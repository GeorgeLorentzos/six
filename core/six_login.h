#ifndef six_login_h
#define six_login_h

#include <map>
#include <string>
#include <vector>
#include "six_sessions.h"
#include "six_sql.h"

using namespace std;

extern http_response* g_current_response;
extern http_request* g_current_request;

string get_client_ip_from_request(const http_request& req) {
    auto it = req.headers.find("X-Forwarded-For");
    if (it != req.headers.end()) {
        string ips = it->second;
        size_t comma = ips.find(",");
        if (comma != string::npos) {
            return ips.substr(0, comma);
        }
        return ips;
    }
    
    it = req.headers.find("X-Real-IP");
    if (it != req.headers.end()) {
        return it->second;
    }
    
    return "127.0.0.1";
}

string get_user_agent_from_request(const http_request& req) {
    auto it = req.headers.find("User-Agent");
    if (it != req.headers.end()) {
        return it->second;
    }
    return "Unknown";
}

string get_session_id_from_request(const http_request& req) {
    auto it = req.headers.find("Cookie");
    if (it == req.headers.end()) {
        return "";
    }
    
    string cookies = it->second;
    size_t pos = cookies.find("session_id=");
    if (pos == string::npos) {
        return "";
    }
    
    pos += 11;
    size_t end = cookies.find(";", pos);
    if (end == string::npos) {
        end = cookies.length();
    }
    
    return cookies.substr(pos, end - pos);
}

string get_refresh_token_from_request(const http_request& req) {
    auto it = req.headers.find("Cookie");
    if (it == req.headers.end()) {
        return "";
    }
    
    string cookies = it->second;
    size_t pos = cookies.find("refresh_token=");
    if (pos == string::npos) {
        return "";
    }
    
    pos += 14;
    size_t end = cookies.find(";", pos);
    if (end == string::npos) {
        end = cookies.length();
    }
    
    return cookies.substr(pos, end - pos);
}

struct CurrentUser {
    bool is_authenticated = false;
    int id = 0;
    map<string, string> data;
    string session_id = "";
    string ip_address = "";
    string user_agent = "";
    
    string refresh_token = "";
    
    void load_from_request(const http_request& req) {
        is_authenticated = false;
        id = 0;
        data.clear();
        session_id = "";
        refresh_token = "";
        
        session_id = get_session_id_from_request(req);
        if (session_id.empty()) {
            return;
        }
        
        ip_address = get_client_ip_from_request(req);
        user_agent = get_user_agent_from_request(req);
        
        refresh_token = get_refresh_token_from_request(req);
        
        try {
            Session session = load_session(session_id, ip_address, user_agent);
            
            if (!session.exists || !session.is_valid || session.user_id == 0) {
                return;
            }

            extern SQLRow six_sql_find_by_readonly(const char *table, const char *column, const char *value);
            SQLRow user = six_sql_find_by_readonly("users", "id", to_string(session.user_id).c_str());
            if (user.empty()) {
                return;
            }
            
            is_authenticated = true;
            id = session.user_id;
            
            for (const auto& [key, value] : user) {
                data[key] = value;
            }
            
        } catch (const exception& e) {
            cerr << "Session validation error: " << e.what() << endl;
            is_authenticated = false;
        }
    }
    
    void clear() {
        is_authenticated = false;
        id = 0;
        data.clear();
        session_id = "";
        refresh_token = "";
        ip_address = "";
        user_agent = "";
    }
    
    string get(const string& key) const {
        auto it = data.find(key);
        if (it != data.end()) {
            return it->second;
        }
        return "";
    }
    
    operator map<string, string>() const {
        map<string, string> ctx = data;
        
        ctx["is_authenticated"] = is_authenticated ? "true" : "false";
        ctx["user_id"] = to_string(id);
        
        return ctx;
    }
};

CurrentUser current_user;

void login_user(SQLRowRef user) {
    if (!user || !g_current_response || !g_current_request) {
        return;
    }
    
    try {
        int user_id = stoi(user["id"]);
        
        string client_ip = get_client_ip_from_request(*g_current_request);
        string user_agent = get_user_agent_from_request(*g_current_request);
        
        Session session = create_session(user_id, client_ip, user_agent);
        
        g_current_response->headers["Set-Cookie"] = 
            "session_id=" + session.session_id + 
            "; Path=/; Max-Age=3600; HttpOnly; Secure; SameSite=Strict";
        
        g_current_response->headers["X-Refresh-Token"] = session.refresh_token;
        g_current_response->headers["X-Refresh-Max-Age"] = to_string(30 * 24 * 3600);
        
        g_current_response->headers["Strict-Transport-Security"] = 
            "max-age=31536000; includeSubDomains; preload";
        g_current_response->headers["X-Frame-Options"] = "DENY";
        g_current_response->headers["X-Content-Type-Options"] = "nosniff";
        g_current_response->headers["X-XSS-Protection"] = "1; mode=block";
        
        SessionAuditLogger::log_session_event(
            session.session_id,
            user_id,
            "login",
            client_ip,
            user_agent,
            "User " + user["username"] + " logged in"
        );
        
        RateLimiter::clear_failed_attempts(client_ip);
        
        current_user.is_authenticated = true;
        current_user.id = user_id;
        current_user.session_id = session.session_id;
        current_user.refresh_token = session.refresh_token;
        current_user.ip_address = client_ip;
        current_user.user_agent = user_agent;
        
        for (const auto& [key, value] : user) {
            current_user.data[key] = value;
        }
        
    } catch (const exception& e) {
        cerr << "Login error: " << e.what() << endl;
    }
}

void logout_user() {
    if (!current_user.session_id.empty()) {
        try {
            destroy_session(current_user.session_id);
            
            SessionAuditLogger::log_session_event(
                current_user.session_id,
                current_user.id,
                "logout",
                current_user.ip_address,
                current_user.user_agent,
                "User logged out"
            );
        } catch (const exception& e) {
            cerr << "Logout error: " << e.what() << endl;
        }
    }
    
    if (g_current_response) {
        g_current_response->headers["Set-Cookie"] = 
            "session_id=; Path=/; Max-Age=0; HttpOnly; Secure; SameSite=Strict";
        g_current_response->headers["X-Clear-Refresh-Token"] = "true";
        g_current_response->headers["Cache-Control"] = "no-store, no-cache, must-revalidate";
    }
    
    current_user.clear();
}

bool refresh_session_token() {
    if (!g_current_request || !g_current_response) {
        return false;
    }
    
    try {
        string session_id = get_session_id_from_request(*g_current_request);
        string refresh_token = get_refresh_token_from_request(*g_current_request);
        
        if (session_id.empty() || refresh_token.empty()) {
            return false;
        }
        
        string client_ip = get_client_ip_from_request(*g_current_request);
        string user_agent = get_user_agent_from_request(*g_current_request);
        
        Session new_session = refresh_session(
            session_id,
            refresh_token,
            client_ip,
            user_agent
        );
        
        g_current_response->headers["Set-Cookie"] = 
            "session_id=" + new_session.session_id + 
            "; Path=/; Max-Age=3600; HttpOnly; Secure; SameSite=Strict";
        
        g_current_response->headers["X-Refresh-Token"] = new_session.refresh_token;
        
        current_user.session_id = new_session.session_id;
        current_user.refresh_token = new_session.refresh_token;
        
        return true;
        
    } catch (const exception& e) {
        cerr << "Session refresh error: " << e.what() << endl;
        logout_user();
        return false;
    }
}

bool require_auth() {
    if (!current_user.is_authenticated) {
        if (g_current_response) {
            g_current_response->status = 401;
            g_current_response->body = "{\"error\": \"Unauthorized\"}";
            g_current_response->headers["Content-Type"] = "application/json";
        }
        return false;
    }
    return true;
}

bool require_admin() {
    if (!current_user.is_authenticated) {
        if (g_current_response) {
            g_current_response->status = 401;
            g_current_response->body = "{\"error\": \"Unauthorized\"}";
            g_current_response->headers["Content-Type"] = "application/json";
        }
        return false;
    }
    
    string role = current_user.get("role");
    if (role != "admin") {
        if (g_current_response) {
            g_current_response->status = 403;
            g_current_response->body = "{\"error\": \"Forbidden\"}";
            g_current_response->headers["Content-Type"] = "application/json";
        }
        return false;
    }
    
    return true;
}

bool require_reauthentication(const string& password) {
    if (!current_user.is_authenticated) {
        return false;
    }
    
    try {
        extern SQLRow six_sql_find_by_readonly(const char *table, const char *column, const char *value);
        SQLRow user = six_sql_find_by_readonly("users", "id", to_string(current_user.id).c_str());
        
        if (user.empty()) {
            return false;
        }
        
        return true;
        
    } catch (const exception& e) {
        cerr << "Reauthentication error: " << e.what() << endl;
        return false;
    }
}

void load_current_user() {
    if (g_current_request) {
        current_user.load_from_request(*g_current_request);
    }
}

void revoke_all_user_sessions_on_critical_event(int user_id, const string& reason) {
    try {
        revoke_all_user_sessions(user_id, reason);
        
        if (current_user.id == user_id) {
            logout_user();
        }
    } catch (const exception& e) {
        cerr << "Error revoking all sessions: " << e.what() << endl;
    }
}

void on_password_changed(int user_id) {
    revoke_all_user_sessions_on_critical_event(
        user_id,
        "Password changed"
    );
}

void on_account_compromised(int user_id) {
    revoke_all_user_sessions_on_critical_event(
        user_id,
        "Account compromised - security alert triggered"
    );
}

bool is_login_rate_limited(const string& ip_address) {
    return RateLimiter::is_rate_limited(ip_address);
}

void record_failed_login(const string& ip_address) {
    RateLimiter::record_failed_attempt(ip_address);
}

#endif