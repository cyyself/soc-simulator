#ifndef SIMPLE_DELAY_MODEL
#define SIMPLE_DELAY_MODEL

#include "memory_timing_model.hpp"

#include <utility>

class simple_delay_model: public memory_timing_model {
public:
    simple_delay_model(uint32_t delay):delay(delay) {
        reset();
    }
    void add_req(uint64_t addr, uint64_t size, bool is_write, int64_t req_id) {
        unfinished_queue.emplace(time + delay, req_id);
        queue_move();
    }
    bool has_finished_req() {
        return !finished_queue.empty();
    }
    bool has_pending_req() {
        return !unfinished_queue.empty() || !finished_queue.empty();
    }
    int64_t get_finished_req_id() {
        if (finished_queue.empty()) return -1;
        else {
            int64_t ret = finished_queue.front();
            finished_queue.pop();
            return ret;
        }
    }
    void tick() {
        time ++;
        queue_move();
    }
    void reset() {
        time = 0;
        while (!unfinished_queue.empty()) unfinished_queue.pop();
        while (!finished_queue.empty()) finished_queue.pop();
    }
private:
    void queue_move() {
        while (!unfinished_queue.empty() && unfinished_queue.front().first == time) {
            finished_queue.push(unfinished_queue.front().second);
            unfinished_queue.pop();
        }
    }
    uint32_t delay;
    uint64_t time;
    std::queue<std::pair<uint64_t, int64_t> > unfinished_queue;
    std::queue<int64_t> finished_queue;
};

#endif