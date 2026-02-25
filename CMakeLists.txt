cmake_minimum_required(VERSION 3.16)
project(QuantSimX VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Executable
add_executable(quantsimx
    src/main.cpp
    src/OrderBook.cpp
)

target_include_directories(quantsimx PRIVATE include)

# NOTE: Add and link dependencies (websocketpp/boost/beast, nlohmann_json, spdlog, imgui)
# when implementing ingestion and visualization.
