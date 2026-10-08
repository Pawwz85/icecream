cmake_minimum_required(VERSION 3.10)
project(IceCream 
    LANGUAGES CXX
    VERSION 1.0
    DESCRIPTION "A spell chess engine written in C++")


set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (sources 
    "src/GameState.cpp"
    "src/main.cpp"
    "src/MoveGen.cpp"
    "src/Perft.cpp"
    "src/search.cpp"
    "src/TranspositionTable.cpp"
    "src/uci_client.cpp"
    "src/Zobrist.cpp"
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG")

add_executable(icecream ${sources}) 

target_include_directories( icecream PRIVATE "include")