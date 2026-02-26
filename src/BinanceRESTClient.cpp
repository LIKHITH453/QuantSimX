#include "BinanceRESTClient.h"
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #define closesocket close
#endif

const std::string BinanceRESTClient::BASE_URL = "api.binance.com";

json BinanceRESTClient::fetch_depth_snapshot(const std::string& symbol, int limit) {
    json result;
    try {
        // Build HTTP request
        std::ostringstream path;
        path << "/api/v3/depth?symbol=" << symbol << "&limit=" << limit;
        std::string path_str = path.str();
        
        // Simple HTTP GET request
        std::string request = "GET " + path_str + " HTTP/1.1\r\n";
        request += "Host: " + BASE_URL + "\r\n";
        request += "User-Agent: QuantSimX/1.0\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";
        
        // Create socket and connect
        struct hostent* host = gethostbyname(BASE_URL.c_str());
        if (!host) {
            std::cerr << "[REST] Failed to resolve hostname\n";
            return result;
        }
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "[REST] Failed to create socket\n";
            return result;
        }
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        addr.sin_addr = *(struct in_addr*)host->h_addr;
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "[REST] Failed to connect\n";
            closesocket(sock);
            return result;
        }
        
        // Send request
        if (send(sock, request.c_str(), request.length(), 0) < 0) {
            std::cerr << "[REST] Failed to send request\n";
            closesocket(sock);
            return result;
        }
        
        // Receive response
        std::string response;
        char buffer[4096];
        int n;
        while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[n] = '\0';
            response += buffer;
        }
        closesocket(sock);
        
        // Parse HTTP response: skip headers, extract JSON body
        size_t body_start = response.find("\r\n\r\n");
        if (body_start == std::string::npos) {
            body_start = response.find("\n\n");
            if (body_start == std::string::npos) {
                // Try to find any JSON-like content (starts with { or [)
                size_t json_start = response.find_first_of("{[");
                if (json_start == std::string::npos) {
                    std::cerr << "[REST] No JSON found in response\n";
                    return result;
                }
                body_start = json_start;
            } else {
                body_start += 2;
            }
        } else {
            body_start += 4;
        }
        
        std::string body = response.substr(body_start);
        // Remove any trailing non-JSON content
        size_t last_brace = body.rfind('}');
        if (last_brace != std::string::npos) {
            body = body.substr(0, last_brace + 1);
        }
        result = json::parse(body);
        
        std::cout << "[REST] Successfully fetched depth snapshot for " << symbol << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "[REST] fetch_depth_snapshot error: " << e.what() << "\n";
    }
    
    return result;
}
