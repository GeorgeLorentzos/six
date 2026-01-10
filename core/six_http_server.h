#ifndef six_http_server_h
#define six_http_server_h

#include <string>
#include <functional>
#include <map>
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <regex>

using namespace std;

const string PROTOCOL = "http";
const string IP = "localhost";
const int PORT = 8000;

inline string url_decode(const string& str) {
    string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value = 0;
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &value);
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

class FormData {
public:
    std::map<std::string, std::string> data;
    
    std::string get(const char* key) const {
        return get(std::string(key));
    }
    
    std::string get(const std::string& key) const {
        auto it = data.find(key);
        if (it != data.end()) {
            return it->second;
        }
        return "";
    }
};

struct http_request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string raw;
    std::string remote_addr;
    FormData forms;
    std::map<std::string, std::string> params;
};

struct http_response {
    int status = 200;
    std::string contentType = "text/html";
    std::string body;
    std::string location = "";
    std::map<std::string, std::string> headers;

    http_response(const std::string& content = "") : body(content) {}
};

http_response* g_current_response = nullptr;
http_request* g_current_request = nullptr;

struct RoutePattern {
    std::string pattern;
    std::vector<std::string> param_names;
    std::regex regex;

    RoutePattern(const std::string& path) : pattern(path) {
        std::string regex_pattern = path;
        std::regex param_regex(R"(\{([^}]+)\})");
        std::smatch match;
        std::string::const_iterator search_start(regex_pattern.cbegin());

        while (std::regex_search(search_start, regex_pattern.cend(), match, param_regex)) {
            param_names.push_back(match[1].str());
            search_start = match.suffix().first;
        }

        regex_pattern = std::regex_replace(regex_pattern, param_regex, "([^/]+)");
        regex = std::regex("^" + regex_pattern + "$");
    }

    bool matches(const std::string& path, std::map<std::string, std::string>& params) const {
        std::smatch match;
        if (std::regex_match(path, match, regex)) {
            params.clear();
            for (size_t i = 0; i < param_names.size(); ++i) {
                params[param_names[i]] = match[i + 1].str();
            }
            return true;
        }
        return false;
    }
};

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;
    
public:
    ThreadPool(size_t num_threads = 4) {
        if (num_threads < 2) num_threads = 2;
        if (num_threads > 16) num_threads = 16;
        
        for(size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while(true) {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    cv.wait(lock, [this] { return !tasks.empty() || stop; });
                    
                    if(stop && tasks.empty()) return;
                    
                    if(tasks.empty()) continue;
                    
                    auto task = tasks.front();
                    tasks.pop();
                    lock.unlock();
                    
                    try {
                        task();
                    } catch(const std::exception& e) {
                        cerr << "[ERROR] Task exception: " << e.what() << endl;
                    } catch(...) {
                        cerr << "[ERROR] Unknown task exception" << endl;
                    }
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for(auto& worker : workers) {
            if(worker.joinable()) {
                worker.join();
            }
        }
    }
    
    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop) {
                throw std::runtime_error("Cannot enqueue task on stopped thread pool");
            }
            tasks.push(task);
        }
        cv.notify_one();
    }
    
    size_t pending_tasks() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks.size();
    }
};

class six {
public:
    using route_handler = std::function<http_response(const http_request)>;
    
    six(int port = PORT, int num_workers = 4) 
        : port(port), pool(num_workers) {}

    void get(const string& route, route_handler h) {
        routesGET.emplace_back(RoutePattern(route), h);
    }

    void post(const string& route, route_handler h) {
        routesPOST.emplace_back(RoutePattern(route), h);
    }

    void setFallback(route_handler h) {
        fallback = h;
    }

    void start() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) { perror("socket"); return; }

        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt");
            close(server_fd);
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind"); close(server_fd); return;
        }

        if (listen(server_fd, 128) < 0) {
            perror("listen"); close(server_fd); return;
        }

        cout << "Server running at " << PROTOCOL << "://" << IP << ":" << port << " (threaded)" << "\n";
        cout << "Worker threads: " << (int)pool.pending_tasks() + 4 << "\n";

        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_addr_len);
            if (client_fd < 0) { 
                cerr << "[ERROR] Accept failed" << endl;
                continue; 
            }

            struct timeval tv;
            tv.tv_sec = 30;
            tv.tv_usec = 0;
            setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
            setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

            try {
                pool.enqueue([this, client_fd, client_addr]() {
                    this->handleClient(client_fd, client_addr);
                    close(client_fd);
                });
            } catch(const std::exception& e) {
                cerr << "[ERROR] Failed to queue task: " << e.what() << endl;
                close(client_fd);
            }
        }
    }

private:
    int port;
    ThreadPool pool;
    std::vector<std::pair<RoutePattern, route_handler>> routesGET;
    std::vector<std::pair<RoutePattern, route_handler>> routesPOST;
    route_handler fallback;

    string getCurrentTime() {
        time_t now = time(0);
        tm* timeinfo = localtime(&now);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%d/%b/%Y %H:%M:%S", timeinfo);
        return string(buffer);
    }

    string loadFile(const string& filepath) {
        ifstream file(filepath);
        if (!file.is_open()) {
            return "";
        }
        stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void handleClient(int client_fd, sockaddr_in client_addr) {
        char buffer[8192] = {0};
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("[ERROR] read");
            }
            return;
        }

        if (bytes_read == 0) {
            return;
        }

        http_request req;
        req.raw = buffer;
        req.remote_addr = inet_ntoa(client_addr.sin_addr);

        istringstream iss(buffer);
        iss >> req.method >> req.path;
        
        string full_request(buffer, bytes_read);
        
        size_t body_start = full_request.find("\r\n\r\n");
        string headers_part;
        
        if (body_start != string::npos) {
            headers_part = full_request.substr(0, body_start);
            req.body = full_request.substr(body_start + 4);
            
            size_t content_length_pos = headers_part.find("Content-Length: ");
            if (content_length_pos != string::npos) {
                size_t eol = headers_part.find("\r\n", content_length_pos);
                string content_length_str = headers_part.substr(
                    content_length_pos + 16,
                    eol - (content_length_pos + 16)
                );
                int content_length = stoi(content_length_str);
                if (req.body.length() > (size_t)content_length) {
                    req.body = req.body.substr(0, content_length);
                }
            }
        } else {
            headers_part = full_request;
        }

        istringstream header_stream(headers_part);
        string line;
        bool first_line = true;
        while (getline(header_stream, line)) {
            if (first_line) {
                first_line = false;
                continue;
            }
            
            size_t colon = line.find(": ");
            if (colon != string::npos) {
                string key = line.substr(0, colon);
                string value = line.substr(colon + 2);
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                req.headers[key] = value;
            }
        }

        if (req.method == "POST" && !req.body.empty()) {
            string form_data = req.body;
            size_t pair_start = 0;
            
            while (pair_start < form_data.length()) {
                size_t pair_end = form_data.find('&', pair_start);
                if (pair_end == string::npos) {
                    pair_end = form_data.length();
                }
                
                string pair = form_data.substr(pair_start, pair_end - pair_start);
                size_t eq_pos = pair.find('=');
                
                if (eq_pos != string::npos) {
                    string key = url_decode(pair.substr(0, eq_pos));
                    string value = url_decode(pair.substr(eq_pos + 1));
                    
                    size_t last_char = value.find_last_not_of(" \t\r\n\0");
                    if (last_char != string::npos) {
                        value = value.substr(0, last_char + 1);
                    } else {
                        value = "";
                    }
                    
                    req.forms.data[key] = value;
                }
                
                pair_start = pair_end + 1;
            }
        }

        http_response res;
        int status_code = 404;
        
        g_current_response = &res;
        g_current_request = &req;
        
        extern void load_current_user();
        load_current_user();
        
        if (req.method == "GET") {
            for (auto& [pattern, handler] : routesGET) {
                if (pattern.matches(req.path, req.params)) {
                    res = handler(req);
                    status_code = res.status;
                    goto send_response;
                }
            }
        }
        
        if (req.method == "POST") {
            for (auto& [pattern, handler] : routesPOST) {
                if (pattern.matches(req.path, req.params)) {
                    res = handler(req);
                    status_code = res.status;
                    goto send_response;
                }
            }
        }
        
        if (fallback) {
            res = fallback(req);
            status_code = res.status;
        } else {
            res.status = 404;
            string template_content = loadFile("./six/six_templates/404.html");
            if (template_content.empty()) {
                res.body = "<h1>404 Not Found</h1>";
            } else {
                res.body = template_content;
            }
            status_code = 404;
        }

        send_response:
        g_current_response = nullptr;
        g_current_request = nullptr;
        
        extern void six_sql_clear_pending();
        six_sql_clear_pending();

        cout << req.remote_addr << " - - [" << getCurrentTime() << "] \"" << req.method << " " << req.path << " HTTP/1.1\" " << status_code << "\n";

        ostringstream response;
        response << "HTTP/1.1 " << res.status << " OK\r\n";
        response << "Content-Type: " << res.contentType << "; charset=utf-8\r\n";
        if (!res.location.empty()) {
            response << "Location: " << res.location << "\r\n";
        }
        for (const auto& [key, value] : res.headers) {
            response << key << ": " << value << "\r\n";
        }
        response << "Content-Length: " << res.body.length() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << res.body;

        string response_str = response.str();
        ssize_t bytes_written = write(client_fd, response_str.c_str(), response_str.length());
        if (bytes_written < 0) {
            cerr << "[ERROR] Failed to write response" << endl;
        }
    }
};

#endif