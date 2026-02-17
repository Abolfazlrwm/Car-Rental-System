#include "HashTable.h"

HashTable::HashTable()
{
    for (int i = 0; i < TABLE_SIZE; i++)
        table[i] = nullptr;
}

HashTable::~HashTable()
{
    for (int i = 0; i < TABLE_SIZE; i++)
        destroyChain(table[i]);
}

/* ===== Hash Function ===== */
int HashTable::hash(const string &key) const
{
    unsigned long hashValue = 0;

    for (char c : key)
        hashValue = hashValue * 31 + c;

    return hashValue % TABLE_SIZE;
}

/* ===== Insert ===== */
void HashTable::insert(const string &key, User *value)
{
    int index = hash(key);
    HashNode *head = table[index];

    // check duplicate key
    while (head)
    {
        if (head->getKey() == key)
            return; // username already exists
        head = head->getNext();
    }

    HashNode *newNode = new HashNode(key, value);
    newNode->setNext(table[index]);
    table[index] = newNode;
}

/* ===== Find ===== */
User *HashTable::find(const string &key) const
{
    int index = hash(key);
    HashNode *current = table[index];

    while (current)
    {
        if (current->getKey() == key)
            return current->getValue();
        current = current->getNext();
    }
    return nullptr;
}

/* ===== Remove ===== */
bool HashTable::remove(const string &key)
{
    int index = hash(key);
    HashNode *current = table[index];
    HashNode *prev = nullptr;

    while (current)
    {
        if (current->getKey() == key)
        {
            if (prev)
                prev->setNext(current->getNext());
            else
                table[index] = current->getNext();

            delete current;
            return true;
        }
        prev = current;
        current = current->getNext();
    }
    return false;
}

/* ===== Cleanup ===== */
void HashTable::destroyChain(HashNode *node)
{
    while (node)
    {
        HashNode *temp = node;
        node = node->getNext();
        delete temp;
    }
}