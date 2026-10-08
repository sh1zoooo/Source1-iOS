#include "Runtime.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>
#ifdef __linux__
#include <execinfo.h>
#include <csignal>
#include <unistd.h>
#endif
int main(int argc,char** argv){

#ifdef __linux__
 auto crash=[](int){void* frames[40];int count=backtrace(frames,40);backtrace_symbols_fd(frames,count,2);_exit(139);};std::signal(SIGSEGV,crash);std::signal(SIGABRT,crash);
#endif
 if(argc!=3&&argc!=4)return 2;source1ios::Runtime r;if(!r.start(std::filesystem::absolute(argv[1])))return 3;
 if(argc==4){source1ios::SourcePlayer player;if(!player.start(argv[2]))return 10;for(int i=0;i<240;++i){r.frame(1./60);player.step();}player.stop();r.stop();std::cerr<<"SERVER PASS\n";return 0;}
 if(!r.startGame(argv[2])){std::cerr<<r.gameError()<<'\n';return 4;}
 for(int i=0;i<240;++i)r.frame(1./60);
 if(!r.playerState().alive)return 5;const auto start=r.playerState();r.cameraMove(1,0,0);
 for(int i=0;i<90;++i)r.frame(1./60);r.cameraMove(0,0,0);
 std::cerr<<"PLAYER "<<r.playerState().model<<" alive="<<r.playerState().alive<<" movement="<<std::hypot(r.playerState().origin[0]-start.origin[0],r.playerState().origin[1]-start.origin[1])<<"\n";
 auto mesh=r.vertices(1.8);if(mesh.empty())return 7;for(const auto& v:mesh)for(float f:v.position)if(!std::isfinite(f))return 8;
 r.stopGame();if(!r.startGame(argv[2]))return 9;for(int i=0;i<60;++i)r.frame(1./60);r.stopGame();r.stop();std::cerr<<"MAP PASS "<<argv[2]<<"\n";
}
