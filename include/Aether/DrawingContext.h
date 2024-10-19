#ifndef AETHER_DRAWINGCONTEXT_H
#define AETHER_DRAWINGCONTEXT_H

namespace xui {

class DrawingContext {
public:
    virtual ~DrawingContext() = default;

    virtual void draw() = 0;
};

} // namespace xui

#endif // AETHER_DRAWINGCONTEXT_H
