#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
  // Make sure there is enough memory in List
  if (key.size() + value.size() > _max_size)
    return false;
  while(_cur_size > _max_size) {
    lru_node *node_to_del = _lru_head->next.get();

    _lru_index.erase(node_to_del->key);
    _cur_size -= node_to_del->key.size() + node_to_del->value.size();

    // Rearrange pointers as if there was no node that we are deleting in the list
    node_to_del->next->prev = node_to_del->prev;
    std::swap(node_to_del->prev->next, node_to_del->next);

    // Delete node
    // by resetting unique pointer corresponding to it
    node_to_del->next.reset();
  }

  auto node_iterator = _lru_index.find(key);

  // Key exists in List
  // Change corresponding node value and make it the "freshest"
  if (node_iterator != _lru_index.end()) {
    node_iterator->second.get().value = value;
    MoveToTail(node_iterator);
  }
  // Key doesnt exist in List
  // Add a new node and insert it at tail
  // i.e. as the "freshest" one
  else {
    AddNewNode(key, value);
  }

  return true;
 }

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
  if(_lru_index.find(key) != _lru_index.end())
    return false;

  AddNewNode(key, value);
  return true;
 }

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
  auto node_iterator = _lru_index.find(key);

  // Key doesnt exist
  if(node_iterator == _lru_index.end())
    return false;

  node_iterator->second.get().value = value;
  MoveToTail(node_iterator);

  return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
  auto node_iterator = _lru_index.find(key);
  if(node_iterator == _lru_index.end())
   return false;

  lru_node &node_to_del = node_iterator->second;

  _lru_index.erase(node_iterator);
  _cur_size -= node_to_del.key.size() + node_to_del.value.size();

  // Rearrange pointers as if there was no node that we are deleting in the list
  node_to_del.next->prev = node_to_del.prev;
  std::swap(node_to_del.prev->next, node_to_del.next);

  // Delete node
  // by resetting unique pointer corresponding to it
  node_to_del.next.reset();

  return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
  auto node_iterator = _lru_index.find(key);
  if (node_iterator == _lru_index.end())
    return false;

  value = node_iterator->second.get().value;
  MoveToTail(node_iterator);

  return true;
}

void SimpleLRU::AddNewNode(const std::string &key, const std::string &value) {
  // Allocate new node
  lru_node *new_node =
    new lru_node{std::move(key), std::move(value), _lru_tail->prev, nullptr};

  // Setup pointers for swap
  new_node->next = std::unique_ptr<lru_node>(new_node);

  // Swap *next pointer of
  // _lru_tail->prev->next i.e. pointer to dummy tail
  // and new_node->next i.e. pointer to new real tail
  // thus new_node->next now points to dummy tail (as it is new real tail)
  // and _lru_tail->prev->next (i.e. second to last node) now points to real tail
  std::swap(_lru_tail->prev->next, new_node->next);
  _lru_tail->prev = new_node;

  _cur_size += key.size() + value.size();
  _lru_index.insert({_lru_tail->prev->key, *_lru_tail->prev});
}

void SimpleLRU::MoveToTail(
  std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>::iterator node_iterator) {
    lru_node &node_to_move = node_iterator->second;

    if (_lru_tail->prev->key == node_to_move.key)
        return;

    // Rearrange pointers as if there was no node that we are moving in the list
    node_to_move.next->prev = node_to_move.prev;
    std::swap(node_to_move.prev->next, node_to_move.next);

    // And insert it at the tail
    node_to_move.prev = _lru_tail->prev;
    std::swap(_lru_tail->prev->next, node_to_move.next);

    _lru_tail->prev = &node_to_move;
}

} // namespace Backend
} // namespace Afina
