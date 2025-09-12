cmake_minimum_required(VERSION 3.16)

project(mac_table_library LANGUAGES C)

# =========================
# 1. Library Target
# =========================
# List only src files explicitly
add_library(mac_table STATIC
    src/mac_table.c
    src/mac_table_expiry_manager.c
    src/main.c            # optional if main.c is part of library
)

# Include directories for users of the library
target_include_directories(mac_table PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>  # for future install
)

# Optional: set C standard
target_compile_features(mac_table PUBLIC c_std_11)

# Install library and headers for external use
install(TARGETS mac_table
        EXPORT mac_tableTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)

install(DIRECTORY src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
)
