#pragma once
#include "PlayerState.hpp"
#include <string>
namespace source1ios {
enum PlayerButton : unsigned { PlayerJump=1, PlayerDuck=2, PlayerAttack=4, PlayerReload=8 };
class SourcePlayer final {
public:
    bool start(const std::string& map);
    void stop();
    void step();
    void move(float forward,float right);
    void look(float yaw,float pitch);
    void button(unsigned flag,bool pressed);
    bool active() const { return state_.active; }
    const PlayerState& state() const { return state_; }
    float tickInterval() const;
    const std::string& error() const { return error_; }
private:
    void* entity_=nullptr;
    void* controller_=nullptr;
    bool ownsLevel_=false;
    int spawnCount_=0,lastTick_=-1,command_=0;
    float forward_=0,right_=0,yaw_=0,pitch_=0;
    unsigned buttons_=0;
    PlayerState state_{};
    std::string error_;
};
}
