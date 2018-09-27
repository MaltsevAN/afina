#include "SimpleLRU.h"

namespace Afina {
    namespace Backend {
        void SimpleLRU::clear_list() {
            bool is_poped = _lru_head != nullptr;;
            bool overflow = _cur_size > _max_size;
            while (overflow && is_poped) {

                is_poped = list_pop_back();
                overflow = _cur_size > _max_size;
            }
            if (overflow) {
                throw std::overflow_error("value+key > max_size");
            }
        }

        void SimpleLRU::push_front(const std::string &key, const std::string &value) {

            //push to list
            std::unique_ptr<lru_node> node = std::unique_ptr<lru_node>(new lru_node(key, value));
            if (_lru_head == nullptr) {
                _lru_head = std::move(node);
                _lru_tail = _lru_head.get();
            } else {
                _lru_head->prev = node.get();
                node->next = std::move(_lru_head);
                _lru_head = std::move(node);
            }
            // push to map
            _lru_index.insert(std::pair<std::reference_wrapper<const std::string>,
                    std::reference_wrapper<lru_node>>(_lru_head->key, *_lru_head));

        }

        bool SimpleLRU::list_pop_back() {
            if (_lru_head == nullptr) {
                return false;
            }

            // Delete from map
            _lru_index.erase(_lru_tail->key);

            //Delete form list
            _cur_size -= _lru_tail->value.size() + _lru_tail->key.size();
            if (_lru_head.get() == _lru_tail) {
                _lru_head.reset();
                _lru_tail = nullptr;
            } else {
                _lru_tail = _lru_tail->prev;
                _lru_tail->next.reset();
            }
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Put(const std::string &key, const std::string &value) {
            bool is_absent = true;
            _it_find = _lru_index.find(key);
            _need_find = false;
            if (_it_find != _lru_index.end()) {
                is_absent = false;
            }
            if (is_absent) {
                try {
                    PutIfAbsent(key, value);
                } catch (const std::exception &exception) {
                    throw exception;
                }
            } else {
                try {
                    Set(key, value);
                } catch (const std::exception &exception) {
                    throw exception;
                }
            }
            _need_find = true;
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
            if (_need_find) {
                _it_find = _lru_index.find(key);
                if (_it_find != _lru_index.end()) {
                    return false;
                }
            }
            _cur_size += value.size() + key.size();
            bool is_overflow = _cur_size > _max_size;
            if (is_overflow) {
                try {
                    clear_list();
                } catch (const std::exception &exception) {
                    throw exception;
                }
            }
            push_front(key, value);

            return true;
        }


// See MapBasedGlobalLockImpl.h

// First version (with moving)
        bool SimpleLRU::Set(const std::string &key, const std::string &value) {
            if (_need_find) {
                _it_find = _lru_index.find(key);
                if (_it_find == _lru_index.end()) {
                    return false;
                }
            }

            _need_find = false;
            Delete(key);
            PutIfAbsent(key, value);
            _need_find = true;
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Delete(const std::string &key) {
            if (_need_find) {
                _it_find = _lru_index.find(key);
                if (_it_find == _lru_index.end()) {
                    return false;
                }
            }

            auto del_node = &((*_it_find).second.get());

            // Delete from map
            _lru_index.erase(_it_find);

            // Delete from list
            _cur_size -= del_node->value.size() + del_node->key.size();
            if (_lru_head.get() == _lru_tail) {
                _lru_head.reset();
                _lru_tail = nullptr;
            } else if (del_node == _lru_head.get()) {
                auto tmp = std::move(_lru_head->next);
                _lru_head.reset();
                _lru_head = std::move(tmp);
                _lru_head->prev = nullptr;
            } else if (del_node == _lru_tail) {
                _lru_tail = _lru_tail->prev;
                _lru_tail->next.reset();
            } else {
                auto tmp = std::move(del_node->prev->next);
                del_node->next->prev = del_node->prev;
                del_node->prev->next = std::move(del_node->next);
                tmp.reset();
            }
            return true;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Get(const std::string &key, std::string &value) {
            _it_find = _lru_index.find(key);
            if (_it_find == _lru_index.end()) {
                return false;
            }
            auto cur_node = &((*_it_find).second.get());
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
            (*_it_find).second = *_lru_head;
            return true;
        }

    } // namespace Backend
} // namespace Afina
