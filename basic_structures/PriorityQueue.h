#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>

/**
 * ============================================================
 *  PriorityQueue<T>
 * ============================================================
 *  A binary min-heap backed by a dynamically growing array.
 *  The element with the HIGHEST priority is always at the root
 *  ("min" refers to the heap order, i.e. the first item to be
 *  served).
 *
 *  Why hand-written?
 *  Used to schedule car reservations: the reservation with the
 *  earliest start date (then earliest request) must be served
 *  first, exactly what a priority queue guarantees.
 *
 *  Complexity:
 *      insert / extractMin ... O(log n)
 *      peek .................. O(1)
 *      forEach ............... O(n)
 *
 *  The heap takes a comparator `higherPriority(a, b)` that
 *  returns true when `a` must be served before `b`.
 *  Capacity doubles automatically when full — the original
 *  implementation silently DROPPED reservations when full,
 *  which is fixed here.
 * ============================================================
 */
template <typename T>
class PriorityQueue
{
public:
    /// Returns true if `a` should be served before `b`.
    using Comparator = std::function<bool(const T &, const T &)>;

private:
    T *heap_;
    std::size_t capacity_;
    std::size_t size_;
    Comparator higherPriority_;

    /* ---------------- Index helpers ---------------- */
    std::size_t parent(std::size_t i) const { return (i - 1) / 2; }
    std::size_t left(std::size_t i) const { return 2 * i + 1; }
    std::size_t right(std::size_t i) const { return 2 * i + 2; }

    void swap(std::size_t i, std::size_t j)
    {
        T tmp = heap_[i];
        heap_[i] = heap_[j];
        heap_[j] = tmp;
    }

    /// Grow the backing array (amortized O(1) per insert).
    void grow()
    {
        std::size_t newCapacity = (capacity_ == 0) ? 16 : capacity_ * 2;
        T *newHeap = new T[newCapacity];
        for (std::size_t i = 0; i < size_; ++i)
            newHeap[i] = heap_[i];
        delete[] heap_;
        heap_ = newHeap;
        capacity_ = newCapacity;
    }

    /* ---------------- Heapify ---------------- */

    void heapifyUp(std::size_t index)
    {
        while (index > 0 &&
               higherPriority_(heap_[index], heap_[parent(index)]))
        {
            swap(index, parent(index));
            index = parent(index);
        }
    }

    void heapifyDown(std::size_t index)
    {
        std::size_t top = index;
        std::size_t l = left(index);
        std::size_t r = right(index);

        if (l < size_ && higherPriority_(heap_[l], heap_[top]))
            top = l;
        if (r < size_ && higherPriority_(heap_[r], heap_[top]))
            top = r;

        if (top != index)
        {
            swap(index, top);
            heapifyDown(top);
        }
    }

public:
    explicit PriorityQueue(Comparator higherPriority)
        : heap_(nullptr), capacity_(0), size_(0), higherPriority_(higherPriority)
    {
        grow(); // start with a small initial block
    }

    ~PriorityQueue()
    {
        delete[] heap_;
    }

    // Non-copyable: the queue owns its backing array.
    PriorityQueue(const PriorityQueue &) = delete;
    PriorityQueue &operator=(const PriorityQueue &) = delete;

    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }

    /// Insert an element, growing the array when needed. O(log n)
    void insert(const T &value)
    {
        if (size_ == capacity_)
            grow();
        heap_[size_] = value;
        heapifyUp(size_);
        ++size_;
    }

    /// Look at the highest-priority element without removing it. O(1)
    const T &peek() const
    {
        if (size_ == 0)
            throw std::out_of_range("PriorityQueue::peek: queue is empty");
        return heap_[0];
    }

    /// Remove and return the highest-priority element. O(log n)
    T extractMin()
    {
        if (size_ == 0)
            throw std::out_of_range("PriorityQueue::extractMin: queue is empty");

        T result = heap_[0];
        heap_[0] = heap_[size_ - 1];
        --size_;
        if (size_ > 0)
            heapifyDown(0);
        return result;
    }

    /// Visit every element in arbitrary (heap) order.
    void forEach(const std::function<void(const T &)> &fn) const
    {
        for (std::size_t i = 0; i < size_; ++i)
            fn(heap_[i]);
    }
};

#endif // PRIORITY_QUEUE_H
