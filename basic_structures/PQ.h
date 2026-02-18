#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "../manager_system/Reservation.h"

class PriorityQueue
{
private:
    Reservation **heap; // array of Reservation pointers

    int parent(int i) const;
    int left(int i) const;
    int right(int i) const;

    void heapifyUp(int index);
    void heapifyDown(int index);
    void swap(int i, int j);

public:
    int capacity;
    int size;
    PriorityQueue(int capacity = 50);
    ~PriorityQueue();

    bool isEmpty() const;
    bool isFull() const;

    bool hasOverlap(int newStart, int newEnd) const;
    
    void insert(Reservation *reservation);
    Reservation *peek() const;
    Reservation *extractMin();
};

#endif