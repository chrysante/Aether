#ifndef AETHER_WINDOW_H
#define AETHER_WINDOW_H

#include <string>

#include <Aether/ADT.h>

namespace xui {

class View;
class ToolbarView;

namespace internal {

struct WindowImpl;

} // namespace internal

struct WindowProperties {
    bool fullSizeContentView = false;
};

/// Represents a window. Creating an instance of this class opens a new window
class Window {
public:
    explicit Window(std::string title, Rect frame, WindowProperties props = {},
                    std::unique_ptr<View> content = nullptr);

    Window(Window const&) = delete;

    ~Window();

    /// Sets the position and size of the window
    void setFrame(Rect frame, bool animate = false);

    /// Sets the title of the window
    void setTitle(std::string title);

    /// Sets the content view of this window. The content view is the top-level
    /// view
    void setContentView(std::unique_ptr<View> view);

    ///
    void setToolbar(std::unique_ptr<ToolbarView> toolbar);

    /// \Returns the content view of the window
    View* contentView() { return _content.get(); }

    /// \Returns the underlying OS window handle
    void* nativeHandle() const { return _handle; }

    /// \Returns the title of the window
    std::string const& title() const { return _title; }

    /// \Returns the properties that the window was constructed with
    WindowProperties const& properties() const { return _props; }

    /// \Returns the frame of the window
    Rect frame() const;

private:
    friend struct internal::WindowImpl;

    void* _handle;
    std::string _title;
    WindowProperties _props;
    std::unique_ptr<View> _content;
    std::unique_ptr<ToolbarView> _toolbar;
};

std::unique_ptr<Window> window(std::string title, Rect frame,
                               WindowProperties props = {},
                               std::unique_ptr<View> content = nullptr);

} // namespace xui

#endif // AETHER_WINDOW_H
