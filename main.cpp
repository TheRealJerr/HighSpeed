#include <iostream>
#include <tools/Strand.hpp>
using namespace hspd;

int main() {
    // 全局的gThreadPool;
    gThreadPool->run();
    Strand strand = makeStrand(gThreadPool.get());

    for(int i = 0;i < 100;i++)
    {
        strand.addTask([i](){
            std::cout << "这是任务:" << i << std::endl;
        });
    }

    std::cout << "主线程等待" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}