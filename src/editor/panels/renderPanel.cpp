#include "renderPanel.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
void SetWindowAlwaysOnTop(sf::RenderWindow* window)
{
    if (!window) return;
#if defined(_WIN32)
    const sf::WindowHandle handle = window->getSystemHandle();
    if (handle)
        ::SetWindowPos(static_cast<HWND>(handle), HWND_TOPMOST,
                       0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
    (void)window;
#endif
}

// Compute cell layout from grid dimensions and window size.
struct CellLayout { float originX, originY, cellSize; };
CellLayout computeLayout(int W, int H, float viewW, float viewH)
{
    constexpr float kMargin = 24.0f;
    const float fitW = viewW - kMargin * 2.0f;
    const float fitH = viewH - kMargin * 2.0f;
    const float cell = std::min(fitW / static_cast<float>(W),
                                fitH / static_cast<float>(H));
    const float totalW = cell * static_cast<float>(W);
    const float totalH = cell * static_cast<float>(H);
    return { kMargin + (fitW - totalW) * 0.5f,
             kMargin + (fitH - totalH) * 0.5f,
             cell };
}
} // namespace

// ---------------------------------------------------------------------------
class RenderPanel::Impl
{
public:
    // --- pixel buffer (written from VM thread, read on main thread) ---
    struct PixelBuffer
    {
        int W = 0;
        int H = 0;
        std::vector<sf::Color> cells;
        mutable std::mutex mutex;
        bool hasContent = false;
    } m_buf;

    // --- SFML window (main thread only) ---
    std::unique_ptr<sf::RenderWindow> m_window;
    sf::Clock m_clock;
    bool m_started = false;

    // --- input state (atomic — safe to read from VM thread) ---
    std::atomic<int> m_mouseCellX{ 0 };
    std::atomic<int> m_mouseCellY{ 0 };

    // --- event queue for waitInput() ---
    std::mutex              m_evMutex;
    std::condition_variable m_evCv;
    std::queue<int>         m_evQueue;
    bool                    m_closed = false;

    // -----------------------------------------------------------------------
    void open()
    {
        if (m_window && m_window->isOpen()) return;
        m_window = std::make_unique<sf::RenderWindow>(
            sf::VideoMode(900, 700), "EMI Visual Render",
            sf::Style::Titlebar | sf::Style::Close);
        SetWindowAlwaysOnTop(m_window.get());
        m_window->setFramerateLimit(60);
        m_clock.restart();
        m_started = true;
        m_closed = false;
    }

    void close()
    {
        if (m_window) { m_window->close(); m_window.reset(); }
        m_started = false;
        {
            std::lock_guard<std::mutex> lk(m_evMutex);
            m_closed = true;
        }
        m_evCv.notify_all();
    }

    bool isOpen() const { return m_window && m_window->isOpen(); }

    // -----------------------------------------------------------------------
    void update()
    {
        if (!m_window || !m_window->isOpen()) return;
        if (!m_started) { m_clock.restart(); m_started = true; }

        sf::Event ev{};
        while (m_window->pollEvent(ev))
        {
            switch (ev.type)
            {
            case sf::Event::Closed:
                close();
                return;

            case sf::Event::KeyPressed:
            {
                std::lock_guard<std::mutex> lk(m_evMutex);
                m_evQueue.push(static_cast<int>(ev.key.code));
            }
            m_evCv.notify_one();
            break;

            case sf::Event::MouseButtonPressed:
            {
                // Match original: left=1, right=2
                const int btn = (ev.mouseButton.button == sf::Mouse::Right) ? 2 : 1;
                updateMouseCell(ev.mouseButton.x, ev.mouseButton.y);
                std::lock_guard<std::mutex> lk(m_evMutex);
                m_evQueue.push(btn);
            }
            m_evCv.notify_one();
            break;

            case sf::Event::MouseMoved:
                updateMouseCell(ev.mouseMove.x, ev.mouseMove.y);
                break;

            default: break;
            }
        }

        m_window->clear(sf::Color(20, 20, 20));
        drawBuffer();
        m_window->display();
    }

    // -----------------------------------------------------------------------
    void drawCell(int x, int y, int r, int g, int b)
    {
        std::lock_guard<std::mutex> lk(m_buf.mutex);
        if (x < 0 || y < 0 || x >= m_buf.W || y >= m_buf.H) return;
        m_buf.cells[static_cast<size_t>(y * m_buf.W + x)] = sf::Color(
            static_cast<sf::Uint8>(std::clamp(r, 0, 255)),
            static_cast<sf::Uint8>(std::clamp(g, 0, 255)),
            static_cast<sf::Uint8>(std::clamp(b, 0, 255)));
        m_buf.hasContent = true;
    }

    void clearGrid(int w, int h)
    {
        std::lock_guard<std::mutex> lk(m_buf.mutex);
        m_buf.W = std::max(1, w);
        m_buf.H = std::max(1, h);
        m_buf.cells.assign(static_cast<size_t>(m_buf.W * m_buf.H), sf::Color(20, 20, 20));
        m_buf.hasContent = true;
    }

    // -----------------------------------------------------------------------
    int getCell(int x, int y) const
    {
        std::lock_guard<std::mutex> lk(m_buf.mutex);
        if (x < 0 || y < 0 || x >= m_buf.W || y >= m_buf.H) return 0;
        const sf::Color& c = m_buf.cells[static_cast<size_t>(y * m_buf.W + x)];
        return (static_cast<int>(c.r) + c.g + c.b) / 3; // luminance 0-255
    }

    int waitInput()
    {
        std::unique_lock<std::mutex> lk(m_evMutex);
        m_evCv.wait(lk, [this] { return !m_evQueue.empty() || m_closed; });
        if (m_evQueue.empty()) return -1;
        const int code = m_evQueue.front();
        m_evQueue.pop();
        return code;
    }

    bool isKeyDown(int sfKeyCode)
    {
        if (sfKeyCode < 0 || sfKeyCode >= sf::Keyboard::KeyCount) return false;
        return sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Key>(sfKeyCode));
    }

    bool isMouseButtonDown(int button)
    {
        switch (button)
        {
        case 0: return sf::Mouse::isButtonPressed(sf::Mouse::Left);
        case 1: return sf::Mouse::isButtonPressed(sf::Mouse::Right);
        case 2: return sf::Mouse::isButtonPressed(sf::Mouse::Middle);
        default: return false;
        }
    }

    int getMouseX() const { return m_mouseCellX.load(); }
    int getMouseY() const { return m_mouseCellY.load(); }

private:
    void updateMouseCell(int pixelX, int pixelY)
    {
        if (!m_window) return;
        std::lock_guard<std::mutex> lk(m_buf.mutex);
        if (m_buf.W <= 0 || m_buf.H <= 0) return;
        const auto sz = m_window->getSize();
        const CellLayout lay = computeLayout(m_buf.W, m_buf.H,
                                             static_cast<float>(sz.x),
                                             static_cast<float>(sz.y));
        if (lay.cellSize <= 0.0f) return;
        m_mouseCellX.store(static_cast<int>((static_cast<float>(pixelX) - lay.originX) / lay.cellSize));
        m_mouseCellY.store(static_cast<int>((static_cast<float>(pixelY) - lay.originY) / lay.cellSize));
    }

    void drawBuffer()
    {
        std::lock_guard<std::mutex> lk(m_buf.mutex);
        if (!m_buf.hasContent || m_buf.W <= 0 || m_buf.H <= 0) return;

        const auto sz = m_window->getSize();
        const CellLayout lay = computeLayout(m_buf.W, m_buf.H,
                                             static_cast<float>(sz.x),
                                             static_cast<float>(sz.y));
        const float gap = lay.cellSize > 4.0f ? 1.0f : 0.0f;
        sf::RectangleShape cell(sf::Vector2f(lay.cellSize - gap, lay.cellSize - gap));
        cell.setOutlineThickness(0.0f);

        for (int y = 0; y < m_buf.H; ++y)
        {
            for (int x = 0; x < m_buf.W; ++x)
            {
                cell.setFillColor(m_buf.cells[static_cast<size_t>(y * m_buf.W + x)]);
                cell.setPosition(lay.originX + static_cast<float>(x) * lay.cellSize,
                                 lay.originY + static_cast<float>(y) * lay.cellSize);
                m_window->draw(cell);
            }
        }
    }
};

// ---------------------------------------------------------------------------
RenderPanel::RenderPanel() : pImpl(new Impl) {}
RenderPanel::~RenderPanel() = default;

void RenderPanel::open()              { pImpl->open(); }
void RenderPanel::close()             { pImpl->close(); }
bool RenderPanel::isOpen() const      { return pImpl->isOpen(); }
void RenderPanel::restartPlayback()   { pImpl->m_clock.restart(); pImpl->m_started = true; }
void RenderPanel::update()            { pImpl->update(); }
void RenderPanel::drawCell(int x, int y, int r, int g, int b) { pImpl->drawCell(x, y, r, g, b); }
void RenderPanel::clearGrid(int w, int h)  { pImpl->clearGrid(w, h); }
void RenderPanel::renderGrid()        { /* frame-driven, no-op */ }
int  RenderPanel::getCell(int x, int y) const { return pImpl->getCell(x, y); }
int  RenderPanel::waitInput()         { return pImpl->waitInput(); }
bool RenderPanel::isKeyDown(int k)    { return pImpl->isKeyDown(k); }
bool RenderPanel::isMouseButtonDown(int b) { return pImpl->isMouseButtonDown(b); }
int  RenderPanel::getMouseX() const   { return pImpl->getMouseX(); }
int  RenderPanel::getMouseY() const   { return pImpl->getMouseY(); }
