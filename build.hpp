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
    void add_dependency(std::vector<std::weak_ptr<file_object>> dependencies) {
        for (auto dependency : dependencies) {
            add_dependency(dependency);
        }
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

auto c_plus_plus_compile(const std::vector<std::string>& src, const std::string& output_path) {
    std::string sources = "";
    std::ranges::for_each(
            src,
            [&sources](auto s) {
                sources += " " + s;
            }
            );
    auto command = std::format("c++ {} -o {} -std=c++23 -ltbb", sources, output_path);
    std::cout << command << std::endl;
    return system(command.c_str());
}
auto c_plus_plus_compile(const std::string src, const std::string& output_path) {
    return c_plus_plus_compile(std::vector<std::string>{src}, output_path);
}
auto amdclang_plus_plus_compile(const std::vector<std::string>& src, const std::string& output_path) {
    std::string sources = "";
    std::ranges::for_each(
            src,
            [&sources](auto s) {
                sources += " " + s;
            }
            );
    auto command = std::format("amdclang++ -x hip --offload-arch=gfx1201 {} -o {} -std=c++23 -ltbb", sources, output_path);
    std::cout << command << std::endl;
    return system(command.c_str());
}

struct cpp_file_compile_action {
static auto operator()(const auto& target){
    auto weak_sources = target.get_dependencies();
    auto sources = std::vector<std::shared_ptr<build::file_object>>(weak_sources.size());
    std::ranges::transform(
            weak_sources,
            sources.begin(),
            [](auto e) {
                return e.lock();
            }
            );
    auto sources_string = std::vector<std::string>(sources.size());
    std::ranges::transform(
            sources,
            sources_string.begin(),
            [](auto& src) {
                return src->get_file_path().string();
            }
            );
    auto res = c_plus_plus_compile(sources_string, target.get_file_path());
    return res == 0 ? build::update_res::success : build::update_res::failed;
}
};
struct hip_file_compile_action {
static auto operator()(const auto& target){
    auto weak_sources = target.get_dependencies();
    auto sources = std::vector<std::shared_ptr<build::file_object>>(weak_sources.size());
    std::ranges::transform(
            weak_sources,
            sources.begin(),
            [](auto e) {
                return e.lock();
            }
            );
    auto sources_string = std::vector<std::string>(sources.size());
    std::ranges::transform(
            sources,
            sources_string.begin(),
            [](auto& src) {
                return src->get_file_path().string();
            }
            );
    auto res = amdclang_plus_plus_compile(sources_string, target.get_file_path());
    return res == 0 ? build::update_res::success : build::update_res::failed;
}
};

class builder {
public:
    void add_executable_help(std::string name, auto compile_action, std::convertible_to<std::string> auto... sources) {
        auto sources_set = std::vector{sources...};
        auto sources_refs = std::vector<std::weak_ptr<build::file_object>>(sources_set.size());
        for (uint32_t i = 0; i < sources_set.size(); i++) {
            auto& source = sources_set[i];
            if (!m_name_map.contains(source)) {
                m_name_map[source] = build::file(source);
            }
            sources_refs[i] = m_name_map[source];
        }
        if (m_name_map.contains(name)) {
            auto& exe = m_name_map[name];
            exe->add_dependency(sources_refs);
        }
        else {
            auto exe = build::file(name, build::dependencies(sources_refs),
                    std::move(compile_action)
                    );
            m_name_map[name] = exe;
            m_graph.add(exe);
        }
    }
    void add_executable(std::string name, std::convertible_to<std::string> auto... sources) {
        add_executable_help(name, build::cpp_file_compile_action{}, sources...);
    }
    void add_hip_executable(std::string name, std::convertible_to<std::string> auto... sources) {
        add_executable_help(name, build::hip_file_compile_action{}, sources...);
    }
    auto build() {
        return m_graph.update();
    }
private:
    std::unordered_map<std::string, std::shared_ptr<build::file_object>> m_name_map;
    build::dependency_graph<> m_graph;
};

}
