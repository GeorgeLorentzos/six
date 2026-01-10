#ifndef six_crypt_h
#define six_crypt_h

#include <string>
#include <sstream>
#include <random>
#include <iomanip>
#include <cstdint>
#include <stdexcept>
#include <argon2.h>
#include <cstring>

using namespace std;

namespace Argon2Config {
    constexpr uint32_t MEMORY_SIZE = 65540;
    constexpr uint32_t TIME_COST = 3;
    constexpr uint32_t PARALLELISM = 4;
    constexpr uint32_t SALT_LENGTH = 16;
    constexpr uint32_t HASH_LENGTH = 32;
    constexpr uint32_t TAG_LENGTH = 32;
}

inline string generate_salt(size_t length = Argon2Config::SALT_LENGTH) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    random_device rd("/dev/urandom");
    mt19937_64 gen(rd());
    uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    
    string salt;
    salt.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        salt += charset[dist(gen)];
    }
    return salt;
}

inline string bytes_to_hex(const unsigned char* bytes, size_t length) {
    stringstream ss;
    ss << hex << setfill('0');
    for (size_t i = 0; i < length; ++i) {
        ss << setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

inline bool hex_to_bytes(const string& hex, unsigned char* bytes, size_t max_length) {
    if (hex.length() % 2 != 0 || hex.length() / 2 > max_length) {
        return false;
    }
    for (size_t i = 0; i < hex.length(); i += 2) {
        string byte_string = hex.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(strtol(byte_string.c_str(), nullptr, 16));
        bytes[i / 2] = byte;
    }
    return true;
}

inline string generate_password_hash(const string& password) {
    if (password.empty()) {
        throw runtime_error("Password cannot be empty");
    }
    if (password.length() > 1024) {
        throw runtime_error("Password too long (max 1024 chars)");
    }

    string salt_str = generate_salt(Argon2Config::SALT_LENGTH);
    
    unsigned char salt[Argon2Config::SALT_LENGTH];
    memcpy(salt, salt_str.c_str(), Argon2Config::SALT_LENGTH);
    
    unsigned char hash[Argon2Config::TAG_LENGTH];
    
    int result = argon2id_hash_raw(
        Argon2Config::TIME_COST,
        Argon2Config::MEMORY_SIZE,
        Argon2Config::PARALLELISM,
        password.c_str(),
        password.length(),
        salt,
        Argon2Config::SALT_LENGTH,
        hash,
        Argon2Config::TAG_LENGTH
    );
    
    if (result != ARGON2_OK) {
        throw runtime_error(string("Argon2 error: ") + argon2_error_message(result));
    }
    
    stringstream ss;
    ss << "$argon2id$v=19$"
       << "m=" << Argon2Config::MEMORY_SIZE << ","
       << "t=" << Argon2Config::TIME_COST << ","
       << "p=" << Argon2Config::PARALLELISM << "$"
       << salt_str << "$"
       << bytes_to_hex(hash, Argon2Config::TAG_LENGTH);
    
    return ss.str();
}

inline bool verify_password(const string& password, const string& stored_hash) {
    if (password.empty() || stored_hash.empty()) {
        return false;
    }
    if (password.length() > 1024) {
        return false;
    }
    
    size_t first_dollar = stored_hash.find('$', 1);
    size_t second_dollar = stored_hash.find('$', first_dollar + 1);
    size_t third_dollar = stored_hash.find('$', second_dollar + 1);
    size_t fourth_dollar = stored_hash.find('$', third_dollar + 1);
    
    if (first_dollar == string::npos || second_dollar == string::npos || 
        third_dollar == string::npos || fourth_dollar == string::npos) {
        return false;
    }
    
    string salt_str = stored_hash.substr(third_dollar + 1, fourth_dollar - third_dollar - 1);
    string stored_hash_hex = stored_hash.substr(fourth_dollar + 1);
    
    if (salt_str.length() != Argon2Config::SALT_LENGTH) {
        return false;
    }
    
    unsigned char stored_hash_bytes[Argon2Config::TAG_LENGTH];
    if (!hex_to_bytes(stored_hash_hex, stored_hash_bytes, Argon2Config::TAG_LENGTH)) {
        return false;
    }
    
    unsigned char salt[Argon2Config::SALT_LENGTH];
    memcpy(salt, salt_str.c_str(), Argon2Config::SALT_LENGTH);
    
    unsigned char computed_hash[Argon2Config::TAG_LENGTH];
    
    int result = argon2id_hash_raw(
        Argon2Config::TIME_COST,
        Argon2Config::MEMORY_SIZE,
        Argon2Config::PARALLELISM,
        password.c_str(),
        password.length(),
        salt,
        Argon2Config::SALT_LENGTH,
        computed_hash,
        Argon2Config::TAG_LENGTH
    );
    
    if (result != ARGON2_OK) {
        return false;
    }
    
    unsigned char diff = 0;
    for (size_t i = 0; i < Argon2Config::TAG_LENGTH; ++i) {
        diff |= (computed_hash[i] ^ stored_hash_bytes[i]);
    }
    
    return diff == 0;
}

#endif