message(STATUS "---------- lib define start ----------")

# Define third-party library path
set(THIRD_PARTY_LIBRARY_PATH ${CMAKE_CURRENT_LIST_DIR})
message(STATUS "THIRD_PARTY_LIBRARY_PATH: ${THIRD_PARTY_LIBRARY_PATH}")

# Include directories for third-party libraries
include_directories(
    ${THIRD_PARTY_LIBRARY_PATH}
    ${THIRD_PARTY_LIBRARY_PATH}/public
)

set(LIB_LIST "")

# --- OpenCV ---
set(OpenCV_LIBRARIES_INCLUDE_DIRS ${THIRD_PARTY_LIBRARY_PATH}/public/opencv455)
include_directories(${OpenCV_LIBRARIES_INCLUDE_DIRS})

# --- fmt ---
set(FMT_LIBRARIES_INCLUDE_DIRS ${THIRD_PARTY_LIBRARY_PATH}/public/fmt/include)
include_directories(${FMT_LIBRARIES_INCLUDE_DIRS})
add_definitions(-DFMT_HEADER_ONLY)


# --- plog ---
set(PLOG_LIBRARIES_INCLUDE_DIRS ${THIRD_PARTY_LIBRARY_PATH}/public/plog/include)
include_directories(${PLOG_LIBRARIES_INCLUDE_DIRS})

# --- nlohmann/json ---
set(NLOHMANN_JSON_LIBRARIES_INCLUDE_DIRS ${THIRD_PARTY_LIBRARY_PATH}/public/nlohmann)
include_directories(${NLOHMANN_JSON_LIBRARIES_INCLUDE_DIRS})

# --- boost ---
set(BOOST_LIBRARIES_INCLUDE_DIRS ${THIRD_PARTY_LIBRARY_PATH}/public/boost)
include_directories(${BOOST_LIBRARIES_INCLUDE_DIRS})

# --- libtiff ---
set(LIBTIFF_LIBRARIES_INCLUDE_DIRS ${THIRD_PARTY_LIBRARY_PATH}/public/libtiff/include)
set(LIBTIFF_LIBRARY ${THIRD_PARTY_LIBRARY_PATH}/public/libtiff/lib/tiff.lib)
set(LIBTIFF_DLL ${THIRD_PARTY_LIBRARY_PATH}/public/libtiff/lib/tiff.dll)
include_directories(${LIBTIFF_LIBRARIES_INCLUDE_DIRS})
list(APPEND LIB_LIST ${LIBTIFF_LIBRARY})


message(STATUS "LIB_LIST: ${LIB_LIST}")


message(STATUS "---------- lib define end ----------")
