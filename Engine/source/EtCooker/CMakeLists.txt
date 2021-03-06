

##################
# Resource Cooker
##################


# files
###########
file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE headers ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

list (APPEND projectFiles ${sources} ${headers})

# setup
#########
target_definitions()

add_definitions(-DUNCOOKED_RESOURCES) 

add_definitions(-D_CONSOLE)
# executable and dependancies
message(STATUS "Adding target: EtCooker")
add_executable(EtCooker ${projectFiles})
targetCompileOptions(EtCooker)

# directory stuff
assign_source_group(${projectFiles})
assignIdeFolder(EtCooker Engine)
outputDirectories(EtCooker "")

# linking
target_link_libraries (EtCooker EtFramework EtBuild)
dependancyLinks(EtCooker)

# library includes
libIncludeDirs()

# general include dirs
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories (EtCooker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

copyDllCommand(EtCooker)