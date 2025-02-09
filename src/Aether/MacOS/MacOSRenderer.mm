#include "Aether/DrawingContext.h"

#include <cassert>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "Aether/MacOS/MacOSUtil.h"
#include "Aether/View.h"

using namespace xui;
using namespace vml::short_types;

static constexpr size_t VertexSize = sizeof(float2);
static constexpr size_t IndexSize = sizeof(uint32_t);

static MTLPixelFormat toMTL(PixelFormat fmt) {
    using enum PixelFormat;
    switch (fmt) {
    case RGBA8Unorm:
        return MTLPixelFormatRGBA8Unorm;
    }
    assert(false);
}

namespace {

struct MacOSRenderer: xui::Renderer {
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
    std::vector<id<MTLBuffer>> uniformBuffers;
    RendererOptions options;

    MacOSRenderer(View* view, RendererOptions const& options);
    void createRenderingState();
    id<MTLBuffer> __strong* getUniformBuffer(size_t index);

    void render(std::span<float2 const> vertices,
                std::span<uint32_t const> indices,
                std::span<DrawCall const> drawCalls) final;

    void uploadDrawData(std::span<float2 const> vertices,
                        std::span<uint32_t const> indices);
};

} // namespace

MacOSRenderer::MacOSRenderer(View* view, RendererOptions const& options):
    view(view), nativeView(transfer(view->nativeHandle())), options(options) {
    device = MTLCreateSystemDefaultDevice();
    commandQueue = [device newCommandQueue];
    metalLayer = [[CAMetalLayer alloc] init];
    metalLayer.device = device;
    metalLayer.pixelFormat = toMTL(options.pixelFormat);
    metalLayer.contentsScale = nativeView.window.backingScaleFactor;
    metalLayer.contentsGravity = kCAGravityTopLeft;
    metalLayer.opaque = NO;
    nativeView.layer = metalLayer;
    nativeView.wantsLayer = YES;
    createRenderingState();
}

namespace {

enum class FillModeType { FlatColor, Gradient };

struct UniformData {
    FillModeType fillMode;
    vml::float4 color, alternateColor;
    vml::float2 beginCoord, endCoord;
};

} // namespace

constexpr auto ShaderSource = R"(
#include <metal_stdlib>

using namespace metal;

enum FillMode { FlatColor, Gradient };

struct UniformData {
    FillMode fillMode;
    float4 color, alternateColor;
    float2 beginCoord, endCoord;
};

vertex float4 vertex_main(float2 device const* vertices [[buffer(0)]],
                          float4x4 device const& transform [[buffer(1)]],
                          UniformData device const& uniforms [[buffer(2)]],
                          uint vertexID [[vertex_id]]) {
    return transform * float4(vertices[vertexID], 0, 1);
}

fragment float4 fragment_main(float4 position [[position]], UniformData device const& uniforms [[buffer(0)]]) {
    switch (uniforms.fillMode) {
    case FlatColor:
        return uniforms.color;
    case Gradient: {
        float2 d = uniforms.endCoord - uniforms.beginCoord;
        float2 p = position.xy - uniforms.beginCoord;
        float t = dot(p, d) / dot(d, d);
        return mix(uniforms.color, uniforms.alternateColor, clamp(t, 0.0f, 1.0f));
    }
    }
    return {};
}
)";

void MacOSRenderer::createRenderingState() {
    NSError* error = nil;
    id<MTLLibrary> library = [device
        newLibraryWithSource:[NSString stringWithCString:ShaderSource
                                                encoding:NSUTF8StringEncoding]
                     options:nil
                       error:&error];
    if (error) {
        NSLog(@"Failed to compile shader: %@", error);
        return;
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
        toMTL(options.pixelFormat);
    pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                      error:&error];
    if (error) {
        NSLog(@"Failed to create pipeline: %@", error);
    }
}

id<MTLBuffer> __strong* MacOSRenderer::getUniformBuffer(size_t index) {
    if (index >= uniformBuffers.size()) {
        uniformBuffers.resize(std::max(index + 1, uniformBuffers.size() * 2));
    }
    return &uniformBuffers[index];
}

static void uploadData(id<MTLDevice> device, id<MTLBuffer> __strong* buffer,
                       void const* data, size_t size) {
    if ((!*buffer && size > 0) || (*buffer).length < size) {
        *buffer = [device newBufferWithBytes:data
                                      length:size
                                     options:MTLResourceStorageModeShared];
        return;
    }
    std::memcpy([*buffer contents], data, size);
}

static UniformData makeUniformData(DrawCallOptions const& opt) {
    // clang-format off
    return std::visit(csp::overload{
        [](FlatColor const& c) -> UniformData {
            return {
                .fillMode = FillModeType::FlatColor,
                .color = c.color
            };
        },
        [](Gradient const& g) -> UniformData {
            return {
                .fillMode = FillModeType::Gradient,
                .color = g.begin.color,
                .alternateColor = g.end.color,
                .beginCoord = g.begin.coord,
                .endCoord = g.end.coord
            };
        },
    }, opt.fill); // clang-format on
}

void MacOSRenderer::render(std::span<float2 const> vertices,
                           std::span<uint32_t const> indices,
                           std::span<DrawCall const> drawCalls) {
    if (!pipeline) return;
    CGSize newSize = nativeView.bounds.size;
    newSize.width *= metalLayer.contentsScale;
    newSize.height *= metalLayer.contentsScale;
    if (!CGSizeEqualToSize(metalLayer.drawableSize, newSize)) {
        metalLayer.drawableSize = newSize;
    }
    id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
    if (!drawable) return;
    uploadDrawData(vertices, indices);
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
    [encoder setCullMode:MTLCullModeNone];
    [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
    [encoder setVertexBuffer:transformMatrixBuffer offset:0 atIndex:1];
    for (size_t DCIndex = 0; auto& drawCall: drawCalls) {
        auto* uniformBuffer = getUniformBuffer(DCIndex);
        UniformData uniformData = makeUniformData(drawCall.options);
        uploadData(device, uniformBuffer, &uniformData, sizeof uniformData);
        [encoder setFragmentBuffer:*uniformBuffer offset:0 atIndex:0];
        [encoder setTriangleFillMode:drawCall.options.wireframe ?
                                         MTLTriangleFillModeLines :
                                         MTLTriangleFillModeFill];
        [encoder setVertexBufferOffset:drawCall.beginVertex * VertexSize
                               atIndex:0];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:drawCall.endIndex - drawCall.beginIndex
                             indexType:MTLIndexTypeUInt32
                           indexBuffer:indexBuffer
                     indexBufferOffset:drawCall.beginIndex * IndexSize];
        ++DCIndex;
    }
    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

void MacOSRenderer::uploadDrawData(std::span<float2 const> vertices,
                                   std::span<uint32_t const> indices) {
    uploadData(device, &vertexBuffer, vertices.data(),
               vertices.size() * VertexSize);
    uploadData(device, &indexBuffer, indices.data(),
               indices.size() * IndexSize);
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

std::unique_ptr<Renderer> xui::createRenderer(View* view,
                                              RendererOptions const& options) {
    return std::make_unique<MacOSRenderer>(view, options);
}

void View::setShadow(ShadowConfig config) {
    NSView* native = transfer(nativeHandle());
    (void)getDrawingContext();
    native.layer.shadowOpacity = config.shadowOpacity;
    native.layer.shadowColor = [toNSColor(config.shadowColor) CGColor];
    native.layer.shadowOffset = toNSSize(config.shadowOffset);
    native.layer.shadowRadius = config.shadowRadius;
    native.layer.masksToBounds = NO;
}
