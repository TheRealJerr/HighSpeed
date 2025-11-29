#ifndef TOOL_HPP
#define TOOL_HPP
// 定义常见的工具包
#include <fstream>
#include <sstream>

namespace hspd
{

    // 文件的工具类
    class FileTool
    {
    public:
        // 读取文件内容
        static std::string read_from_file(const std::string& file_path);

        // 写入文件内容
        static void write_to_file(const std::string& file_path, const std::string& content);
    };






    std::string FileTool::read_from_file(const std::string& file_path)
    {
        std::ifstream ifs(file_path);
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        return buffer.str();
    }

    void FileTool::write_to_file(const std::string& file_path, const std::string& content)
    {
        std::ofstream ofs(file_path);
        ofs << content;
    }


}



#endif // TOOL_HPP