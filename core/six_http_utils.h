#ifndef six_http_utils_h
#define six_http_utils_h

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
using namespace std;

string loadFile(const string& filepath) {
    ifstream file(filepath);
    if (!file.is_open()) {
        return "";
    }
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

http_response redirect(const string& url) {
    http_response res;
    res.status = 302;
    res.contentType = "text/html";
    res.location = url;
    res.body = "";
    
    if (g_current_response) {
        for (const auto& [key, value] : g_current_response->headers) {
            res.headers[key] = value;
        }
    }
    
    return res;
}

http_response send_from_directory(const string& directory, const string& filepath) {
    http_response res;
    string full_path = directory + "/" + filepath;
    filesystem::path requested_path = filesystem::absolute(full_path);
    filesystem::path base_path = filesystem::absolute(directory);
    
    if (requested_path.string().find(base_path.string()) != 0) {
        res.status = 403;
        res.contentType = "text/html";
        res.body = "<h1>403 Forbidden</h1>";
        return res;
    }
    
    ifstream file(full_path);
    if (!file.is_open()) {
        res.status = 404;
        res.contentType = "text/html";
        string template_content = loadFile("./six/six_templates/404.html");
        if (template_content.empty()) {
            res.body = "<h1>404 Not Found</h1>";
        } else {
            res.body = template_content;
        }
        return res;
    }
    
    stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    res.status = 200;
    
    size_t dot_pos = filepath.find_last_of(".");
    if (dot_pos != string::npos) {
        string ext = filepath.substr(dot_pos + 1);
        
        if (ext == "html") res.contentType = "text/plain";
        else if (ext == "css") res.contentType = "text/plain";
        else if (ext == "js") res.contentType = "text/plain";
        else if (ext == "json") res.contentType = "text/plain";
        else if (ext == "xml") res.contentType = "text/plain";
        else if (ext == "txt") res.contentType = "text/plain";
        else if (ext == "png") res.contentType = "image/png";
        else if (ext == "jpg" || ext == "jpeg") res.contentType = "image/jpeg";
        else if (ext == "gif") res.contentType = "image/gif";
        else if (ext == "svg") res.contentType = "image/svg+xml";
        else res.contentType = "text/plain";
    } else {
        res.contentType = "text/plain";
    }
    
    res.body = buffer.str();
    return res;
}

#endif