#ifndef DOUBLYLINKEDLIST_H
#define DOUBLYLINKEDLIST_H

#include "Node.h"
#include <string>

class DoublyLinkedList
{
private:
    Node *head;
    Node *tail;
    int size;

public:
    void pushFront(const string &value);
    void pushBack(const string &value);
    void insert(int pos, const string &value);

    void popFront();
    void popBack();
    void deleteAt(int pos);

    int search(const string &value);
    string getAt(int pos);
    void clear();

    const string &front();
    const string &back();

    bool empty();
    int Size();

    DoublyLinkedList();
    ~DoublyLinkedList();
};

#endif
