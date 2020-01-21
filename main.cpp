#include <algorithm>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <unordered_map>
#include <map>
#include <vector>
#include <queue>

using namespace std;

#define INPUT_SIZE 64000  // 1 page means 1MB
#define RAM_SIZE    4000    // 1 means 1MB
#define STORAGE_SIZE 1000
#define ZPOOL_RATE 40      // percentage

// speed factors
#define RAM_SPEED   15000
#define ZPOOL_SPEED   2000
#define RAM_EVICTION_TIME (RAM_SPEED / 10)

/* function proto type */
float ram_rd(int addr);
float ram_wr(int addr);
float zpool_rd(int addr, int &depth);
float zpool_wr(int addr, int &depth);
int ram_eviction(void);
bool isinzpool(int addr);

long get_time(void) {
	static long count = 0;
	return ++count;
}

int main(int argc, char *argv[]) {
	int m_cnt = 0;
	float m_time = 0;
	float mw_time = 0, mw_cnt = 0;
	float mr_time = 0, mr_cnt = 0;
	float ret;
	bool flag = false;
	int count = 0;
	int init_write=30;
	while(init_write--)
		ram_wr(random()%RAM_SIZE);
	while(++count < INPUT_SIZE) {
		int addr = random()%INPUT_SIZE;
		if(flag^=1) {
			int read_cnt = 1000;
			while(read_cnt-- && (ret=ram_rd(addr)) == -1)
				addr = random()%INPUT_SIZE;
			if(ret > 0) {
				mr_time += ret;
				mr_cnt++;
				m_time += ret;
				m_cnt++;
			} else
				continue;
		} else {
			ret = ram_wr(addr);
			mw_time += ret;
			mw_cnt++;
			m_time += ret;
			m_cnt++;
		}
		if((m_cnt%100)==0) {
			float avg = m_time/100.;
			float w_avg = mw_time/mw_cnt;
			float r_avg = 0;
			if(mr_cnt > 0) r_avg = mr_time/mr_cnt;
			m_time = 0;
			mw_time = mw_cnt = mr_time = mr_cnt = 0;
			cout << avg << '\t' << r_avg << '\t' << w_avg << endl;
		}
	}
}

// input: address to access
// return: -1 if it doesn't exist, speed if it has it.
unordered_map<int, long> RAM_TBL;
vector<int> RAM_Q; // maintain cache for RAM size. addr.
int ram_cache_size=(RAM_SIZE*(100-ZPOOL_RATE)/100);
float ram_rd(int addr) {
	float weight = 1. - (float)RAM_TBL.size()/(float)ram_cache_size;
	if(weight > 0.2) weight = 1;
	else weight += 0.4;
	float t = 0;
	int depth = 0;
	if(RAM_TBL.count(addr) > 0)
		return RAM_SPEED*weight;
	// check next level memory which is zpool
	float ret = zpool_rd(addr, depth);
	long time = get_time();
	if(ret > 0) {
		t += ret;
		// ram cache evict to next level then rd it from zpool and save
		// it in ram
		if(RAM_TBL.size() >= ram_cache_size) {
			int old_addr = ram_eviction();
			t += RAM_EVICTION_TIME;
			t += zpool_wr(old_addr, depth);
			RAM_TBL[addr] = time;
			RAM_Q.push_back(addr);
			t += RAM_SPEED;
			depth += 4;
		} else if(isinzpool(addr)) {
			// zpool eviction TBD
			RAM_TBL[addr] = time;
			RAM_Q.push_back(addr);
			return weight*ZPOOL_SPEED;
		}else {
			RAM_TBL[addr] = time;
			RAM_Q.push_back(addr);
			t += RAM_SPEED;
			depth += 2;
		}
		return weight*t/depth;
	}
	return -1;
}

int ram_eviction(void) {
	if(RAM_Q.empty()) return -1;
	int addr = RAM_Q[0]; 
	RAM_TBL.erase(addr); // erase address
	RAM_Q.erase(RAM_Q.begin());
	return addr;
}

float ram_wr(int addr) {
	float weight = 1. - (float)RAM_TBL.size()/(float)ram_cache_size;
	if(weight > 0.2) weight = 1;
	else weight += 0.4;
	float t = 0;
	int depth = 0;
	int time = get_time();
	if(RAM_TBL.count(addr)>0) {
		RAM_TBL[addr] = time;
		auto it = find(RAM_Q.begin(), RAM_Q.end(), addr);
		RAM_Q.erase(it);
		RAM_Q.push_back(addr);
		return RAM_SPEED*weight;
	} else if(RAM_TBL.size() >= ram_cache_size) {
		// cache eviction in RAM
		int old_addr = ram_eviction();
		t += RAM_EVICTION_TIME;
		t += zpool_wr(old_addr, depth);
		t += RAM_SPEED;
		RAM_TBL[addr] = time;
		RAM_Q.push_back(addr);
		depth += 4;
		return weight*t/depth;
	} else if(isinzpool(addr)) {
		// zpool eviction TBD
		RAM_TBL[addr] = time;
		RAM_Q.push_back(addr);
		return weight*ZPOOL_SPEED;
	} else {
		RAM_TBL[addr] = time;
		RAM_Q.push_back(addr);
		return weight*RAM_SPEED;
	}
}

// input: address to access
// return: -1 if it doesn't exist, speed if it has it.
unordered_map<int, long> ZPOOL_TBL;
vector<int> ZPOOL_Q; // maintain cache for RAM size. addr.
int zpool_cache_size=(RAM_SIZE*ZPOOL_RATE/100)*3; // compressed zpool
float zpool_rd(int addr, int &depth) {
	depth++;
	if(ZPOOL_TBL.count(addr) > 0)
		return ZPOOL_SPEED;
	// add next level access later
	return -1; // page fault
}

bool zpool_full_flag = false;
float zpool_wr(int addr, int &depth) {
	depth++;
	if(ZPOOL_TBL.size() >= zpool_cache_size) {
		if(!zpool_full_flag){
			cout << "ZPOOL FULL" << endl;
			zpool_full_flag = true;
		}
		// need next level function later to add zpool handler.
		depth += 4;
		return -1;
	} else {
		if(ZPOOL_TBL.count(addr)>0) {
			ZPOOL_TBL[addr] = get_time();
			depth+=2;
			return ZPOOL_SPEED;
		}
		ZPOOL_TBL[addr] = get_time();
		return ZPOOL_SPEED;
	}
}

bool isinzpool(int addr) {
	return ZPOOL_TBL.count(addr) > 0;
}
