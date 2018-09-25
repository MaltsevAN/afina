#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include <iostream> // del
#include <exception>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
public:
    //1024
    explicit SimpleLRU(size_t max_size = 1024) : _max_size(max_size), _cur_size(0) {}

    ~SimpleLRU() {
        _lru_index.clear();
//        _lru_head.reset(); // TODO: Here is stack overflow

        while (_lru_tail != _lru_head.get())
        {
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
    bool Get(const std::string &key, std::string &value) const override;

    void push_front(const std::string &key, const std::string &value);

    bool pop_back();

    void print_map() const;
    void print_list() const;
    size_t get_max_size() const;
    size_t get_cur_size() const;


private:
    struct cmp_for_wraper {
        bool operator()(std::reference_wrapper<const std::string> a, std::reference_wrapper<const std::string> b) const {
            return a.get() < b.get();
        }
    };
    // LRU cache node
    using lru_node = struct lru_node {
        const std::string key; // const may be
        std::string value;
        std::unique_ptr<lru_node> next;
        lru_node* prev = nullptr;
        lru_node () {}
        lru_node(const std::string &k, const std::string &v):
                key(k),
                value(v)
        {
        }
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
    lru_node* _lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, cmp_for_wraper> _lru_index;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
