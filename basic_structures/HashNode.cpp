#include "HashNode.h"

HashNode::HashNode(const string &key, User *value)
    : key(key), value(value), next(nullptr) {}

string HashNode::getKey() const { return key; }

User *HashNode::getValue() const { return value; }

HashNode *HashNode::getNext() const { return next; }

void HashNode::setNext(HashNode *nextNode) { next = nextNode; }
