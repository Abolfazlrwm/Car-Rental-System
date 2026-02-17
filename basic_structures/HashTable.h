#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <string>
#include "HashNode.h"
#include "../manager_system/User.h"

class HashTable
{
private:
    static const int TABLE_SIZE = 101; // prime number
    HashNode *table[TABLE_SIZE];

    int hash(const string &key) const;
    void destroyChain(HashNode *node);

public:
    HashTable();
    ~HashTable();

    void insert(const string &key, User *value);
    User *find(const string &key) const;
    bool remove(const string &key);

    string get(const string &username) const;
};

#endif