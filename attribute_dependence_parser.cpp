#include <iostream>
#include <map>
#include <string>
#include <algorithm>
#include <ranges>

void generate_init_all_func(std::multimap<std::string, std::string> dependences, std::map<std::string, std::string> init_funcs);

std::multimap<std::string, std::string> dependences{};
std::map<std::string, std::string> init_funcs{};
void add_depend(const char* a, const char* b) {
	dependences.emplace(a,b);
}
void add_init(const char* a, const char* b) {
	init_funcs.emplace(a,b);
}
void yyparse();

int main() {
	yyparse();
    generate_init_all_func(dependences, init_funcs);
    return 0;
}


void generate_init_all_func(std::multimap<std::string, std::string> dependences, std::map<std::string, std::string> init_funcs) {
    std::map<std::string, bool> are_inited{};
    for (auto& [k, v] : init_funcs) {
        are_inited.emplace(k, false);
    }
    for (auto& [k, v] : dependences) {
	    are_inited.emplace(k, false);
	    are_inited.emplace(v, false);
    }
    while (
        std::ranges::any_of(
            are_inited,
            [](bool is_inited) {return false == is_inited; },
            [](auto& k_v) { return k_v.second; })
        ) {
        for (auto& [k, v] : are_inited) {
            if (false == are_inited[k]) {
                if (auto [b, e] = dependences.equal_range(k);
				std::all_of(b, e,
					[&are_inited](auto& k_v){
					return are_inited[k_v.second];
					})) {
		    if (init_funcs.contains(k)) {
                        std::cout << init_funcs[k] << std::endl;
		    }
                    are_inited[k] = true;
                }
            }
        }
    }
}
