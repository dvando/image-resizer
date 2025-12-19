#include <libasyik/service.hpp>
#include <libasyik/http.hpp>
#include <boost/algorithm/string.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <stdexcept>
#include <cstring>

// Base64 encoding/decoding utilities using Boost
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

namespace {

// Decode base64 string to binary data
std::vector<uint8_t> base64_decode(const std::string& encoded) {

        try {
            using namespace boost::archive::iterators;
            using It = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
            
            // Remove padding characters for proper decoding
            std::string clean = encoded;
            boost::trim(clean);
            clean.erase(std::remove(clean.begin(), clean.end(), '\n'), clean.end());
            clean.erase(std::remove(clean.begin(), clean.end(), '\r'), clean.end());
            
            if (clean.empty()) return {};
            
            size_t padding = 0;
            if (!clean.empty()) {
                if (clean[clean.size() - 1] == '=') padding++;
                if (clean.size() > 1 && clean[clean.size() - 2] == '=') padding++;
            }
            
            std::vector<uint8_t> result(It(clean.begin()), It(clean.end()));
            result.erase(result.end() - padding, result.end());
            
            return result;
        } catch (const std::exception& e) {
            throw std::runtime_error(e.what());
        }
}

// Encode binary data to base64 string
std::string base64_encode(const unsigned char* data, size_t len) {
    using namespace boost::archive::iterators;
    using It = base64_from_binary<transform_width<const unsigned char*, 6, 8>>;
    
    std::string result(It(data), It(data + len));
    
    // Add padding
    size_t padding = (3 - len % 3) % 3;
    result.append(padding, '=');
    
    return result;
}

// Core image resizing function using OpenCV
std::string resize_jpeg(const std::string& input_base64, int target_width, int target_height) {
    if (target_width <= 0 || target_height <= 0) {
        throw std::invalid_argument("Target dimensions must be positive integers");
    }
    
    if (target_width > 65500 || target_height > 65500) {
        throw std::invalid_argument("Target dimensions exceed maximum JPEG size");
    }
    
    std::vector<uint8_t> jpeg_data = base64_decode(input_base64);
    
    if (jpeg_data.empty()) {
        throw std::invalid_argument("Invalid or empty base64 input");
    }
    
    cv::Mat input_image = cv::imdecode(jpeg_data, cv::IMREAD_COLOR);
    
    if (input_image.empty()) {
        throw std::runtime_error("Failed to decode JPEG image - invalid format or corrupted data");
    }
    
    cv::Mat resized_image;
    cv::resize(input_image, resized_image, cv::Size(target_width, target_height), 
               0, 0, cv::INTER_AREA);
    
    std::vector<uint8_t> output_buffer;
    std::vector<int> encode_params = {
        cv::IMWRITE_JPEG_QUALITY, 85,
        cv::IMWRITE_JPEG_OPTIMIZE, 1 
    };
    
    bool encode_success = cv::imencode(".jpg", resized_image, output_buffer, encode_params);
    
    if (!encode_success || output_buffer.empty()) {
        throw std::runtime_error("Failed to encode resized image to JPEG");
    }
    
    // Encode output to base64
    return base64_encode(output_buffer.data(), output_buffer.size());
}

}

using json = nlohmann::json;

int main() {
    try {
        // Create libasyik service - this manages the async I/O
        auto service = asyik::make_service();
        
        // Create HTTP server on port 8080
        auto server = asyik::make_http_server(service, "0.0.0.0", 8080);
        
        // Register the /resize_image endpoint
        server->on_http_request("/resize_image", "POST",[](auto req, auto args) 
        {
                try {
                    std::string raw_body = req->body;
                    auto data = json::parse(raw_body);
                    auto input_jpeg = data["input_jpeg"];
                    auto desired_width = data["desired_width"];
                    auto desired_height = data["desired_height"];
                    
                    // Perform image resizing
                    std::string output_jpeg = resize_jpeg(input_jpeg, desired_width, desired_height);
                    
                    req->response.result(200);
                    req->response.headers.set("content-type", "application/json");
                    req->response.body = "{\"code\": \"200\", \"message\": \"success\", \"output_jpeg\": \"" + output_jpeg + "\"}";
                    
                } catch (const std::invalid_argument& e) {
                    // Client error - invalid input
                    req->response.result(400);
                    req->response.headers.set("content-type", "application/json");
                    req->response.body = "{\"code\": 400, \"message\": \"Invalid input: " + std::string(e.what()) + "\"}";
                    
                } catch (const std::exception& e) {
                    // Server error - processing failed
                    req->response.result(500);
                    req->response.headers.set("content-type", "application/json");
                    req->response.body = "{\"code\": 500, \"message\": \"Internal server error: " + std::string(e.what()) + "\"}";
                }
            });
        
        std::cout << "Server started on http://0.0.0.0:8080" << std::endl;
        std::cout << "Endpoint: POST /resize_image" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        service->run();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}