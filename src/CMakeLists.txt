add_executable(emi-editor
  main.cpp
  app/app.cpp
  app/editorLayout.cpp
  editor/mainEditor.cpp
  editor/graph/graphEditor.cpp
  editor/graph/graphEditorUtils.cpp
  editor/graph/graphCompilation.cpp
  editor/graph/graphState.cpp
  editor/graph/graphSerializer.cpp

  editor/panels/leftPanel.cpp
  editor/panels/topPanel.cpp
  editor/panels/consolePanel.cpp
  editor/panels/consoleEmiLogger.cpp
  editor/panels/graphPreviewPanel.cpp
  editor/panels/inspectorPanel.cpp
  editor/settings.cpp
  editor/renderer/pinRenderer.cpp
  editor/renderer/linkRenderer.cpp
  editor/renderer/nodeRenderer.cpp
  editor/renderer/fieldWidgetRenderer.cpp
  core/graph/pin.cpp
  core/graph/link.cpp
  core/registry/nodeFactory.cpp
  core/registry/nodeRegistry.cpp
  core/registry/nodes/eventNodes.cpp
  core/registry/nodes/dataNodes.cpp
  core/registry/nodes/logicNodes.cpp
  core/registry/nodes/flowNodes.cpp
  core/registry/nodes/structNodes.cpp
  core/registry/nodes/functionNodes.cpp
  core/compiler/graphCompiler.cpp
  core/compiler/pinResolver.cpp

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
  ${_emi_inc_dir}
  ${_emi_src_dir}

  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/core/graph
  ../dependencies/imgui-node-editor
)
target_include_directories(emi-editor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(emi-editor PRIVATE
  ImGui-SFML::ImGui-SFML
  EMI
  nlohmann_json::nlohmann_json
)

target_compile_definitions(emi-editor PRIVATE INCLUDE_DEBUGGER)

if(MSVC)
  target_compile_options(emi-editor PRIVATE /FS)
endif()

if(WIN32)
  if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET emi-editor POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:ImGui-SFML::ImGui-SFML>
          $<TARGET_FILE:sfml-graphics>
          $<TARGET_FILE:sfml-window>
          $<TARGET_FILE:sfml-system>
          $<TARGET_FILE_DIR:emi-editor>
    )
  endif()
endif()

# Copy assets to build directory so fonts and other resources are always found
add_custom_command(TARGET emi-editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/assets
    $<TARGET_FILE_DIR:emi-editor>/assets
)