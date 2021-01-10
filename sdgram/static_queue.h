//
// A simple static queue.
//
// author: aleksandar
//

#pragma once

namespace SerialDatagram {

template<
    typename T,
    uint8_t Capacity>
class StaticQueue {
public:
    StaticQueue()
            : head(0),
            tail(0),
            is_full(false) {
        // empty
    }

    bool IsEmpty() const {
        return head == tail && !is_full;
    }

    bool IsFull() const {
        return is_full;
    }

    T &Peek() {
        return items[head];
    }

    T Pop() {
        if(head == tail) {
            is_full = false;
        }

        auto old_head = head;
        head = Advance(head);

        return items[old_head];
    }

    void Push(T val) {
        items[tail] = val;

        tail = Advance(tail);
        is_full = (head == tail);
    }

private:
    //
    // Functions.
    //
    uint8_t Advance(uint8_t ptr) {
        ptr++;

        if(ptr == Capacity) {
            ptr = 0;
        }

        return ptr;
    }

    //
    // Data.
    //
    T items[Capacity];
    uint8_t head;
    uint8_t tail;
    bool is_full;
};

}
