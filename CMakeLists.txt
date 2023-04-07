
PROJECT(test_trace)
# SET(CMAKE_CXX_FLAGS  "-Wall -O3 -pthread -std=c++14")
SET(CMAKE_CXX_FLAGS  " -Wl,--rpath=./lib/ -m64 -Wall -O3 -pthread -std=c++14 -fPIC -g")

SET(SRC_LIST test_trace.cpp)

INCLUDE_DIRECTORIES(cache)
INCLUDE_DIRECTORIES(ssdlogging)

ADD_EXECUTABLE(test_trace ${SRC_LIST})
target_link_libraries(test_trace tbb)

# test!
INCLUDE_DIRECTORIES(./CLHT/include)
INCLUDE_DIRECTORIES(./CLHT/external/include)
find_library(LIBCLHT clht PATHS ./CLHT/)
message("${LIBCLHT}")
find_library(LIBSSMEM ssmem PATHS ./CLHT/external/ssmem)
message("${LIBSSMEM}")
ADD_EXECUTABLE(test_trace ${SRC_LIST})
target_link_libraries(test_trace tbb ${LIBCLHT} ${LIBSSMEM})


