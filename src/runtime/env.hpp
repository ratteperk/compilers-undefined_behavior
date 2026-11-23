#pragma once

#include "value.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class Env {
public:
    explicit Env(std::shared_ptr<Env> parent = nullptr, bool isFunctionBoundary = false)
        : parent(std::move(parent)), isFunctionBoundary(isFunctionBoundary) {}

    const Value* lookup(const std::string& name) const {
        for (const Env* cur = this; cur != nullptr; cur = cur->parent.get()) {
            auto it = cur->vars.find(name);
            if (it != cur->vars.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

    // Создаёт/перезаписывает строго в текущем фрейме (параметры, заголовок prog, func, builtins)
    void setLocal(const std::string& name, Value val) { vars[name] = std::move(val); }

    // Для setq: обновляет переменную в пределах текущей функции, не пробивая границу func/lambda
    void set(const std::string& name, Value val) {
        for (Env* cur = this; cur != nullptr; cur = cur->parent.get()) {
            if (cur->vars.find(name) != cur->vars.end()) {
                cur->vars[name] = std::move(val);
                return;
            }
            if (cur->isFunctionBoundary) {
                break;
            }
        }
        vars[name] = std::move(val);
    }

    void clear() { vars.clear(); }

private:
    std::unordered_map<std::string, Value> vars;
    std::shared_ptr<Env> parent;
    bool isFunctionBoundary;
};