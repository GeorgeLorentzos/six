# SIX SERVER 🚀

**A lightweight, modern C++ web framework for building web applications**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-beta-orange.svg)]()
[![C++](https://img.shields.io/badge/C++-17-blue.svg)]()

---

## 📋 Table of Contents

- Documentation
  - [Server Setup](#1-server-setup)
  - [Routing](#2-routing)
  - [Authentication](#3-authentication)
  - [Template Engine](#4-template-engine)
  - [HTTP Utilities](#5-http-utilities)
  - [Database (SQL)](#6-database-sql)
  - [Cryptography](#7-cryptography)
  - [Request Data](#8-request-data)
---

## 🎯 Overview

**SIX SERVER** is a C++ web framework designed to make web development simple and intuitive. With built-in authentication, database operations, template rendering, and military-grade encryption, SIX SERVER provides everything you need to build secure web applications.

> ⚠️ **Note:** This framework is currently in **BETA**. APIs may change.

---

## ✨ Features

- 🔌 **Simple Server Setup** - Start a server with just a few lines of code
- 🛣️ **Easy Routing** - Define GET and POST routes effortlessly
- 🔐 **Built-in Authentication** - Login, logout, and authorization out of the box
- 🎨 **Template Engine** - Render dynamic HTML with context variables
- 💾 **SQL Database** - SQLite integration for data persistence
- 🔒 **Argon2 Encryption** - Military-grade password hashing
- 📁 **File Serving** - Serve static files easily
- 🔄 **Redirects** - Simple URL redirection
- 📝 **Form Handling** - Parse form data from requests

---

## 🚀 Quick Start

```cpp
#include "six/six.h"

int main() {
    six server(8000); // Create server on port 8000

    // Define a route
    routeGet("/") {
        return "Hello, World!";
    } end();

    server.start(); // Start the server
    return 0;
}
```

Visit `http://localhost:8000` and see your server in action!

---

## 📚 Documentation

### 1. Server Setup

**Creating a Server**

```cpp
int main() {
    six server(8000); // Set server at port 8000
    
    // Define your routes here
    
    server.start(); // Start server
    return 0;
}
```

---

### 2. Routing

**GET Routes**

```cpp
routeGet("/home") {
    return "Welcome to the home page!";
} end();
```

**POST Routes**

```cpp
routePost("/submit") {
    return "Form submitted!";
} end();
```

**Dynamic URL Parameters**

```cpp
routePost("/{username}") {
    auto username = getParam("username");
    return "Hello " << username;
} end();
```

**Example:**
- URL: `/john` → Returns: "Hello john"
- URL: `/alice` → Returns: "Hello alice"

---

### 3. Authentication

SIX SERVER includes built-in authentication to protect your routes.

#### **Login User**

```cpp
routePost("/login") {
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");
    
    auto existing_user = six_sql_find_by("users", "email", email.c_str());
    
    if (existing_user.empty()) {
        vars ctx;
        ctx["error"] = "User not found";
        return render_template("login.html", ctx);
    }
    
    if (!verify_password(password, existing_user["hashed_password"])) {
        vars ctx;
        ctx["error"] = "Wrong password";
        return render_template("login.html", ctx);
    }
    
    login_user(existing_user); // Log the user in
    return redirect("/dashboard");
} end();
```

#### **Logout User**

```cpp
routeGet("/logout") {
    logout_user(); // Logs out the current user
    return redirect("/login");
} end();
```

#### **Require Authentication**

```cpp
routeGet("/dashboard") {
    require_auth(); // Only authenticated users can access this route
    return render_template("dashboard.html");
} end();
```

#### **Require Admin**

```cpp
routeGet("/admin") {
    require_admin(); // Only admin users can access this route
    return render_template("admin.html");
} end();
```

---

### 4. Template Engine

Render HTML templates with dynamic data.

#### **Basic Template Rendering**

```cpp
routeGet("/profile") {
    return render_template("profile.html");
} end();
```

#### **Rendering with Context Variables**

```cpp
routeGet("/profile") {
    vars ctx;
    ctx["username"] = current_user.get("name");
    ctx["email"] = current_user.get("email");
    ctx["is_authenticated"] = current_user.is_authenticated ? "true" : "false";
    
    return render_template("profile.html", ctx);
} end();
```

#### **Rendering with Database Data**

```cpp
routeGet("/posts") {
    auto posts_list = six_sql_query_all("posts");
    
    vars ctx;
    if (!posts_list.empty()) {
        auto template_posts = convertToTemplateData(posts_list);
        ctx["posts"] = template_posts;
    }
    
    return render_template("posts.html", ctx);
} end();
```

**Template Functions:**
- `render_template(filename, ctx)` - Render a template with context
- `convertToTemplateData(data)` - Convert database results to template format

---

### 5. HTTP Utilities

#### **Send Files from Directory**

```cpp
routeGet("/downloads/{filename}") {
    auto filename = getParam("filename");
    return send_from_directory("downloads", filename);
} end();
```

**Example:**
- URL: `/downloads/report.pdf` → Serves `downloads/report.pdf`

#### **Redirect**

```cpp
routeGet("/old-page") {
    return redirect("/new-page");
} end();
```

---

### 6. Database (SQL)

SIX SERVER uses SQLite for database operations.

#### **Create Table**

```cpp
six_sql_exec(
    "CREATE TABLE IF NOT EXISTS users ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "name TEXT NOT NULL, "
    "email TEXT UNIQUE NOT NULL, "
    "hashed_password TEXT NOT NULL, "
    "is_admin INTEGER DEFAULT 0 "
    ")"
);
```

#### **Insert Data**

```cpp
six_sql_insert("users", {
    {"name", "Lorentzos"},
    {"email", "dev@lorentzos.com"},
    {"hashed_password", generate_password_hash("lorentzos")},
    {"is_admin", "1"}
});

six_sql_commit(); // Commit changes to database
```

#### **Find by Column**

```cpp
auto user = six_sql_find_by("users", "email", "dev@lorentzos.com");

if (!user.empty()) {
    std::cout << "User found: " << user["name"] << std::endl;
}
```

#### **Execute Custom SQL**

```cpp
auto results = six_sql_exec("SELECT * FROM posts WHERE user_id = 1");
```

#### **Query All Records**

```cpp
auto posts = six_sql_query_all("posts");
```

**SQL Functions:**
- `six_sql_exec(query)` - Execute any SQL query
- `six_sql_query_all(table)` - Get all records from a table
- `six_sql_insert(table, data)` - Insert data into a table
- `six_sql_find_by(table, column, value)` - Find a record by column value
- `six_sql_commit()` - Commit database changes

---

### 7. Cryptography

SIX SERVER uses **Argon2** for military-grade password encryption to keep your users safe.

#### **Hash a Password**

```cpp
string hashed = generate_password_hash("my_secure_password");
```

#### **Verify a Password**

```cpp
auto user = six_sql_find_by("users", "email", email.c_str());

if (!verify_password(password, user["hashed_password"])) {
    // Password is incorrect
    vars ctx;
    ctx["error"] = "Wrong password";
    return render_template("login.html", ctx);
}
```

**Cryptography Functions:**
- `generate_password_hash(password)` - Hash a password using Argon2
- `verify_password(password, hash)` - Verify a password against a hash

---

### 8. Request Data

#### **Get Form Data**

```cpp
routePost("/register") {
    const string name = req.forms.get("name");
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");
    
    // Process registration...
} end();
```

#### **Get URL Parameters**

```cpp
routeGet("/{username}/{post_id}") {
    auto username = getParam("username");
    auto post_id = getParam("post_id");
    
    return "User: " << username << ", Post: " << post_id;
} end();
```

---

## 💡 Examples

### Complete Login System

```cpp
#include "six.h"

int main() {
    six server(8000);
    
    // Create users table
    six_sql_exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "email TEXT UNIQUE NOT NULL, "
        "hashed_password TEXT NOT NULL, "
        "is_admin INTEGER DEFAULT 0 "
        ")"
    );
    
    // Home page
    routeGet("/") {
        return render_template("index.html");
    } end();
    
    // Register page
    routeGet("/register") {
        return render_template("register.html");
    } end();
    
    // Register user
    routePost("/register") {
        const string name = req.forms.get("name");
        const string email = req.forms.get("email");
        const string password = req.forms.get("password");
        
        six_sql_insert("users", {
            {"name", name},
            {"email", email},
            {"hashed_password", generate_password_hash(password)},
            {"is_admin", "0"}
        });
        six_sql_commit();
        
        return redirect("/login");
    } end();
    
    // Login page
    routeGet("/login") {
        return render_template("login.html");
    } end();
    
    // Login user
    routePost("/login") {
        const string email = req.forms.get("email");
        const string password = req.forms.get("password");
        
        auto user = six_sql_find_by("users", "email", email.c_str());
        
        if (user.empty() || !verify_password(password, user["hashed_password"])) {
            vars ctx;
            ctx["error"] = "Invalid email or password";
            return render_template("login.html", ctx);
        }
        
        login_user(user);
        return redirect("/dashboard");
    } end();
    
    // Dashboard (protected)
    routeGet("/dashboard") {
        require_auth();
        
        vars ctx;
        ctx["username"] = current_user.get("name");
        ctx["email"] = current_user.get("email");
        
        return render_template("dashboard.html", ctx);
    } end();
    
    // Logout
    routeGet("/logout") {
        logout_user();
        return redirect("/");
    } end();
    
    server.start();
    return 0;
}
```

---

## 🔒 Security

- **Argon2 Password Hashing** - Industry-standard, memory-hard hashing algorithm
- **Session Management** - Built-in user session handling
- **SQL Injection Prevention** - Safe parameterized queries
- **Authentication Guards** - `require_auth()` and `require_admin()` protect routes

---

## 🤝 Contributing

Contributions are welcome! This project is in beta and we're actively looking for feedback and improvements.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

---

## 🙏 Acknowledgments

- Built with ❤️ for the C++ community
- Uses Argon2 for secure password hashing
- SQLite for lightweight database operations

---

## 📞 Contact

For questions, issues, or suggestions, please open an issue on GitHub.

**Happy coding with SIX SERVER! 🎉**
