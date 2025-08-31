#pragma once

#include <unordered_map>
#include <memory>
#include <functional>
#include <filesystem>
#include <iostream>
#include <execution>

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

enum class update_res : uint32_t {
    success = 0,
    failed = 1,
};

class file_object {
public:
    file_object(std::filesystem::path file,
            std::vector<std::weak_ptr<file_object>> dependencies = {},
            std::function<update_res(const file_object&)> update_action = {})
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
        if (!exists(m_file)) {
            return true;
        }
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
    update_res update() {
        auto res = m_update_action(*this);
        update_dependencies_hash();
        return res;
    }
    void add_dependency(std::weak_ptr<file_object> dependency) {
        m_dependencies.push_back(dependency);
        m_dependencies_hash.push_back({});
    }

    auto& get_file_path() const {
        return m_file;
    }
    auto& get_dependencies() const {
        return m_dependencies;
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
    std::function<update_res(const file_object&)> m_update_action;

    std::vector<uint64_t> m_dependencies_hash;

    uint64_t m_hash;
};

template<typename Object=file_object, typename Action = std::function<update_res(const file_object&)>>
class dependency_graph {
public:
    void add(std::shared_ptr<file_object> obj) {
        m_objects.push_back(obj);
    }
    update_res update() {
        bool is_updated = false;
        bool is_success = true;
        do {
            is_updated = false;
            std::for_each(
                std::execution::par,
                m_objects.begin(), m_objects.end(),
                [&is_updated, &is_success](auto& object) {
                    if (object->need_update()) {
                        is_updated = true;
                        auto res = object->update();
                        if (res != update_res::success) {
                            is_success = false;
                        }
                    }
                });
        } while (is_updated && is_success);
        return is_success ? update_res::success : update_res::failed;
    }
private:
    std::vector<std::shared_ptr<file_object>> m_objects;
};

auto dependencies(auto... args) {
    return std::vector<std::weak_ptr<build::file_object>>{args...};
}

auto file(std::filesystem::path path, std::vector<std::weak_ptr<build::file_object>> dependencies = {}, std::function<update_res(const file_object&)> action = {}) {
    return std::make_shared<build::file_object>(
            path,
            dependencies,
            action
    );
}

}
