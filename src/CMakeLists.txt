add_executable(emi-editor
  main.cpp
  app/app.cpp
  app/leftPanel.cpp
  app/topPanel.cpp
  app/mainEditor.cpp
  app/ui.cpp
  app/dropBar.cpp
  app/visualNode.cpp

  ../dependencies/imgui-node-editor/imgui_node_editor.cpp
  ../dependencies/imgui-node-editor/imgui_node_editor_api.cpp
  ../dependencies/imgui-node-editor/imgui_canvas.cpp
  ../dependencies/imgui-node-editor/crude_json.cpp
)

set(_emi_src_dir "")
set(_emi_inc_dir "")

if(DEFINED EMI_SOURCE_DIR AND EXISTS "${EMI_SOURCE_DIR}/src/VM.h")
  set(_emi_src_dir "${EMI_SOURCE_DIR}/src")
  set(_emi_inc_dir "${EMI_SOURCE_DIR}/include")
elseif(EXISTS "${CMAKE_BINARY_DIR}/_deps/emi-src/src/VM.h")
  set(_emi_src_dir "${CMAKE_BINARY_DIR}/_deps/emi-src/src")
  set(_emi_inc_dir "${CMAKE_BINARY_DIR}/_deps/emi-src/include")
else()
  message(FATAL_ERROR
    "EMI-Script not found. Make sure that FetchContent_MakeAvailable(EMI) is called before add_subdirectory(src).")
endif()

target_include_directories(emi-editor PRIVATE
  ${EMI_SOURCE_DIR}/include

  ${CMAKE_CURRENT_SOURCE_DIR}
  ../dependencies/imgui-node-editor
)

target_link_libraries(emi-editor PRIVATE
  ImGui-SFML::ImGui-SFML
  EMI
)

# Copy DLLs needed for runtime on Windows
if(WIN32)
  if (BUILD_SHARED_LIBS)
    add_custom_command(TARGET emi-editor POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:ImGui-SFML::ImGui-SFML>
          $<TARGET_FILE:sfml-graphics>
          $<TARGET_FILE:sfml-window>
          $<TARGET_FILE:sfml-system>
          $<TARGET_FILE_DIR:emi-editor>)
  endif()
endif()