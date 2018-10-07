#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

bool SimpleLRU::_erase_from_list(lru_node *erase_node) {
    _cur_size -= erase_node->value.size() + erase_node->key.size();
    if (_lru_head.get() == _lru_tail) {
        _lru_head.reset();
        _lru_tail = nullptr;
    } else if (erase_node == _lru_head.get()) {
        auto tmp = std::move(_lru_head->next);
        _lru_head.reset();
        _lru_head = std::move(tmp);
        _lru_head->prev = nullptr;
    } else if (erase_node == _lru_tail) {
        _lru_tail = _lru_tail->prev;
        _lru_tail->next.reset();
    } else {
        auto tmp = std::move(erase_node->prev->next);
        erase_node->next->prev = erase_node->prev;
        erase_node->prev->next = std::move(erase_node->next);
        tmp.reset();
    }
    return true;
}

bool SimpleLRU::_remove_old_data() {
    if (_lru_head == nullptr) {
        return false;
    }

    // Delete from map
    _lru_index.erase(_lru_tail->key);

    return _erase_from_list(_lru_tail);
}

bool SimpleLRU::_insert_to_list(const std::string &key, const std::string &value) {
    std::size_t size_of_node = key.size() + value.size();
    if (size_of_node > _max_size) {
        return false;
    }
    while (size_of_node + _cur_size > _max_size) {
        if (!_remove_old_data()) {
            return false;
        }
    }

    std::unique_ptr<lru_node> node = std::unique_ptr<lru_node>(new lru_node(key, value));
    _cur_size += size_of_node;
    if (_lru_head == nullptr) {
        _lru_head = std::move(node);
        _lru_tail = _lru_head.get();
    } else {
        _lru_head->prev = node.get();
        node->next = std::move(_lru_head);
        _lru_head = std::move(node);
    }
    return true;
}
void SimpleLRU::_insert_new_data_in_map() {
    _lru_index.insert(std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>(
        _lru_head->key, *_lru_head));
}

bool SimpleLRU::_change_value_in_list(lru_node *node, const std::string &value) {
    _erase_from_list(node);
    return _insert_to_list(node->key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find != _lru_index.end()) {
        return _change_value_in_list(&(it_find->second.get()), value);
    }
    if (!_insert_to_list(key, value)) {
        return false;
    }
    _insert_new_data_in_map();
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find != _lru_index.end()) {
        return false;
    }
    if (!_insert_to_list(key, value)) {
        return false;
    }
    _insert_new_data_in_map();
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find == _lru_index.end()) {
        return false;
    }
    return _change_value_in_list(&(it_find->second.get()), value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it_find = _lru_index.find(key);
    if (it_find == _lru_index.end()) {
        return false;
    }
    // Delete from map
    _lru_index.erase(it_find);

    return _erase_from_list(&(it_find->second.get()));
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find == _lru_index.end()) {
        return false;
    }
    auto cur_node = &(it_find->second.get());
    value = cur_node->value;

    // Moving cur_node to _lru_head
    if (cur_node != _lru_head.get()) {
        if (cur_node == _lru_tail) {
            _lru_head->prev = cur_node;
            _lru_tail->next = std::move(_lru_head);
            _lru_tail = _lru_tail->prev;
            _lru_head = std::move(_lru_tail->next);
            _lru_head->prev = nullptr;
        } else {
            _lru_head->prev = cur_node;
            cur_node->next->prev = cur_node->prev;
            auto tmp = std::move(cur_node->prev->next);
            cur_node->prev->next = std::move(cur_node->next);
            cur_node->next = std::move(_lru_head);
            _lru_head = std::move(tmp);
            _lru_head->prev = nullptr;
        }
    }
    // Update map
    (*it_find).second = *_lru_head;
    return true;
}

} // namespace Backend
} // namespace Afina
