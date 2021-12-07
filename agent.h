/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
 *
 * Author: Theory of Computer Games (TCG 2021)
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"
#include <fstream>
#include <queue>

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * random player for both side
 * put a legal piece randomly
 */
class random_player : public random_agent {
public:
	random_player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) {
		std::shuffle(space.begin(), space.end(), engine);
		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal)
				return move;
		}
		return action();
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
};


class mtcs_with_sample_rave_player : public random_agent {
public:
	mtcs_with_sample_rave_player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y),space_opponent(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black"){
			who = board::black;
			opponent = board::white;
		}
		if (role() == "white"){
			who = board::white;
			opponent = board::black;
		}
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++){
			space[i] = action::place(i, who);
			space1.push_back(action::place(i,who));
			space_opponent[i] = action::place(i, opponent);
			space_opponent1.push_back(action::place(i,opponent));
		}
	}

	static int myrandom (int i) { return std::rand()%i;}

	bool simulation(const board& now,board::piece_type a){
		bool ch=true;
		board next=now;
		int stat;
		if(a==who) stat=1;
		else stat=0;
		std::srand(time(0));
		std::random_shuffle(space1.begin(),space1.end(),myrandom);
		std::random_shuffle(space_opponent1.begin(),space_opponent1.end(),myrandom);
		
		std::vector<action::place> rem;

		for(int i=1;i<=74;i++){
			if(i%2){
				if(stat){
					for (const action::place& move : space1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							rem.push_back(move);
							node_state[move].second++;
							next=ne;
							goto L_nextround;
						}
					}
					ch=false;
					break;
				}
				else{
					for (const action::place& move : space_opponent1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							next=ne;
							goto L_nextround;
						}
					}
					ch=true;
					break;
				}
			}
			else{
				if(stat){
					for (const action::place& move : space_opponent1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							next=ne;
							goto L_nextround;
						}
					}
					ch=true;
					break;
				}
				else{
					for (const action::place& move : space1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							rem.push_back(move);
							node_state[move].second++;
							next=ne;
							goto L_nextround;
						}
					}
					ch=false;
					break;
				}
			}
			L_nextround:;
		}
		if(ch) for(auto it:rem) node_state[it].first++;
		return ch;
	}

	virtual action take_action(const board& state) {
		std::shuffle(space.begin(), space.end(), engine);
		std::shuffle(space_opponent.begin(),space_opponent.end(),engine);
		node_state.clear();
		for(auto it:space) node_state[it]=std::make_pair(0,0);

		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal){
				for(int i=0;i<30;i++) {
					node_state[move].second++;
					if(simulation(after,opponent)) node_state[move].first++;
				}
			}
		}
		action::place best_move;
		float best_win_rate=0;
		for(auto it:node_state){
			if(it.second.second!=0&&(float)it.second.first/it.second.second>best_win_rate){
				best_move=it.first;
				best_win_rate=(float)it.second.first/it.second.second;
			}
		}
		return best_move;
	}

private:
	std::vector<action::place> space;
	std::vector<action::place> space_opponent;
	std::vector<action::place> space1;
	std::vector<action::place> space_opponent1;
	board::piece_type who;
	board::piece_type opponent;
	std::map<action::place,std::pair<int,int>> node_state;
};



class mtcs_uct_rave_player : public random_agent {
public:
	mtcs_uct_rave_player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y),space_opponent(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black"){
			who = board::black;
			opponent = board::white;
		}
		if (role() == "white"){
			who = board::white;
			opponent = board::black;
		}
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++){
			space[i] = action::place(i, who);
			space1.push_back(action::place(i,who));
			space_opponent[i] = action::place(i, opponent);
			space_opponent1.push_back(action::place(i,opponent));
		}
	}

	static int myrandom (int i) { return std::rand()%i;}

	bool simulation(const board& now,board::piece_type a){
		bool ch=true;
		board next=now;
		int stat;
		if(a==who) stat=1;
		else stat=0;
		std::srand(time(0));
		std::random_shuffle(space1.begin(),space1.end(),myrandom);
		std::random_shuffle(space_opponent1.begin(),space_opponent1.end(),myrandom);
		
		std::vector<action::place> rem;

		for(int i=1;i<=74;i++){
			if(i%2){
				if(stat){
					for (const action::place& move : space1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							rem.push_back(move);
							node_state[move].second++;
							next=ne;
							goto L_nextround;
						}
					}
					ch=false;
					break;
				}
				else{
					for (const action::place& move : space_opponent1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							next=ne;
							goto L_nextround;
						}
					}
					ch=true;
					break;
				}
			}
			else{
				if(stat){
					for (const action::place& move : space_opponent1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							next=ne;
							goto L_nextround;
						}
					}
					ch=true;
					break;
				}
				else{
					for (const action::place& move : space1) {
						board ne=next;
						if (move.apply(ne) == board::legal){
							rem.push_back(move);
							node_state[move].second++;
							next=ne;
							goto L_nextround;
						}
					}
					ch=false;
					break;
				}
			}
			L_nextround:;
		}
		
		if(ch) for(auto it:rem) node_state[it].first++;
		
		return ch;
	}

	struct tree_node{
		tree_node* next[100]={nullptr};
		int win_cnt=0;
		int game_cnt=0;
		board b;
		board::piece_type w;
		action::place pos;
		tree_node(board b,board::piece_type w):b(b),w(w){}
		tree_node(board b,board::piece_type w,action::place pos):b(b),w(w),pos(pos){}
		bool is_leaf(){
			bool t=true;
			for(int i=0;i<100;i++){
				if(next[i]) t=false; 
			}
			return t;
		}
	};

	void init(const board& state,board::piece_type w){
		std::queue<tree_node *> q;
		if(root) q.push(root);
		while(q.size()!=0){
			tree_node* now=q.front();
			q.pop();
			for(int i=0;i<100;i++) if(now->next[i]) q.push(now->next[i]);
			free(now);
		}
		root=new tree_node(state,w);
	}

	void update(){
		std::queue<tree_node *> q;
		tree_node *now=root;
		bool not_end=true;
		if(now){
			// find leaf
			q.push(now);
			while(!now->is_leaf()){
				float score=0;
				float max_score=0;
				int max_ind=0;
				not_end=false;
				for(int i=0;i<100;i++){
					if(now->next[i]){
						board t=now->b;
						if(now->next[i]->pos.apply(t)==board::legal){
							not_end=true;
							//std::cout << now->next[i]->pos << '\n';
							if(now->next[i]->game_cnt==0) score=100000;
							else score= (float)now->next[i]->win_cnt/now->next[i]->game_cnt + sqrt(log(now->game_cnt)/now->next[i]->game_cnt);
							//std::cout << i << " " << score << '\n';
							if(score>max_score){
								//std::cout << i << " " << score << " " << max_score << '\n';
								max_score=score;
								max_ind=i;
							}
						}
					}
				}
				if(not_end){
					now=now->next[max_ind];
					q.push(now);
				}
				else break;
			}

			// expension
			if(not_end){
				if(now->w==who){
					int i=0;
					for(auto it:space){
						board b_now=now->b;
						it.apply(b_now);
						now->next[i]=new tree_node(b_now,opponent,it);
						i++;
					}
				}
				else{
					int i=0;
					for(auto it:space_opponent){
						board b_now=now->b;
						it.apply(b_now);
						now->next[i]=new tree_node(b_now,who,it);
						i++;
					}
				}
			}

			// simulation
			bool win=false;
			if(not_end){
				for(int i=0;i<100;i++){
					board t=now->b;
					if(now->next[i]){
						if(now->next[i]->pos.apply(t)==board::legal){
							now=now->next[i];
							q.push(now);
							win=simulation(now->b,now->w);
							break;
						}
					}
				}
			}
			else{
				q.push(now);
				win=simulation(now->b,now->w);
			}
			// propagation back
			while(q.size()!=0){
				tree_node* now=q.front();
				q.pop();
				node_state[now->pos].second++;
				now->game_cnt++;
				if(win){
					node_state[now->pos].first++;
					now->win_cnt++;
				}
			}

		}
	}

	void dump_root(){
		std::queue<tree_node *> q;
		if(root) q.push(root);
		
		for(int i=0;i<100;i++){ 
			if(root->next[i]&&root->next[i]->game_cnt!=0){
				std::cout << root->next[i]->pos << " " << root->next[i]->win_cnt << " " << root->next[i]->game_cnt << '\n';
			}
		}
		std::cout << '\n' << '\n';
		
		/*
		while(q.size()!=0){
			tree_node* now=q.front();
			q.pop();
			for(int i=0;i<100;i++){
				if(now->next[i]&&now->next[i]->game_cnt!=0){
					if(now->next[i]) q.push(now->next[i]);
				}
			}
			std::cout << now->pos << " " <<  now->win_cnt << " " << now->game_cnt << '\n';
		}
		std::cout << '\n' << '\n';
		*/
	}

	virtual action take_action(const board& state) {
		//std::cout << state << '\n';
		std::shuffle(space.begin(), space.end(), engine);
		std::shuffle(space_opponent.begin(),space_opponent.end(),engine);

		node_state.clear();
		for(auto it:space) node_state[it]=std::make_pair(0,0);

		action::place best_move;		
		
		init(state,who);
		for(int i=0;i<time_control;i++) update();
		if(time_control<600) time_control+=30;
		else time_control-=20;
		//dump_root();
		
		float best_win_rate=0;
		for(int i=0;i<100;i++){ 
			board t=root->b;
			if(root->next[i]&&root->next[i]->pos.apply(t)==board::legal){
				//std::cout << root->next[i]->pos << " " <<  node_state[root->next[i]->pos].first << " " << node_state[root->next[i]->pos].second << '\n';
				
				if(node_state[root->next[i]->pos].second!=0){
					if((float)node_state[root->next[i]->pos].first/node_state[root->next[i]->pos].second>best_win_rate){
						best_win_rate=(float)node_state[root->next[i]->pos].first/node_state[root->next[i]->pos].second;
						best_move=root->next[i]->pos;
					}
				}
				
				/*
				if((float)root->next[i]->win_cnt/root->next[i]->game_cnt>best_win_rate){
					best_win_rate=(float)root->next[i]->win_cnt/root->next[i]->game_cnt;
					best_move=root->next[i]->pos;
				}
				*/	
			}
		}
		
		//std::cout << best_move << " " << best_win_rate << '\n';
		//std::cout << '\n';

		return best_move;
	}

private:
	std::vector<action::place> space;
	std::vector<action::place> space_opponent;
	std::vector<action::place> space1;
	std::vector<action::place> space_opponent1;
	board::piece_type who;
	board::piece_type opponent;
	std::map<action::place,std::pair<int,int>> node_state;
	tree_node *root=nullptr;

	int time_control=10;
};

