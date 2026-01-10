# SIX Web Framework - Complete Documentation

A lightweight, fast C++ web framework for building web applications with built-in database support, authentication, routing, and template rendering.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Server Setup](#server-setup)
3. [Routes](#routes)
4. [Authentication](#authentication)
5. [Template Engine](#template-engine)
6. [HTTP Utilities](#http-utilities)
7. [SQL Database](#sql-database)
8. [Cryptography](#cryptography)
9. [Dynamic Routes](#dynamic-routes)
10. [Form Data](#form-data)
11. [Complete Example](#complete-example)

---

## Getting Started

### Installation

Include the SIX framework header in your C++ project:

```cpp
#include "six/six.h"
```

Make sure to link the six library when compiling.

---

## Server Setup

### Create and Start a Server

To create a web server, create a `six` object with a port number and call `start()`:

```cpp
int main() {
    six server(8000);  // Create server on port 8000
    
    // Define your routes here
    
    server.start();    // Start the server
    return 0;
}
```

The server will now listen on `http://localhost:8000`

---

## Routes

Routes define how your application responds to HTTP requests. SIX supports two types: **GET** and **POST**.

### GET Routes

GET routes are used to display pages and retrieve data. They do not modify data.

**Syntax:**
```cpp
routeGet("/path") {
    // Your code here
} end();
```

**Example - Home Page:**
```cpp
routeGet("/") {
    return render_template("index.html");
} end();
```

**Example - User Profile:**
```cpp
routeGet("/profile") {
    vars ctx;
    ctx["username"] = current_user.get("name");
    ctx["email"] = current_user.get("email");
    return render_template("profile.html", ctx);
} end();
```

### POST Routes

POST routes handle form submissions and data modifications. They receive data from HTML forms.

**Syntax:**
```cpp
routePost("/path") {
    // Your code here
} end();
```

**Example - Create a Post:**
```cpp
routePost("/api/posts") {
    const string title = req.forms.get("title");
    const string content = req.forms.get("content");
    
    // Process the data
    
    return redirect("/");
} end();
```

---

## Authentication

SIX provides built-in authentication functions to manage user sessions.

### `login_user()`

Log in a user and create a session.

**Syntax:**
```cpp
login_user(user_object);
```

**Example:**
```cpp
routePost("/login") {
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");
    
    // Find user in database
    auto user = six_sql_find_by("users", "email", email.c_str());
    
    if (user && verify_password(password, user["hashed_password"])) {
        login_user(user);  // Log the user in
        return redirect("/");
    }
    
    vars ctx;
    ctx["error"] = "Invalid credentials";
    return render_template("login.html", ctx);
} end();
```

### `logout_user()`

Log out the current user and destroy the session. Does not require parameters.

**Syntax:**
```cpp
logout_user();
```

**Example:**
```cpp
routeGet("/logout") {
    logout_user();  // No parameters needed
    return redirect("/login");
} end();
```

### `require_auth()`

Protect a route by requiring user authentication. Automatically redirects to login if not authenticated.

**Example:**
```cpp
routeGet("/dashboard") {
    require_auth();  // User must be logged in
    
    vars ctx;
    ctx["username"] = current_user.get("name");
    return render_template("dashboard.html", ctx);
} end();
```

### `require_admin()`

Protect a route by requiring admin privileges. Only admin users can access.

**Example:**
```cpp
routeGet("/admin-panel") {
    require_admin();  // User must be admin
    
    return render_template("admin.html");
} end();
```

### Check Current User

Access information about the logged-in user:

```cpp
if (current_user.is_authenticated) {
    string name = current_user.get("name");
    string email = current_user.get("email");
    int user_id = current_user.id;
}
```

---

## Template Engine

### `render_template()`

Render an HTML template and optionally pass data to it.

**Syntax:**
```cpp
render_template("filename.html");
render_template("filename.html", context);
```

**Example - Render without data:**
```cpp
routeGet("/about") {
    return render_template("about.html");
} end();
```

### Pass Data with `vars ctx`

Create a context object to pass data from C++ to HTML templates.

**Syntax:**
```cpp
vars ctx;
ctx["key"] = "value";
return render_template("filename.html", ctx);
```

**Example - Pass user data:**
```cpp
routeGet("/profile") {
    vars ctx;
    ctx["username"] = current_user.get("name");
    ctx["email"] = current_user.get("email");
    ctx["is_authenticated"] = current_user.is_authenticated ? "true" : "false";
    
    return render_template("profile.html", ctx);
} end();
```

**Example - Pass error messages:**
```cpp
routePost("/login") {
    const string email = req.forms.get("email");
    
    auto user = six_sql_find_by("users", "email", email.c_str());
    if (!user) {
        vars ctx;
        ctx["error"] = "Account does not exist";
        return render_template("login.html", ctx);
    }
    
    return redirect("/");
} end();
```

**Example - Pass lists of data:**
```cpp
routeGet("/") {
    auto posts_list = six_sql_query_all("posts");
    
    vars ctx;
    ctx["posts"] = convertToTemplateData(posts_list);
    
    return render_template("index.html", ctx);
} end();
```

### Using Variables in HTML Templates

In your `.html` files, access the context variables:

```html
<h1>Welcome, {{ username }}!</h1>
<p>Email: {{ email }}</p>

{% if is_authenticated %}
    <a href="/logout">Logout</a>
{% else %}
    <a href="/login">Login</a>
{% endif %}

{% for post in posts %}
    <div class="post">
        <h2>{{ post.title }}</h2>
        <p>{{ post.content }}</p>
    </div>
{% endfor %}

{% if error %}
    <div class="alert">{{ error }}</div>
{% endif %}
```

---

## HTTP Utilities

### `redirect()`

Redirect the user to another page or route.

**Syntax:**
```cpp
redirect("/path");
```

**Example - Redirect after login:**
```cpp
routePost("/login") {
    // ... authentication code ...
    login_user(user);
    return redirect("/");  // Redirect to home page
} end();
```

**Example - Redirect if already logged in:**
```cpp
routeGet("/login") {
    if (current_user.is_authenticated) {
        return redirect("/");  // Go to home if already logged in
    }
    return render_template("login.html");
} end();
```

**Example - Redirect with error handling:**
```cpp
routePost("/register") {
    const string email = req.forms.get("email");
    
    auto existing = six_sql_find_by("users", "email", email.c_str());
    if (existing) {
        vars ctx;
        ctx["error"] = "Email already exists";
        return render_template("register.html", ctx);
    }
    
    // ... create user ...
    return redirect("/login");  // Redirect to login page
} end();
```

### `send_from_directory()`

Send files from a directory to the client (images, CSS, JS, documents, etc.).

**Syntax:**
```cpp
send_from_directory("directory_path", "filename");
```

**Example - Serve a file:**
```cpp
routeGet("/download-file") {
    return send_from_directory("uploads", "document.pdf");
} end();
```

**Example - Serve user avatar:**
```cpp
routeGet("/avatar/{username}") {
    auto username = getParam("username");
    return send_from_directory("avatars", username + ".jpg");
} end();
```

---

## SQL Database

SIX uses SQLite for database management. All SQL functions handle the database automatically.

### `six_sql_exec()`

Execute raw SQL commands. Used to create tables, run custom queries, etc.

**Syntax:**
```cpp
six_sql_exec("SQL_COMMAND_HERE");
```

**Example - Create users table:**
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

**Example - Create posts table:**
```cpp
six_sql_exec(
    "CREATE TABLE IF NOT EXISTS posts ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "title TEXT NOT NULL, "
    "content TEXT NOT NULL, "
    "author TEXT NOT NULL, "
    "user_id INTEGER NOT NULL, "
    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
    "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
    "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
    ")"
);
```

### `six_sql_insert()`

Insert data into a table.

**Syntax:**
```cpp
six_sql_insert("table_name", {
    {"column_name", "value"},
    {"column_name", "value"},
    {"column_name", "value"}
});
```

**Example - Create a user:**
```cpp
six_sql_insert("users", {
    {"name", "John Doe"},
    {"email", "john@example.com"},
    {"hashed_password", generate_password_hash("password123")},
    {"is_admin", "0"}
});
six_sql_commit();  // Save changes to database
```

**Example - Create a post:**
```cpp
six_sql_insert("posts", {
    {"title", "My First Post"},
    {"content", "This is the content of my post"},
    {"author", "John Doe"},
    {"user_id", "1"}
});
six_sql_commit();  // Save changes to database
```

### `six_sql_find_by()`

Find a record in the database by searching for a column value. Returns a writable object.

**Syntax:**
```cpp
auto result = six_sql_find_by("table_name", "column_name", "search_value");
```

**Example - Find user by email:**
```cpp
auto user = six_sql_find_by("users", "email", "john@example.com");

if (user) {
    cout << "Found user: " << user["name"] << endl;
}
```

**Example - Update user data:**
```cpp
auto user = six_sql_find_by("users", "id", "1");

if (user) {
    user["email"] = "newemail@example.com";
    user["name"] = "Jane Doe";
    six_sql_commit();  // Save changes
}
```

### `six_sql_find_by_readonly()`

Find a record without the ability to modify it. Use this when you only need to read data.

**Syntax:**
```cpp
auto result = six_sql_find_by_readonly("table_name", "column_name", "search_value");
```

**Example - Check if email exists (read-only):**
```cpp
routePost("/register") {
    const string email = req.forms.get("email");
    
    auto existing = six_sql_find_by_readonly("users", "email", email.c_str());
    
    if (existing) {
        vars ctx;
        ctx["error"] = "This email is already registered";
        return render_template("register.html", ctx);
    }
    
    // ... create new user ...
} end();
```

### `six_sql_query_all()`

Get all records from a table.

**Syntax:**
```cpp
auto results = six_sql_query_all("table_name");
```

**Example - Get all posts:**
```cpp
routeGet("/") {
    auto posts_list = six_sql_query_all("posts");
    
    vars ctx;
    if (!posts_list.empty()) {
        ctx["posts"] = convertToTemplateData(posts_list);
    }
    
    return render_template("index.html", ctx);
} end();
```

### `six_sql_find_by_and_delete()`

Find and delete a record from the database.

**Syntax:**
```cpp
six_sql_find_by_and_delete("table_name", "column_name", "search_value");
```

**Example - Delete a post:**
```cpp
routePost("/api/posts/{postId}/delete") {
    auto postId = getParam("postId");
    
    // Check if post exists and belongs to user
    auto post = six_sql_find_by_readonly("posts", "id", postId);
    
    if (post && post["user_id"] == to_string(current_user.id)) {
        six_sql_find_by_and_delete("posts", "id", postId);
        return redirect("/");
    }
    
    vars ctx;
    ctx["error"] = "Post not found";
    return render_template("index.html", ctx);
} end();
```

### `six_sql_commit()`

Save all database changes to disk. Call this after `insert()`, `update()`, or `delete()` operations.

**Syntax:**
```cpp
six_sql_commit();
```

**Example:**
```cpp
routePost("/api/posts") {
    six_sql_insert("posts", {
        {"title", "New Post"},
        {"content", "Post content"},
        {"author", current_user.get("name")},
        {"user_id", to_string(current_user.id)}
    });
    
    six_sql_commit();  // Save to database
    return redirect("/");
} end();
```

---

## Cryptography

SIX uses **Argon2** (military-grade encryption) to securely hash and verify passwords.

### `generate_password_hash()`

Hash a password securely. Use when creating user accounts.

**Syntax:**
```cpp
string hashed = generate_password_hash("password");
```

**Example - Create user with hashed password:**
```cpp
routePost("/register") {
    const string password = req.forms.get("password");
    
    six_sql_insert("users", {
        {"name", "John Doe"},
        {"email", "john@example.com"},
        {"hashed_password", generate_password_hash(password)},  // Hash the password
        {"is_admin", "0"}
    });
    
    six_sql_commit();
    return redirect("/login");
} end();
```

### `verify_password()`

Verify that a password matches the stored hash. Use when logging in users.

**Syntax:**
```cpp
bool is_correct = verify_password("password_to_check", "hashed_password_from_db");
```

**Example - Login verification:**
```cpp
routePost("/login") {
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");
    
    auto user = six_sql_find_by("users", "email", email.c_str());
    
    if (!user) {
        vars ctx;
        ctx["error"] = "Account does not exist";
        return render_template("login.html", ctx);
    }
    
    // Verify password against the hash
    if (!verify_password(password, user["hashed_password"])) {
        vars ctx;
        ctx["error"] = "Wrong password";
        return render_template("login.html", ctx);
    }
    
    // Password correct - log in user
    login_user(user);
    return redirect("/");
} end();
```

---

## Dynamic Routes

Routes can have dynamic segments that capture URL parameters.

### Custom Route Paths with `getParam()`

Use curly braces `{}` in the route path to create dynamic segments. Extract the value with `getParam()`.

**Syntax:**
```cpp
routePost("/{parameter_name}") {
    auto param_value = getParam("parameter_name");
} end();
```

**Example - Greet a user by name:**
```cpp
routeGet("/{username}") {
    auto username = getParam("username");
    return "Hello, " + username + "!";
} end();
```

**Example - Edit a post by ID:**
```cpp
routePost("/api/posts/{postId}/edit") {
    auto postId = getParam("postId");
    
    auto post = six_sql_find_by("posts", "id", postId);
    
    if (!post) {
        vars ctx;
        ctx["error"] = "Post not found";
        return render_template("index.html", ctx);
    }
    
    // Update the post
    const string title = req.forms.get("title");
    post["title"] = title;
    six_sql_commit();
    
    return redirect("/");
} end();
```

**Example - Get user profile by ID:**
```cpp
routeGet("/user/{userId}") {
    auto userId = getParam("userId");
    
    auto user = six_sql_find_by_readonly("users", "id", userId);
    
    if (user) {
        vars ctx;
        ctx["name"] = user["name"];
        ctx["email"] = user["email"];
        return render_template("user_profile.html", ctx);
    }
    
    return redirect("/");
} end();
```

---

## Form Data

Receive data from HTML forms submitted via POST requests.

### `req.forms.get()`

Get a value from the submitted form by its field name.

**Syntax:**
```cpp
string value = req.forms.get("field_name");
```

**Example - Login form:**
```cpp
routePost("/login") {
    const string email = req.forms.get("email");      // From form input name="email"
    const string password = req.forms.get("password"); // From form input name="password"
    
    // Use the form data
    auto user = six_sql_find_by("users", "email", email.c_str());
    
    if (user && verify_password(password, user["hashed_password"])) {
        login_user(user);
        return redirect("/");
    }
    
    vars ctx;
    ctx["error"] = "Invalid credentials";
    return render_template("login.html", ctx);
} end();
```

**Example - Create post form:**
```cpp
routePost("/api/posts") {
    const string title = req.forms.get("title");       // From form input name="title"
    const string content = req.forms.get("content");   // From form input name="content"
    
    if (title.empty() || content.empty()) {
        vars ctx;
        ctx["error"] = "Title and content cannot be empty";
        return render_template("index.html", ctx);
    }
    
    six_sql_insert("posts", {
        {"title", title},
        {"content", content},
        {"author", current_user.get("name")},
        {"user_id", to_string(current_user.id)}
    });
    
    six_sql_commit();
    return redirect("/");
} end();
```

**Example - User registration form:**
```cpp
routePost("/register") {
    const string username = req.forms.get("username");
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");
    
    // Validate form data
    if (username.empty() || email.empty() || password.empty()) {
        vars ctx;
        ctx["error"] = "Please fill all fields";
        return render_template("register.html", ctx);
    }
    
    // Create user
    six_sql_insert("users", {
        {"name", username},
        {"email", email},
        {"hashed_password", generate_password_hash(password)},
        {"is_admin", "0"}
    });
    
    six_sql_commit();
    return redirect("/login");
} end();
```

**Corresponding HTML Form:**
```html
<form method="POST" action="/login">
    <input type="email" name="email" placeholder="Email" required>
    <input type="password" name="password" placeholder="Password" required>
    <button type="submit">Login</button>
</form>
```

---

## Complete Example

Here's a complete, working blog application demonstrating all SIX features:

```cpp
#include "six/six.h"

// ============================================
// DATABASE SETUP
// ============================================

void create_db_models() {
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

    // Create posts table
    six_sql_exec(
        "CREATE TABLE IF NOT EXISTS posts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "title TEXT NOT NULL, "
        "content TEXT NOT NULL, "
        "author TEXT NOT NULL, "
        "user_id INTEGER NOT NULL, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
        "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
        ")"
    );
}

void create_admin_account() {
    auto existing_user = six_sql_find_by("users", "email", "admin@example.com");
    
    if (!existing_user) {
        six_sql_insert("users", {
            {"name", "Admin"},
            {"email", "admin@example.com"},
            {"hashed_password", generate_password_hash("admin123")},
            {"is_admin", "1"}
        });
        six_sql_commit();
        cout << "Admin account created" << endl;
    }
}

// ============================================
// MAIN APPLICATION
// ============================================

int main() {
    six server(8000);

    // ============================================
    // HOME PAGE
    // ============================================
    routeGet("/") {
        if (!current_user.is_authenticated) {
            return redirect("/login");
        }

        auto posts_list = six_sql_query_all("posts");
        
        vars ctx;
        ctx["username"] = current_user.get("name");
        ctx["posts"] = convertToTemplateData(posts_list);
        
        return render_template("index.html", ctx);
    } end();

    // ============================================
    // CREATE POST
    // ============================================
    routePost("/api/posts") {
        require_auth();

        const string title = req.forms.get("title");
        const string content = req.forms.get("content");

        if (title.empty() || content.empty()) {
            vars ctx;
            ctx["error"] = "Please fill all fields";
            return render_template("index.html", ctx);
        }

        six_sql_insert("posts", {
            {"title", title},
            {"content", content},
            {"author", current_user.get("name")},
            {"user_id", to_string(current_user.id)}
        });
        six_sql_commit();

        return redirect("/");
    } end();

    // ============================================
    // EDIT POST
    // ============================================
    routePost("/api/posts/{postId}/edit") {
        auto postId = getParam("postId");
        auto post = six_sql_find_by_readonly("posts", "id", postId);

        if (!post || post["user_id"] != to_string(current_user.id)) {
            vars ctx;
            ctx["error"] = "Cannot edit this post";
            return render_template("index.html", ctx);
        }

        const string title = req.forms.get("title");
        const string content = req.forms.get("content");

        auto editable_post = six_sql_find_by("posts", "id", postId);
        editable_post["title"] = title;
        editable_post["content"] = content;
        editable_post["updated_at"] = timestamp_now();
        six_sql_commit();

        return redirect("/");
    } end();

    // ============================================
    // DELETE POST
    // ============================================
    routePost("/api/posts/{postId}/delete") {
        auto postId = getParam("postId");
        auto post = six_sql_find_by_readonly("posts", "id", postId);

        if (!post || post["user_id"] != to_string(current_user.id)) {
            return redirect("/");
        }

        six_sql_find_by_and_delete("posts", "id", postId);
        return redirect("/");
    } end();

    // ============================================
    // LOGIN PAGE
    // ============================================
    routeGet("/login") {
        if (current_user.is_authenticated) {
            return redirect("/");
        }
        return render_template("login.html");
    } end();

    // ============================================
    // LOGIN HANDLER
    // ============================================
    routePost("/login") {
        const string email = req.forms.get("email");
        const string password = req.forms.get("password");

        auto user = six_sql_find_by("users", "email", email.c_str());

        if (!user) {
            vars ctx;
            ctx["error"] = "Account does not exist";
            return render_template("login.html", ctx);
        }

        if (!verify_password(password, user["hashed_password"])) {
            vars ctx;
            ctx["error"] = "Wrong password";
            return render_template("login.html", ctx);
        }

        login_user(user);
        return redirect("/");
    } end();

    // ============================================
    // REGISTER PAGE
    // ============================================
    routeGet("/register") {
        if (current_user.is_authenticated) {
            return redirect("/");
        }
        return render_template("register.html");
    } end();

    // ============================================
    // REGISTER HANDLER
    // ============================================
    routePost("/register") {
        const string username = req.forms.get("username");
        const string email = req.forms.get("email");
        const string password = req.forms.get("password");

        auto existing_user = six_sql_find_by_readonly("users", "email", email.c_str());
        if (existing_user) {
            vars ctx;
            ctx["error"] = "This email is already registered";
            return render_template("register.html", ctx);
        }

        six_sql_insert("users", {
            {"name", username},
            {"email", email},
            {"hashed_password", generate_password_hash(password)},
            {"is_admin", "0"}
        });
        six_sql_commit();

        return redirect("/login");
    } end();

    // ============================================
    // LOGOUT
    // ============================================
    routeGet("/logout") {
        logout_user();
        return redirect("/login");
    } end();

    // ============================================
    // ADMIN PANEL
    // ============================================
    routeGet("/admin") {
        require_admin();
        return render_template("admin.html");
    } end();

    // ============================================
    // SERVER INITIALIZATION
    // ============================================
    
    create_db_models();
    create_admin_account();
    
    server.start();
    return 0;
}
```

---

## Quick Reference

| Feature | Function | Purpose |
|---------|----------|---------|
| Create Server | `six server(8000)` | Initialize web server on port 8000 |
| GET Route | `routeGet("/path") { } end();` | Handle GET requests |
| POST Route | `routePost("/path") { } end();` | Handle POST requests |
| Login | `login_user(user)` | Create user session |
| Logout | `logout_user()` | Destroy user session |
| Require Auth | `require_auth()` | Protect route - needs login |
| Require Admin | `require_admin()` | Protect route - needs admin |
| Render Template | `render_template("file.html", ctx)` | Display HTML page |
| Redirect | `redirect("/path")` | Redirect to another page |
| Get Param | `getParam("name")` | Extract URL parameter |
| Get Form Data | `req.forms.get("name")` | Get form field value |
| Insert Data | `six_sql_insert("table", {...})` | Add record to database |
| Find Record | `six_sql_find_by("table", "col", "val")` | Get record by value |
| Find All | `six_sql_query_all("table")` | Get all records |
| Delete Record | `six_sql_find_by_and_delete("table", "col", "val")` | Delete record |
| Execute SQL | `six_sql_exec("SQL")` | Run custom SQL |
| Commit Changes | `six_sql_commit()` | Save database changes |
| Hash Password | `generate_password_hash("pass")` | Secure password hash |
| Verify Password | `verify_password("pass", "hash")` | Check password match |
| Send File | `send_from_directory("dir", "file")` | Send file to client |

---

## Best Practices

1. **Always authenticate before sensitive operations** - Use `require_auth()` or check `current_user.is_authenticated`
2. **Validate form input** - Check for empty strings and valid data before processing
3. **Hash passwords** - Always use `generate_password_hash()` when storing passwords
4. **Commit database changes** - Always call `six_sql_commit()` after insert/update/delete
5. **Check authorization** - Verify users can only modify their own data
6. **Use read-only queries** - Use `six_sql_find_by_readonly()` when you don't need to modify data
7. **Handle errors gracefully** - Re-render forms with error messages, don't crash
8. **Redirect after POST** - Redirect after successful form submission to avoid duplicate submissions

---

## Support

For issues, feature requests, or questions about the SIX framework, please refer to the official documentation or submit an issue on GitHub.
