#include "PQ.h"
#include <iostream>

/* ===== Constructor / Destructor ===== */
PriorityQueue::PriorityQueue(int capacity)
    : capacity(capacity), size(0)
{
    heap = new Reservation *[capacity];
}

PriorityQueue::~PriorityQueue()
{
    delete[] heap;
}

/* ===== Utility ===== */
int PriorityQueue::parent(int i) const { return (i - 1) / 2; }
int PriorityQueue::left(int i) const { return 2 * i + 1; }
int PriorityQueue::right(int i) const { return 2 * i + 2; }

bool PriorityQueue::isEmpty() const { return size == 0; }
bool PriorityQueue::isFull() const { return size == capacity; }

void PriorityQueue::swap(int i, int j)
{
    Reservation *temp = heap[i];
    heap[i] = heap[j];
    heap[j] = temp;
}

/* ===== Heapify Up ===== */
void PriorityQueue::heapifyUp(int index)
{
    while (index > 0 &&
           heap[index]->hasHigherPriorityThan(heap[parent(index)]))
    {
        swap(index, parent(index));
        index = parent(index);
    }
}

/* ===== Heapify Down ===== */
void PriorityQueue::heapifyDown(int index)
{
    int smallest = index;
    int l = left(index);
    int r = right(index);

    if (l < size &&
        heap[l]->hasHigherPriorityThan(heap[smallest]))
        smallest = l;

    if (r < size &&
        heap[r]->hasHigherPriorityThan(heap[smallest]))
        smallest = r;

    if (smallest != index)
    {
        swap(index, smallest);
        heapifyDown(smallest);
    }
}

bool PriorityQueue::hasOverlap(int newStart, int newEnd) const {
    for (int i = 0; i < size; i++) {
        int resStart = heap[i]->getStartDate();
        int resEnd = heap[i]->getEndDate();
        if (newStart <= resEnd && newEnd >= resStart) {
            return true; 
        }
    }
    return false;
}

/* ===== Insert ===== */
void PriorityQueue::insert(Reservation *reservation)
{
    if (isFull())
    {
        std::cout << "Reservation queue is full!\n";
        return;
    }

    heap[size] = reservation;
    heapifyUp(size);
    size++;
}

/* ===== Peek ===== */
Reservation *PriorityQueue::peek() const
{
    if (isEmpty())
        return nullptr;
    return heap[0];
}

/* ===== Extract Min (Highest Priority) ===== */
Reservation *PriorityQueue::extractMin()
{
    if (isEmpty())
        return nullptr;

    Reservation *result = heap[0];
    heap[0] = heap[size - 1];
    size--;
    heapifyDown(0);

    return result;
}