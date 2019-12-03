#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
public:
    SimpleLRU(size_t max_size = 1024) : _max_size(max_size) {
      _lru_head = (std::unique_ptr<lru_node>)(new lru_node);
      _lru_tail = new lru_node;

      _lru_head->prev = nullptr;
      _lru_tail->next = nullptr;

      _lru_head->next = std::unique_ptr<lru_node>(_lru_tail);
      _lru_tail->prev = _lru_head.get();
    }

    ~SimpleLRU() {
        _lru_index.clear();

        while (_lru_head.get() != _lru_tail) {
            _lru_tail = _lru_tail->prev;
            _lru_tail->next.reset();
        }
        _lru_head.reset();
    }

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

private:
    // LRU cache node
    using lru_node = struct lru_node {
        std::string key;
        std::string value;
        lru_node *prev;
        std::unique_ptr<lru_node> next;
    };

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;
    std::size_t _cur_size = 0;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    lru_node* _lru_tail;
    std::unique_ptr<lru_node> _lru_head;

    typedef std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>
      lru_index;
    // Index of nodes from list above, allows fast random access to elements by lru_node#key
     lru_index _lru_index;

    // Add new node to the list and insert it at tail
    void AddNewNode(const std::string &key, const std::string &value);

    // Move to tail of the list
    // i.e. make the most "fresh"
    void MoveToTail(
      std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>::iterator node_iterator);

    // Clean up space until there is enough for new node/value
    bool CleanUpSpace(std::size_t needed_space);
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
