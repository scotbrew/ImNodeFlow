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
// Style definitions
// =============================================================================

// StyleManager (Singleton)
class StyleManager 
{
public:
    // remove the copy constructor and assignment
    StyleManager(const StyleManager&) = delete;
    StyleManager& operator=(const StyleManager&) = delete;

    // get the singleton static instance
    static StyleManager& instance() {
        static StyleManager s_instance;
        return s_instance;
    }

    // Styles (Nodes)
    std::shared_ptr<ImFlow::NodeStyle> nodeStyleGrey;

    // Styles (Pins)
    //   socket_shape is the number of segments of the shape.  3=triangle, 4=diamond, 0=circle
    std::shared_ptr<ImFlow::PinStyle> pinStyleBigRed;

private:
    StyleManager() {
        nodeStyleGrey = std::make_shared<ImFlow::NodeStyle>(
                IM_COL32(41, 48, 56, 255),
                IM_COL32(200, 200, 200, 255),
                10.f
                );  

        pinStyleBigRed = std::make_shared<ImFlow::PinStyle>(*ImFlow::PinStyle::red()); // copy the pinStyle      
        pinStyleBigRed->socket_radius = 10.f;
    }
};

// =============================================================================
// Class Overrides
// =============================================================================

    template<class T> class InMultiPin : public ImFlow::InPin<T>
    {
    public:
        explicit InMultiPin<T>(
            ImFlow::PinUID uid, 
            const std::string& name, 
            T defReturn, 
            std::function<bool(ImFlow::Pin*, ImFlow::Pin*)> filter,
            std::shared_ptr<ImFlow::PinStyle> style,
            ImFlow::BaseNode* parent,
            ImFlow::ImNodeFlow** inf)
            : ImFlow::InPin<T>(uid, name, defReturn, std::move(filter), style, parent, inf) {}

        void createLink(ImFlow::Pin* other) override;
        void deleteLink(ImFlow::Link* pLinkToDelete) override;

    private:
        std::vector<std::shared_ptr<ImFlow::Link>> m_links;  // Additional links past m_link      
    };

    template<class T>
    void InMultiPin<T>::createLink(ImFlow::Pin *other)
    {
        if (other == this || other->getType() == ImFlow::PinType_Input)
            return;

        if (m_parent == other->getParent() && !m_allowSelfConnection)
            return;

        if (m_link && m_link->left() == other)
        {
            //m_link.reset();            
            return;
        }

        if (!m_filter(other, this)) // Check Filter
            return;

        if (m_link) {
            // keep the link and add to m_links;
            auto new_link = std::make_shared<ImFlow::Link>(other, this, (*m_inf));
            m_links.emplace_back(new_link);
            other->setLink(new_link);
            (*m_inf)->addLink(new_link);
        }
        else {
            // Standard for single input InPin
            m_link = std::make_shared<ImFlow::Link>(other, this, (*m_inf));
            other->setLink(m_link);
            (*m_inf)->addLink(m_link);
        }
    }


    template<class T>
    void InMultiPin<T>::deleteLink(ImFlow::Link* pLinkToDelete) {
        if (m_links.empty()) {
            // trivial case as there is only one.  Standard single connection behavior
            m_link.reset(); 
        }
        else {
            // find the pLinkToDelete in m_links
            auto it = std::find_if(m_links.begin(), m_links.end(), 
                [pLinkToDelete](const std::shared_ptr<ImFlow::Link>& ptr) {
                    return ptr.get() == pLinkToDelete; // Compare raw addresses
                });
            if (it != m_links.end()) {
                // found it in m_links, so erase it there
                m_links.erase(it);
            }
            else {
                // did not find it in m_links so must be m_link
                m_link.reset(); 
            }
        }
    }



// =============================================================================
// Node definitions
// =============================================================================

// A node with four typed inputs and four typed outputs, used for stress testing.
class QtNode_DesignSpec : public ImFlow::BaseNode
{
public:
    QtNode_DesignSpec()
    {
        auto& styleManager = StyleManager::instance();

        setTitle("DesignSpec Node");
        setStyle(ImFlow::NodeStyle::green());

        addIN_uid<int, std::string, InMultiPin<int>>("Alpha", "Alpha", 0, ImFlow::ConnectionFilter::SameType(), ImFlow::PinStyle::red());
        addIN<float> ("Bravo",   0, ImFlow::ConnectionFilter::SameType(), ImFlow::PinStyle::blue());
        addIN<bool>  ("Charlie", false, ImFlow::ConnectionFilter::SameType(), ImFlow::PinStyle::green());
        addIN<double>("Delta",   0.0, ImFlow::ConnectionFilter::SameType(), ImFlow::PinStyle::cyan());

        addOUT_uid<int, std::string>("Alpha", "",    ImFlow::PinStyle::red())->behaviour([this]() { return 0; });
        addOUT_uid<float, std::string>("Bravo", "",  ImFlow::PinStyle::blue())->behaviour([this]() { return 0.f; });
        addOUT_uid<bool, std::string>("Charlie", "", ImFlow::PinStyle::green())->behaviour([this]() { return false; });
        addOUT_uid<double, std::string>("Delta", "", ImFlow::PinStyle::cyan())->behaviour([this]() { return 0.0; });

        addOUT<int>  ("Echo",    ImFlow::PinStyle::red())->behaviour([this]() { return 0; });
        addOUT<bool> ("Foxtrot", ImFlow::PinStyle::green())->behaviour([this]() { return false; });
        addOUT<bool> ("Golf",    ImFlow::PinStyle::green())->behaviour([this]() { return false; });
        addOUT<float>("Hotel",   ImFlow::PinStyle::blue())->behaviour([this]() { return 0.f; });
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

        // configure ImNodeFlow (the handler)
        ImFlow::InfStyler& infStyler = mINF.getStyle();
        //infStyler.colors.grid = IM_COL32(255, 100, 100, 255);
        //infStyler.colors.subGrid = IM_COL32(200, 255, 200, 255);

        // Note: the background has to be set directly to the ContainedContext m_context
        //   Does not work: infStyler.colors.background = ...
        //   Use mINF.getGrid().config().color
        //mINF.getGrid().config().color = IM_COL32(255,255,255, 255);

        std::cerr << "Node count: " << mINF.getNodesCount() << std::endl;

        // mINF.getGrid() gets one the ContainedContext with lots of useful params
        auto& gridConfig = mINF.getGrid().config();
        gridConfig.zoom_min = 0.05f;
        gridConfig.zoom_max = 3.f;

        if (!populate)
            return;

        // == Populate ==
        auto& styleManager = StyleManager::instance();

        constexpr int   kRows      = 50;
        constexpr int   kCols      = 40;
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
                n->setStyle(styleManager.nodeStyleGrey);
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
                nodes.at(outIdx)->outPin("Foxtrot")->createLink(
                    nodes.at(inIdx)->inPin("Charlie"));
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
