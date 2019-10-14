# Add executable called "helloDemo" that is built from the source files
# "demo.cxx" and "demo_b.cxx". The extensions are automatically found.
include_directories(${PROJECT_SOURCE_DIR}/mptcp)
file(GLOB HEADERS *.h)
add_library (fattree fat_tree_topology.cpp ${HEADERS})

# Link the executable to the Hello library. Since the Hello library has
# public include directories we will use those link directories when building
# helloDemo
target_link_libraries (fattree LINK_PUBLIC mptcp)