# CAN/main/CMakeLists.txt
file(GLOB_RECURSE main_srcs "main.cpp")
file(GLOB_RECURSE lib_srcs "src/*.c" "src/*.cpp") # Agora inclui .cpp de src/

set(COMPONENT_SRCS ${main_srcs} ${lib_srcs})

idf_component_register(SRCS ${COMPONENT_SRCS}
                       INCLUDE_DIRS "./inc" "src") # Adicione "src" se os headers da lib estiverem lá