# Six Web Framework Guide

A lightweight C++ web framework for building web applications with built-in database support, routing, and template rendering.

## Table of Contents
- [Getting Started](#getting-started)
- [Starting a Server](#starting-a-server)
- [Creating Routes](#creating-routes)
- [Database Setup](#database-setup)
- [Rendering HTML Templates](#rendering-html-templates)
- [Authentication](#authentication)
- [Example Project](#example-project)

## Getting Started

### Installation

Include the six framework header in your project:

```cpp
#include "six/six.h"
```

Make sure to link the six library when compiling your project.

## Starting a Server

### Basic Server Setup

To create and start a web server, initialize a `six` object with your desired port number and call `start()`:

```cpp
int main() {
    six server(8000);  // Create server on port 8000
    
    // Define your routes here...
    
    server.start();    // Start the server
    return 0;
}
```

This will start listening on `http://localhost:8000`.

## Creating Routes

Routes define how your application responds to HTTP requests. The framework supports GET and POST methods.

### Route Syntax

```cpp
routeGet("/path") {
    // Your code here
} end();

routePost("/path") {
    // Your code here
} end();
```

### GET Routes - Display Pages

GET routes are used to display pages and retrieve data:

```cpp
routeGet("/") {
    // Check if user is logged in
    if (!current_user.is_authenticated) {
        return redirect("/login");
    }
    
    // Render the template with context data
    return render_template("index.html", ctx);
} end();

routeGet("/login") {
    if (current_user.is_authenticated) {
        return redirect("/");
    }
    return render_template("login.html");
} end();
```

### POST Routes - Handle Form Submissions

POST routes handle form submissions and data modifications:

```cpp
routePost("/api/posts") {
    // Authentication check
    if (!current_user.is_authenticated) {
        return redirect("/login");
    }
    
    // Get form data from request
    const string title = req.forms.get("title");
    const string content = req.forms.get("content");

    // Validate input
    if (title.empty() || content.empty()) {
        vars ctx;
        ctx["error"] = "Please fill all forms!";
        return render_template("index.html", ctx);
    }

    // Process data (save to database, etc.)
    six_sql_insert("posts", {
        {"title", title},
        {"content", content},
        {"author", current_user.get("name")},
        {"user_id", to_string(current_user.id)}
    });
    six_sql_commit();
    
    // Redirect after success
    return redirect("/");
} end();
```

### Dynamic Routes with Parameters

Routes can include dynamic parameters in curly braces for URL segments:

```cpp
routePost("/api/posts/{editPostId}/edit") {
    // Extract the parameter from the URL
    auto editPostId = getParam("editPostId");
    
    // Fetch the post from database
    auto post = six_sql_find_by_readonly("posts", "id", editPostId);
    
    if (!post) {
        vars ctx;
        ctx["error"] = "This post does not exist";
        return render_template("index.html", ctx);
    }

    // Check authorization - user can only edit their own posts
    if (post["user_id"] != to_string(current_user.id)) {
        vars ctx;
        ctx["error"] = "You can only edit your own posts";
        return render_template("index.html", ctx);
    }

    // Update the post
    const string title = req.forms.get("title");
    const string content = req.forms.get("content");
    
    auto editablePost = six_sql_find_by("posts", "id", editPostId);
    if (editablePost) {
        editablePost["title"] = title;
        editablePost["content"] = content;
        editablePost["updated_at"] = timestamp_now();
        six_sql_commit();
    }

    return redirect("/");
} end();

routePost("/api/posts/{deletePostId}/delete") {
    auto deletePostId = getParam("deletePostId");
    auto post = six_sql_find_by_readonly("posts", "id", deletePostId);

    if (!post) {
        vars ctx;
        ctx["error"] = "This post does not exist";
        return render_template("index.html", ctx);
    }

    if (post["user_id"] != to_string(current_user.id)) {
        vars ctx;
        ctx["error"] = "You can only delete your own posts";
        return render_template("index.html", ctx);
    }

    six_sql_find_by_and_delete("posts", "id", deletePostId);
    return redirect("/");
} end();
```

## Rendering Templates & Common Commands

### The `render_template()` Command

Render an HTML template with context data passed to it:

```cpp
vars ctx;  // Create a context object to pass data
ctx["key"] = "value";
return render_template("filename.html", ctx);
```

### The `redirect()` Command

Redirect the user to another page. Commonly used after form submission or for authentication checks:

```cpp
return redirect("/path");
```

### Showcase: Blog Application Template Commands

Here are practical examples of template rendering and redirection from the blog app:

**Display Home Page with Posts:**
```cpp
routeGet("/") {
    if (!current_user.is_authenticated) {
        return redirect("/login");  // Redirect if not logged in
    }

    // Fetch all posts from database
    auto posts_list = six_sql_query_all("posts");
    
    // Create context with user and posts data
    vars ctx;
    ctx["username"] = current_user.get("name");
    ctx["email"] = current_user.get("email");
    ctx["is_authenticated"] = current_user.is_authenticated ? "true" : "false";
    ctx["user_id"] = current_user.get("id");
    
    if (!posts_list.empty()) {
        auto template_posts = convertToTemplateData(posts_list);
        ctx["posts"] = template_posts;
    }
    
    // Render the template with all context data
    return render_template("index.html", ctx);
} end();
```

**Login Form Handling with Validation:**
```cpp
routePost("/login") {
    // Get form input
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");

    // Check if user exists
    auto existing_user = six_sql_find_by("users", "email", email.c_str());
    if (!existing_user) {
        vars ctx;
        ctx["error"] = "Account does not exist";
        return render_template("login.html", ctx);  // Re-render with error
    }

    // Verify password
    if (!verify_password(password, existing_user["hashed_password"])) {
        vars ctx;
        ctx["error"] = "Wrong password";
        return render_template("login.html", ctx);  // Re-render with error
    }

    // Login successful
    login_user(existing_user);
    return redirect("/");  // Redirect to home page
} end();
```

**User Registration with Multiple Validations:**
```cpp
routePost("/register") {
    const string username = req.forms.get("username");
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");

    // Check if email already exists
    auto existing_email_user = six_sql_find_by("users", "email", email.c_str());
    if (existing_email_user) {
        vars ctx;
        ctx["error"] = "This Email is already Registered";
        return render_template("register.html", ctx);  // Show error
    }
    
    // Check if username already exists
    auto existing_username_user = six_sql_find_by("users", "name", username.c_str());
    if (existing_username_user) {
        vars ctx;
        ctx["error"] = "This Username is already Registered";
        return render_template("register.html", ctx);  // Show error
    }

    // Create new user
    six_sql_insert("users", {
        {"name", username},
        {"email", email},
        {"hashed_password", generate_password_hash(password)},
        {"is_admin", "0"}
    });
    six_sql_commit();

    // Redirect to login page
    return redirect("/login");
} end();
```

**Logout:**
```cpp
routeGet("/logout") {
    logout_user();
    return redirect("/login");  // Redirect to login page
} end();
```

## Database Setup

### Creating Database Tables

Define your database schema using SQL:

```cpp
void create_db_models() {
    six_sql_exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "email TEXT UNIQUE NOT NULL, "
        "hashed_password TEXT NOT NULL, "
        "is_admin INTEGER DEFAULT 0 "
        ")"
    );

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
```

Call this function during server initialization:

```cpp
int main() {
    six server(8000);
    
    // ... route definitions ...
    
    create_db_models();  // Initialize database tables
    create_admin_account();
    
    server.start();
    return 0;
}
```

### Database Operations

**Insert Data:**
```cpp
six_sql_insert("users", {
    {"name", "John Doe"},
    {"email", "john@example.com"},
    {"hashed_password", generate_password_hash("password123")},
    {"is_admin", "0"}
});
six_sql_commit();
```

**Find Data:**
```cpp
auto user = six_sql_find_by("users", "email", "john@example.com");
if (user) {
    cout << "Found user: " << user["name"] << endl;
}
```

**Find All Records:**
```cpp
auto posts = six_sql_query_all("posts");
for (auto post : posts) {
    cout << post["title"] << endl;
}
```

**Update Data:**
```cpp
auto user = six_sql_find_by("users", "id", userId);
if (user) {
    user["email"] = "newemail@example.com";
    six_sql_commit();
}
```

**Delete Data:**
```cpp
six_sql_find_by_and_delete("posts", "id", postId);
```

**Read-Only Queries:**
```cpp
auto post = six_sql_find_by_readonly("posts", "id", "123");
if (post) {
    cout << post["title"] << endl;
}
```

## Rendering HTML Templates

### Template Rendering Basics

Pass data from C++ to your HTML templates using the `vars ctx` object:

```cpp
vars ctx;
ctx["username"] = current_user.get("name");
ctx["posts"] = posts_list;
ctx["error"] = "Some error message";

return render_template("index.html", ctx);
```

### Passing Data to Templates

**Single Values:**
```cpp
ctx["title"] = "My Web App";
ctx["user_id"] = current_user.get("id");
ctx["is_authenticated"] = "true";
```

**Lists and Complex Data:**
```cpp
auto posts_list = six_sql_query_all("posts");
auto template_posts = convertToTemplateData(posts_list);
ctx["posts"] = template_posts;
```

### Template Variables in HTML

In your HTML files, access context variables using the framework's template syntax:

```html
<!-- Display a simple value -->
<h1>Welcome, {{ username }}!</h1>

<!-- Display email -->
<p>Email: {{ email }}</p>

<!-- Conditional rendering -->
{% if is_authenticated %}
    <a href="/logout">Logout</a>
{% else %}
    <a href="/login">Login</a>
{% endif %}

<!-- Loop through posts -->
{% for post in posts %}
    <div class="post">
        <h2>{{ post.title }}</h2>
        <p>{{ post.content }}</p>
        <p>By: {{ post.author }}</p>
    </div>
{% endfor %}

<!-- Display error messages -->
{% if error %}
    <div class="alert alert-danger">{{ error }}</div>
{% endif %}
```

## Authentication

### User Authentication

The framework provides built-in authentication support through the `current_user` object:

```cpp
if (!current_user.is_authenticated) {
    return redirect("/login");
}
```

**Access Current User Data:**
```cpp
string username = current_user.get("name");
string email = current_user.get("email");
int user_id = current_user.id;
```

### Login Handler

```cpp
routePost("/login") {
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");

    auto existing_user = six_sql_find_by("users", "email", email.c_str());
    if (!existing_user) {
        vars ctx;
        ctx["error"] = "Account does not exist";
        return render_template("login.html", ctx);
    }

    if (!verify_password(password, existing_user["hashed_password"])) {
        vars ctx;
        ctx["error"] = "Wrong password";
        return render_template("login.html", ctx);
    }

    login_user(existing_user);
    return redirect("/");
} end();
```

### User Registration

```cpp
routePost("/register") {
    const string username = req.forms.get("username");
    const string email = req.forms.get("email");
    const string password = req.forms.get("password");

    auto existing_user = six_sql_find_by("users", "email", email.c_str());
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
```

### Logout

```cpp
routeGet("/logout") {
    logout_user();
    return redirect("/login");
} end();
```

## Complete Blog Application Showcase

Here's a complete, fully-annotated blog application demonstrating routes, template rendering, and redirects:

```cpp
#include "six/six.h"

// ============================================
// DATABASE INITIALIZATION
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
    auto existing_user = six_sql_find_by("users", "email", "dev@lorentzos.com");
    
    if (existing_user) {
        cout << "Admin account already exists." << endl;
        return;
    }
    
    six_sql_insert("users", {
        {"name", "Lorentzos"},
        {"email", "dev@lorentzos.com"},
        {"hashed_password", generate_password_hash("lorentzos")},
        {"is_admin", "1"}
    });
    
    six_sql_commit();
    cout << "Admin account created." << endl;
}

// ============================================
// MAIN APPLICATION
// ============================================

int main() {
    six server(8000);  // Create server on port 8000

    // ============================================
    // HOME PAGE - Display all posts
    // ============================================
    routeGet("/") {
        // Check authentication - redirect if not logged in
        if (!current_user.is_authenticated) {
            return redirect("/login");
        }

        // Fetch all posts from database
        auto posts_list = six_sql_query_all("posts");
        
        // Create context object to pass data to template
        vars ctx;
        ctx["username"] = current_user.get("name");
        ctx["email"] = current_user.get("email");
        ctx["is_authenticated"] = current_user.is_authenticated ? "true" : "false";
        ctx["user_id"] = current_user.get("id");
        
        // Pass posts to template
        if (!posts_list.empty()) {
            auto template_posts = convertToTemplateData(posts_list);
            ctx["posts"] = template_posts;
        }
        
        // Render the home page template with context
        return render_template("index.html", ctx);
    } end();

    // ============================================
    // CREATE POST - Handle new post submission
    // ============================================
    routePost("/api/posts") {
        // Protect route - require authentication
        if (!current_user.is_authenticated) {
            return redirect("/login");
        }

        // Get form data from POST request
        const string title = req.forms.get("title");
        const string content = req.forms.get("content");

        // Validate form input
        if (title.empty() || content.empty()) {
            vars ctx;
            ctx["error"] = "Please fill all fields!";
            // Re-render page with error message
            return render_template("index.html", ctx);
        }

        // Save post to database
        six_sql_insert("posts", {
            {"title", title},
            {"content", content},
            {"author", current_user.get("name")},
            {"user_id", to_string(current_user.id)},
            {"created_at", timestamp_now()},
            {"updated_at", timestamp_now()}
        });
        six_sql_commit();

        // Redirect back to home page after success
        return redirect("/");
    } end();

    // ============================================
    // EDIT POST - Handle post updates
    // ============================================
    routePost("/api/posts/{editPostId}/edit") {
        // Extract post ID from URL parameter
        auto editPostId = getParam("editPostId");
        
        // Fetch post (read-only to check ownership)
        auto post = six_sql_find_by_readonly("posts", "id", editPostId);

        // Check if post exists
        if (!post) {
            vars ctx;
            ctx["error"] = "This post does not exist";
            return render_template("index.html", ctx);
        }

        // Authorization check - user can only edit their own posts
        if (post["user_id"] != to_string(current_user.id)) {
            vars ctx;
            ctx["error"] = "You can only edit your own posts";
            return render_template("index.html", ctx);
        }

        // Get updated form data
        const string title = req.forms.get("title");
        const string content = req.forms.get("content");

        // Fetch post for editing (writable)
        auto editablePost = six_sql_find_by("posts", "id", editPostId);

        if (editablePost) {
            // Update post fields
            editablePost["title"] = title;
            editablePost["content"] = content;
            editablePost["updated_at"] = timestamp_now();
            
            // Commit changes to database
            six_sql_commit();
        }

        // Redirect back to home page
        return redirect("/");
    } end();

    // ============================================
    // DELETE POST - Handle post deletion
    // ============================================
    routePost("/api/posts/{deletePostId}/delete") {
        // Extract post ID from URL parameter
        string deletePostId = getParam("deletePostId");
        auto post = six_sql_find_by_readonly("posts", "id", deletePostId);

        // Check if post exists
        if (!post) {
            vars ctx;
            ctx["error"] = "This post does not exist";
            return render_template("index.html", ctx);
        }

        // Authorization check
        if (post["user_id"] != to_string(current_user.id)) {
            vars ctx;
            ctx["error"] = "You can only delete your own posts";
            return render_template("index.html", ctx);
        }

        // Delete the post
        six_sql_find_by_and_delete("posts", "id", deletePostId);
        
        // Redirect back to home page
        return redirect("/");
    } end();

    // ============================================
    // LOGIN PAGE - Display login form
    // ============================================
    routeGet("/login") {
        // Redirect if already logged in
        if (current_user.is_authenticated) {
            return redirect("/");
        }
        // Render login template (no context needed)
        return render_template("login.html");
    } end();

    // ============================================
    // LOGIN HANDLER - Process login submission
    // ============================================
    routePost("/login") {
        // Get form credentials
        const string email = req.forms.get("email");
        const string password = req.forms.get("password");

        // Find user in database by email
        auto existing_user = six_sql_find_by("users", "email", email.c_str());
        
        // Check if user exists
        if (!existing_user) {
            vars ctx;
            ctx["error"] = "Account does not exist";
            // Re-render login page with error
            return render_template("login.html", ctx);
        }

        // Verify password
        if (!verify_password(password, existing_user["hashed_password"])) {
            vars ctx;
            ctx["error"] = "Wrong password";
            // Re-render login page with error
            return render_template("login.html", ctx);
        }

        // Login successful - create user session
        login_user(existing_user);
        
        // Redirect to home page
        return redirect("/");
    } end();

    // ============================================
    // REGISTER PAGE - Display registration form
    // ============================================
    routeGet("/register") {
        // Redirect if already logged in
        if (current_user.is_authenticated) {
            return redirect("/");
        }
        return render_template("register.html");
    } end();

    // ============================================
    // REGISTRATION HANDLER - Process signup
    // ============================================
    routePost("/register") {
        // Get form input
        const string username = req.forms.get("username");
        const string email = req.forms.get("email");
        const string password = req.forms.get("password");

        // Check if email already registered
        auto existing_email_user = six_sql_find_by("users", "email", email.c_str());
        if (existing_email_user) {
            vars ctx;
            ctx["error"] = "This Email is already Registered";
            return render_template("register.html", ctx);
        }
        
        // Check if username already taken
        auto existing_username_user = six_sql_find_by("users", "name", username.c_str());
        if (existing_username_user) {
            vars ctx;
            ctx["error"] = "This Username is already Registered";
            return render_template("register.html", ctx);
        }

        // Create new user account
        six_sql_insert("users", {
            {"name", username},
            {"email", email},
            {"hashed_password", generate_password_hash(password)},
            {"is_admin", "0"}
        });

        six_sql_commit();

        // Redirect to login page
        return redirect("/login");
    } end();

    // ============================================
    // LOGOUT - End user session
    // ============================================
    routeGet("/logout") {
        logout_user();
        // Redirect to login page
        return redirect("/login");
    } end();

    // ============================================
    // SERVER STARTUP
    // ============================================
    
    // Initialize database tables
    create_db_models();
    
    // Create admin account
    create_admin_account();
    
    // Start the server
    server.start();
    return 0;
}
```

### Key Takeaways from the Blog App

1. **Routes** - Use `routeGet()` and `routePost()` to define endpoints
2. **Redirects** - Use `return redirect("/path")` to navigate users after actions
3. **Templates** - Use `vars ctx` to prepare data and `render_template()` to display it
4. **Dynamic Routes** - Use `{paramName}` in the route and `getParam()` to extract values
5. **Validation** - Always validate input and show errors by re-rendering with context
6. **Authorization** - Check ownership before allowing edits/deletes (like checking `post["user_id"]`)
7. **Database** - Fetch with `six_sql_find_by()`, insert with `six_sql_insert()`, and commit with `six_sql_commit()`

## Best Practices

1. **Always authenticate before sensitive operations** - Check `current_user.is_authenticated` before allowing data modifications
2. **Validate user input** - Check that form data is not empty and meets requirements
3. **Use read-only queries when appropriate** - Use `six_sql_find_by_readonly()` when you don't need to modify data
4. **Commit changes** - Call `six_sql_commit()` after database modifications
5. **Handle authorization** - Verify that users can only modify their own data
6. **Use parameterized routing** - Take advantage of dynamic route parameters instead of query strings
