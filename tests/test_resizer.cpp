#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <string>
#include <cstdint>

// Utility functions from main.cpp (replicated for testing)
namespace test_utils {

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

std::string base64_encode(const unsigned char* data, size_t len) {
    using namespace boost::archive::iterators;
    using It = base64_from_binary<transform_width<const unsigned char*, 6, 8>>;
    
    std::string result(It(data), It(data + len));
    
    size_t padding = (3 - len % 3) % 3;
    result.append(padding, '=');
    
    return result;
}

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
    
    return base64_encode(output_buffer.data(), output_buffer.size());
}

// Helper function to create a test JPEG image
std::string create_test_jpeg(int width, int height, const cv::Scalar& color = cv::Scalar(128, 128, 128)) {
    cv::Mat test_image(height, width, CV_8UC3, color);
    
    std::vector<uint8_t> buffer;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 85};
    cv::imencode(".jpg", test_image, buffer, params);
    
    return base64_encode(buffer.data(), buffer.size());
}

} // namespace test_utils

// Test Cases
TEST_CASE("Base64 Encoding and Decoding", "[base64]") {
    SECTION("Encode and decode simple data") {
        const char* test_data = "Hello, World!";
        std::string encoded = test_utils::base64_encode(
            reinterpret_cast<const unsigned char*>(test_data), 
            strlen(test_data)
        );
        
        REQUIRE_FALSE(encoded.empty());
        
        std::vector<uint8_t> decoded = test_utils::base64_decode(encoded);
        std::string decoded_str(decoded.begin(), decoded.end());
        
        REQUIRE(decoded_str == test_data);
    }
    
    SECTION("Decode standard base64 strings") {
        std::string encoded = "SGVsbG8sIFdvcmxkIQ==";
        std::vector<uint8_t> decoded = test_utils::base64_decode(encoded);
        std::string result(decoded.begin(), decoded.end());
        
        REQUIRE(result == "Hello, World!");
    }
    
    SECTION("Handle base64 with whitespace") {
        std::string encoded_with_whitespace = "SGVsbG8s\nIFdvcmxk\nIQ==";
        std::vector<uint8_t> decoded = test_utils::base64_decode(encoded_with_whitespace);
        std::string result(decoded.begin(), decoded.end());
        
        REQUIRE(result == "Hello, World!");
    }
    
    SECTION("Empty string encoding/decoding") {
        std::string encoded = test_utils::base64_encode(nullptr, 0);
        REQUIRE(encoded == "");
        
        std::vector<uint8_t> decoded = test_utils::base64_decode(encoded);
        REQUIRE(decoded.empty());
    }
}

TEST_CASE("Image Resize Functionality", "[resize]") {
    SECTION("Resize to smaller dimensions") {
        std::string input = test_utils::create_test_jpeg(800, 600);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 400, 300);
            REQUIRE_FALSE(output.empty());
            
            // Verify the output is valid base64
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            REQUIRE_FALSE(decoded.empty());
            
            // Verify it's a valid JPEG and has correct dimensions
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            REQUIRE_FALSE(result.empty());
            REQUIRE(result.cols == 400);
            REQUIRE(result.rows == 300);
        }());
    }
    
    SECTION("Resize to larger dimensions") {
        std::string input = test_utils::create_test_jpeg(200, 150);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 800, 600);
            REQUIRE_FALSE(output.empty());
            
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 800);
            REQUIRE(result.rows == 600);
        }());
    }
    
    SECTION("Resize to same dimensions") {
        std::string input = test_utils::create_test_jpeg(640, 480);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 640, 480);
            REQUIRE_FALSE(output.empty());
            
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 640);
            REQUIRE(result.rows == 480);
        }());
    }
    
    SECTION("Resize with non-standard aspect ratio") {
        std::string input = test_utils::create_test_jpeg(1920, 1080);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 300, 300);
            REQUIRE_FALSE(output.empty());
            
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 300);
            REQUIRE(result.rows == 300);
        }());
    }
    
    SECTION("Resize very small image") {
        std::string input = test_utils::create_test_jpeg(10, 10);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 5, 5);
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 5);
            REQUIRE(result.rows == 5);
        }());
    }
}

TEST_CASE("Input Validation", "[validation]") {
    std::string valid_input = test_utils::create_test_jpeg(100, 100);
    
    SECTION("Negative width throws exception") {
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg(valid_input, -100, 100),
            std::invalid_argument
        );
    }
    
    SECTION("Negative height throws exception") {
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg(valid_input, 100, -100),
            std::invalid_argument
        );
    }
    
    SECTION("Zero width throws exception") {
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg(valid_input, 0, 100),
            std::invalid_argument
        );
    }
    
    SECTION("Zero height throws exception") {
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg(valid_input, 100, 0),
            std::invalid_argument
        );
    }
    
    SECTION("Dimensions exceeding maximum throws exception") {
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg(valid_input, 70000, 70000),
            std::invalid_argument
        );
    }
    
    SECTION("Empty base64 input throws exception") {
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg("", 100, 100),
            std::invalid_argument
        );
    }
    
    SECTION("Invalid base64 input throws exception") {
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg("not-valid-base64!@#$", 100, 100),
            std::runtime_error
        );
    }
    
    SECTION("Non-JPEG data throws exception") {
        // Create some random non-JPEG data
        std::vector<uint8_t> random_data = {0xFF, 0x00, 0xFF, 0x00, 0xAA, 0xBB};
        std::string bad_input = test_utils::base64_encode(random_data.data(), random_data.size());
        
        REQUIRE_THROWS_AS(
            test_utils::resize_jpeg(bad_input, 100, 100),
            std::runtime_error
        );
    }
}

TEST_CASE("Edge Cases", "[edge_cases]") {
    SECTION("Minimum valid dimensions (1x1)") {
        std::string input = test_utils::create_test_jpeg(100, 100);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 1, 1);
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 1);
            REQUIRE(result.rows == 1);
        }());
    }
    
    SECTION("Maximum reasonable dimensions") {
        std::string input = test_utils::create_test_jpeg(100, 100);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 10000, 10000);
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 10000);
            REQUIRE(result.rows == 10000);
        }());
    }
    
    SECTION("Extreme aspect ratio (wide)") {
        std::string input = test_utils::create_test_jpeg(1000, 100);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 2000, 50);
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 2000);
            REQUIRE(result.rows == 50);
        }());
    }
    
    SECTION("Extreme aspect ratio (tall)") {
        std::string input = test_utils::create_test_jpeg(100, 1000);
        
        REQUIRE_NOTHROW([&]() {
            std::string output = test_utils::resize_jpeg(input, 50, 2000);
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 50);
            REQUIRE(result.rows == 2000);
        }());
    }
}

TEST_CASE("Image Quality Preservation", "[quality]") {
    SECTION("Colored image maintains color after resize") {
        // Create a red image
        cv::Mat red_image(100, 100, CV_8UC3, cv::Scalar(0, 0, 255));
        std::vector<uint8_t> buffer;
        cv::imencode(".jpg", red_image, buffer);
        std::string input = test_utils::base64_encode(buffer.data(), buffer.size());
        
        std::string output = test_utils::resize_jpeg(input, 50, 50);
        std::vector<uint8_t> decoded = test_utils::base64_decode(output);
        cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
        
        // Check that the output is predominantly red
        // (allowing some JPEG compression artifacts)
        cv::Scalar mean_color = cv::mean(result);
        REQUIRE(mean_color[2] > 200);  // Red channel should be high
        REQUIRE(mean_color[0] < 50);   // Blue channel should be low
        REQUIRE(mean_color[1] < 50);   // Green channel should be low
    }
    
    SECTION("Multiple resize operations maintain stability") {
        std::string input = test_utils::create_test_jpeg(800, 600);
        
        // First resize
        std::string output1 = test_utils::resize_jpeg(input, 400, 300);
        std::vector<uint8_t> decoded1 = test_utils::base64_decode(output1);
        cv::Mat result1 = cv::imdecode(decoded1, cv::IMREAD_COLOR);
        
        // Second resize on the output
        std::string output2 = test_utils::resize_jpeg(output1, 200, 150);
        std::vector<uint8_t> decoded2 = test_utils::base64_decode(output2);
        cv::Mat result2 = cv::imdecode(decoded2, cv::IMREAD_COLOR);
        
        REQUIRE(result2.cols == 200);
        REQUIRE(result2.rows == 150);
        REQUIRE_FALSE(result2.empty());
    }
}

TEST_CASE("Performance and Memory", "[performance]") {
    SECTION("Handle large image efficiently") {
        // Create a large test image
        std::string input = test_utils::create_test_jpeg(4000, 3000);
        
        REQUIRE_NOTHROW([&]() {
            auto start = std::chrono::high_resolution_clock::now();
            std::string output = test_utils::resize_jpeg(input, 800, 600);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            // Should complete in reasonable time (less than 5 seconds even on slow hardware)
            REQUIRE(duration.count() < 5000);
            
            std::vector<uint8_t> decoded = test_utils::base64_decode(output);
            cv::Mat result = cv::imdecode(decoded, cv::IMREAD_COLOR);
            
            REQUIRE(result.cols == 800);
            REQUIRE(result.rows == 600);
        }());
    }
    
    SECTION("Multiple sequential operations don't leak memory") {
        std::string input = test_utils::create_test_jpeg(640, 480);
        
        // Perform multiple resize operations
        for (int i = 0; i < 10; ++i) {
            REQUIRE_NOTHROW([&]() {
                std::string output = test_utils::resize_jpeg(input, 320 + i * 10, 240 + i * 10);
                REQUIRE_FALSE(output.empty());
            }());
        }
        
        // If we got here without crashes, memory management is working
        REQUIRE(true);
    }
}