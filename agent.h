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


class mtcs_player : public random_agent {
public:
	mtcs_player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
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
		
		if(ch){
			for(auto it:rem) node_state[it].first++;
		}
		
		return ch;
	}

	virtual action take_action(const board& state) {
		//std::cout << state << '\n';
		std::shuffle(space.begin(), space.end(), engine);
		std::shuffle(space_opponent.begin(),space_opponent.end(),engine);
		node_state.clear();
		for(auto it:space) node_state[it]=std::make_pair(0,0);

		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal){
				for(int i=0;i<10;i++) {
					node_state[move].second++;
					if(simulation(after,opponent)) node_state[move].first++;
				}
			}
		}
		action::place best_move;
		float best_win_rate=0;
		for(auto it:node_state){
			//std::cout << it.first << " " << it.second.first << " " << it.second.second << '\n';
			if(it.second.second!=0&&(float)it.second.first/it.second.second>best_win_rate){
				best_move=it.first;
				best_win_rate=(float)it.second.first/it.second.second;
			}
		}
		//std::cout << best_win_rate << " " << best_move << '\n';
		//std::cout << '\n' << '\n';
		
		/*
		int cnt=0;
		for(int i=0;i<1000;i++){
			if(simulation(state,who)) cnt++;
		}
		std::cout << cnt << '\n';
		*/
		/*	
		action::place best_move;		
		int best_cnt=0;
		std::shuffle(space.begin(), space.end(), engine);
		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal){
				int cnt=0;
				for(int i=0;i<10;i++) if(simulation(after,opponent)) cnt++;
				if(cnt>best_cnt){
					best_cnt=cnt;
					best_move=move;
				}
			}
		}
		*/
		//std::cout << check << '\n';
		//std::cout << best_move << " " << best_cnt << '\n';
		//std::cout << "_______________" << '\n';
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
