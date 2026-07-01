set(SRC "imgui.cpp" "imgui_demo.cpp" "imgui_draw.cpp" "imgui_stdlib" "imgui_tables.cpp" "imgui_widgets.cpp" 
"backends/imgui_impl_vulkan.cpp"
"backends/imgui_impl_win32.cpp"
)

add_library(IMGUI STATIC ${SRC})

target_link_libraries(IMGUI PRIVATE Vulkan::Vulkan)

target_include_directories(IMGUI PRIVATE ${CMAKE_CURRENT_LIST_DIR})
