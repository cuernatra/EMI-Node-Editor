cmake_minimum_required(VERSION 3.15)

add_library(imgui_node_editor STATIC)

target_sources(imgui_node_editor PRIVATE
    ../imgui-node-editor/imgui_node_editor.cpp
)

# crude_json.cpp jos löytyy
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../imgui-node-editor/crude_json.cpp")
    target_sources(imgui_node_editor PRIVATE
        ../imgui-node-editor/crude_json.cpp
    )
endif()

target_include_directories(imgui_node_editor PUBLIC
    ../imgui-node-editor
)

target_link_libraries(imgui_node_editor PUBLIC ImGui-SFML::ImGui-SFML)
