#include "game_state.hpp"
#include <bits/stdc++.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <random>

using namespace std;

const std::vector<std::string> _actions = {"up", "left", "right", "down", "bomb", "detonate"};

class Agent {
private:
  static std::string my_agent_id;
  static std::vector<std::string> my_units;
  static std::string their_agent_id;
  static std::vector<std::string> their_units;

  static int tick;

  static void on_game_tick(int tick, const json& game_state);

  static int metal_ore[225];
  static int wood[225];
  static int unit_present[225];
  static int distances[225][3];
  static int parent[225][3];
  static bool visited[225][3];
  static std::map<std::string,int> unit_string_num;

public:
  static int last_spiral_not_occupied(int bl);
  static bool che(int r,int c);
  static void fill_metal_wood(const json& game_state);
  static int dist(int bl,int bl1);
  static void update_distances_parents(int unit_id, int bl);
  static void run();
};

std::string Agent::my_agent_id;
std::vector<std::string> Agent::my_units;
std::string Agent::their_agent_id;
std::vector<std::string> Agent::their_units;

int Agent::tick;
int Agent::wood[225];
int Agent::metal_ore[225];
int Agent::distances[225][3];
int Agent::unit_present[225];
int Agent::parent[225][3];
bool Agent::visited[225][3];
std::map<std::string,int> Agent::unit_string_num;


int Agent::last_spiral_not_occupied(int bl){
  // Check this function somewhat correct
  vector<int> spiral;
  int curr = 0;
  while(curr<8){
    for(int i=curr;i<15-curr;i++){
      spiral.push_back(curr*15+i);
      spiral.push_back((15-curr)*15+15-i);
    }
    for(int i=curr;i<15-curr;i++){
      spiral.push_back((15-i)*15+15-curr);
      spiral.push_back((i)*15+curr);
    }
    curr++;
  }
  for(int i = spiral.size()-1;i>=0;i--){
    if(spiral[i]==bl){
      return bl;
    }
    if(!metal_ore[spiral[i]] && !wood[spiral[i]] && !unit_present[spiral[i]]){
      return spiral[i];
    }
  }
  return 15*7+7;
}

bool Agent::che(int r, int c){
  return (r>=0 && c>=0 && r<=14 && c<=14);
}


void Agent::fill_metal_wood(const json& game_state){
  for(int i=0;i<225;i++){
    metal_ore[i]=0;
    wood[i]=0;
  }
  const json& entities = game_state["entities"];
      for (const auto& entity: entities) 
      {
        if (entity["type"] == "m" || entity["type"] == "o") 
        {
          int x = entity["x"], y = entity["y"];
          metal_ore[x*15+y] = 1;
        } else if(entity["type"] == "w"){
          int x = entity["x"], y = entity["y"];
          wood[x*15+y] = 1;
        }
      }
}


int Agent::dist(int bl1,int bl2){
  if(bl1==bl2){
    return 0;
  }
  if(metal_ore[bl2] || unit_present[bl2]){
    return 1e8;
  } else if(wood[bl2]){
    return 6;
  } else {
    return 1;
  }
}


void Agent::update_distances_parents(int unit_id, int bl)
{
  for(int i=0; i<225; i++){
    distances[i][unit_id]=1e8;
    visited[i][unit_id]=0;
    parent[i][unit_id]=-1;
  }
  priority_queue<pair<int, int>> pq;
  distances[bl][unit_id]=0;
  pq.push({distances[bl][unit_id], bl});
  
  vector<pair<int, int>> ne;

  ne.push_back({1, 0});
  ne.push_back({-1, 0});
  ne.push_back({0, 1});
  ne.push_back({0, -1});

  while(!pq.empty()){
    auto u=pq.top();
    pq.pop();
    if(visited[u.second][unit_id]){
      continue;
    }
    visited[u.second][unit_id]=1;
    int r=(u.second)/15;
    int c=(u.second)%15;
    for(auto u:ne){
      if(che(r+u.first, c+u.second) && distances[15*r+c][unit_id]+dist(15*r+c,(15*(r+u.first)+(c+u.second)))<=distances[(15*(r+u.first)+(c+u.second))][unit_id]){
        distances[(15*(r+u.first)+(c+u.second))][unit_id]=distances[15*r+c][unit_id]+dist(15*r+c,(15*(r+u.first)+(c+u.second)));
        parent[(15*(r+u.first)+(c+u.second))][unit_id]=15*r+c;
        pq.push({-1*distances[(15*(r+u.first)+(c+u.second))][unit_id], (15*(r+u.first)+(c+u.second))});
      }
    }
  }
}

void Agent::run() 
{
  const char* connection_string = getenv ("GAME_CONNECTION_STRING");

  if (connection_string == NULL) 
  {
    connection_string = "ws://127.0.0.1:3000/?role=agent&agentId=agentId&name=defaultName";
  }
  std::cout << "The current connection_string is: " << connection_string << std::endl;

  GameState::connect(connection_string); 
  GameState::set_game_tick_callback(on_game_tick);
  GameState::handle_messages();
}

void Agent::on_game_tick(int tick_nr, const json& game_state) 
{
  DEBUG("*************** on_game_tick");
  DEBUG(game_state.dump());
  TEST(tick_nr, game_state.dump());
  
  tick = tick_nr;
  std::cout << "Tick #" << tick << std::endl;
  if (tick == 1)
  {
    my_agent_id = game_state["connection"]["agent_id"];
    their_agent_id = my_agent_id=="a"?"b":"a";
    my_units = game_state["agents"][my_agent_id]["unit_ids"].get<std::vector<std::string>>();
    their_units = game_state["agents"][their_agent_id]["unit_ids"].get<std::vector<std::string>>();
    int k=0;
    for (const auto& unit_id: my_units)
    {
      unit_string_num[unit_id] = k;
      k++;
    }
  }

  srand(1234567 * (my_agent_id == "a" ? 1 : 0) + tick * 100 + 13);

  fill_metal_wood(game_state);

  for(int i=0;i<225;i++){
    unit_present[i]=0;
  }

  for (const auto& unit_id: my_units)
  {
     const json& unit = game_state["unit_state"][unit_id];
     unit_present[(int)unit["coordinates"][0]*15+(int)unit["coordinates"][1]] = 1;
  }

  for (const auto& unit_id: their_units)
  {
     const json& unit = game_state["unit_state"][unit_id];
     unit_present[(int)unit["coordinates"][0]*15+(int)unit["coordinates"][1]] = 1;
  }

  // send each (alive) unit a random action
  for (const auto& unit_id: my_units)
  {
    const json& unit = game_state["unit_state"][unit_id];
    if (unit["hp"] <= 0) continue;
    int x = unit["coordinates"][0];
    int y = unit["coordinates"][1];
    update_distances_parents(unit_string_num[unit_id],15*x+y);
    int id = unit_string_num[unit_id];

    int curr = last_spiral_not_occupied(15*x+y);
    cout<<curr<<'\n';
    parent[15*x+y][id]=15*x+y;
    while(parent[curr][id]!=15*x+y && parent[curr][id]!=-1){
      curr = parent[curr][id];
    }
    int row = curr/15;
    int col = curr%15;

    std::string action = "right";
    if(row==x-1 && col==y){
      action = "left";
    } else if(row==x+1 && col==y){
      action = "right";
    } else if(row==x && col==y-1){
      action = "down";
    } else if(row==x && col==y+1){
      action = "up";
    }
    std::cout << "action: " << unit_id << " -> " << action << std::endl;

    if (action == "up" || action == "left" || action == "right" || action == "down")
    {
      GameState::send_move(action, unit_id);
    }
    else if (action == "bomb")
    {
      GameState::send_bomb(unit_id);
    }
    else if (action == "detonate")
    {
      const json& entities = game_state["entities"];
      for (const auto& entity: entities) 
      {
        if (entity["type"] == "b" && entity["unit_id"] == unit_id) 
        {
          int x = entity["x"], y = entity["y"];
          GameState::send_detonate(x, y, unit_id);
          break;
        }
      }
    }
    else 
    {
      std::cerr << "Unhandled action: " << action << " for unit " << unit_id << std::endl;
    }
  }
}

int main()
{
  Agent::run();
}

