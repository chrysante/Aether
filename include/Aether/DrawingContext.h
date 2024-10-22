#ifndef AETHER_DRAWINGCONTEXT_H
#define AETHER_DRAWINGCONTEXT_H

#include <span>

#include <Aether/Shapes.h>
#include <Aether/Vec.h>

namespace xui {

class DrawingContext {
public:
    virtual ~DrawingContext() = default;

    virtual void addLine(std::span<vml::float2 const> points,
                         LineMeshOptions const& options) = 0;
    virtual void addPolygon(std::span<vml::float2 const> points) = 0;

    virtual void draw() = 0;
};

} // namespace xui

#endif // AETHER_DRAWINGCONTEXT_H
