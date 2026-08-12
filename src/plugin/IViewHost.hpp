#ifndef SRC_PLUGIN_IVIEW_HOST_HPP
#define SRC_PLUGIN_IVIEW_HOST_HPP

// 视图宿主抽象：跨技术栈的统一视图接口。
// 因核心层不依赖 Qt，这里用前置声明 + 不完整类型。
// 具体实现（Widgets/QML）在 UI 层提供，见 src/ui/ViewHost.hpp。
struct QWidget;
struct QQuickItem;

namespace sd {

// 统一视图宿主：让同一插件在 Widgets 壳与 QML 壳下都能工作。
class IViewHost {
public:
    virtual ~IViewHost() = default;

    // 嵌入 Widgets 壳（返回真实 QWidget*，核心层不解引用）。
    virtual QWidget*   asWidget()  = 0;
    // 嵌入 QML 壳（返回真实 QQuickItem*）。
    virtual QQuickItem* asQmlItem() = 0;
};

} // namespace sd

#endif // SRC_PLUGIN_IVIEW_HOST_HPP