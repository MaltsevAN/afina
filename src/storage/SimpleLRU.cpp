#include "SimpleLRU.h"

namespace Afina {
namespace Backend {


void SimpleLRU::_cut_node(lru_node *cut_node) {
    _cur_size -= cut_node->value.size() + cut_node->key.size();
    if (cut_node->next != nullptr) {
        cut_node->next->prev = cut_node->prev;
    } else {
        _lru_tail = cut_node->prev;
    }
    if (cut_node->prev != nullptr) {
        cut_node->prev->next.swap(cut_node->next);
        cut_node->prev = nullptr;
    } else {
        _lru_head.swap(cut_node->next);
    }
}
bool SimpleLRU::_erase_from_list(lru_node *erase_node) {
    _cut_node(erase_node);
    erase_node->next.reset();
    return true;
}

bool SimpleLRU::_erase_from_storage() {
    if (_lru_head == nullptr) {
        return false;
    }

    // Delete from map
    _lru_index.erase(_lru_tail->key);

    return _erase_from_list(_lru_tail);
}

bool SimpleLRU::_free_space_for_node(const std::string &key, const std::string &value)
{
    std::size_t size_of_node = key.size() + value.size();
    if (size_of_node > _max_size) {
        return false;
    }
    while (size_of_node + _cur_size > _max_size) {
        if (!_erase_from_storage()) {
            return false;
        }
    }
    return true;
}
bool SimpleLRU::_push_node(lru_node *push_node) {
    _cur_size += push_node->key.size() + push_node->value.size();;
    if (_lru_head == nullptr) {
        _lru_head.swap(push_node->next);
        _lru_tail = _lru_head.get();
    } else {
        _lru_head->prev = push_node;
        push_node->next.swap(_lru_head);
    }
    return true;
}

bool SimpleLRU::_insert_to_list(const std::string &key, const std::string &value) {
    if (!_free_space_for_node(key, value))
    {
        return false;
    }
    auto new_node = new lru_node(key, value);
    new_node->next = std::unique_ptr<lru_node>(new_node);
    if (!_push_node(new_node)) {
        return false;
    }
    _insert_to_map();
    return true;
}
void SimpleLRU::_insert_to_map() {
    _lru_index.insert(std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>(
        _lru_head->key, *_lru_head));
}

bool SimpleLRU::_change_value_in_list(lru_node *change_node, const std::string &value) {
    _cut_node(change_node);
    change_node->value = value;
    if (!_free_space_for_node(change_node->key, value))
    {
        return false;
    }
    return _push_node(change_node);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find != _lru_index.end()) {
        if (!_change_value_in_list(&(it_find->second.get()), value)) {
            throw std::overflow_error("Error: Put");
        }
        return true;
    }
    if (!_insert_to_list(key, value)) {
        throw std::overflow_error("Error: Put");
    }
    _insert_to_map();
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find != _lru_index.end()) {
        return false;
    }
    if (!_insert_to_list(key, value)) {
        throw std::overflow_error("Error: PutIfAbsent");
    }
    _insert_to_map();
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find == _lru_index.end()) {
        return false;
    }
    if (!_change_value_in_list(&(it_find->second.get()), value)) {
        throw std::overflow_error("Error: Set");
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it_find = _lru_index.find(key);
    if (it_find == _lru_index.end()) {
        return false;
    }
    lru_node *erase_node = &(it_find->second.get());
    // Delete from map
    _lru_index.erase(it_find);
    return _erase_from_list(erase_node);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it_find = _lru_index.find(key);
    if (it_find == _lru_index.end()) {
        return false;
    }
    auto cur_node = &(it_find->second.get());
    value = cur_node->value;

    _cut_node(cur_node);
    return _push_node(cur_node);
}

} // namespace Backend
} // namespace Afina
