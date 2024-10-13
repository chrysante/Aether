#ifndef AETHER_VIEWPROPERTIES_H
#define AETHER_VIEWPROPERTIES_H

namespace xui {

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

namespace detail {

enum class ViewAttributeKey {
    SplitViewCollapsable, // bool
};

}

} // namespace xui

#endif // AETHER_VIEWPROPERTIES_H
