add_executable(emi-editor
  main.cpp
  app/app.cpp
  app/leftPanel.cpp
  app/topPanel.cpp
  app/mainEditor.cpp
  app/ui.cpp
  app/dropBar.cpp

  ../dependencies/imgui-node-editor/imgui_node_editor.cpp
  ../dependencies/imgui-node-editor/imgui_node_editor_api.cpp
  ../dependencies/imgui-node-editor/imgui_canvas.cpp
  ../dependencies/imgui-node-editor/crude_json.cpp
)

target_include_directories(emi-editor PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../dependencies/imgui-node-editor
)

target_link_libraries(emi-editor PRIVATE
  ImGui-SFML::ImGui-SFML
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
