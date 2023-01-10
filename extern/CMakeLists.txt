set(EXTERN_DIR "${LIAM_DIR}/extern")

# VulkanSDK
set(VULKANSDK_DIR "${EXTERN_DIR}/VulkanSDK-1.3.236.0")

set(VULKAN_INCLUDE "${VULKANSDK_DIR}/include" CACHE STRING "")
if(APPLE)
  set(VULKAN_LIB "${VULKANSDK_DIR}/APPLE/lib/libvulkan.1.dylib" CACHE STRING "")
  set(VK_LAYER_PATH "${VULKANSDK_DIR}/APPLE/layer" CACHE STRING "")
  set(VK_ICD_FILENAMES "${VULKANSDK_DIR}/APPLE/icd.d/MoltenVK_icd.json" CACHE STRING "")
  set(GLS "${VULKANSDK_DIR}/APPLE/bin/glslangValidator" CACHE STRING "")
elseif(WIN32)
  set(VULKAN_LIB "${VULKANSDK_DIR}/Win32/lib/vulkan-1.lib" CACHE STRING "")
  set(VK_LAYER_PATH "${VULKANSDK_DIR}/Win32/layer" CACHE STRING "")
  set(GLS "${VULKANSDK_DIR}/Win32/bin/glslangValidator.exe" CACHE STRING "")
else()
  message(FATAL_ERRPR "Unknown Platform")
endif()

# glm
set(GLM_DIR "${EXTERN_DIR}/glm-0.9.9.8")
set(GLM_INCLUDE "${GLM_DIR}" CACHE STRING "")

# glfw
set(GLFW_DIR "${EXTERN_DIR}/glfw-3.3.8")
set(GLFW_INCLUDE "${GLFW_DIR}/include" CACHE STRING "")
if(NOT TARGET glfw)
  option(GLFW_BUILD_EXAMPLES "" OFF)
  option(GLFW_BUILD_TESTS "" OFF)
  option(GLFW_BUILD_DOCS "" OFF)
  option(GLFW_INSTALL "" OFF)
  add_subdirectory(glfw-3.3.8)
  set_target_properties(glfw PROPERTIES FOLDER ${GLFW_DIR})
  set_target_properties(update_mappings PROPERTIES FOLDER ${GLFW_DIR})
endif()

# stb_image
set(STB_IMAGE_DIR "${EXTERN_DIR}/stb_image-2.27x")
set(STB_IMAGE_INCLUDE "${STB_IMAGE_DIR}" CACHE STRING "")

# tiny_obj_loader
set(TINY_OBJ_LOADER_DIR "${EXTERN_DIR}/tiny_obj_loader-1.0.6")
set(TINY_OBJ_LOADER_INCLUDE "${TINY_OBJ_LOADER_DIR}" CACHE STRING "")