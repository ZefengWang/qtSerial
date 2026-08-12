#include "ViewManager.hpp"

#include <utility>

namespace sd {

void ViewManager::setType(ViewType t) {
    if (type_ == t) return;
    type_ = t;
    views_.clear(); // 互斥切换：清空旧类型实例
    seq_ = 0;
}

std::string ViewManager::addView(const std::vector<std::string>& fields, const std::string& title) {
    if (fields.empty() || type_ == ViewType::None) return {};
    ViewBinding vb;
    vb.viewId = "v" + std::to_string(++seq_);
    vb.fields = fields;
    vb.title  = title.empty()
        ? std::string(viewTypeName(type_)) + "-" + std::to_string(seq_)
        : title;
    std::string id = vb.viewId;
    views_.push_back(std::move(vb));
    return id;
}

bool ViewManager::removeView(const std::string& viewId) {
    for (auto it = views_.begin(); it != views_.end(); ++it) {
        if (it->viewId == viewId) {
            views_.erase(it);
            return true;
        }
    }
    return false;
}

const ViewBinding* ViewManager::findView(const std::string& viewId) const {
    for (const auto& v : views_) {
        if (v.viewId == viewId) return &v;
    }
    return nullptr;
}

std::vector<std::string> ViewManager::validateFields(const FieldPool& pool) const {
    std::vector<std::string> invalid;
    for (const auto& v : views_) {
        for (const auto& f : v.fields) {
            if (!pool.findSource(f)) invalid.push_back(f);
        }
    }
    return invalid;
}

void ViewManager::clear() {
    type_ = ViewType::None;
    views_.clear();
    seq_ = 0;
}

} // namespace sd