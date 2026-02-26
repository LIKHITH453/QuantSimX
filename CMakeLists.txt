cmake_minimum_required(VERSION 3.16)
project(QuantSimX VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(WARNING "nlohmann_json not found via find_package, will try header-only")
endif()

find_package(websocketpp QUIET)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# ImGui sources
set(IMGUI_DIR external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Main executable
add_executable(quantsimx
    src/main.cpp
    src/OrderBook.cpp
    src/NetworkClient.cpp
    src/CSVWriter.cpp
    src/BinanceRESTClient.cpp
    src/BinanceWebSocketClient.cpp
    src/TradeTracker.cpp
    src/VWAPCalculator.cpp
    src/SignalGenerator.cpp
    src/DashboardUI.cpp
    src/Backtester.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(quantsimx PRIVATE include ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# Link libraries
target_link_libraries(quantsimx PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    OpenGL::GL
    glfw
    glm
)

# If nlohmann_json found via package, link it; otherwise assume header-only in /usr/include
if(nlohmann_json_FOUND)
    target_link_libraries(quantsimx PRIVATE nlohmann_json::nlohmann_json)
endif()

# If websocketpp found, link it; otherwise assume header-only
if(websocketpp_FOUND)
    target_link_libraries(quantsimx PRIVATE websocketpp::websocketpp)
endif()
