#include <cassert> 
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(std::ifstream& input, std::ofstream& output,
                const path& current_file, const vector<path>& include_directories) {
    const std::regex include_global(R"(\s*#\s*include\s*<([^>]*)>\s*)");
    const std::regex include_local(R"(\s*#\s*include\s*"([^"]*)\"\s*)");

    std::string line;
    int line_count = 0;

    while (std::getline(input, line)) {
        ++line_count;

        std::smatch match;
        if (std::regex_match(line, match, include_local)) {
      
            std::filesystem::path included_file = match[1].str();
            std::filesystem::path included_path = current_file.parent_path() / included_file;

            std::ifstream included_input(included_path);
            if (!included_input.is_open()) {
         
                bool found = false;
                for (const auto& dir : include_directories) {
                    included_path = dir / included_file;
                    included_input.open(included_path);
                    if (included_input.is_open()) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    std::cout << "unknown include file " << included_file.string()
                              << " at file " << current_file.string()
                              << " at line " << line_count << "\n";
                    return false;
                }
            }

            if (!Preprocess(included_input, output, included_path, include_directories)) {
                return false;
            }
            included_input.close();

        } else if (std::regex_match(line, match, include_global)) {
   
            std::filesystem::path included_file = match[1].str();
            std::ifstream included_input;
            std::filesystem::path included_path;
            bool found = false;

            for (const auto& dir : include_directories) {
                included_path = dir / included_file;
                included_input.open(included_path);
                if (included_input.is_open()) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::cout << "unknown include file " << included_file.string()
                          << " at file " << current_file.string()
                          << " at line " << line_count << "\n";
                return false;
            }

            if (!Preprocess(included_input, output, included_path, include_directories)) {
                return false;
            }
            included_input.close();

        } else {

            output << line << '\n';
        }
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    std::ifstream input(in_file);
    if(!input) {
        return false;
    }
    std::ofstream output(out_file);
    if(!output) {
        return false;
    }
    return Preprocess(input, output, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                        {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
