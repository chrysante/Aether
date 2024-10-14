#ifndef AETHER_VIEWPROPERTIES_H
#define AETHER_VIEWPROPERTIES_H

namespace xui {

enum class AlignX { Left, Center, Right };
enum class AlignY { Top, Center, Bottom };

enum class SplitterStyle { Thin, Thick, Pane, Default = Thin };

enum class SplitViewResizeStrategy {
    Proportional,
    CutMin,
    CutLeft = CutMin,
    CutTop = CutMin,
    CutMax,
    CutRight = CutMax,
    CutBottom = CutMax,
    None,
    Default = Proportional
};

/// X-Macro definition of view attributes
#define AETHER_VIEW_ATTRIB_KEY(X)                                              \
    /* Name                 Type */                                            \
    X(SplitViewCollapsable, bool)                                              \
    X(PaddingX, double)                                                        \
    X(PaddingY, double)                                                        \
    X(AlignX, AlignX)                                                          \
    X(AlignY, AlignY)

enum class ViewAttributeKey {
#define X(Name, Type) Name,
    AETHER_VIEW_ATTRIB_KEY(X)
#undef X
};

namespace detail {

template <ViewAttributeKey>
struct ViewAttribKeyTypeImpl;
#define X(Name, Type)                                                          \
    template <>                                                                \
    struct ViewAttribKeyTypeImpl<ViewAttributeKey::Name>:                      \
        std::type_identity<Type> {};
AETHER_VIEW_ATTRIB_KEY(X)
#undef X

template <ViewAttributeKey Key>
using ViewAttribKeyType = typename ViewAttribKeyTypeImpl<Key>::type;

template <typename T>
struct ViewAttributeKeySetterImpl {
    consteval ViewAttributeKeySetterImpl(ViewAttributeKey key): value(key) {
        switch (key) {
#define X(Name, Type)                                                          \
    case ViewAttributeKey::Name:                                               \
        assert((std::constructible_from<                                       \
                std::optional<ViewAttribKeyType<ViewAttributeKey::Name>>,      \
                T&&>));                                                        \
        break;
            AETHER_VIEW_ATTRIB_KEY(X)
#undef X
        }
    }

    ViewAttributeKey value;
};

template <typename T>
using ViewAttributeKeySetter =
    ViewAttributeKeySetterImpl<std::type_identity_t<T>>;

template <typename T>
struct ViewAttributeKeyGetterImpl {
    consteval ViewAttributeKeyGetterImpl(ViewAttributeKey key): value(key) {
        switch (key) {
#define X(Name, Type)                                                          \
    case ViewAttributeKey::Name:                                               \
        assert((std::same_as<T, ViewAttribKeyType<ViewAttributeKey::Name>>));  \
        break;
            AETHER_VIEW_ATTRIB_KEY(X)
#undef X
        }
    }

    ViewAttributeKey value;
};

template <typename T>
using ViewAttributeKeyGetter =
    ViewAttributeKeyGetterImpl<std::type_identity_t<T>>;

} // namespace detail

} // namespace xui

#endif // AETHER_VIEWPROPERTIES_H
