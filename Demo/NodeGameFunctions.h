#pragma once

//class GraphPreviewPanel;
class RenderPanel;

/**
 * @brief Bind the live GraphPreviewPanel instance to the native EMI functions.
 *
 * Must be called before any graph is compiled and executed. The pointer must
 * remain valid for the lifetime of the application (owned by EditorLayout).
 */
//void SetPreviewPanelForNatives(GraphPreviewPanel* panel);

/**
 * @brief Bind the live RenderPanel instance to the native EMI functions.
 *
 * Must be called before any graph is compiled and executed. The pointer must
 * remain valid for the lifetime of the application (owned by EditorLayout).
 */
void SetRenderPanelForNatives(RenderPanel* panel);

/// Returns the currently bound render panel (used by previewNatives.cpp).
RenderPanel* GetNativeRenderPanel();

/// Example stub for a button to open the RenderPanel (to be hooked up in UI layer)
void ShowRenderPanelButton();
