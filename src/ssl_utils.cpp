#include "ssl_utils.hpp"
#include <stdexcept>
#include <filesystem>

static std::string cert_directory;


void set_cert_directory(const std::string& dir) {
    cert_directory = dir;
    if (!cert_directory.empty() && cert_directory.back() != '/') {
        cert_directory += '/';
    }
}

std::string get_cert_directory() {
    return cert_directory;
}

ssl_certificate platform_certificate(const std::string &base_name, const std::string &passwd) {
    std::string cert_path, key_path;
    cert_path = cert_directory + base_name + ".crt";
    key_path = cert_directory + base_name + ".key";

    // Check if the certificate file exists
    if (!std::filesystem::exists(cert_path)) {
        throw std::runtime_error("Certificate file not found: " + cert_path);
    }

    // Check if the key file exists (only for OpenSSL)
    if (!std::filesystem::exists(key_path)) {
        throw std::runtime_error("Key file not found: " + key_path);
    }

    return ssl_certificate(cert_path, key_path, passwd);
}

std::string platform_CA(const std::string &base_name) {
    return cert_directory + base_name + ".pem";
}

std::string find_CN(const std::string &subject) {
    size_t pos = subject.find("CN=");
    if (pos == std::string::npos) throw std::runtime_error("No common name in certificate subject");
    std::string cn = subject.substr(pos);
    pos = cn.find(',');
    return pos == std::string::npos ? cn : cn.substr(0, pos);
} 