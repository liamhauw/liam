set(TARGET_NAME main)

add_executable(${TARGET_NAME} main.cpp)

add_dependencies(${TARGET_NAME} ${SPV_SHADERS})

target_compile_definitions(${TARGET_NAME} PRIVATE "LIAM_VK_LAYER_PATH=${VK_LAYER_PATH}")
if(APPLE) 
target_compile_definitions(${TARGET_NAME} PRIVATE "LIAM_VK_ICD_FILENAMES=${VK_ICD_FILENAMES}")
endif()

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
set_target_properties(${TARGET_NAME} PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(${TARGET_NAME} PRIVATE ${VULKAN_INCLUDE} ${GLFW_INCLUDE} ${GLM_INCLUDE} ${STB_IMAGE_INCLUDE} ${TINY_OBJ_LOADER_INCLUDE})
target_link_libraries(${TARGET_NAME} PRIVATE glfw ${VULKAN_LIB})