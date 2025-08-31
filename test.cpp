#include "build.hpp"
#include <iostream>

int main() {
    build::file_object("a");
    auto build_hpp =
        std::make_shared<build::file_object>(
                "build.hpp"
        );
    auto test_cpp = std::make_shared<build::file_object>("test.cpp");
    
    auto test = std::make_shared<build::file_object>(
            "test",
            std::vector<std::weak_ptr<build::file_object>>{build_hpp, test_cpp},
            [](auto){
                std::cout << "building" << std::endl;
                auto command = "c++ test.cpp -o test -std=c++20 -ltbb";
                int res = system(command);
                std::cout << command << std::endl;
                return res == 0? build::update_res::success : build::update_res::failed;
            }
            );

    auto graph = build::dependency_graph{};
    graph.add(test);
    graph.add(test_cpp);
    graph.add(build_hpp);

    graph.update();
    return 0;
}
