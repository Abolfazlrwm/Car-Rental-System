#ifndef HASH_NODE_H
#define HASH_NODE_H

#include <string>
#include "../manager_system/User.h"
using namespace std;

class HashNode
{
private:
    string key;    
    User *value;    
    HashNode *next;

public:
    HashNode(const string &key, User *value);

    string getKey() const;
    User *getValue() const;
    HashNode *getNext() const;

    void setNext(HashNode *next);
};

#endif