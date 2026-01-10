#ifndef six_tpl_engine_h
#define six_tpl_engine_h

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <regex>
#include <sstream>
#include <any>

using namespace std;

const string templates_path = "templates/";

string escape_regex(const string& str) {
    string result;
    for (char c : str) {
        if (string(".-+*?[]{}()^$|\\").find(c) != string::npos) {
            result += '\\';
        }
        result += c;
    }
    return result;
}

string any_to_string(const any& val) {
    if (!val.has_value()) {
        return "";
    }
    
    try {
        if (val.type() == typeid(string)) {
            return any_cast<string>(val);
        } else if (val.type() == typeid(const char*)) {
            return string(any_cast<const char*>(val));
        } else if (val.type() == typeid(int)) {
            return to_string(any_cast<int>(val));
        } else if (val.type() == typeid(long)) {
            return to_string(any_cast<long>(val));
        } else if (val.type() == typeid(double)) {
            return to_string(any_cast<double>(val));
        } else if (val.type() == typeid(float)) {
            return to_string(any_cast<float>(val));
        } else if (val.type() == typeid(bool)) {
            return any_cast<bool>(val) ? "true" : "false";
        }
    } catch (...) {
        return "";
    }
    
    return "";
}

string extract_nested_value(const string& key, const map<string, string>& item_data) {
    size_t dot_pos = key.find('.');
    if (dot_pos != string::npos) {
        string field_name = key.substr(dot_pos + 1);
        auto it = item_data.find(field_name);
        if (it != item_data.end()) {
            return it->second;
        }
    }
    return "";
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

bool evaluate_condition(const string& condition, const map<string, any>& context, 
                       const map<string, string>& loop_item = {}) {
    string cond = trim(condition);
    
    bool is_negated = false;
    if (cond.find("not ") == 0) {
        is_negated = true;
        cond = trim(cond.substr(4));
    }
    
    if (cond.find(" and ") != string::npos) {
        size_t pos = cond.find(" and ");
        string left = cond.substr(0, pos);
        string right = cond.substr(pos + 5);
        
        bool left_result = evaluate_condition(left, context, loop_item);
        bool right_result = evaluate_condition(right, context, loop_item);
        return is_negated ? !(left_result && right_result) : (left_result && right_result);
    }
    
    if (cond.find(" or ") != string::npos) {
        size_t pos = cond.find(" or ");
        string left = cond.substr(0, pos);
        string right = cond.substr(pos + 4);
        
        bool left_result = evaluate_condition(left, context, loop_item);
        bool right_result = evaluate_condition(right, context, loop_item);
        return is_negated ? !(left_result || right_result) : (left_result || right_result);
    }
    
    vector<string> operators = {">=", "<=", "==", "!=", ">", "<"};
    
    for (const string& op : operators) {
        size_t pos = cond.find(op);
        if (pos != string::npos) {
            string left = trim(cond.substr(0, pos));
            string right = trim(cond.substr(pos + op.length()));
            
            string left_val = left;
            string right_val = right;
            
            if (left.find('.') != string::npos) {
                string nested = extract_nested_value(left, loop_item);
                if (!nested.empty()) {
                    left_val = nested;
                }
            }
            
            if (right.find('.') != string::npos) {
                string nested = extract_nested_value(right, loop_item);
                if (!nested.empty()) {
                    right_val = nested;
                }
            }
            
            if (left_val == left) {
                auto left_it = context.find(left);
                if (left_it != context.end()) {
                    left_val = any_to_string(left_it->second);
                }
            }
            
            if (right_val == right) {
                auto right_it = context.find(right);
                if (right_it != context.end()) {
                    right_val = any_to_string(right_it->second);
                }
            }
            
            bool is_numeric = true;
            double left_num = 0, right_num = 0;
            
            try {
                left_num = stod(left_val);
                right_num = stod(right_val);
            } catch (...) {
                is_numeric = false;
            }
            
            bool result = false;
            
            if (is_numeric) {
                if (op == ">") result = left_num > right_num;
                else if (op == "<") result = left_num < right_num;
                else if (op == ">=") result = left_num >= right_num;
                else if (op == "<=") result = left_num <= right_num;
                else if (op == "==") result = left_num == right_num;
                else if (op == "!=") result = left_num != right_num;
            } else {
                if (op == "==") result = left_val == right_val;
                else if (op == "!=") result = left_val != right_val;
            }
            
            return is_negated ? !result : result;
        }
    }
    
    bool result = context.find(cond) != context.end();
    return is_negated ? !result : result;
}

size_t find_matching_endif(const string& input, size_t if_pos) {
    int depth = 1;
    size_t search_pos = if_pos + 6; 
    
    while (search_pos < input.length() && depth > 0) {
        size_t next_if = input.find("{% if ", search_pos);
        size_t next_endif = input.find("{% endif %}", search_pos);
        
        if (next_endif == string::npos) {
            return string::npos;
        }
        
        if (next_if != string::npos && next_if < next_endif) {
            depth++;
            search_pos = next_if + 6;
        } else {
            depth--;
            if (depth == 0) {
                return next_endif;
            }
            search_pos = next_endif + 11;
        }
    }
    
    return string::npos;
}

string process_if_blocks(string input, const map<string, any>& context, 
                        const map<string, string>& loop_item = {}) {
    bool found = true;
    while (found) {
        found = false;
        
        size_t pos = 0;
        while ((pos = input.find("{% if ", pos)) != string::npos) {
            size_t end_if = find_matching_endif(input, pos);
            if (end_if == string::npos) {
                pos++;
                continue;
            }
            
            size_t start = pos + 6; 
            size_t cond_end = input.find("%}", start);
            if (cond_end == string::npos || cond_end > end_if) {
                pos++;
                continue;
            }
            
            string condition = input.substr(start, cond_end - start);
            string content = input.substr(cond_end + 2, end_if - (cond_end + 2));
            
            string replacement = "";
            if (evaluate_condition(condition, context, loop_item)) {
                replacement = content;
            }
            
            input.replace(pos, end_if + 11 - pos, replacement);
            found = true;
            break;
        }
    }
    
    return input;
}

string process_vector_for_blocks(string input, const map<string, any>& context) {
    bool found = true;
    while (found) {
        found = false;
        
        size_t pos = 0;
        while ((pos = input.find("{% for ", pos)) != string::npos) {
            size_t endfor = input.find("{% endfor %}", pos);
            if (endfor == string::npos) {
                pos++;
                continue;
            }
            
            size_t start = pos + 7; 
            size_t for_end = input.find("%}", start);
            if (for_end == string::npos || for_end > endfor) {
                pos++;
                continue;
            }
            
            string for_statement = input.substr(start, for_end - start);
            
            size_t in_pos = for_statement.find(" in ");
            if (in_pos == string::npos) {
                pos++;
                continue;
            }
            
            string item_name = trim(for_statement.substr(0, in_pos));
            string list_name = trim(for_statement.substr(in_pos + 4));
            
            string content = input.substr(for_end + 2, endfor - (for_end + 2));
            
            string replacement = "";
            
            auto list_it = context.find(list_name);
            if (list_it != context.end()) {
                try {
                    auto rows = any_cast<vector<map<string, string>>>(list_it->second);
                    
                    for (size_t i = 0; i < rows.size(); i++) {
                        string item_content = content;
                        
                        item_content = process_if_blocks(item_content, context, rows[i]);
                        
                        size_t var_pos = 0;
                        while ((var_pos = item_content.find("{{ " + item_name + ".", var_pos)) != string::npos) {
                            size_t end_var = item_content.find(" }}", var_pos);
                            if (end_var == string::npos) break;
                            
                            string full_var = item_content.substr(var_pos, end_var + 3 - var_pos);
                            string field_name = full_var.substr(("{{ " + item_name + ".").length());
                            field_name = field_name.substr(0, field_name.length() - 3); 
                            
                            string field_value = "";
                            auto field_it = rows[i].find(field_name);
                            if (field_it != rows[i].end()) {
                                field_value = field_it->second;
                            }
                            item_content.replace(var_pos, full_var.length(), field_value);
                            var_pos += field_value.length();
                        }
                        
                        replacement += item_content;
                    }
                    
                    input.replace(pos, endfor + 12 - pos, replacement);
                    found = true;
                    break;
                } catch (...) {
                
                }
            }
            
            string size_key = list_name + "_vector_size";
            auto size_it = context.find(size_key);
            if (size_it != context.end()) {
                int size = stoi(any_to_string(size_it->second));
                
                for (int i = 0; i < size; i++) {
                    string item_content = content;
                    
                    size_t var_pos = 0;
                    while ((var_pos = item_content.find("{{ " + item_name + ".", var_pos)) != string::npos) {
                        size_t end_var = item_content.find(" }}", var_pos);
                        if (end_var == string::npos) break;
                        
                        string full_var = item_content.substr(var_pos, end_var + 3 - var_pos);
                        string field_name = full_var.substr(("{{ " + item_name + ".").length());
                        field_name = field_name.substr(0, field_name.length() - 3);
                        
                        string field_key = list_name + "_vector_" + to_string(i) + "_" + field_name;
                        auto field_it = context.find(field_key);
                        if (field_it != context.end()) {
                            string field_value = any_to_string(field_it->second);
                            item_content.replace(var_pos, full_var.length(), field_value);
                            var_pos += field_value.length();
                        } else {
                            var_pos += full_var.length();
                        }
                    }
                    
                    replacement += item_content;
                }
                
                input.replace(pos, endfor + 12 - pos, replacement);
                found = true;
                break;
            }
            
            pos++;
        }
    }
    
    return input;
}

string process_for_blocks(string input, const map<string, any>& context) {
    bool found = true;
    while (found) {
        found = false;
        
        size_t pos = 0;
        while ((pos = input.find("{% for ", pos)) != string::npos) {
            size_t endfor = input.find("{% endfor %}", pos);
            if (endfor == string::npos) {
                pos++;
                continue;
            }
            
            size_t start = pos + 7; 
            size_t for_end = input.find("%}", start);
            if (for_end == string::npos || for_end > endfor) {
                pos++;
                continue;
            }
            
            string for_statement = input.substr(start, for_end - start);
            
            size_t in_pos = for_statement.find(" in ");
            if (in_pos == string::npos) {
                pos++;
                continue;
            }
            
            string item_name = trim(for_statement.substr(0, in_pos));
            string list_name = trim(for_statement.substr(in_pos + 4));
            
            string content = input.substr(for_end + 2, endfor - (for_end + 2));
            
            string replacement = "";
            
            for (int i = 1; i <= 100; i++) {
                string key = list_name + to_string(i);
                auto it = context.find(key);
                if (it != context.end()) {
                    string item_content = content;
                    size_t var_pos = 0;
                    string value = any_to_string(it->second);
                    while ((var_pos = item_content.find("{{ " + item_name + " }}", var_pos)) != string::npos) {
                        item_content.replace(var_pos, ("{{ " + item_name + " }}").length(), value);
                        var_pos += value.length();
                    }
                    replacement += item_content;
                } else {
                    break;
                }
            }
            
            input.replace(pos, endfor + 12 - pos, replacement);
            found = true;
            break;
        }
    }
    
    return input;
}

string process_variables(string input, const map<string, any>& context) {
    for (const auto& [key, value] : context) {
        size_t pos = 0;
        string value_str = any_to_string(value);
        while ((pos = input.find("{{ " + key + " }}", pos)) != string::npos) {
            input.replace(pos, ("{{ " + key + " }}").length(), value_str);
            pos += value_str.length();
        }
    }
    
    return input;
}

string render_template(const string& filename, const map<string, any>& context = {}) {
    ifstream file(templates_path + filename);
    if (!file) {
        cerr << "Cannot open " << filename << endl;
        return "";
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string output = buffer.str();

    output = process_vector_for_blocks(output, context);
    output = process_for_blocks(output, context);
    output = process_if_blocks(output, context);
    output = process_variables(output, context);

    return output;
}

vector<map<string, string>> convertToTemplateData(const vector<SQLRow>& rows) {
    vector<map<string, string>> result;
    for (const auto& row : rows) {
        map<string, string> post_map;
        for (const auto& [key, value] : row) {
            post_map[key] = value;
        }
        result.push_back(post_map);
    }
    return result;
}


#endif