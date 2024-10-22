#include "Aether/DrawingContext.h"

#include <cassert>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <vml/vml.hpp>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/Shapes.h"
#include "Aether/View.h"

using namespace xui;
using namespace vml::short_types;

static constexpr size_t VertexSize = sizeof(Vec2<float>);
static constexpr size_t IndexSize = 4;

namespace {

struct DrawCall {
    size_t beginVertex, endVertex;
    size_t beginIndex, endIndex;
};

struct MacOSDrawingContext: DrawingContext {
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    CAMetalLayer* metalLayer;
    View* view;
    NSView* __unsafe_unretained nativeView;

    // Rendering objects
    id<MTLRenderPipelineState> pipeline;
    id<MTLBuffer> vertexBuffer;
    id<MTLBuffer> indexBuffer;
    id<MTLBuffer> transformMatrixBuffer;

    //
    std::vector<float2> vertexStorage;
    std::vector<uint32_t> indexStorage;
    std::vector<DrawCall> drawCalls;

    MacOSDrawingContext(View* view);
    void createRenderingState();

    void addDrawCall(DrawCall const& dc);
    auto vertexEmitter() {
        return [this](float2 p) { vertexStorage.push_back(p); };
    }
    auto triangleEmitter() {
        return [this](uint32_t a, uint32_t b, uint32_t c) {
            indexStorage.push_back(a);
            indexStorage.push_back(b);
            indexStorage.push_back(c);
        };
    }

    void addLine(std::span<vml::float2 const> points,
                 LineMeshOptions const& options) final;
    void addPolygon(std::span<vml::float2 const> points) final;

    void draw() final;

    void uploadDrawData();
};

} // namespace

MacOSDrawingContext::MacOSDrawingContext(View* view):
    view(view), nativeView(transfer(view->nativeHandle())) {
    device = MTLCreateSystemDefaultDevice();
    commandQueue = [device newCommandQueue];
    metalLayer = [[CAMetalLayer alloc] init];
    metalLayer.device = device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.contentsScale = nativeView.window.backingScaleFactor;
    metalLayer.contentsGravity = kCAGravityTopLeft;
    metalLayer.opaque = NO;
    createRenderingState();
}

constexpr auto ShaderSource = R"(
#include <metal_stdlib>

using namespace metal;

vertex float4 vertex_main(float2 device const* vertices [[buffer(0)]],
                          float4x4 device const& transform [[buffer(1)]],
                          uint vertexID [[vertex_id]]) {
    return transform * float4(vertices[vertexID], 0, 1);
}

fragment float4 fragment_main() {
    return float4(1, 0, 0, 1);
}
)";

static id<MTLRenderPipelineState> createPipeline(id<MTLDevice> device) {
    NSError* error = nil;
    id<MTLLibrary> library = [device
        newLibraryWithSource:[NSString stringWithCString:ShaderSource
                                                encoding:NSUTF8StringEncoding]
                     options:nil
                       error:&error];
    if (error) {
        NSLog(@"Failed to compile shader: %@", error);
        return nil;
    }
    id<MTLFunction> vertexFunction =
        [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction =
        [library newFunctionWithName:@"fragment_main"];
    MTLRenderPipelineDescriptor* pipelineDescriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat =
        MTLPixelFormatBGRA8Unorm; // Assuming the color format of your view
    id<MTLRenderPipelineState> pipelineState = [device
        newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                       error:&error];
    if (error) {
        NSLog(@"Failed to create pipeline: %@", error);
        return nil;
    }
    return pipelineState;
}

void MacOSDrawingContext::createRenderingState() {
    pipeline = createPipeline(device);
}

void MacOSDrawingContext::addDrawCall(DrawCall const& dc) {
    if (dc.beginVertex == dc.endVertex || dc.beginIndex == dc.endIndex) {
        return;
    }
    drawCalls.push_back(dc);
}

void MacOSDrawingContext::addLine(std::span<vml::float2 const> points,
                                  LineMeshOptions const& options) {
    DrawCall drawCall{ .beginVertex = vertexStorage.size(),
                       .beginIndex = indexStorage.size() };
    buildLineMesh(points, vertexEmitter(), triangleEmitter(), options);
    drawCall.endVertex = vertexStorage.size();
    drawCall.endIndex = indexStorage.size();
    addDrawCall(drawCall);
}

void MacOSDrawingContext::addPolygon(std::span<vml::float2 const> points) {
    DrawCall drawCall{ .beginVertex = vertexStorage.size(),
                       .beginIndex = indexStorage.size() };
    std::ranges::copy(points, std::back_inserter(vertexStorage));
    triangulatePolygon(points, triangleEmitter());
    drawCall.endVertex = vertexStorage.size();
    drawCall.endIndex = indexStorage.size();
    addDrawCall(drawCall);
}

void MacOSDrawingContext::draw() {
    if (!pipeline) return;
    CGSize newSize = nativeView.bounds.size;
    newSize.width *= metalLayer.contentsScale;
    newSize.height *= metalLayer.contentsScale;
    if (!CGSizeEqualToSize(metalLayer.drawableSize, newSize)) {
        metalLayer.drawableSize = newSize;
    }
    id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
    if (!drawable) return;
    uploadDrawData();
    MTLRenderPassDescriptor* passDescriptor =
        [MTLRenderPassDescriptor renderPassDescriptor];
    id<MTLTexture> tex = drawable.texture;
    passDescriptor.colorAttachments[0].texture = tex;
    passDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDescriptor.colorAttachments[0].clearColor =
        MTLClearColorMake(0, 0, 0, 0);
    passDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> encoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    [encoder setRenderPipelineState:pipeline];
    [encoder setCullMode:MTLCullModeBack];
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [encoder setVertexBuffer:transformMatrixBuffer offset:0 atIndex:1];
    //    [encoder setTriangleFillMode:MTLTriangleFillModeLines];
    for (auto& drawCall: drawCalls) {
        [encoder setVertexBufferOffset:drawCall.beginVertex * VertexSize
                               atIndex:0];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:drawCall.endIndex - drawCall.beginIndex
                             indexType:MTLIndexTypeUInt32
                           indexBuffer:indexBuffer
                     indexBufferOffset:drawCall.beginIndex * IndexSize];
    }
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    // Reset
    drawCalls.clear();
    vertexStorage.clear();
    indexStorage.clear();
}

static void uploadData(id<MTLDevice> device, id<MTLBuffer> __strong* buffer,
                       void* data, size_t size) {
    if ((!*buffer && size > 0) || (*buffer).length < size) {
        *buffer = [device newBufferWithBytes:data
                                      length:size
                                     options:MTLResourceStorageModeShared];
        return;
    }
    std::memcpy([*buffer contents], data, size);
}

void MacOSDrawingContext::uploadDrawData() {
    uploadData(device, &vertexBuffer, vertexStorage.data(),
               vertexStorage.size() * VertexSize);
    uploadData(device, &indexBuffer, indexStorage.data(),
               indexStorage.size() * IndexSize);
    float l = 0;
    float r = view->size().width();
    float t = 0;
    float b = view->size().height();
    float n = -1;
    float f = 1;
    Vec<float, 4> transform[4] = {
        { 2 / (r - l), 0, 0, 0 },  // column 0
        { 0, 2 / (t - b), 0, 0 },  // column 1
        { 0, 0, -2 / (f - n), 0 }, // column 2
        { -(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1 }
        // column 3
    };
    uploadData(device, &transformMatrixBuffer, transform, sizeof(transform));
}

static MacOSDrawingContext* createDrawingContext(View& view) {
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
