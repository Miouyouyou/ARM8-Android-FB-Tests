cmake_minimum_required(VERSION 3.4.1)

project(test C ASM)

set(CMAKE_SYSTEM_NAME Android)

set(Sources
	android_native_app_glue.c
	fill_pixels.c)
message(WARNING ${ANDROID_ABI})

if (${ANDROID_ABI} STREQUAL "arm64-v8a")
	list(APPEND Sources fill_pixels_func.S)
endif(${ANDROID_ABI} STREQUAL "arm64-v8a")
message(WARNING ${Sources})

# now build app's shared lib
add_library(fbdirect SHARED
	${Sources})

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
	"${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate"
)

find_library(android-lib-log log)
find_library(android-lib-android android)

# add lib dependencies
target_link_libraries(fbdirect
	android
	log
)

target_compile_options(fbdirect PRIVATE
	"$<$<CONFIG:RELEASE>:-O3>"
	"$<$<CONFIG:DEBUG>:-O3>"
)
