cmake_minimum_required(VERSION 3.21)
project(LlamaMail LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM QUIET NO_MODULE)
if(ECM_FOUND)
  list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Network Sql Quick Qml QuickControls2 Test)
find_package(KF6Kirigami REQUIRED)

add_subdirectory(core)
add_subdirectory(app)
enable_testing()
add_subdirectory(tests)
