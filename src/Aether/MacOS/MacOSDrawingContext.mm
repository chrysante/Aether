#include "Aether/DrawingContext.h"

#include <cassert>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/View.h"

using namespace xui;

namespace xui {

struct MacOSDrawingContext: DrawingContext {
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    CAMetalLayer* metalLayer;
    View* view;
    NSView* __unsafe_unretained nativeView;

    MacOSDrawingContext(View* view);

    void draw() final;
};

} // namespace xui

MacOSDrawingContext::MacOSDrawingContext(View* view):
    view(view), nativeView(transfer(view->nativeHandle())) {
    device = MTLCreateSystemDefaultDevice();
    commandQueue = [device newCommandQueue];
    metalLayer = [[CAMetalLayer alloc] init];
    metalLayer.device = device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
}

void MacOSDrawingContext::draw() {
    id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
    if (!drawable) {
        return;
    }
    MTLRenderPassDescriptor* passDescriptor =
        [MTLRenderPassDescriptor renderPassDescriptor];
    id<MTLTexture> tex = drawable.texture;
    passDescriptor.colorAttachments[0].texture = tex;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].clearColor =
        MTLClearColorMake(0.0, 0.5, 0.5, 1.0);
    passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    [renderEncoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

static DrawingContext* createDrawingContext(View& view) {
    view.setAttribute<ViewAttributeKey::DrawingContext>(
        std::make_shared<MacOSDrawingContext>(&view));
    auto opt = view.getAttribute<ViewAttributeKey::DrawingContext>();
    assert(opt);
    MacOSDrawingContext* ctx = static_cast<MacOSDrawingContext*>(opt->get());
    NSView* native = transfer(view.nativeHandle());
    native.layer = ctx->metalLayer;
    native.wantsLayer = YES;
    return ctx;
}

DrawingContext* View::getDrawingContext() {
    auto ctx = getAttribute<ViewAttributeKey::DrawingContext>();
    if (ctx) {
        return ctx->get();
    }
    return ::createDrawingContext(*this);
}

void View::setShadow(ShadowConfig config) {
    ::createDrawingContext(*this);
    NSView* native = transfer(nativeHandle());
    native.layer.shadowOpacity = config.shadowOpacity;
    native.layer.shadowColor = [toNSColor(config.shadowColor) CGColor];
    native.layer.shadowOffset = toNSSize(config.shadowOffset);
    native.layer.shadowRadius = config.shadowRadius;
    native.layer.masksToBounds = NO;
}
