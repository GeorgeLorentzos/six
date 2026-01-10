#ifndef six_macros_h
#define six_macros_h

#define routeGet(path) \
    server.get(path, [](const http_request& req) -> http_response {

#define routePost(path) \
    server.post(path, [](const http_request& req) -> http_response {

#define end() });

#define vars map<string, any>

#define getParam(name) (req.params.count(name) ? req.params.at(name) : string())

#endif