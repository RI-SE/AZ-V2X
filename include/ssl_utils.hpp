#ifndef SSL_UTILS_HPP
#define SSL_UTILS_HPP

#include <proton/ssl.hpp>
#include <string>

using proton::ssl_certificate;

bool using_OpenSSL();
std::string platform_CA(const std::string &base_name);
ssl_certificate platform_certificate(const std::string &base_name, const std::string &passwd);
std::string find_CN(const std::string &subject);
void set_cert_directory(const std::string& dir);
std::string get_cert_directory();

#endif // SSL_UTILS_HPP 