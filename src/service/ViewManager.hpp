#ifndef SRC_SERVICE_VIEW_MANAGER_HPP
#define SRC_SERVICE_VIEW_MANAGER_HPP

// 可视化视图管理器：管理"互斥视图类型"与"数据源绑定"。
// 用户需求要点：
//   - 可视化与协议解析分离：本类只关心"用哪个视图、绑哪些数据源"；
//   - 视图类型互斥（波形 / 3D 姿态 等只能同时激活一个视图类型）；
//   - 一个视图类型可绑定多个数据源字段，可多实例、可多源合一渲染；
//   - 数据源列表来自 FieldPool（读协议字段）。
//
// 纯 C++、无 Qt 依赖。

#include "../core/FieldSchema.hpp"
#include "FieldPool.hpp"

#include <string>
#include <vector>

namespace sd {

// 视图类型（互斥）。后续可扩展（如 3D 姿态）。
enum class ViewType {
    None,   // 未配置
    Wave,   // 波形图
    Table,  // 表格（协议解析结果表）
    // Future: Pose3D, Spectrum ...
};

// 视图类型 -> 显示名（供 UI 下拉/按钮）。
inline const char* viewTypeName(ViewType t) {
    switch (t) {
        case ViewType::None:  return "未配置";
        case ViewType::Wave:  return "波形图";
        case ViewType::Table: return "数据表格";
    }
    return "未知";
}

// 一个视图实例：绑定若干数据源字段。
struct ViewBinding {
    std::string            viewId;   // 实例标识（如 "wave-1"）
    std::vector<std::string> fields; // 绑定字段名（数据源）
    std::string            title;    // 显示标题
};

// 视图管理器：维护当前选中的视图类型与各实例的绑定。
class ViewManager {
public:
    // 是否已配置视图（非 None）。
    bool configured() const { return type_ != ViewType::None; }

    // 当前视图类型。
    ViewType type() const { return type_; }

    // 设置视图类型（互斥切换；会清空已有实例绑定）。
    void setType(ViewType t);

    // 添加一个视图实例（绑定若干字段，来自 FieldPool）。
    // 返回实例 id；字段为空则忽略。
    std::string addView(const std::vector<std::string>& fields, const std::string& title = {});

    // 移除实例。
    bool removeView(const std::string& viewId);

    // 全部实例。
    const std::vector<ViewBinding>& views() const { return views_; }

    // 按 id 查实例；不存在返回 nullptr。
    const ViewBinding* findView(const std::string& viewId) const;

    // 校验：字段是否都存在于 FieldPool（切换协议后可能失效）。
    // 返回失效字段名列表；空表示全部有效。
    std::vector<std::string> validateFields(const FieldPool& pool) const;

    // 清空。
    void clear();

private:
    ViewType                type_ = ViewType::None;
    std::vector<ViewBinding> views_;
    int                     seq_ = 0;
};

} // namespace sd

#endif // SRC_SERVICE_VIEW_MANAGER_HPP