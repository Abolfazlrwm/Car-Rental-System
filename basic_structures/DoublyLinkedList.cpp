#include "DoublyLinkedList.h"

DoublyLinkedList::DoublyLinkedList()
    : head(nullptr), tail(nullptr), size(0) {}

DoublyLinkedList::~DoublyLinkedList()
{
    clear();
}

void DoublyLinkedList::pushFront(const string &value)
{
    Node *newNode = new Node(value);
    if (!head)
        head = tail = newNode;
    else
    {
        newNode->next = head;
        head->prev = newNode;
        head = newNode;
    }
    size++;
}

void DoublyLinkedList::pushBack(const string &value)
{
    Node *newNode = new Node(value);
    if (!tail)
        head = tail = newNode;
    else
    {
        tail->next = newNode;
        newNode->prev = tail;
        tail = newNode;
    }
    size++;
}

void DoublyLinkedList::insert(int pos, const string &value)
{
    if (pos < 0 || pos > size)
        return;

    if (pos == 0)
    {
        pushFront(value);
        return;
    }
    if (pos == size)
    {
        pushBack(value);
        return;
    }

    Node *newNode = new Node(value);
    Node *curr = head;

    for (int i = 0; i < pos - 1; i++)
        curr = curr->next;

    newNode->next = curr->next;
    newNode->prev = curr;
    curr->next->prev = newNode;
    curr->next = newNode;

    size++;
}

void DoublyLinkedList::popFront()
{
    if (!head)
        return;

    Node *temp = head;
    head = head->next;

    if (head)
        head->prev = nullptr;
    else
        tail = nullptr;

    delete temp;
    size--;
}

void DoublyLinkedList::popBack()
{
    if (!tail)
        return;

    Node *temp = tail;
    tail = tail->prev;

    if (tail)
        tail->next = nullptr;
    else
        head = nullptr;

    delete temp;
    size--;
}

void DoublyLinkedList::deleteAt(int pos)
{
    if (pos < 0 || pos >= size)
        return;

    if (pos == 0)
    {
        popFront();
        return;
    }
    if (pos == size - 1)
    {
        popBack();
        return;
    }

    Node *curr = head;
    for (int i = 0; i < pos; i++)
        curr = curr->next;

    curr->prev->next = curr->next;
    curr->next->prev = curr->prev;

    delete curr;
    size--;
}

int DoublyLinkedList::search(const string &value)
{
    Node *curr = head;
    int index = 0;

    while (curr)
    {
        if (curr->data == value)
            return index;
        curr = curr->next;
        index++;
    }
    return -1;
}

string DoublyLinkedList::getAt(int pos)
{
    if (pos < 0 || pos >= size)
        return "";

    Node *curr = head;
    for (int i = 0; i < pos; i++)
        curr = curr->next;

    return curr->data;
}

const string &DoublyLinkedList::front()
{
    return head->data;
}

const string &DoublyLinkedList::back()
{
    return tail->data;
}

bool DoublyLinkedList::empty()
{
    return size == 0;
}

int DoublyLinkedList::Size()
{
    return size;
}

void DoublyLinkedList::clear()
{
    while (head)
    {
        Node *temp = head;
        head = head->next;
        delete temp;
    }
    tail = nullptr;
    size = 0;
}
