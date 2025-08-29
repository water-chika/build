#pragma once

#include <unordered_map>
#include <memory>
#include <functional>
#include <filesystem>
#include <iostream>

namespace build {

uint64_t hash_file(const std::filesystem::path& file) {
    if (exists(file)) {
        auto t = last_write_time(file);
        auto duration = t.time_since_epoch();
        return duration.count();
    }
    else {
        return 0;
    }
}

class file_object {
public:
    file_object(std::filesystem::path file,
            std::vector<std::weak_ptr<file_object>> dependencies = {},
            std::function<void()> update_action = {})
        : m_file{file},
        m_dependencies{dependencies},
        m_update_action{std::move(update_action)},
        m_dependencies_hash(dependencies.size()),
        m_hash{hash_file(file)}
    {
        update_dependencies_hash();
    }
    auto hash() const {
        return m_hash;
    }
    bool need_update() const {
        for (uint32_t i = 0; i < m_dependencies.size(); i++) {
            auto& dependency = m_dependencies[i];
            auto& dependency_prev_hash = m_dependencies_hash[i];
            if (auto shared_obj = dependency.lock()) {
                if (last_write_time(shared_obj->m_file) >= last_write_time(m_file)) {
                    return true;
                }
            }
        }

        return false;
    }
    void update() {
        m_update_action();
        update_dependencies_hash();
    }
    void add_dependency(std::weak_ptr<file_object> dependency) {
        m_dependencies.push_back(dependency);
        m_dependencies_hash.push_back({});
    }

private:
    void update_dependencies_hash() {
        for (uint32_t i = 0; i < m_dependencies.size(); i++) {
            auto& dependency = m_dependencies[i];
            auto& dependency_prev_hash = m_dependencies_hash[i];
            if (auto shared_obj = dependency.lock()) {
                dependency_prev_hash = shared_obj->hash();
            }
        }
    }
    std::filesystem::path m_file;
    std::vector<std::weak_ptr<file_object>> m_dependencies;
    std::function<void()> m_update_action;

    std::vector<uint64_t> m_dependencies_hash;

    uint64_t m_hash;
};

template<typename Object=file_object, typename Action = std::function<void()>>
class dependency_graph {
public:
    void add(std::shared_ptr<file_object> obj) {
        m_objects.push_back(obj);
    }
    void update() {
        bool is_updated = false;
        do {
            is_updated = false;
            for (auto object : m_objects) {
                if (object->need_update()) {
                    is_updated = true;
                    object->update();
                }
            }
        } while (is_updated);
    }
private:
    std::vector<std::shared_ptr<file_object>> m_objects;
};

}
