#include "example-large-graph.hpp"

#include "ImNodeFlow.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <memory>
#include <vector>

#define SDL_MAIN_HANDLED  // MUST appear before SDL.h
#include <SDL.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#   include <SDL_opengles2.h>
#else
#   include <SDL_opengl.h>
#endif

#ifdef __EMSCRIPTEN__
#   include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

// =============================================================================
// Node definitions
// =============================================================================

// A node with four typed inputs and four typed outputs, used for stress testing.
class QtNode_DesignSpec : public ImFlow::BaseNode
{
public:
    QtNode_DesignSpec()
    {
        setTitle("DesignSpec Node");
        setStyle(ImFlow::NodeStyle::green());

        addIN<int>   ("Alpha",   0, ImFlow::ConnectionFilter::SameType());
        addIN<float> ("Bravo",   0, ImFlow::ConnectionFilter::SameType());
        addIN<bool>  ("Charlie", false, ImFlow::ConnectionFilter::SameType());
        addIN<double>("Delta",   0.0, ImFlow::ConnectionFilter::SameType());

        addOUT<int>  ("Echo",    nullptr)->behaviour([this]() { return 0; });
        addOUT<bool> ("Foxtrot", nullptr)->behaviour([this]() { return false; });
        addOUT<bool> ("Golf",    nullptr)->behaviour([this]() { return false; });
        addOUT<float>("Hotel",   nullptr)->behaviour([this]() { return 0.f; });
    }

    void draw() override {
        ImGui::SetNextItemWidth(100.f);
    }
};

// Simple integer adder with a user-editable constant.
class SimpleSum : public ImFlow::BaseNode
{
public:
    SimpleSum()
    {
        setTitle("Simple sum");
        setStyle(ImFlow::NodeStyle::green());
        addIN<int>("In", 0, ImFlow::ConnectionFilter::SameType());
        addOUT<int>("Out", nullptr)->behaviour(
            [this]() { return getInVal<int>("In") + m_valB; });
    }

    void draw() override
    {
        ImGui::SetNextItemWidth(100.f);
        ImGui::InputInt("##ValB", &m_valB);
    }

private:
    int m_valB = 0;
};

// Node whose content is only visible when the node is selected.
class CollapsingNode : public ImFlow::BaseNode
{
public:
    CollapsingNode()
    {
        setTitle("Collapsing node");
        setStyle(ImFlow::NodeStyle::red());
        addIN<int>("A", 0, ImFlow::ConnectionFilter::SameType());
        addIN<int>("B", 0, ImFlow::ConnectionFilter::SameType());
        addOUT<int>("Out", nullptr)->behaviour(
            [this]() { return getInVal<int>("A") + getInVal<int>("B"); });
    }

    void draw() override
    {
        if (isSelected())
            ImGui::Text("Only visible when selected!");
    }
};

// Leaf node that displays the sum of its two integer inputs.
class ResultNode : public ImFlow::BaseNode
{
public:
    ResultNode()
    {
        setTitle("Result node");
        setStyle(ImFlow::NodeStyle::brown());
        addIN<int>("A", 0, ImFlow::ConnectionFilter::SameType());
        addIN<int>("B", 0, ImFlow::ConnectionFilter::SameType());
    }

    void draw() override
    {
        ImGui::Text("Result: %d", getInVal<int>("A") + getInVal<int>("B"));
    }
};

// NOTE: Difference from example.cpp:
//   Previously NodeEditor : BaseNode was used only to get a draw() method,
//   but the instance was never actually added to any ImNodeFlow graph – it was
//   called directly from main().  Inheriting BaseNode gave the illusion that
//   NodeEditor *is* a node, caused a confusing unused m_inf member from the
//   base class, and added unnecessary overhead (pin lists, style, UID, etc.).
//   A plain struct with the same interface is cleaner and correct.
struct NodeEditor
{
    ImFlow::ImNodeFlow mINF;

    // Create a stress-test grid of nodes and connect them in a chain.
    NodeEditor(ImVec2 size, bool populate)
    {
        mINF.setSize(size);

        if (!populate)
            return;

        constexpr int   kRows      = 5;
        constexpr int   kCols      = 5;
        constexpr float kCellW     = 200.f;
        constexpr float kCellH     = 200.f;

        std::vector<std::shared_ptr<ImFlow::BaseNode>> nodes;
        nodes.reserve(kRows * kCols);

        for (int col = 0; col < kCols; ++col)
        {
            for (int row = 0; row < kRows; ++row)
            {
                auto n = mINF.addNode<QtNode_DesignSpec>(
                    { kCellW * static_cast<float>(row),
                      kCellH * static_cast<float>(col) });
                nodes.push_back(n);
            }
        }

        // Chain each node's "Echo" output into the next node's "Alpha" input
        // within each column.
        for (int col = 0; col < kCols - 1; ++col)
        {
            for (int row = 0; row < kRows - 1; ++row)
            {
                const size_t outIdx = static_cast<size_t>(col * kRows + row);
                const size_t inIdx  = static_cast<size_t>(col * kRows + row + 1);
                nodes.at(outIdx)->outPin("Echo")->createLink(
                    nodes.at(inIdx)->inPin("Alpha"));
            }
        }
    }

    void setSize(ImVec2 size) { mINF.setSize(size); }

    // Call this from within an ImGui window each frame.
    void draw() { mINF.update(); }
};

// =============================================================================
// Entry point
// =============================================================================
int main(int, char**)
{
    // ------------------------------------------------------------------
    // SDL initialisation
    // ------------------------------------------------------------------
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return -1;
    }

    // GL version selection
#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                        SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,  8);

    SDL_Window* window = SDL_CreateWindow(
        "ImNodeFlow example",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
    {
        printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // vsync

    // ------------------------------------------------------------------
    // ImGui initialisation
    // ------------------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // ------------------------------------------------------------------
    // Node editor setup
    // ------------------------------------------------------------------
    // Defer the actual size until we know the window dimensions (set each frame).
    NodeEditor neditor({ 500.f, 500.f }, /*populate=*/true);

    // ------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------
    bool done = false;

#ifdef __EMSCRIPTEN__
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
#endif
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Start frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Fill the whole OS window with one ImGui window that hosts the editor.
        const ImVec2 win_pos  = ImVec2(1.f, 1.f);
        const ImVec2 win_size = io.DisplaySize - ImVec2(1.f, 1.f);
        ImGui::SetNextWindowPos(win_pos);
        ImGui::SetNextWindowSize(win_size);
        ImGui::Begin("Node Editor", nullptr,
                     ImGuiWindowFlags_NoTitleBar  |
                     ImGuiWindowFlags_NoResize    |
                     ImGuiWindowFlags_NoMove      |
                     ImGuiWindowFlags_NoCollapse);
        {
            // Leave a small margin so the scrollbar doesn't overlay the editor.
            const ImVec2 editorSize = win_size - ImVec2(16.f, 16.f);
            neditor.setSize(editorSize);
            neditor.draw();
        }
        ImGui::End();

        // Render
        ImGui::Render();
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x),
                         static_cast<int>(io.DisplaySize.y));
        glClearColor(
            clear_color.x * clear_color.w,
            clear_color.y * clear_color.w,
            clear_color.z * clear_color.w,
            clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // ------------------------------------------------------------------
    // Cleanup
    // ------------------------------------------------------------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
