#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
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
    // 1024
    explicit SimpleLRU(size_t max_size = 1024) : _max_size(max_size), _cur_size(0), _need_find(true) {}

    ~SimpleLRU() {
        _lru_index.clear();

        while (_lru_tail != _lru_head.get()) {
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

    void print_map() const;

    void print_list() const;

private:
    struct cmp_for_wraper {
        bool operator()(std::reference_wrapper<const std::string> a,
                        std::reference_wrapper<const std::string> b) const {
            return a.get() < b.get();
        }
    };

    void push_front(const std::string &key, const std::string &value);

    // clear list wile cur_size > max_size and
    void clear_list();

    bool list_pop_back();

    // LRU cache node
    using lru_node = struct lru_node {
        const std::string key; // const may be
        std::string value;
        std::unique_ptr<lru_node> next;
        lru_node *prev;

        lru_node() = default;

        lru_node(const std::string &k, const std::string &v) : key(k), value(v), prev(nullptr), next(nullptr) {}
    };

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;
    std::size_t _cur_size;
    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;
    lru_node *_lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, cmp_for_wraper> _lru_index;

    // need_find == true if need map.find(key)
    // it_find = map.find(key)
    bool _need_find;
    decltype(_lru_index.begin()) _it_find;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
