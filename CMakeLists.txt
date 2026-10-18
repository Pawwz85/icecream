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

add_executable(icecream ${sources}) 

target_include_directories( icecream PRIVATE "include")

target_compile_options(icecream PRIVATE
  $<$<CONFIG:Release>:$<$<CXX_COMPILER_ID:MSVC>:/O2>>
  $<$<CONFIG:Release>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3>>
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/MTd>
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Debug>>>:/MT>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
endif()

install(TARGETS icecream RUNTIME DESTINATION bin)

include(CPack)